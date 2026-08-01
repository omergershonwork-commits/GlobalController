#pragma once

#include <cstdint>

#include "app/RemoteApplication.h"
#include "profile/TvProfile.h"

class RemoteScreen final {
public:
    explicit RemoteScreen(const TvProfile& profile);

    void begin();
    void showReady();
    void showEvent(const RemoteEvent& event);

private:
    void drawLayout();
    void drawStatus(const char* state, const char* detail, std::uint16_t accentColor);

    const TvProfile& profile_;
};
