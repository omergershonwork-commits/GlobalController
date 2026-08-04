#include <M5Cardputer.h>
#include <WiFi.h>

#include <cstdio>
#include <cstdint>

#include "app/HybridTvCommandSender.h"
#include "app/RemoteApplication.h"
#include "config/DeviceConfig.h"
#include "input/KeyboardCommandMapper.h"
#include "ir/ArduinoIrTransmitter.h"
#include "profile/Lg37Ld450Profile.h"
#include "ui/RemoteScreen.h"
#include "wifi/AndroidTvRemoteAdapter.h"
#include "wifi/AndroidTvRemoteRuntime.h"

namespace {
constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kLoopDelayMs = 5;
constexpr unsigned long kHealthLogIntervalMs = 2000;
constexpr unsigned long kIndicatorRefreshIntervalMs = 2000;
constexpr unsigned long kMetricSampleIntervalMs = 1000;
constexpr unsigned long kMetricsRefreshIntervalMs = 1000;
constexpr std::uint8_t kIrTxPin = 44;
constexpr std::uint32_t kInitialRepeatDelayMs = 450;
constexpr std::uint32_t kRepeatIntervalMs = 150;
constexpr std::uint8_t kXiaomiProfileIndex = 1;
constexpr std::size_t kPairingCodeLength = 6;

struct CandidateUiStatus {
    char hostname[33];
    char ipAddress[16];
    std::uint8_t number;
    std::uint8_t count;
    std::uint32_t revision;
};

Lg37Ld450Profile lgProfile;
XiaomiMiTvMssp3Profile xiaomiProfile;
TvProfile* const kProfiles[] = {&lgProfile, &xiaomiProfile};
constexpr std::uint8_t kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);
std::uint8_t activeProfileIndex = 0;

ArduinoIrTransmitter irTransmitter(kIrTxPin);
AndroidTvRemoteAdapter androidTvAdapter;
AndroidTvRemoteRuntime wifiRemote(androidTvAdapter);
HybridTvCommandSender commandSender(irTransmitter, wifiRemote);
RemoteApplication remoteApplication(
    lgProfile,
    commandSender,
    kInitialRepeatDelayMs,
    kRepeatIntervalMs
);
RemoteScreen remoteScreen(lgProfile);

AndroidTvRemoteState lastWifiState = AndroidTvRemoteState::Disabled;
String pairingCode;
bool pairingEntryActive = false;
bool metricsVisible = false;
unsigned long lastHealthLogMs = 0;
unsigned long lastIndicatorRefreshMs = 0;
unsigned long lastMetricSampleMs = 0;
unsigned long lastMetricsRefreshMs = 0;
unsigned long maxLoopDurationMs = 0;
unsigned long recentMaxUiLoopMs = 0;
unsigned long recentMaxNetworkTickMs = 0;
std::uint32_t recentFreeHeapBytes = 0;
std::uint32_t lastCandidateRevision = 0;
std::int32_t recentWifiRssi = 0;
std::int32_t lastBatteryPercent = -1;
bool lastBatteryCharging = false;

const TvProfile& activeProfile() {
    return *kProfiles[activeProfileIndex];
}

bool xiaomiSelected() {
    return activeProfileIndex == kXiaomiProfileIndex;
}

CandidateUiStatus readCandidateStatus() {
    CandidateUiStatus status{};
    wifiRemote.candidateStatus(
        status.hostname,
        sizeof(status.hostname),
        status.ipAddress,
        sizeof(status.ipAddress),
        status.number,
        status.count,
        status.revision
    );
    return status;
}

bool pairingPhaseActive() {
    if (!xiaomiSelected() || !wifiRemote.requested()) {
        return false;
    }

    const AndroidTvRemoteState state = wifiRemote.state();
    return state == AndroidTvRemoteState::PairingConnecting ||
           state == AndroidTvRemoteState::PairingCodeRequired;
}

bool candidateAttemptActive() {
    if (!xiaomiSelected() || !wifiRemote.requested()) {
        return false;
    }

    const AndroidTvRemoteState state = wifiRemote.state();
    return state == AndroidTvRemoteState::RemoteConnecting ||
           state == AndroidTvRemoteState::PairingConnecting ||
           state == AndroidTvRemoteState::PairingCodeRequired;
}

