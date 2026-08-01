#include <M5Cardputer.h>

#include "domain/TvCommand.h"
#include "ir/ArduinoIrTransmitter.h"
#include "profile/Lg37Ld450Profile.h"

namespace {
constexpr unsigned long kSerialBaud = 115200;
constexpr unsigned long kLoopDelayMs = 5;
constexpr std::uint8_t kIrTxPin = 44;

Lg37Ld450Profile tvProfile;
ArduinoIrTransmitter irTransmitter(kIrTxPin);

const char* verificationLabel(CodeVerification verification) {
    return verification == CodeVerification::VerifiedOnDevice ? "verified" : "provisional";
}

void drawScreen(const String& status, std::uint16_t statusColor = WHITE) {
    auto& display = M5Cardputer.Display;

    display.fillScreen(BLACK);
    display.setCursor(8, 8);
    display.setTextSize(2);
    display.setTextColor(GREEN, BLACK);
    display.println("GlobalController");

    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.println();
    display.print("TV: ");
    display.print(tvProfile.brand());
    display.print(' ');
    display.println(tvProfile.model());
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

void sendCommand(TvCommand command, const char* commandName) {
    const TvProfileEntry* entry = tvProfile.find(command);
    if (entry == nullptr) {
        drawScreen(String(commandName) + " unavailable", RED);
        return;
    }

    Serial.printf(
        "Sending %s %s: protocol=%u address=0x%02X command=0x%02X repeats=%d pin=%u\n",
        tvProfile.brand(),
        commandName,
        static_cast<unsigned int>(entry->code.protocol),
        entry->code.address,
        entry->code.command,
        entry->code.repeats,
        kIrTxPin
    );
    Serial.printf("Profile code status: %s\n", verificationLabel(entry->verification));

    const SendResult result = irTransmitter.send(entry->code);
    if (result == SendResult::Success) {
        drawScreen(String(commandName) + " IR sent", GREEN);
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

    Serial.println("GlobalController TV-004 ready");
    Serial.printf("Loaded profile: %s %s\n", tvProfile.brand(), tvProfile.model());
    Serial.printf("IR transmitter initialized on GPIO %u\n", kIrTxPin);
}

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        const auto state = M5Cardputer.Keyboard.keysState();

        if (containsPowerKey(state)) {
            sendCommand(TvCommand::Power, "Power");
        } else {
            const String description = describeKeyboardState(state);
            Serial.print("Keyboard input: ");
            Serial.println(description);
            drawScreen("Input: " + description);
        }
    }

    delay(kLoopDelayMs);
}
