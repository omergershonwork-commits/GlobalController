#include <M5Cardputer.h>

#include <cstdint>

#include "app/RemoteApplication.h"
#include "input/KeyboardCommandMapper.h"
#include "ir/ArduinoIrTransmitter.h"
#include "profile/Lg37Ld450Profile.h"

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

const char* verificationLabel(CodeVerification verification) {
    return verification == CodeVerification::VerifiedOnDevice ? "verified" : "provisional";
}

void drawScreen(const String& status, std::uint16_t statusColor = WHITE) {
    auto& display = M5Cardputer.Display;

    display.fillScreen(BLACK);
    display.setCursor(8, 6);
    display.setTextSize(2);
    display.setTextColor(GREEN, BLACK);
    display.println("GlobalController");

    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.print("TV: ");
    display.print(tvProfile.brand());
    display.print(' ');
    display.println(tvProfile.model());
    display.println("P power  M mute  I input  H home");
    display.println("U/J volume       R/F channel");
    display.println("W/A/S/D move     Enter/O OK");
    display.println("Delete/B back");
    display.setTextColor(statusColor, BLACK);
    display.print("Status: ");
    display.println(status);
}

void renderEvent(const RemoteEvent& event) {
    switch (event.type) {
        case RemoteEventType::None:
            return;

        case RemoteEventType::UnmappedInput:
            drawScreen("Unmapped key");
            return;

        case RemoteEventType::CommandUnavailable:
            drawScreen(String(event.label) + " unavailable", RED);
            return;

        case RemoteEventType::UnsupportedProtocol:
            drawScreen("Unsupported protocol", RED);
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

    const bool verified = event.verification == CodeVerification::VerifiedOnDevice;
    const String suffix = event.repeated ? " repeat sent" : " IR sent";
    drawScreen(String(event.label) + suffix, verified ? GREEN : YELLOW);
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
    renderEvent(event);
}
}  // namespace

void setup() {
    auto config = M5.config();
    M5Cardputer.begin(config, true);
    Serial.begin(kSerialBaud);

    M5Cardputer.Display.setRotation(1);
    irTransmitter.begin();

    drawScreen("Ready");

    Serial.println("GlobalController TV-006 ready");
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
