#include "AndroidTvRemoteAdapter.h"

#include <ESPmDNS.h>

#include <cstring>

#include "remote/RemoteKeycode.h"

namespace {
constexpr std::uint16_t kDefaultRemotePort = 6466;
constexpr std::uint16_t kPairingPort = 6467;
constexpr unsigned long kDiscoveryRetryMs = 5000;
constexpr const char* kAndroidTvService = "androidtvremote2";
constexpr const char* kAndroidTvProtocol = "tcp";
constexpr const char* kPairingServiceName = "GlobalController";

bool isEmpty(const char* value) {
    return value == nullptr || value[0] == '\0';
}

bool isValidAddress(const IPAddress& address) {
    return address != IPAddress(0, 0, 0, 0);
}

bool looksLikeXiaomi(const String& hostname) {
    String normalized = hostname;
    normalized.toLowerCase();
    return normalized.indexOf("xiaomi") >= 0 || normalized.indexOf("mitv") >= 0;
}
}  // namespace

AndroidTvRemoteAdapter::AndroidTvRemoteAdapter()
    : state_(AndroidTvRemoteState::Disabled),
      tvIp_(0, 0, 0, 0),
      remotePort_(kDefaultRemotePort),
      configured_(false),
      pairingCodeSubmitted_(false),
      mdnsStarted_(false),
      lastDiscoveryAttemptMs_(0),
      pairingServiceName_{} {
    std::strncpy(
        pairingServiceName_,
        kPairingServiceName,
        sizeof(pairingServiceName_) - 1
    );
}

void AndroidTvRemoteAdapter::begin(
    const char* ssid,
    const char* password
) {
    configured_ = !isEmpty(ssid);
    pairingCodeSubmitted_ = false;
    tvIp_ = IPAddress(0, 0, 0, 0);
    remotePort_ = kDefaultRemotePort;

    if (!configured_) {
        state_ = AndroidTvRemoteState::Disabled;
        Serial.println("Android TV Wi-Fi remote is not configured");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password == nullptr ? "" : password);
    state_ = AndroidTvRemoteState::WifiConnecting;
    Serial.printf("Connecting to Wi-Fi SSID: %s\n", ssid);
}

void AndroidTvRemoteAdapter::loop() {
    switch (state_) {
        case AndroidTvRemoteState::Disabled:
        case AndroidTvRemoteState::Error:
            return;

        case AndroidTvRemoteState::WifiConnecting:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf(
                    "Wi-Fi connected: %s\n",
                    WiFi.localIP().toString().c_str()
                );
                startDiscovery();
            }
            return;

        case AndroidTvRemoteState::TvDiscovering:
            if (WiFi.status() != WL_CONNECTED) {
                if (mdnsStarted_) {
                    MDNS.end();
                    mdnsStarted_ = false;
                }
                WiFi.reconnect();
                state_ = AndroidTvRemoteState::WifiConnecting;
                return;
            }

            if (millis() - lastDiscoveryAttemptMs_ >= kDiscoveryRetryMs) {
                discoverTv();
            }
            return;

        case AndroidTvRemoteState::RemoteConnecting:
        case AndroidTvRemoteState::Ready:
            if (WiFi.status() != WL_CONNECTED) {
                if (mdnsStarted_) {
                    MDNS.end();
                    mdnsStarted_ = false;
                }
                WiFi.reconnect();
                state_ = AndroidTvRemoteState::WifiConnecting;
                return;
            }

            remoteManager_.loop();

            if (remoteManager_.error_auth) {
                remoteManager_.error_auth = false;
                startPairing();
                return;
            }

            if (remoteManager_.connected()) {
                state_ = AndroidTvRemoteState::Ready;
            }
            return;

        case AndroidTvRemoteState::PairingConnecting:
        case AndroidTvRemoteState::PairingCodeRequired:
        case AndroidTvRemoteState::PairingSubmitting:
            if (WiFi.status() != WL_CONNECTED) {
                if (mdnsStarted_) {
                    MDNS.end();
                    mdnsStarted_ = false;
                }
                WiFi.reconnect();
                state_ = AndroidTvRemoteState::WifiConnecting;
                return;
            }

            pairingManager_.loop();

            if (pairingManager_.isSecure && !pairingCodeSubmitted_) {
                state_ = AndroidTvRemoteState::PairingCodeRequired;
                return;
            }

            if (
                pairingCodeSubmitted_ &&
                !pairingManager_.connected()
            ) {
                pairingCodeSubmitted_ = false;
                connectRemote();
            }
            return;
    }
}