const char* verificationLabel(CodeVerification verification) {
    return verification == CodeVerification::VerifiedOnDevice ? "verified" : "provisional";
}

const char* routeLabel(TvCommandRoute route) {
    return route == TvCommandRoute::Wifi ? "wifi" : "infrared";
}

const char* activityLabel() {
    if (!xiaomiSelected()) {
        return "IR READY";
    }

    switch (wifiRemote.state()) {
        case AndroidTvRemoteState::Disabled:
            return kDeviceConfig.hasWifiCredentials() ? "WIFI OFF" : "WIFI SETUP";
        case AndroidTvRemoteState::WifiConnecting:
            return "WIFI JOIN";
        case AndroidTvRemoteState::TvDiscovering:
            return "TV SEARCH";
        case AndroidTvRemoteState::RemoteConnecting:
            return "TV LINK";
        case AndroidTvRemoteState::PairingConnecting:
            return "PAIR TRY";
        case AndroidTvRemoteState::PairingCodeRequired:
            return "PAIR CODE";
        case AndroidTvRemoteState::PairingSubmitting:
            return "PAIR CHECK";
        case AndroidTvRemoteState::Ready:
            return "WIFI READY";
        case AndroidTvRemoteState::Error:
            return "NO TV MATCH";
    }

    return "UNKNOWN";
}

std::uint16_t activityColor() {
    if (!xiaomiSelected()) {
        return GREEN;
    }

    switch (wifiRemote.state()) {
        case AndroidTvRemoteState::Disabled:
        case AndroidTvRemoteState::PairingConnecting:
        case AndroidTvRemoteState::PairingCodeRequired:
        case AndroidTvRemoteState::PairingSubmitting:
            return YELLOW;
        case AndroidTvRemoteState::WifiConnecting:
        case AndroidTvRemoteState::TvDiscovering:
        case AndroidTvRemoteState::RemoteConnecting:
            return CYAN;
        case AndroidTvRemoteState::Ready:
            return GREEN;
        case AndroidTvRemoteState::Error:
            return RED;
    }

    return WHITE;
}

void samplePower() {
    lastBatteryPercent = M5Cardputer.Power.getBatteryLevel();
    lastBatteryCharging =
        M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging;
}

void refreshIndicators(unsigned long now, bool force = false) {
    if (!force && now - lastIndicatorRefreshMs < kIndicatorRefreshIntervalMs) {
        return;
    }

    lastIndicatorRefreshMs = now;
    samplePower();

    if (metricsVisible) {
        return;
    }

    remoteScreen.showIndicators(
        activityLabel(),
        activityColor(),
        lastBatteryPercent,
        lastBatteryCharging
    );
}

void sampleMetrics(unsigned long now, bool force = false) {
    if (!force && now - lastMetricSampleMs < kMetricSampleIntervalMs) {
        return;
    }

    lastMetricSampleMs = now;
    samplePower();
    recentMaxUiLoopMs = maxLoopDurationMs;
    maxLoopDurationMs = 0;
    recentMaxNetworkTickMs = wifiRemote.maxTickDurationMs();
    recentFreeHeapBytes = ESP.getFreeHeap();
    recentWifiRssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

void renderMetrics(unsigned long now, bool force = false) {
    if (!metricsVisible) {
        return;
    }

    if (!force && now - lastMetricsRefreshMs < kMetricsRefreshIntervalMs) {
        return;
    }

    lastMetricsRefreshMs = now;
    sampleMetrics(now, force);
    remoteScreen.showMetrics(
        activeProfile().model(),
        wifiRemote.stateLabel(),
        wifiRemote.requested(),
        recentWifiRssi,
        lastBatteryPercent,
        lastBatteryCharging,
        recentFreeHeapBytes,
        recentMaxUiLoopMs,
        recentMaxNetworkTickMs,
        wifiRemote.minimumFreeStackBytes(),
        wifiRemote.configuredStackBytes(),
        now
    );
}

bool containsProfileSwitchKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 't' || character == 'T') {
            return true;
        }
    }
    return false;
}

bool containsNetworkToggleKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 'n' || character == 'N') {
            return true;
        }
    }
    return false;
}

bool containsMetricsToggleKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 'g' || character == 'G') {
            return true;
        }
    }
    return false;
}

