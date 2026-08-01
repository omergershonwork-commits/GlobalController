#include "AndroidTvRemoteAdapter.h"

#include <cstring>

#include "remote/RemoteKeycode.h"

namespace {
constexpr std::uint16_t kRemotePort = 6466;
constexpr std::uint16_t kPairingPort = 6467;
constexpr const char* kPairingServiceName = "GlobalController";

bool isEmpty(const char* value) {
    return value == nullptr || value[0] == '\0';
}
}  // namespace

AndroidTvRemoteAdapter::AndroidTvRemoteAdapter()
    : state_(AndroidTvRemoteState::Disabled),
      tvIp_(0, 0, 0, 0),
      configured_(false),
      pairingCodeSubmitted_(false),
      pairingServiceName_{} {
    std::strncpy(
        pairingServiceName_,
        kPairingServiceName,
        sizeof(pairingServiceName_) - 1
    );
}

void AndroidTvRemoteAdapter::begin(
    const char* ssid,
    const char* password,
    const IPAddress& tvIp
) {
    tvIp_ = tvIp;
    configured_ = !isEmpty(ssid) && !isEmpty(password) && tvIp_ != IPAddress(0, 0, 0, 0);
    pairingCodeSubmitted_ = false;

    if (!configured_) {
        state_ = AndroidTvRemoteState::Disabled;
        Serial.println("Android TV Wi-Fi remote is not configured");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
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
                connectRemote();
            }
            return;

        case AndroidTvRemoteState::RemoteConnecting:
        case AndroidTvRemoteState::Ready:
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

    for (const char character : normalized) {
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

void AndroidTvRemoteAdapter::connectRemote() {
    state_ = AndroidTvRemoteState::RemoteConnecting;
    remoteManager_.start(tvIp_, kRemotePort);

    if (remoteManager_.connected()) {
        state_ = AndroidTvRemoteState::Ready;
        Serial.printf(
            "Android TV remote connected to %s:%u\n",
            tvIp_.toString().c_str(),
            kRemotePort
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
