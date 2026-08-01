#pragma once

#include <cstdint>

#include "domain/CommandBinding.h"
#include "ir/IrTransmitter.h"
#include "profile/TvProfile.h"

enum class RemoteEventType : std::uint8_t {
    None,
    UnmappedInput,
    CommandUnavailable,
    UnsupportedProtocol,
    CommandSent,
};

struct RemoteEvent {
    RemoteEventType type;
    TvCommand command;
    const char* label;
    CodeVerification verification;
    IrCode code;
    bool repeated;
};

class RemoteApplication final {
public:
    RemoteApplication(
        const TvProfile& profile,
        IrTransmitter& transmitter,
        std::uint32_t initialRepeatDelayMs,
        std::uint32_t repeatIntervalMs
    );

    RemoteEvent update(
        const CommandBinding& binding,
        bool pressed,
        bool inputChanged,
        std::uint32_t nowMs
    );

    void setProfile(const TvProfile& profile);
    void reset();

private:
    RemoteEvent send(const CommandBinding& binding, bool repeated);
    static bool hasElapsed(std::uint32_t now, std::uint32_t since, std::uint32_t duration);
    static RemoteEvent noEvent();
    static RemoteEvent unmappedEvent();

    const TvProfile* profile_;
    IrTransmitter& transmitter_;
    const std::uint32_t initialRepeatDelayMs_;
    const std::uint32_t repeatIntervalMs_;

    bool commandHeld_;
    CommandBinding activeCommand_;
    std::uint32_t commandPressedAtMs_;
    std::uint32_t lastCommandSentAtMs_;
};