bool containsPairingEntryKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 'c' || character == 'C') {
            return true;
        }
    }
    return false;
}

bool containsCandidateSkipKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 's' || character == 'S') {
            return true;
        }
    }
    return false;
}

bool isHexCharacter(char character) {
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F');
}

char toUpperAscii(char character) {
    if (character >= 'a' && character <= 'f') {
        return static_cast<char>(character - 'a' + 'A');
    }
    return character;
}

void showPairingCode() {
    String detail = "Code: ";
    detail += pairingCode;
    while (detail.length() < 6 + kPairingCodeLength) {
        detail += '_';
    }
    remoteScreen.showMessage("PAIR CODE", detail.c_str(), YELLOW);
}

void showCandidateProgress(const char* phase, std::uint16_t color) {
    const CandidateUiStatus candidate = readCandidateStatus();
    if (candidate.count == 0 || candidate.number == 0) {
        remoteScreen.showMessage(
            phase,
            "Finding Android TVs; N cancels",
            color
        );
        return;
    }

    char state[24];
    std::snprintf(
        state,
        sizeof(state),
        "%s %u/%u",
        phase,
        static_cast<unsigned int>(candidate.number),
        static_cast<unsigned int>(candidate.count)
    );

    char detail[64];
    std::snprintf(
        detail,
        sizeof(detail),
        "%s %s  S=NEXT N=STOP",
        candidate.hostname[0] == '\0' ? "Android TV" : candidate.hostname,
        candidate.ipAddress
    );
    remoteScreen.showMessage(state, detail, color);
}

void showWifiState(AndroidTvRemoteState state) {
    switch (state) {
        case AndroidTvRemoteState::Disabled:
            pairingEntryActive = false;
            pairingCode = "";
            if (kDeviceConfig.hasWifiCredentials()) {
                remoteScreen.showMessage("WIFI OFF", "Press N to connect", YELLOW);
            } else {
                remoteScreen.showMessage("WIFI SETUP", "Add local Wi-Fi credentials", YELLOW);
            }
            return;
        case AndroidTvRemoteState::WifiConnecting:
            pairingEntryActive = false;
            remoteScreen.showMessage("WIFI CONNECT", "N cancels; G shows metrics", CYAN);
            return;
        case AndroidTvRemoteState::TvDiscovering:
            pairingEntryActive = false;
            remoteScreen.showMessage("TV SEARCH", "Discovering current IPs; N cancels", CYAN);
            return;
        case AndroidTvRemoteState::RemoteConnecting:
            pairingEntryActive = false;
            showCandidateProgress("TV LINK", CYAN);
            return;
        case AndroidTvRemoteState::PairingConnecting:
            pairingEntryActive = false;
            showCandidateProgress("PAIR TRY", YELLOW);
            return;
        case AndroidTvRemoteState::PairingCodeRequired:
            if (!pairingEntryActive) {
                pairingCode = "";
            }
            pairingEntryActive = true;
            showPairingCode();
            return;
        case AndroidTvRemoteState::PairingSubmitting:
            pairingEntryActive = false;
            remoteScreen.showMessage("PAIRING", "Code submitted; N cancels", YELLOW);
            return;
        case AndroidTvRemoteState::Ready:
            pairingEntryActive = false;
            pairingCode = "";
            remoteScreen.showMessage("WIFI READY", "N disconnects; G metrics", GREEN);
            return;
        case AndroidTvRemoteState::Error: {
            pairingEntryActive = false;
            const CandidateUiStatus candidate = readCandidateStatus();
            char detail[64];
            std::snprintf(
                detail,
                sizeof(detail),
                "Tried %u TVs; N stops/retries",
                static_cast<unsigned int>(candidate.count)
            );
            remoteScreen.showMessage("NO TV MATCH", detail, RED);
            return;
        }
    }
}

void redrawRemoteScreen() {
    remoteScreen.setProfile(activeProfile());
    refreshIndicators(millis(), true);

    if (xiaomiSelected()) {
        showWifiState(wifiRemote.state());
    } else {
        remoteScreen.showReady();
    }
}

void toggleMetrics() {
    metricsVisible = !metricsVisible;
    Serial.printf("Metrics screen: %s\n", metricsVisible ? "open" : "closed");

    if (metricsVisible) {
        pairingEntryActive = false;
        renderMetrics(millis(), true);
    } else {
        redrawRemoteScreen();
    }
}

