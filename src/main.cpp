#include <M5Cardputer.h>

namespace {
constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kLoopDelayMs = 5;

void drawScreen(const String& lastInput) {
    auto& display = M5Cardputer.Display;

    display.fillScreen(BLACK);
    display.setCursor(8, 8);
    display.setTextSize(2);
    display.setTextColor(GREEN, BLACK);
    display.println("GlobalController");

    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.println();
    display.println("TV remote bootstrap");
    display.println("Press a keyboard key");
    display.println();
    display.print("Last input: ");
    display.println(lastInput);
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
}  // namespace

void setup() {
    auto config = M5.config();
    M5Cardputer.begin(config, true);
    Serial.begin(kSerialBaud);

    M5Cardputer.Display.setRotation(1);
    drawScreen("none");

    Serial.println("GlobalController TV remote bootstrap ready");
}

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        const auto state = M5Cardputer.Keyboard.keysState();
        const String description = describeKeyboardState(state);

        Serial.print("Keyboard input: ");
        Serial.println(description);
        drawScreen(description);
    }

    delay(kLoopDelayMs);
}
