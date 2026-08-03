#include <M5Cardputer.h>

#include <cstdint>

#include "app/HybridTvCommandSender.h"
#include "app/RemoteApplication.h"
#include "config/DeviceConfig.h"
#include "input/KeyboardCommandMapper.h"
#include "ir/ArduinoIrTransmitter.h"
#include "profile/Lg37Ld450Profile.h"
#include "ui/RemoteScreen.h"
#include "wifi/AndroidTvRemoteAdapter.h"

namespace {
constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kLoopDelayMs = 5;
constexpr unsigned long kHealthLogIntervalMs = 2000;
constexpr unsigned long kSlowNetworkTickMs = 100;
constexpr std::uint8_t kIrTxPin = 44;
constexpr std::uint32_t kInitialRepeatDelayMs = 450;
constexpr std::uint32_t kRepeatIntervalMs = 150;
constexpr std::uint8_t kXiaomiProfileIndex = 1;
constexpr std::size_t kPairingCodeLength = 6;

Lg37Ld450Profile lgProfile;
XiaomiMiTvMssp3Profile xiaomiProfile;
TvProfile* const kProfiles[] = {&lgProfile, &xiaomiProfile};
constexpr std::uint8_t kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);
std::uint8_t activeProfileIndex = 0;

ArduinoIrTransmitter irTransmitter(kIrTxPin);
AndroidTvRemoteAdapter androidTvRemote;
HybridTvCommandSender commandSender(irTransmitter, androidTvRemote);
RemoteApplication remoteApplication(
    lgProfile,
    commandSender,
    kInitialRepeatDelayMs,
    kRepeatIntervalMs
);
RemoteScreen remoteScreen(lgProfile);

AndroidTvRemoteState lastWifiState = AndroidTvRemoteState::Disabled;
String pairingCode;
bool wifiRemoteStarted = false;
unsigned long lastHealthLogMs = 0;
unsigned long maxLoopDurationMs = 0;
unsigned long maxNetworkTickMs = 0;

const TvProfile& activeProfile() {
    return *kProfiles[activeProfileIndex];
}

bool xiaomiSelected() {
    return activeProfileIndex == kXiaomiProfileIndex;
}

const char* verificationLabel(CodeVerification verification) {
    return verification == CodeVerification::VerifiedOnDevice ? "verified" : "provisional";
}

const char* routeLabel(TvCommandRoute route) {
    return route == TvCommandRoute::Wifi ? "wifi" : "infrared";
}

bool containsProfileSwitchKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 't' || character == 'T') {
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

void showWifiState(AndroidTvRemoteState state) {
    switch (state) {
        case AndroidTvRemoteState::Disabled:
            remoteScreen.showMessage("WIFI SETUP", "Add local Wi-Fi credentials", YELLOW);
            return;
        case AndroidTvRemoteState::WifiConnecting:
            remoteScreen.showMessage("WIFI CONNECT", "Joining configured network", YELLOW);
            return;
        case AndroidTvRemoteState::TvDiscovering:
            remoteScreen.showMessage("TV SEARCH", "Finding Android TV automatically", YELLOW);
            return;
        case AndroidTvRemoteState::RemoteConnecting:
            remoteScreen.showMessage("TV CONNECT", "Opening Android TV remote", YELLOW);
            return;
        case AndroidTvRemoteState::PairingConnecting:
            remoteScreen.showMessage("PAIRING", "Starting TV pairing", YELLOW);
            return;
        case AndroidTvRemoteState::PairingCodeRequired:
            pairingCode = "";
            showPairingCode();
            return;
        case AndroidTvRemoteState::PairingSubmitting:
            remoteScreen.showMessage("PAIRING", "Checking displayed code", YELLOW);
            return;
        case AndroidTvRemoteState::Ready:
            remoteScreen.showMessage("WIFI READY", "Xiaomi controls connected", GREEN);
            return;
        case AndroidTvRemoteState::Error:
            remoteScreen.showMessage("WIFI ERROR", "Check serial monitor", RED);
            return;
    }
}

void updateWifiStateUi() {
    const AndroidTvRemoteState state = androidTvRemote.state();
    if (state == lastWifiState) {
        return;
    }

    lastWifiState = state;
    Serial.printf("Android TV remote state: %s\n", androidTvRemote.stateLabel());

    if (xiaomiSelected()) {
        showWifiState(state);
    }
}

void startWifiRemoteIfNeeded() {
    if (wifiRemoteStarted) {
        return;
    }

    wifiRemoteStarted = true;
    Serial.println("Starting Android TV Wi-Fi remote after Xiaomi profile selection");
    androidTvRemote.begin(
        kDeviceConfig.wifiSsid,
        kDeviceConfig.wifiPassword
    );
    lastWifiState = androidTvRemote.state();
}

void selectNextProfile() {
    activeProfileIndex = static_cast<std::uint8_t>((activeProfileIndex + 1) % kProfileCount);
    const TvProfile& profile = activeProfile();

    remoteApplication.setProfile(profile);
    remoteScreen.setProfile(profile);

    Serial.printf("Selected profile: %s %s\n", profile.brand(), profile.model());

    if (xiaomiSelected()) {
        startWifiRemoteIfNeeded();
        showWifiState(androidTvRemote.state());
    }
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
            Serial.printf("Wi-Fi command needs local credentials: %s\n", event.label);
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
            "Sent %s %s over Wi-Fi held-repeat=%s\n",
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

        if (!androidTvRemote.submitPairingCode(pairingCode)) {
            remoteScreen.showMessage("PAIR ERROR", "Code was not accepted", RED);
        }
        return;
    }

    for (const char character : state.word) {
        if (
            pairingCode.length() < kPairingCodeLength &&
            isHexCharacter(character)
        ) {
            pairingCode += toUpperAscii(character);
        }
    }

    showPairingCode();
}

