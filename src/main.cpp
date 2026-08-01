#define DISABLE_CODE_FOR_RECEIVER
#define SEND_PWM_BY_TIMER

#include <M5Cardputer.h>
#include <IRremote.hpp>

namespace {
constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kLoopDelayMs = 5;
constexpr uint8_t kIrTxPin = 44;

// LG TV power signal commonly represented by legacy MSB code 0x20DF10EF.
// Arduino-IRremote's structured NEC form is address 0x04, command 0x08.
constexpr uint16_t kLgNecAddress = 0x04;
constexpr uint16_t kLgPowerCommand = 0x08;
constexpr int_fast8_t kPowerRepeats = 0;

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
        kLgNecAddress,
        kLgPowerCommand,
        kPowerRepeats,
        kIrTxPin
    );

    IrSender.sendNEC(kLgNecAddress, kLgPowerCommand, kPowerRepeats);
    drawScreen("Power IR sent", GREEN);
}
}  // namespace

void setup() {
    auto config = M5.config();
    M5Cardputer.begin(config, true);
    Serial.begin(kSerialBaud);

    M5Cardputer.Display.setRotation(1);

    IrSender.begin(DISABLE_LED_FEEDBACK);
    IrSender.setSendPin(kIrTxPin);

    drawScreen("Ready");

    Serial.println("GlobalController TV-002 ready");
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
