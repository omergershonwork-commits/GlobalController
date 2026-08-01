#include <M5Cardputer.h>

#include <cstdint>

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

bool commandHeld = false;
KeyboardCommand activeCommand{false, TvCommand::Power, "", false};
std::uint32_t commandPressedAtMs = 0;
std::uint32_t lastCommandSentAtMs = 0;

const char* verificationLabel(CodeVerification verification) {
    return verification == CodeVerification::VerifiedOnDevice ? "verified" : "provisional";
}

bool hasElapsed(std::uint32_t now, std::uint32_t since, std::uint32_t duration) {
    return static_cast<std::uint32_t>(now - since) >= duration;
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

void sendCommand(const KeyboardCommand& binding) {
    const TvProfileEntry* entry = tvProfile.find(binding.command);
    if (entry == nullptr) {
        drawScreen(String(binding.label) + " unavailable", RED);
        return;
    }

    Serial.printf(
        "Sending %s %s: protocol=%u address=0x%02X command=0x%02X repeats=%d pin=%u\n",
        tvProfile.brand(),
        binding.label,
        static_cast<unsigned int>(entry->code.protocol),
        entry->code.address,
        entry->code.command,
        entry->code.repeats,
        kIrTxPin
    );
    Serial.printf(
        "Profile code status: %s; key repeat: %s\n",
        verificationLabel(entry->verification),
        binding.repeatable ? "enabled" : "disabled"
    );

    const SendResult result = irTransmitter.send(entry->code);
    if (result != SendResult::Success) {
        drawScreen("Unsupported protocol", RED);
        return;
    }

    if (entry->verification == CodeVerification::VerifiedOnDevice) {
        drawScreen(String(binding.label) + " IR sent", GREEN);
    } else {
        drawScreen(String(binding.label) + " test sent", YELLOW);
    }
}

void resetHeldCommand() {
    commandHeld = false;
    activeCommand = {false, TvCommand::Power, "", false};
}

void handleKeyboard() {
    const bool pressed = M5Cardputer.Keyboard.isPressed();
    const auto state = M5Cardputer.Keyboard.keysState();
    const KeyboardCommand mapped = KeyboardCommandMapper::map(state);

    if (!pressed) {
        resetHeldCommand();
        return;
    }

    if (!mapped.matched) {
        if (M5Cardputer.Keyboard.isChange()) {
            drawScreen("Unmapped key");
        }
        resetHeldCommand();
        return;
    }

    const std::uint32_t now = millis();
    if (!commandHeld || mapped.command != activeCommand.command) {
        activeCommand = mapped;
        commandHeld = true;
        commandPressedAtMs = now;
        lastCommandSentAtMs = now;
        sendCommand(mapped);
        return;
    }

    if (
        mapped.repeatable &&
        hasElapsed(now, commandPressedAtMs, kInitialRepeatDelayMs) &&
        hasElapsed(now, lastCommandSentAtMs, kRepeatIntervalMs)
    ) {
        lastCommandSentAtMs = now;
        sendCommand(mapped);
    }
}
}  // namespace

void setup() {
    auto config = M5.config();
    M5Cardputer.begin(config, true);
    Serial.begin(kSerialBaud);

    M5Cardputer.Display.setRotation(1);
    irTransmitter.begin();

    drawScreen("Ready");

    Serial.println("GlobalController TV-005 ready");
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