void handleKeyboard() {
    const bool inputChanged = M5Cardputer.Keyboard.isChange();
    const bool pressed = M5Cardputer.Keyboard.isPressed();
    const auto& state = M5Cardputer.Keyboard.keysState();

    if (pressed && containsProfileSwitchKey(state)) {
        if (inputChanged) {
            Serial.println("Keyboard: profile switch requested");
            selectNextProfile();
        }
        return;
    }

    if (
        xiaomiSelected() &&
        androidTvRemote.pairingCodeRequired()
    ) {
        remoteApplication.reset();
        if (pressed && inputChanged) {
            handlePairingInput(state);
        }
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
    Serial.printf(
        "HEALTH uptime=%lu profile=%s wifi-started=%s state=%s free-heap=%u max-loop-ms=%lu max-network-ms=%lu\n",
        now,
        activeProfile().brand(),
        wifiRemoteStarted ? "yes" : "no",
        androidTvRemote.stateLabel(),
        static_cast<unsigned int>(ESP.getFreeHeap()),
        maxLoopDurationMs,
        maxNetworkTickMs
    );

    maxLoopDurationMs = 0;
    maxNetworkTickMs = 0;
}
}  // namespace

void setup() {
    auto config = M5.config();
    M5Cardputer.begin(config, true);
    Serial.begin(kSerialBaud);

    M5Cardputer.Display.setRotation(1);
    irTransmitter.begin();
    remoteScreen.begin();

    Serial.println("GlobalController TV-009 responsive-runtime build ready");
    Serial.printf("Loaded profile: %s %s\n", activeProfile().brand(), activeProfile().model());
    Serial.printf("IR transmitter initialized on GPIO %u\n", kIrTxPin);
    Serial.println("Android TV Wi-Fi startup is deferred until Xiaomi is selected");
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

    if (wifiRemoteStarted && xiaomiSelected()) {
        const unsigned long networkStartedMs = millis();
        androidTvRemote.loop();
        const unsigned long networkDurationMs = millis() - networkStartedMs;
        if (networkDurationMs > maxNetworkTickMs) {
            maxNetworkTickMs = networkDurationMs;
        }
        if (networkDurationMs >= kSlowNetworkTickMs) {
            Serial.printf(
                "SLOW NETWORK TICK duration=%lu ms state=%s\n",
                networkDurationMs,
                androidTvRemote.stateLabel()
            );
        }
        updateWifiStateUi();
    }

    const unsigned long loopDurationMs = millis() - loopStartedMs;
    if (loopDurationMs > maxLoopDurationMs) {
        maxLoopDurationMs = loopDurationMs;
    }
    logHealthIfDue(millis());
    delay(kLoopDelayMs);
}
