#include "AndroidTvRemoteAdapter.h"

#include <ESPmDNS.h>

#include <cstdio>
#include <cstring>

#include "remote/RemoteKeycode.h"

namespace {
constexpr std::uint16_t kDefaultRemotePort = 6466;
constexpr std::uint16_t kPairingPort = 6467;
constexpr unsigned long kDiscoveryRetryMs = 5000;
constexpr unsigned long kPairingResponseTimeoutMs = 15000;
constexpr const char* kAndroidTvService = "androidtvremote2";
constexpr const char* kGoogleCastService = "googlecast";
constexpr const char* kMdnsProtocol = "tcp";
constexpr const char* kPairingServiceName = "GlobalController";
constexpr const char* kPreferencesNamespace = "globalctrl";
constexpr const char* kPairingKnownKey = "xiaomiPair";
constexpr const char* kPreferredHostKey = "xiaomiHost";
constexpr const char* kPreferredDeviceIdKey = "xiaomiId";

bool isEmpty(const char* value) {
    return value == nullptr || value[0] == '\0';
}

bool isValidAddress(const IPAddress& address) {
    return address != IPAddress(0, 0, 0, 0);
}

void copyText(char* destination, std::size_t capacity, const char* source) {
    if (destination == nullptr || capacity == 0) {
        return;
    }

    const char* safeSource = source == nullptr ? "" : source;
    std::strncpy(destination, safeSource, capacity - 1);
    destination[capacity - 1] = '\0';
}

bool containsIgnoreCase(const char* value, const char* needle) {
    String normalizedValue = value == nullptr ? "" : value;
    String normalizedNeedle = needle == nullptr ? "" : needle;
    normalizedValue.toLowerCase();
    normalizedNeedle.toLowerCase();
    return normalizedValue.indexOf(normalizedNeedle) >= 0;
}

String txtValue(int serviceIndex, const char* key) {
    if (!MDNS.hasTxt(serviceIndex, key)) {
        return "";
    }

    String value = MDNS.txt(serviceIndex, key);
    value.trim();
    return value;
}
}  // namespace

AndroidTvRemoteAdapter::AndroidTvRemoteAdapter()
    : state_(AndroidTvRemoteState::Disabled),
      tvIp_(0, 0, 0, 0),
      remotePort_(kDefaultRemotePort),
      configured_(false),
      pairingCodeSubmitted_(false),
      mdnsStarted_(false),
      preferencesOpened_(false),
      pairingKnown_(false),
      candidateSelectionReady_(false),
      lastDiscoveryAttemptMs_(0),
      pairingAttemptStartedMs_(0),
      pairingServiceName_{},
      preferredHostname_{},
      preferredDeviceId_{},
      candidates_{},
      candidateCount_(0),
      candidateIndex_(0),
      candidateRevision_(0),
      currentCandidateHostname_{},
      currentCandidateDisplayName_{},
      currentCandidateDeviceId_{},
      currentCandidateIp_{} {
    copyText(pairingServiceName_, sizeof(pairingServiceName_), kPairingServiceName);
}

