#include "RemoteScreen.h"

#include <M5Cardputer.h>

namespace {
constexpr std::int32_t kStatusTop = 105;
constexpr std::int32_t kStatusHeight = 30;

void drawHintRow(const char* text, std::int32_t y) {
    auto& display = M5Cardputer.Display;
    display.setCursor(7, y);
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.print(text);
}
}  // namespace

RemoteScreen::RemoteScreen(const TvProfile& profile) : profile_(profile) {}

void RemoteScreen::begin() {
    drawLayout();
    showReady();
}

void RemoteScreen::showReady() {
    drawStatus("READY", "Point IR edge toward television", GREEN);
}

void RemoteScreen::showEvent(const RemoteEvent& event) {
    switch (event.type) {
        case RemoteEventType::None:
            return;

        case RemoteEventType::UnmappedInput:
            drawStatus("UNMAPPED KEY", "No TV action assigned", YELLOW);
            return;

        case RemoteEventType::CommandUnavailable:
            drawStatus("UNAVAILABLE", event.label, RED);
            return;

        case RemoteEventType::UnsupportedProtocol:
            drawStatus("IR ERROR", "Protocol is not supported", RED);
            return;

        case RemoteEventType::CommandSent:
            break;
    }

    const bool verified = event.verification == CodeVerification::VerifiedOnDevice;
    const std::uint16_t accentColor = verified ? (event.repeated ? CYAN : GREEN) : YELLOW;
    const char* state = event.repeated ? "HOLD REPEAT" : (verified ? "COMMAND SENT" : "TEST SIGNAL");
    drawStatus(state, event.label, accentColor);
}

void RemoteScreen::drawLayout() {
    auto& display = M5Cardputer.Display;

    display.setTextWrap(false);
    display.fillScreen(BLACK);

    display.setCursor(7, 4);
    display.setTextSize(2);
    display.setTextColor(GREEN, BLACK);
    display.print("GlobalController");

    display.setCursor(7, 25);
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.print(profile_.brand());
    display.print(' ');
    display.print(profile_.model());

    display.drawFastHLine(0, 35, display.width(), GREEN);

    drawHintRow("[P] POWER  [M] MUTE  [I] INPUT", 42);
    drawHintRow("[H] HOME   [U/J] VOLUME", 57);
    drawHintRow("[R/F] CHANNEL   [WASD] MOVE", 72);
    drawHintRow("[ENTER/O] OK    [DEL/B] BACK", 87);

    display.drawFastHLine(0, kStatusTop, display.width(), WHITE);
}

void RemoteScreen::drawStatus(
    const char* state,
    const char* detail,
    std::uint16_t accentColor
) {
    auto& display = M5Cardputer.Display;

    display.fillRect(0, kStatusTop, display.width(), kStatusHeight, BLACK);
    display.drawFastHLine(0, kStatusTop, display.width(), accentColor);

    display.setCursor(7, 109);
    display.setTextSize(1);
    display.setTextColor(accentColor, BLACK);
    display.print(state);

    display.setCursor(7, 121);
    display.setTextColor(WHITE, BLACK);
    display.print(detail);
}
