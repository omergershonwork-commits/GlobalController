#include <M5Cardputer.h>

#include "ir/ArduinoIrTransmitter.h"

namespace {
constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kLoopDelayMs = 5;
constexpr uint8_t kIrTxPin = 44;

constexpr IrCode kLgPowerCode{
    IrProtocol::Nec,
    0x04,
    0x08,
    0,
};

ArduinoIrTransmitter irTransmitter(kIrTxPin);

void drawScreen(const String& status, uint16_t statusColor = WHITE) {
    auto& display = M5Cardputer.Display;

    display.fillScreen(BLACK);
    display.setCursor(8, 8);
    display.setTextSize(2);
    display.setTextColor(GREEN, BLACK);
    display.println("GlobalController");

    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.println();
    display.println("TV: LG 37LD450-ZA");
    display.println("Press P to toggle power");
    display.println("Point IR edge at the TV");
    display.println();
    display.setTextColor(statusColor, BLACK);
    display.print("Status: ");
    display.println(status);
}

String describeKeyboardState(const Keyboard_Class::KeysState& state) {
    String description;

    for (const char character : state.word) {
        description += character;
    }

    if (state.enter) {
        if (description.length() > 0) {
            description += ' ';
        }
        description += "[ENTER]";
    }

    if (state.del) {
        if (description.length() > 0) {
            description += ' ';
        }
        description += "[DEL]";
    }

    if (description.length() == 0) {
        description = "[SPECIAL KEY]";
    }

    return description;
}

bool containsPowerKey(const Keyboard_Class::KeysState& state) {
    for (const char character : state.word) {
        if (character == 'p' || character == 'P') {
            return true;
        }
    }

    return false;
}

void sendLgPower() {
    Serial.printf(
        "Sending LG power: protocol=NEC address=0x%02X command=0x%02X repeats=%d pin=%u\n",
        kLgPowerCode.address,
        kLgPowerCode.command,
        kLgPowerCode.repeats,
        kIrTxPin
    );

    const SendResult result = irTransmitter.send(kLgPowerCode);
    if (result == SendResult::Success) {
        drawScreen("Power IR sent", GREEN);
    } else {
        drawScreen("Unsupported protocol", RED);
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

    Serial.println("GlobalController TV-003 ready");
    Serial.printf("IR transmitter initialized on GPIO %u\n", kIrTxPin);
}

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        const auto state = M5Cardputer.Keyboard.keysState();

        if (containsPowerKey(state)) {
            sendLgPower();
        } else {
            const String description = describeKeyboardState(state);
            Serial.print("Keyboard input: ");
            Serial.println(description);
            drawScreen("Input: " + description);
        }
    }

    delay(kLoopDelayMs);
}