void AndroidTvRemoteAdapter::begin(const char* ssid, const char* password) {
    configured_ = !isEmpty(ssid);
    pairingCodeSubmitted_ = false;
    tvIp_ = IPAddress(0, 0, 0, 0);
    remotePort_ = kDefaultRemotePort;
    resetCandidates();

    if (!preferencesOpened_) {
        preferencesOpened_ = preferences_.begin(kPreferencesNamespace, false);
        if (!preferencesOpened_) {
            Serial.println("Unable to open local pairing preferences");
        }
    }

    pairingKnown_ = preferencesOpened_ &&
                    preferences_.getBool(kPairingKnownKey, false);

    if (preferencesOpened_) {
        const String preferredHost =
            preferences_.getString(kPreferredHostKey, "");
        const String preferredDeviceId =
            preferences_.getString(kPreferredDeviceIdKey, "");
        copyText(
            preferredHostname_,
            sizeof(preferredHostname_),
            preferredHost.c_str()
        );
        copyText(
            preferredDeviceId_,
            sizeof(preferredDeviceId_),
            preferredDeviceId.c_str()
        );
    } else {
        preferredHostname_[0] = '\0';
        preferredDeviceId_[0] = '\0';
    }

    if (
        pairingKnown_ &&
        preferredHostname_[0] == '\0' &&
        preferredDeviceId_[0] == '\0'
    ) {
        pairingKnown_ = false;
    }

    Serial.printf(
        "Stored Xiaomi pairing state: %s preferred-host=%s preferred-id=%s\n",
        pairingKnown_ ? "paired" : "not paired",
        preferredHostname_[0] == '\0' ? "none" : preferredHostname_,
        preferredDeviceId_[0] == '\0' ? "none" : preferredDeviceId_
    );

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

void AndroidTvRemoteAdapter::cancel() {
    if (mdnsStarted_) {
        MDNS.end();
        mdnsStarted_ = false;
    }

    remoteManager_.stop();
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);

    pairingCodeSubmitted_ = false;
    pairingManager_.isSecure = false;
    tvIp_ = IPAddress(0, 0, 0, 0);
    remotePort_ = kDefaultRemotePort;
    lastDiscoveryAttemptMs_ = 0;
    pairingAttemptStartedMs_ = 0;
    resetCandidates();
    state_ = AndroidTvRemoteState::Disabled;

    Serial.println("Android TV Wi-Fi remote cancelled");
}

bool AndroidTvRemoteAdapter::requestPairing() {
    if (
        !configured_ ||
        WiFi.status() != WL_CONNECTED ||
        !isValidAddress(tvIp_)
    ) {
        Serial.println("Cannot connect before Wi-Fi and TV selection complete");
        return false;
    }

    if (
        state_ == AndroidTvRemoteState::TvDiscovering &&
        candidateSelectionReady_
    ) {
        return connectSelectedCandidate();
    }

    switch (state_) {
        case AndroidTvRemoteState::RemoteConnecting:
        case AndroidTvRemoteState::PairingConnecting:
        case AndroidTvRemoteState::PairingCodeRequired:
            Serial.println("Explicit Android TV pairing requested");
            setPairingKnown(false);
            if (startPairing()) {
                return true;
            }
            return returnToCandidateSelection("Explicit pairing failed");

        default:
            Serial.println("TV selection ignored in the current state");
            return false;
    }
}