void openPairingEntryManually() {
    if (!pairingPhaseActive()) {
        if (!metricsVisible) {
            remoteScreen.showMessage("PAIR CODE", "Available after TV pairing starts", YELLOW);
        }
        Serial.println("Pairing-code shortcut ignored: pairing is not active");
        return;
    }

    metricsVisible = false;
    pairingEntryActive = true;
    pairingCode = "";
    showPairingCode();
    Serial.println("Manual pairing-code entry opened with C");
}

void skipCurrentCandidate() {
    if (!candidateAttemptActive()) {
        if (!metricsVisible) {
            remoteScreen.showMessage("TV NEXT", "Available while trying Android TVs", YELLOW);
        }
        Serial.println("Candidate skip ignored: no candidate attempt is active");
        return;
    }

    pairingEntryActive = false;
    pairingCode = "";
    if (!wifiRemote.skipCandidate()) {
        if (!metricsVisible) {
            remoteScreen.showMessage("TV NEXT", "Unable to queue candidate skip", RED);
        }
        return;
    }

    if (!metricsVisible) {
        remoteScreen.showMessage("TV NEXT", "Skipping after current TLS call", CYAN);
    }
    Serial.println("Keyboard: Android TV candidate skip requested");
}

void updateWifiStateUi() {
    const AndroidTvRemoteState state = wifiRemote.state();
    const CandidateUiStatus candidate = readCandidateStatus();
    const bool stateChanged = state != lastWifiState;
    const bool candidateChanged = candidate.revision != lastCandidateRevision;

    if (!stateChanged && !candidateChanged) {
        return;
    }

    lastWifiState = state;
    lastCandidateRevision = candidate.revision;

    if (stateChanged) {
        Serial.printf("Android TV remote state: %s\n", wifiRemote.stateLabel());
    }

    if (candidateChanged && candidate.count > 0) {
        Serial.printf(
            "UI candidate %u/%u: host=%s ip=%s\n",
            static_cast<unsigned int>(candidate.number),
            static_cast<unsigned int>(candidate.count),
            candidate.hostname,
            candidate.ipAddress
        );
    }

    if (state == AndroidTvRemoteState::PairingCodeRequired) {
        metricsVisible = false;
        showWifiState(state);
        refreshIndicators(millis(), true);
        return;
    }

    if (!metricsVisible && xiaomiSelected()) {
        showWifiState(state);
        refreshIndicators(millis(), true);
    }
}

void startWifiRemote() {
    if (wifiRemote.requested()) {
        return;
    }

    Serial.println("Network button: starting Android TV Wi-Fi remote");
    if (!wifiRemote.start(kDeviceConfig.wifiSsid, kDeviceConfig.wifiPassword)) {
        if (!metricsVisible) {
            remoteScreen.showMessage("WIFI ERROR", "Unable to start network task", RED);
        }
        return;
    }

    pairingEntryActive = false;
    pairingCode = "";
    lastWifiState = AndroidTvRemoteState::Disabled;
    lastCandidateRevision = 0;
    if (metricsVisible) {
        renderMetrics(millis(), true);
    } else {
        showWifiState(AndroidTvRemoteState::WifiConnecting);
        refreshIndicators(millis(), true);
    }
}

void cancelWifiRemote(bool updateScreen = true) {
    if (wifiRemote.requested()) {
        Serial.println("Network button: cancelling Android TV Wi-Fi remote");
        wifiRemote.cancel();
    }

    pairingEntryActive = false;
    pairingCode = "";
    lastWifiState = AndroidTvRemoteState::Disabled;
    lastCandidateRevision = 0;
    remoteApplication.reset();

    if (!updateScreen || !xiaomiSelected()) {
        return;
    }

    if (metricsVisible) {
        renderMetrics(millis(), true);
    } else {
        showWifiState(AndroidTvRemoteState::Disabled);
        refreshIndicators(millis(), true);
    }
}

void toggleWifiRemote() {
    if (!xiaomiSelected()) {
        if (!metricsVisible) {
            remoteScreen.showMessage("NETWORK", "Select Xiaomi with T", YELLOW);
        }
        Serial.println("Network button ignored: Xiaomi profile is not selected");
        return;
    }

    if (wifiRemote.requested()) {
        cancelWifiRemote();
    } else {
        startWifiRemote();
    }
}