bool AndroidTvRemoteAdapter::submitPairingCode(const String& code) {
    if (state_ != AndroidTvRemoteState::PairingCodeRequired) {
        return false;
    }

    String normalized = code;
    normalized.trim();
    normalized.toUpperCase();

    if (normalized.length() != 6) {
        Serial.println("Android TV pairing code must contain 6 hexadecimal characters");
        return false;
    }

    for (std::size_t index = 0; index < normalized.length(); ++index) {
        const char character = normalized[index];
        const bool isDigit = character >= '0' && character <= '9';
        const bool isHexLetter = character >= 'A' && character <= 'F';
        if (!isDigit && !isHexLetter) {
            Serial.println("Android TV pairing code contains a non-hexadecimal character");
            return false;
        }
    }

    if (!pairingManager_.sendCode(normalized)) {
        state_ = AndroidTvRemoteState::Error;
        return false;
    }

    pairingCodeSubmitted_ = true;
    state_ = AndroidTvRemoteState::PairingSubmitting;
    return true;
}

bool AndroidTvRemoteAdapter::send(TvCommand command) {
    if (!ready() || !remoteManager_.connected()) {
        return false;
    }

    Remote__RemoteKeyCode keyCode;
    if (!toRemoteKey(command, keyCode)) {
        return false;
    }

    return remoteManager_.sendKey(
        keyCode,
        REMOTE__REMOTE_DIRECTION__SHORT
    );
}

AndroidTvRemoteState AndroidTvRemoteAdapter::state() const {
    return state_;
}

bool AndroidTvRemoteAdapter::configured() const {
    return configured_;
}

bool AndroidTvRemoteAdapter::ready() const {
    return state_ == AndroidTvRemoteState::Ready;
}

bool AndroidTvRemoteAdapter::pairingCodeRequired() const {
    return state_ == AndroidTvRemoteState::PairingCodeRequired;
}

const char* AndroidTvRemoteAdapter::stateLabel() const {
    switch (state_) {
        case AndroidTvRemoteState::Disabled:
            return "Wi-Fi config required";
        case AndroidTvRemoteState::WifiConnecting:
            return "Connecting Wi-Fi";
        case AndroidTvRemoteState::TvDiscovering:
            return "Discovering Android TV";
        case AndroidTvRemoteState::RemoteConnecting:
            return "Connecting TV";
        case AndroidTvRemoteState::PairingConnecting:
            return "Starting pairing";
        case AndroidTvRemoteState::PairingCodeRequired:
            return "Enter TV pairing code";
        case AndroidTvRemoteState::PairingSubmitting:
            return "Finishing pairing";
        case AndroidTvRemoteState::Ready:
            return "Wi-Fi remote ready";
        case AndroidTvRemoteState::Error:
            return "Wi-Fi remote error";
    }

    return "Unknown";
}

void AndroidTvRemoteAdapter::startDiscovery() {
    if (!mdnsStarted_) {
        char hostname[32];
        const unsigned long chipSuffix = static_cast<unsigned long>(
            ESP.getEfuseMac() & 0xFFFFFFULL
        );
        std::snprintf(
            hostname,
            sizeof(hostname),
            "globalcontroller-%06lX",
            chipSuffix
        );

        if (!MDNS.begin(hostname)) {
            state_ = AndroidTvRemoteState::Error;
            Serial.println("Failed to start mDNS for Android TV discovery");
            return;
        }

        mdnsStarted_ = true;
        Serial.printf("mDNS started as %s.local\n", hostname);
    }

    state_ = AndroidTvRemoteState::TvDiscovering;
    lastDiscoveryAttemptMs_ = 0;
    discoverTv();
}