bool AndroidTvRemoteAdapter::skipCurrentCandidate() {
    if (candidateCount_ == 0) {
        Serial.println(
            "Candidate navigation ignored: no Android TVs are available"
        );
        return false;
    }

    remoteManager_.stop();
    pairingManager_.isSecure = false;
    pairingCodeSubmitted_ = false;
    pairingAttemptStartedMs_ = 0;

    candidateIndex_ = static_cast<std::uint8_t>(
        (candidateIndex_ + 1) % candidateCount_
    );
    candidateSelectionReady_ = true;
    state_ = AndroidTvRemoteState::TvDiscovering;
    publishCurrentCandidate();

    Serial.printf(
        "Cardputer selected candidate %u/%u: name=%s mdns=%s id=%s ip=%s\n",
        static_cast<unsigned int>(candidateIndex_ + 1),
        static_cast<unsigned int>(candidateCount_),
        currentCandidateDisplayName_,
        currentCandidateHostname_,
        currentCandidateDeviceId_[0] == '\0'
            ? "unknown"
            : currentCandidateDeviceId_,
        currentCandidateIp_
    );
    return true;
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
                candidateSelectionReady_ = false;
                WiFi.reconnect();
                state_ = AndroidTvRemoteState::WifiConnecting;
                return;
            }

            if (candidateSelectionReady_) {
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
                candidateSelectionReady_ = false;
                WiFi.reconnect();
                state_ = AndroidTvRemoteState::WifiConnecting;
                return;
            }

            remoteManager_.loop();

            if (remoteManager_.error_auth) {
                remoteManager_.error_auth = false;
                Serial.println(
                    "Remote authentication failed; starting pairing service"
                );
                setPairingKnown(false);
                if (!startPairing()) {
                    returnToCandidateSelection(
                        "Remote authentication and pairing failed"
                    );
                }
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
                candidateSelectionReady_ = false;
                WiFi.reconnect();
                state_ = AndroidTvRemoteState::WifiConnecting;
                return;
            }

            pairingManager_.loop();

            if (pairingManager_.isSecure && !pairingCodeSubmitted_) {
                state_ = AndroidTvRemoteState::PairingCodeRequired;
                return;
            }

            if (state_ == AndroidTvRemoteState::PairingConnecting) {
                if (!pairingManager_.connected()) {
                    returnToCandidateSelection(
                        "Pairing connection closed before code stage"
                    );
                    return;
                }

                if (
                    millis() - pairingAttemptStartedMs_ >=
                    kPairingResponseTimeoutMs
                ) {
                    returnToCandidateSelection("Pairing response timed out");
                    return;
                }
            }

            if (
                pairingCodeSubmitted_ &&
                !pairingManager_.connected()
            ) {
                pairingCodeSubmitted_ = false;
                savePreferredIdentity();
                setPairingKnown(true);
                if (!connectRemote()) {
                    returnToCandidateSelection(
                        "Pairing completed but Android TV remote connection failed"
                    );
                }
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
        Serial.println(
            "Android TV pairing code must contain 6 hexadecimal characters"
        );
        return false;
    }

    for (std::size_t index = 0; index < normalized.length(); ++index) {
        const char character = normalized[index];
        const bool isDigit = character >= '0' && character <= '9';
        const bool isHexLetter = character >= 'A' && character <= 'F';
        if (!isDigit && !isHexLetter) {
            Serial.println(
                "Android TV pairing code contains a non-hexadecimal character"
            );
            return false;
        }
    }

    if (!pairingManager_.sendCode(normalized)) {
        Serial.println(
            "Android TV pairing code was rejected before submission"
        );
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

bool AndroidTvRemoteAdapter::candidateIdentityKnown() const {
    return candidateCount_ > 0 &&
           candidateIndex_ < candidateCount_ &&
           candidates_[candidateIndex_].identityKnown;
}

const char* AndroidTvRemoteAdapter::stateLabel() const {
    switch (state_) {
        case AndroidTvRemoteState::Disabled:
            return "Wi-Fi remote stopped";
        case AndroidTvRemoteState::WifiConnecting:
            return "Connecting Wi-Fi";
        case AndroidTvRemoteState::TvDiscovering:
            return candidateSelectionReady_
                ? "Select Android TV"
                : "Discovering Android TV names";
        case AndroidTvRemoteState::RemoteConnecting:
            return "Connecting selected TV";
        case AndroidTvRemoteState::PairingConnecting:
            return "Pairing selected TV";
        case AndroidTvRemoteState::PairingCodeRequired:
            return "Enter TV pairing code";
        case AndroidTvRemoteState::PairingSubmitting:
            return "Finishing pairing";
        case AndroidTvRemoteState::Ready:
            return "Wi-Fi remote ready";
        case AndroidTvRemoteState::Error:
            return "Android TV discovery error";
    }

    return "Unknown";
}

void AndroidTvRemoteAdapter::candidateStatus(
    char* hostname,
    std::size_t hostnameCapacity,
    char* ipAddress,
    std::size_t ipAddressCapacity,
    std::uint8_t& candidateNumber,
    std::uint8_t& candidateCount,
    std::uint32_t& revision
) const {
    for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
        const std::uint32_t revisionBefore = candidateRevision_;
        const std::uint8_t count = candidateCount_;
        const std::uint8_t index = candidateIndex_;

        copyText(
            hostname,
            hostnameCapacity,
            currentCandidateDisplayName_
        );
        copyText(
            ipAddress,
            ipAddressCapacity,
            currentCandidateIp_
        );

        const std::uint32_t revisionAfter = candidateRevision_;
        if (revisionBefore == revisionAfter) {
            candidateNumber = count == 0
                ? 0
                : static_cast<std::uint8_t>(index + 1);
            candidateCount = count;
            revision = revisionAfter;
            return;
        }
    }

    candidateNumber = candidateCount_ == 0
        ? 0
        : static_cast<std::uint8_t>(candidateIndex_ + 1);
    candidateCount = candidateCount_;
    revision = candidateRevision_;
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
            Serial.println(
                "Failed to start mDNS for Android TV discovery"
            );
            return;
        }

        mdnsStarted_ = true;
        Serial.printf("mDNS started as %s.local\n", hostname);
    }

    candidateSelectionReady_ = false;
    state_ = AndroidTvRemoteState::TvDiscovering;
    lastDiscoveryAttemptMs_ = 0;
    discoverTv();
}