void selectNextProfile() {
    if (xiaomiSelected() && wifiRemote.requested()) {
        cancelWifiRemote(false);
    }

    pairingEntryActive = false;
    pairingCode = "";
    activeProfileIndex = static_cast<std::uint8_t>((activeProfileIndex + 1) % kProfileCount);
    const TvProfile& profile = activeProfile();

    remoteApplication.setProfile(profile);
    Serial.printf("Selected profile: %s %s\n", profile.brand(), profile.model());

    if (metricsVisible) {
        renderMetrics(millis(), true);
        return;
    }

    remoteScreen.setProfile(profile);
    if (xiaomiSelected()) {
        showWifiState(AndroidTvRemoteState::Disabled);
    }
    refreshIndicators(millis(), true);
}

void logEvent(const RemoteEvent& event) {
    switch (event.type) {
        case RemoteEventType::None:
            return;
        case RemoteEventType::UnmappedInput:
            Serial.println("Keyboard input is not mapped to a TV command");
            return;
        case RemoteEventType::CommandUnavailable:
            Serial.printf("TV command unavailable: %s\n", event.label);
            return;
        case RemoteEventType::UnsupportedProtocol:
            Serial.printf("Unsupported IR protocol for command: %s\n", event.label);
            return;
        case RemoteEventType::WifiNotConfigured:
            Serial.printf("Wi-Fi command needs N to start the network remote: %s\n", event.label);
            return;
        case RemoteEventType::WifiNotReady:
            Serial.printf("Wi-Fi remote is not ready for command: %s\n", event.label);
            return;
        case RemoteEventType::TransportError:
            Serial.printf("Command transport failed: %s\n", event.label);
            return;
        case RemoteEventType::CommandSent:
            break;
    }

    const TvProfile& profile = activeProfile();
    if (event.route == TvCommandRoute::Wifi) {
        Serial.printf(
            "Queued %s %s over Wi-Fi held-repeat=%s\n",
            profile.brand(),
            event.label,
            event.repeated ? "yes" : "no"
        );
        return;
    }

    Serial.printf(
        "Sent %s %s: route=%s protocol=%u address=0x%02X command=0x%02X repeats=%d pin=%u held-repeat=%s\n",
        profile.brand(),
        event.label,
        routeLabel(event.route),
        static_cast<unsigned int>(event.code.protocol),
        event.code.address,
        event.code.command,
        event.code.repeats,
        kIrTxPin,
        event.repeated ? "yes" : "no"
    );
    Serial.printf("Profile code status: %s\n", verificationLabel(event.verification));
}

void handlePairingInput(const Keyboard_Class::KeysState& state) {
    if (state.del) {
        if (!pairingCode.isEmpty()) {
            pairingCode.remove(pairingCode.length() - 1);
        }
        showPairingCode();
        return;
    }

    if (state.enter) {
        if (pairingCode.length() != kPairingCodeLength) {
            remoteScreen.showMessage("PAIR CODE", "Enter all 6 characters", RED);
            return;
        }

        if (!wifiRemote.pairingCodeRequired()) {
            remoteScreen.showMessage("PAIR WAIT", "TV handshake is not ready yet", YELLOW);
            return;
        }

        if (!wifiRemote.submitPairingCode(pairingCode)) {
            remoteScreen.showMessage("PAIR ERROR", "Code queue is busy", RED);
            return;
        }

        pairingEntryActive = false;
        remoteScreen.showMessage("PAIR SENT", "Waiting for television", CYAN);
        return;
    }

    for (const char character : state.word) {
        if (pairingCode.length() < kPairingCodeLength && isHexCharacter(character)) {
            pairingCode += toUpperAscii(character);
        }
    }

    showPairingCode();
}

