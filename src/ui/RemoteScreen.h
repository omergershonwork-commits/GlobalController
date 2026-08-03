#pragma once

#include <cstdint>

#include "app/RemoteApplication.h"
#include "profile/TvProfile.h"

class RemoteScreen final {
public:
    explicit RemoteScreen(const TvProfile& profile);

    void begin();
    void setProfile(const TvProfile& profile);
    void showReady();
    void showEvent(const RemoteEvent& event);
    void showMessage(
        const char* state,
        const char* detail,
        std::uint16_t accentColor
    );
    void showIndicators(
        const char* activity,
        std::uint16_t activityColor,
        std::int32_t batteryPercent,
        bool charging
    );

private:
    void drawLayout();
    void drawStatus(const char* state, const char* detail, std::uint16_t accentColor);
    void drawIndicators();

    const TvProfile* profile_;
    char activity_[16];
    std::uint16_t activityColor_;
    std::int32_t batteryPercent_;
    bool charging_;
};