bool AndroidTvRemoteAdapter::discoverTv() {
    lastDiscoveryAttemptMs_ = millis();
    Serial.printf(
        "Searching for _%s._%s.local\n",
        kAndroidTvService,
        kAndroidTvProtocol
    );

    const int serviceCount = MDNS.queryService(
        kAndroidTvService,
        kAndroidTvProtocol
    );

    if (serviceCount <= 0) {
        Serial.println("No Android TV Remote v2 service found; retrying");
        return false;
    }

    int fallbackIndex = -1;
    int selectedIndex = -1;

    for (int index = 0; index < serviceCount; ++index) {
        const IPAddress address = MDNS.IP(index);
        const std::uint16_t port = MDNS.port(index);
        const String hostname = MDNS.hostname(index);

        Serial.printf(
            "Discovered Android TV service: host=%s ip=%s port=%u\n",
            hostname.c_str(),
            address.toString().c_str(),
            port
        );

        if (!isValidAddress(address)) {
            continue;
        }

        if (fallbackIndex < 0) {
            fallbackIndex = index;
        }

        if (looksLikeXiaomi(hostname)) {
            selectedIndex = index;
            break;
        }
    }

    if (selectedIndex < 0) {
        selectedIndex = fallbackIndex;
    }

    if (selectedIndex < 0) {
        Serial.println("Android TV services had no usable IPv4 address; retrying");
        return false;
    }

    tvIp_ = MDNS.IP(selectedIndex);
    remotePort_ = MDNS.port(selectedIndex);
    if (remotePort_ == 0) {
        remotePort_ = kDefaultRemotePort;
    }

    Serial.printf(
        "Selected Android TV at %s:%u\n",
        tvIp_.toString().c_str(),
        remotePort_
    );

    connectRemote();
    return true;
}

void AndroidTvRemoteAdapter::connectRemote() {
    state_ = AndroidTvRemoteState::RemoteConnecting;
    remoteManager_.start(tvIp_, remotePort_);

    if (remoteManager_.connected()) {
        state_ = AndroidTvRemoteState::Ready;
        Serial.printf(
            "Android TV remote connected to %s:%u\n",
            tvIp_.toString().c_str(),
            remotePort_
        );
    }
}

void AndroidTvRemoteAdapter::startPairing() {
    state_ = AndroidTvRemoteState::PairingConnecting;
    pairingCodeSubmitted_ = false;
    pairingManager_.isSecure = false;
    pairingManager_.begin(tvIp_, kPairingPort, pairingServiceName_);
}

bool AndroidTvRemoteAdapter::toRemoteKey(
    TvCommand command,
    Remote__RemoteKeyCode& keyCode
) {
    switch (command) {
        case TvCommand::Power:
            keyCode = KEYCODE_POWER;
            return true;
        case TvCommand::VolumeUp:
            keyCode = KEYCODE_VOLUME_UP;
            return true;
        case TvCommand::VolumeDown:
            keyCode = KEYCODE_VOLUME_DOWN;
            return true;
        case TvCommand::Mute:
            keyCode = KEYCODE_VOLUME_MUTE;
            return true;
        case TvCommand::ChannelUp:
            keyCode = KEYCODE_CHANNEL_UP;
            return true;
        case TvCommand::ChannelDown:
            keyCode = KEYCODE_CHANNEL_DOWN;
            return true;
        case TvCommand::NavigateUp:
            keyCode = KEYCODE_DPAD_UP;
            return true;
        case TvCommand::NavigateDown:
            keyCode = KEYCODE_DPAD_DOWN;
            return true;
        case TvCommand::NavigateLeft:
            keyCode = KEYCODE_DPAD_LEFT;
            return true;
        case TvCommand::NavigateRight:
            keyCode = KEYCODE_DPAD_RIGHT;
            return true;
        case TvCommand::Ok:
            keyCode = KEYCODE_DPAD_CENTER;
            return true;
        case TvCommand::Back:
            keyCode = KEYCODE_BACK;
            return true;
        case TvCommand::Home:
            keyCode = KEYCODE_HOME;
            return true;
        case TvCommand::Input:
            keyCode = KEYCODE_TV_INPUT;
            return true;
    }

    return false;
}