void handleKeyboard() {
    const bool inputChanged = M5Cardputer.Keyboard.isChange();
    const bool pressed = M5Cardputer.Keyboard.isPressed();
    const auto& state = M5Cardputer.Keyboard.keysState();

    if (pressed && containsNetworkToggleKey(state)) {
        if (inputChanged) {
            Serial.println("Keyboard: network toggle requested");
            toggleWifiRemote();
        }
        return;
    }

    if (pressed && containsProfileSwitchKey(state)) {
        if (inputChanged) {
            Serial.println("Keyboard: profile switch requested");
            selectNextProfile();
        }
        return;
    }

    if (pressed && containsCandidateSkipKey(state) && candidateAttemptActive()) {
        if (inputChanged) {
            skipCurrentCandidate();
        }
        return;
    }

    if (pairingEntryActive) {
        remoteApplication.reset();
        if (pressed && inputChanged) {
            handlePairingInput(state);
        }
        return;
    }

    if (pressed && containsMetricsToggleKey(state)) {
        if (inputChanged) {
            toggleMetrics();
        }
        return;
    }

    if (pressed && containsPairingEntryKey(state)) {
        if (inputChanged) {
            openPairingEntryManually();
        }
        return;
    }

    if (metricsVisible) {
        remoteApplication.reset();
        return;
    }

    const CommandBinding binding = KeyboardCommandMapper::map(state);
    const RemoteEvent event = remoteApplication.update(
        binding,
        pressed,
        inputChanged,
        millis()
    );

    if (event.type != RemoteEventType::None) {
        logEvent(event);
        remoteScreen.showEvent(event);
    } else if (inputChanged && !pressed) {
        remoteScreen.showReady();
    }
}

void logHealthIfDue(unsigned long now) {
    if (now - lastHealthLogMs < kHealthLogIntervalMs) {
        return;
    }

    lastHealthLogMs = now;
    const CandidateUiStatus candidate = readCandidateStatus();
    Serial.printf(
        "HEALTH uptime=%lu profile=%s wifi-active=%s state=%s candidate=%u/%u host=%s ip=%s rssi=%ld battery=%ld charging=%s free-heap=%u max-ui-loop-ms=%lu max-network-ms=%lu network-stack-free=%lu/%lu\n",
        now,
        activeProfile().brand(),
        wifiRemote.requested() ? "yes" : "no",
        wifiRemote.stateLabel(),
        static_cast<unsigned int>(candidate.number),
        static_cast<unsigned int>(candidate.count),
        candidate.hostname,
        candidate.ipAddress,
        static_cast<long>(recentWifiRssi),
        static_cast<long>(lastBatteryPercent),
        lastBatteryCharging ? "yes" : "no",
        static_cast<unsigned int>(recentFreeHeapBytes),
        recentMaxUiLoopMs,
        recentMaxNetworkTickMs,
        static_cast<unsigned long>(wifiRemote.minimumFreeStackBytes()),
        static_cast<unsigned long>(wifiRemote.configuredStackBytes())
    );
}
}  // namespace

void setup() {
    auto config = M5.config();
    M5Cardputer.begin(config, true);
    Serial.begin(kSerialBaud);

    M5Cardputer.Display.setRotation(1);
    irTransmitter.begin();
    remoteScreen.begin();
    wifiRemote.begin();
    sampleMetrics(millis(), true);
    refreshIndicators(millis(), true);

    Serial.println("GlobalController TV-009 multi-candidate diagnostics build ready");
    Serial.printf("Loaded profile: %s %s\n", activeProfile().brand(), activeProfile().model());
    Serial.printf("IR transmitter initialized on GPIO %u\n", kIrTxPin);
    Serial.println("T selects TV; N starts/cancels Wi-Fi; S skips candidate; G metrics; C pairing code");
    Serial.printf(
        "Repeat timing: initial=%lu ms interval=%lu ms\n",
        static_cast<unsigned long>(kInitialRepeatDelayMs),
        static_cast<unsigned long>(kRepeatIntervalMs)
    );
}

void loop() {
    const unsigned long loopStartedMs = millis();

    M5Cardputer.update();
    handleKeyboard();
    updateWifiStateUi();

    const unsigned long loopDurationMs = millis() - loopStartedMs;
    if (loopDurationMs > maxLoopDurationMs) {
        maxLoopDurationMs = loopDurationMs;
    }

    const unsigned long now = millis();
    sampleMetrics(now);
    if (metricsVisible) {
        renderMetrics(now);
    } else {
        refreshIndicators(now);
    }
    logHealthIfDue(now);
    delay(kLoopDelayMs);
}
