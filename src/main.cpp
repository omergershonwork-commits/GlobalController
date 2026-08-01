#include <M5Cardputer.h>

#include <cstdint>

#include "app/RemoteApplication.h"
#include "input/KeyboardCommandMapper.h"
#include "ir/ArduinoIrTransmitter.h"
#include "profile/Lg37Ld450Profile.h"
#include "ui/RemoteScreen.h"

namespace {
constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kLoopDelayMs = 5;
constexpr std::uint8_t kIrTxPin = 44;
constexpr std::uint32_t kInitialRepeatDelayMs = 450;
constexpr std::uint32_t kRepeatIntervalMs = 150;

Lg37Ld450Profile lgProfile;
XiaomiMiTvMssp3Profile xiaomiProfile;
TvProfile* const kProfiles[] = {&lgProfile, &xiaomiProfile};
constexpr std::uint8_t kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);
std::uint8_t activeProfileIndex = 0;

ArduinoIrTransmitter irTransmitter(kIrTxPin);
RemoteApplication remoteApplication(
    lgProfile,
    irTransmitter,
    kInitialRepeatDelayMs,
    kRepeatIntervalMs
);
RemoteScreen remoteScreen(lgProfile);

const TvProfile& activeProfile() {
    return *kProfiles[activeProfileIndex];
}

const char* verificationLabel(CodeVerification verification) {
    return verification == CodeVerification::VerifiedOnDevice ? "verified" : "provisional";
}

bool containsProfileSwitchKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 't' || character == 'T') {
            return true;
        }
    }

    return false;
}

void selectNextProfile() {
    activeProfileIndex = static_cast<std::uint8_t>((activeProfileIndex + 1) % kProfileCount);
    const TvProfile& profile = activeProfile();

    remoteApplication.setProfile(profile);
    remoteScreen.setProfile(profile);

    Serial.printf("Selected profile: %s %s\n", profile.brand(), profile.model());
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

        case RemoteEventType::CommandSent:
            break;
    }

    const TvProfile& profile = activeProfile();
    Serial.printf(
        "Sent %s %s: protocol=%u address=0x%02X command=0x%02X repeats=%d pin=%u held-repeat=%s\n",
        profile.brand(),
        event.label,
        static_cast<unsigned int>(event.code.protocol),
        event.code.address,
        event.code.command,
        event.code.repeats,
        kIrTxPin,
        event.repeated ? "yes" : "no"
    );
    Serial.printf("Profile code status: %s\n", verificationLabel(event.verification));
}

void handleKeyboard() {
    const bool inputChanged = M5Cardputer.Keyboard.isChange();
    const bool pressed = M5Cardputer.Keyboard.isPressed();
    const auto& state = M5Cardputer.Keyboard.keysState();

    if (pressed && containsProfileSwitchKey(state)) {
        if (inputChanged) {
            selectNextProfile();
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
}  // namespace

void setup() {
    auto config = M5.config();
    M5Cardputer.begin(config, true);
    Serial.begin(kSerialBaud);

    M5Cardputer.Display.setRotation(1);
    irTransmitter.begin();
    remoteScreen.begin();

    Serial.println("GlobalController TV-008 ready");
    Serial.printf("Loaded profile: %s %s\n", activeProfile().brand(), activeProfile().model());
    Serial.printf("IR transmitter initialized on GPIO %u\n", kIrTxPin);
    Serial.printf(
        "Repeat timing: initial=%lu ms interval=%lu ms\n",
        static_cast<unsigned long>(kInitialRepeatDelayMs),
        static_cast<unsigned long>(kRepeatIntervalMs)
    );
}

void loop() {
    M5Cardputer.update();
    handleKeyboard();
    delay(kLoopDelayMs);
}
