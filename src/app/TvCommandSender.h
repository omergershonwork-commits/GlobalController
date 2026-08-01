#pragma once

#include <cstdint>

#include "profile/TvProfile.h"

enum class CommandDeliveryStatus : std::uint8_t {
    Sent,
    Unavailable,
    UnsupportedProtocol,
    WifiNotConfigured,
    WifiNotReady,
    Failed,
};

struct CommandDeliveryResult {
    CommandDeliveryStatus status;
    TvCommandRoute route;
    CodeVerification verification;
    IrCode code;
};

class TvCommandSender {
public:
    virtual ~TvCommandSender() = default;

    virtual CommandDeliveryResult send(
        const TvProfile& profile,
        TvCommand command
    ) = 0;
};
