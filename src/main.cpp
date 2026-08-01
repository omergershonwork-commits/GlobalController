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

Lg37Ld450Profile tvProfile;
ArduinoIrTransmitter irTransmitter(kIrTxPin);
RemoteApplication remoteApplication(
    tvProfile,
    irTransmitter,
    kInitialRepeatDelayMs,
    kRepeatIntervalMs
);
RemoteScreen remoteScreen(tvProfile);

const char* verificationLabel(CodeVerification verification) {
    return verification == CodeVerification::VerifiedOnDevice ? "verified" : "provisional";
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

    Serial.printf(
        "Sent %s %s: protocol=%u address=0x%02X command=0x%02X repeats=%d pin=%u held-repeat=%s\n",
        tvProfile.brand(),
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

    Serial.println("GlobalController TV-007 ready");
    Serial.printf("Loaded profile: %s %s\n", tvProfile.brand(), tvProfile.model());
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
