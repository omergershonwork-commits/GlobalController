#include "RemoteScreen.h"

#include <M5Cardputer.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr std::int32_t kStatusTop = 105;
constexpr std::int32_t kStatusHeight = 30;
constexpr std::int32_t kBatteryLeft = 204;
constexpr std::int32_t kBatteryWidth = 36;
constexpr std::int32_t kActivityLeft = 150;
constexpr std::int32_t kActivityWidth = 90;

void drawHintRow(const char* text, std::int32_t y) {
    auto& display = M5Cardputer.Display;
    display.setCursor(7, y);
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.print(text);
}
}  // namespace

RemoteScreen::RemoteScreen(const TvProfile& profile)
    : profile_(&profile),
      activity_{},
      activityColor_(GREEN),
      batteryPercent_(-1),
      charging_(false) {
    std::strncpy(activity_, "IR READY", sizeof(activity_) - 1);
}

void RemoteScreen::begin() {
    drawLayout();
    showReady();
}

void RemoteScreen::setProfile(const TvProfile& profile) {
    profile_ = &profile;
    drawLayout();
    drawStatus("TV SELECTED", profile.model(), CYAN);
}

void RemoteScreen::showReady() {
    const bool wifiProfile = profile_->routeFor(TvCommand::VolumeUp) == TvCommandRoute::Wifi;
    drawStatus(
        "READY",
        wifiProfile ? "[N] Wi-Fi  [P] IR power" : "Point IR edge toward television",
        GREEN
    );
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

        case RemoteEventType::WifiNotConfigured:
            drawStatus("WIFI OFF", "Press N to start Wi-Fi", YELLOW);
            return;

        case RemoteEventType::WifiNotReady:
            drawStatus("WIFI WAIT", "N cancels or restarts connection", YELLOW);
            return;

        case RemoteEventType::TransportError:
            drawStatus("SEND ERROR", event.label, RED);
            return;

        case RemoteEventType::CommandSent:
            break;
    }

    if (event.route == TvCommandRoute::Wifi) {
        drawStatus(
            event.repeated ? "WIFI REPEAT" : "WIFI SENT",
            event.label,
            event.repeated ? CYAN : GREEN
        );
        return;
    }

    const bool verified = event.verification == CodeVerification::VerifiedOnDevice;
    const std::uint16_t accentColor = verified ? (event.repeated ? CYAN : GREEN) : YELLOW;
    const char* state = event.repeated ? "HOLD REPEAT" : (verified ? "COMMAND SENT" : "TEST SIGNAL");
    drawStatus(state, event.label, accentColor);
}

void RemoteScreen::showMessage(
    const char* state,
    const char* detail,
    std::uint16_t accentColor
) {
    if (state != nullptr && std::strcmp(state, "PAIR CODE") == 0) {
        char pairingDetail[64];
        std::snprintf(
            pairingDetail,
            sizeof(pairingDetail),
            "%s  ENTER=SEND  N=CANCEL",
            detail == nullptr ? "Code: ______" : detail
        );
        drawStatus(state, pairingDetail, accentColor);
        return;
    }

    drawStatus(state, detail, accentColor);
}

void RemoteScreen::showIndicators(
    const char* activity,
    std::uint16_t activityColor,
    std::int32_t batteryPercent,
    bool charging
) {
    const char* safeActivity = activity == nullptr ? "UNKNOWN" : activity;
    const bool activityChanged =
        std::strncmp(activity_, safeActivity, sizeof(activity_)) != 0 ||
        activityColor_ != activityColor;
    const bool batteryChanged =
        batteryPercent_ != batteryPercent || charging_ != charging;

    if (!activityChanged && !batteryChanged) {
        return;
    }

    std::strncpy(activity_, safeActivity, sizeof(activity_) - 1);
    activity_[sizeof(activity_) - 1] = '\0';
    activityColor_ = activityColor;
    batteryPercent_ = batteryPercent;
    charging_ = charging;
    drawIndicators();
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
    display.print(profile_->brand());
    display.print(' ');
    display.print(profile_->model());

    drawIndicators();
    display.drawFastHLine(0, 35, display.width(), GREEN);

    drawHintRow("[T] TV [N] WIFI [P] POWER", 42);
    drawHintRow("[I] INPUT [H] HOME [U/J] VOLUME", 57);
    drawHintRow("[R/F] CHANNEL   [WASD] MOVE", 72);
    drawHintRow("[ENTER/O] OK    [DEL/B] BACK", 87);

    display.drawFastHLine(0, kStatusTop, display.width(), WHITE);
}

void RemoteScreen::drawIndicators() {
    auto& display = M5Cardputer.Display;

    display.setTextSize(1);

    display.fillRect(kBatteryLeft, 0, kBatteryWidth, 18, BLACK);
    display.setTextColor(charging_ ? CYAN : WHITE, BLACK);

    char batteryText[8];
    if (batteryPercent_ < 0 || batteryPercent_ > 100) {
        std::snprintf(batteryText, sizeof(batteryText), "--%%");
    } else {
        std::snprintf(
            batteryText,
            sizeof(batteryText),
            charging_ ? "+%ld%%" : "%ld%%",
            static_cast<long>(batteryPercent_)
        );
    }

    const std::int32_t batteryTextWidth = display.textWidth(batteryText);
    display.setCursor(display.width() - 5 - batteryTextWidth, 5);
    display.print(batteryText);

    display.fillRect(kActivityLeft, 20, kActivityWidth, 14, BLACK);
    display.setTextColor(activityColor_, BLACK);
    const std::int32_t activityTextWidth = display.textWidth(activity_);
    const std::int32_t activityX = display.width() - 5 - activityTextWidth;
    display.setCursor(activityX < kActivityLeft ? kActivityLeft : activityX, 25);
    display.print(activity_);
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