bool AndroidTvRemoteAdapter::discoverTv() {
    lastDiscoveryAttemptMs_ = millis();
    resetCandidates();

    Candidate discovered[kMaxCandidates]{};
    std::uint8_t discoveredCount = 0;

    Serial.printf(
        "Searching for _%s._%s.local\n",
        kAndroidTvService,
        kMdnsProtocol
    );

    const int serviceCount = MDNS.queryService(
        kAndroidTvService,
        kMdnsProtocol
    );
    if (serviceCount <= 0) {
        Serial.println(
            "No Android TV Remote v2 service found; retrying"
        );
        return false;
    }

    for (
        int serviceIndex = 0;
        serviceIndex < serviceCount &&
        discoveredCount < kMaxCandidates;
        ++serviceIndex
    ) {
        const IPAddress address = MDNS.IP(serviceIndex);
        if (!isValidAddress(address)) {
            continue;
        }

        bool duplicate = false;
        for (
            std::uint8_t index = 0;
            index < discoveredCount;
            ++index
        ) {
            if (discovered[index].address == address) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        Candidate& candidate = discovered[discoveredCount];
        candidate.address = address;
        candidate.remotePort = MDNS.port(serviceIndex);
        if (candidate.remotePort == 0) {
            candidate.remotePort = kDefaultRemotePort;
        }

        const String hostname = MDNS.hostname(serviceIndex);
        copyText(
            candidate.hostname,
            sizeof(candidate.hostname),
            hostname.c_str()
        );
        copyText(
            candidate.friendlyName,
            sizeof(candidate.friendlyName),
            candidate.hostname[0] == '\0'
                ? "Android TV"
                : candidate.hostname
        );
        candidate.modelName[0] = '\0';
        candidate.deviceId[0] = '\0';
        candidate.identityKnown = false;

        Serial.printf(
            "Discovered remote candidate %u: mdns=%s ip=%s port=%u\n",
            static_cast<unsigned int>(discoveredCount + 1),
            candidate.hostname,
            candidate.address.toString().c_str(),
            candidate.remotePort
        );

        discoveredCount = static_cast<std::uint8_t>(
            discoveredCount + 1
        );
    }

    if (discoveredCount == 0) {
        Serial.println(
            "Android TV services had no usable unique IPv4 address; retrying"
        );
        return false;
    }

    Serial.printf(
        "Searching for _%s._%s.local to obtain user-visible TV names\n",
        kGoogleCastService,
        kMdnsProtocol
    );

    const int castCount = MDNS.queryService(
        kGoogleCastService,
        kMdnsProtocol
    );

    for (int castIndex = 0; castIndex < castCount; ++castIndex) {
        const IPAddress castAddress = MDNS.IP(castIndex);
        if (!isValidAddress(castAddress)) {
            continue;
        }

        const String friendlyName = txtValue(castIndex, "fn");
        const String modelName = txtValue(castIndex, "md");
        const String deviceId = txtValue(castIndex, "id");

        Serial.printf(
            "Discovered Cast identity: name=%s model=%s id=%s ip=%s\n",
            friendlyName.isEmpty()
                ? "unknown"
                : friendlyName.c_str(),
            modelName.isEmpty()
                ? "unknown"
                : modelName.c_str(),
            deviceId.isEmpty()
                ? "unknown"
                : deviceId.c_str(),
            castAddress.toString().c_str()
        );

        for (
            std::uint8_t candidateIndex = 0;
            candidateIndex < discoveredCount;
            ++candidateIndex
        ) {
            Candidate& candidate = discovered[candidateIndex];
            if (candidate.address != castAddress) {
                continue;
            }

            if (!friendlyName.isEmpty()) {
                copyText(
                    candidate.friendlyName,
                    sizeof(candidate.friendlyName),
                    friendlyName.c_str()
                );
            } else if (!modelName.isEmpty()) {
                copyText(
                    candidate.friendlyName,
                    sizeof(candidate.friendlyName),
                    modelName.c_str()
                );
            }

            copyText(
                candidate.modelName,
                sizeof(candidate.modelName),
                modelName.c_str()
            );
            copyText(
                candidate.deviceId,
                sizeof(candidate.deviceId),
                deviceId.c_str()
            );
            candidate.identityKnown =
                !friendlyName.isEmpty() ||
                !modelName.isEmpty() ||
                !deviceId.isEmpty();

            Serial.printf(
                "Matched Android TV service to Cast device: "
                "name=%s model=%s mdns=%s ip=%s\n",
                candidate.friendlyName,
                candidate.modelName[0] == '\0'
                    ? "unknown"
                    : candidate.modelName,
                candidate.hostname,
                candidate.address.toString().c_str()
            );
            break;
        }
    }

    for (
        std::uint8_t index = 0;
        index < discoveredCount;
        ++index
    ) {
        candidates_[index] = discovered[index];
    }
    candidateCount_ = discoveredCount;

    sortCandidates();
    candidateIndex_ = 0;
    publishCurrentCandidate();
    candidateSelectionReady_ = true;
    state_ = AndroidTvRemoteState::TvDiscovering;

    Serial.printf(
        "TV picker ready: %u Android TV candidates found\n",
        static_cast<unsigned int>(candidateCount_)
    );
    return true;
}

bool AndroidTvRemoteAdapter::connectSelectedCandidate() {
    if (
        !candidateSelectionReady_ ||
        candidateIndex_ >= candidateCount_
    ) {
        Serial.println("No Android TV candidate is selected");
        return false;
    }

    candidateSelectionReady_ = false;
    publishCurrentCandidate();

    Serial.printf(
        "Cardputer confirmed candidate %u/%u: "
        "name=%s mdns=%s id=%s ip=%s\n",
        static_cast<unsigned int>(candidateIndex_ + 1),
        static_cast<unsigned int>(candidateCount_),
        currentCandidateDisplayName_,
        currentCandidateHostname_,
        currentCandidateDeviceId_[0] == '\0'
            ? "unknown"
            : currentCandidateDeviceId_,
        currentCandidateIp_
    );

    const Candidate& selected = candidates_[candidateIndex_];
    if (pairingKnown_ && candidateMatchesPreferred(selected)) {
        Serial.println(
            "Trying saved Android TV identity at its newly discovered IP"
        );
        if (connectRemote()) {
            return true;
        }

        Serial.println(
            "Saved remote connection failed; retrying selected TV through pairing"
        );
        setPairingKnown(false);
    }

    if (startPairing()) {
        return true;
    }

    return returnToCandidateSelection(
        "Selected TV failed before pairing-code stage"
    );
}

bool AndroidTvRemoteAdapter::returnToCandidateSelection(
    const char* reason
) {
    Serial.printf(
        "%s: name=%s mdns=%s ip=%s\n",
        reason == nullptr
            ? "Returning to TV picker"
            : reason,
        currentCandidateDisplayName_,
        currentCandidateHostname_,
        currentCandidateIp_
    );

    remoteManager_.stop();
    pairingManager_.isSecure = false;
    pairingCodeSubmitted_ = false;
    pairingAttemptStartedMs_ = 0;

    if (candidateCount_ == 0) {
        candidateSelectionReady_ = false;
        state_ = AndroidTvRemoteState::TvDiscovering;
        return false;
    }

    candidateSelectionReady_ = true;
    state_ = AndroidTvRemoteState::TvDiscovering;
    publishCurrentCandidate();
    return true;
}

bool AndroidTvRemoteAdapter::connectRemote() {
    remoteManager_.stop();
    state_ = AndroidTvRemoteState::RemoteConnecting;
    remoteManager_.error_auth = false;
    remoteManager_.start(tvIp_, remotePort_);

    if (
        remoteManager_.error_auth ||
        !remoteManager_.connected()
    ) {
        remoteManager_.error_auth = false;
        Serial.printf(
            "Android TV remote connection failed for name=%s ip=%s\n",
            currentCandidateDisplayName_,
            currentCandidateIp_
        );
        return false;
    }

    state_ = AndroidTvRemoteState::Ready;
    Serial.printf(
        "Android TV remote connected to "
        "name=%s mdns=%s ip=%s port=%u\n",
        currentCandidateDisplayName_,
        currentCandidateHostname_,
        currentCandidateIp_,
        remotePort_
    );
    return true;
}

bool AndroidTvRemoteAdapter::startPairing() {
    remoteManager_.stop();
    state_ = AndroidTvRemoteState::PairingConnecting;
    pairingCodeSubmitted_ = false;
    pairingManager_.isSecure = false;
    pairingAttemptStartedMs_ = millis();
    pairingManager_.begin(
        tvIp_,
        kPairingPort,
        pairingServiceName_
    );

    if (!pairingManager_.connected()) {
        Serial.printf(
            "Unable to pair selected "
            "name=%s mdns=%s ip=%s on port %u\n",
            currentCandidateDisplayName_,
            currentCandidateHostname_,
            currentCandidateIp_,
            kPairingPort
        );
        return false;
    }

    Serial.printf(
        "Pairing transport connected for name=%s ip=%s; "
        "waiting for TV response\n",
        currentCandidateDisplayName_,
        currentCandidateIp_
    );
    return true;
}

void AndroidTvRemoteAdapter::resetCandidates() {
    candidateCount_ = 0;
    candidateIndex_ = 0;
    candidateSelectionReady_ = false;
    currentCandidateHostname_[0] = '\0';
    currentCandidateDisplayName_[0] = '\0';
    currentCandidateDeviceId_[0] = '\0';
    currentCandidateIp_[0] = '\0';
    candidateRevision_ = candidateRevision_ + 1;
}

void AndroidTvRemoteAdapter::publishCurrentCandidate() {
    if (candidateIndex_ >= candidateCount_) {
        return;
    }

    const Candidate& candidate = candidates_[candidateIndex_];
    tvIp_ = candidate.address;
    remotePort_ = candidate.remotePort;

    copyText(
        currentCandidateHostname_,
        sizeof(currentCandidateHostname_),
        candidate.hostname
    );
    copyText(
        currentCandidateDisplayName_,
        sizeof(currentCandidateDisplayName_),
        candidate.friendlyName[0] == '\0'
            ? candidate.hostname
            : candidate.friendlyName
    );
    copyText(
        currentCandidateDeviceId_,
        sizeof(currentCandidateDeviceId_),
        candidate.deviceId
    );

    std::snprintf(
        currentCandidateIp_,
        sizeof(currentCandidateIp_),
        "%u.%u.%u.%u",
        candidate.address[0],
        candidate.address[1],
        candidate.address[2],
        candidate.address[3]
    );

    candidateRevision_ = candidateRevision_ + 1;
}

void AndroidTvRemoteAdapter::sortCandidates() {
    auto rank = [this](const Candidate& candidate) -> std::uint8_t {
        if (candidateMatchesPreferred(candidate)) {
            return 0;
        }

        if (
            containsIgnoreCase(candidate.friendlyName, "xiaomi") ||
            containsIgnoreCase(candidate.friendlyName, "mi tv") ||
            containsIgnoreCase(candidate.friendlyName, "mitv") ||
            containsIgnoreCase(candidate.modelName, "xiaomi") ||
            containsIgnoreCase(candidate.modelName, "mitv") ||
            containsIgnoreCase(candidate.hostname, "xiaomi") ||
            containsIgnoreCase(candidate.hostname, "mitv")
        ) {
            return 1;
        }

        return candidate.identityKnown ? 2 : 3;
    };

    for (
        std::uint8_t left = 0;
        left < candidateCount_;
        ++left
    ) {
        for (
            std::uint8_t right =
                static_cast<std::uint8_t>(left + 1);
            right < candidateCount_;
            ++right
        ) {
            if (
                rank(candidates_[right]) <
                rank(candidates_[left])
            ) {
                const Candidate temporary = candidates_[left];
                candidates_[left] = candidates_[right];
                candidates_[right] = temporary;
            }
        }
    }

    Serial.println("Android TV picker order:");
    for (
        std::uint8_t index = 0;
        index < candidateCount_;
        ++index
    ) {
        Serial.printf(
            "  %u/%u name=%s model=%s mdns=%s id=%s ip=%s "
            "identity=%s priority=%u\n",
            static_cast<unsigned int>(index + 1),
            static_cast<unsigned int>(candidateCount_),
            candidates_[index].friendlyName,
            candidates_[index].modelName[0] == '\0'
                ? "unknown"
                : candidates_[index].modelName,
            candidates_[index].hostname,
            candidates_[index].deviceId[0] == '\0'
                ? "unknown"
                : candidates_[index].deviceId,
            candidates_[index].address.toString().c_str(),
            candidates_[index].identityKnown
                ? "cast"
                : "mdns-only",
            static_cast<unsigned int>(rank(candidates_[index]))
        );
    }
}

void AndroidTvRemoteAdapter::setPairingKnown(bool known) {
    pairingKnown_ = known;
    if (preferencesOpened_) {
        preferences_.putBool(kPairingKnownKey, known);
    }

    Serial.printf(
        "Stored Xiaomi pairing state updated: %s\n",
        known ? "paired" : "not paired"
    );
}

void AndroidTvRemoteAdapter::savePreferredIdentity() {
    copyText(
        preferredHostname_,
        sizeof(preferredHostname_),
        currentCandidateHostname_
    );
    copyText(
        preferredDeviceId_,
        sizeof(preferredDeviceId_),
        currentCandidateDeviceId_
    );

    if (preferencesOpened_) {
        preferences_.putString(
            kPreferredHostKey,
            preferredHostname_
        );
        preferences_.putString(
            kPreferredDeviceIdKey,
            preferredDeviceId_
        );
    }

    Serial.printf(
        "Saved Android TV identity: "
        "name=%s host=%s id=%s; IP will be rediscovered\n",
        currentCandidateDisplayName_,
        preferredHostname_[0] == '\0'
            ? "none"
            : preferredHostname_,
        preferredDeviceId_[0] == '\0'
            ? "none"
            : preferredDeviceId_
    );
}

bool AndroidTvRemoteAdapter::candidateMatchesPreferred(
    const Candidate& candidate
) const {
    if (
        preferredDeviceId_[0] != '\0' &&
        candidate.deviceId[0] != '\0'
    ) {
        return std::strcmp(
            candidate.deviceId,
            preferredDeviceId_
        ) == 0;
    }

    return preferredHostname_[0] != '\0' &&
           candidate.hostname[0] != '\0' &&
           std::strcmp(
               candidate.hostname,
               preferredHostname_
           ) == 0;
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
