#include "RemoteApplication.h"

namespace {
constexpr IrCode kEmptyCode{
    IrProtocol::Nec,
    0,
    0,
    0,
};

RemoteEvent event(
    RemoteEventType type,
    TvCommand command,
    const char* label,
    TvCommandRoute route,
    CodeVerification verification,
    const IrCode& code,
    bool repeated
) {
    return {type, command, label, route, verification, code, repeated};
}

RemoteEventType eventTypeFor(CommandDeliveryStatus status) {
    switch (status) {
        case CommandDeliveryStatus::Sent:
            return RemoteEventType::CommandSent;
        case CommandDeliveryStatus::Unavailable:
            return RemoteEventType::CommandUnavailable;
        case CommandDeliveryStatus::UnsupportedProtocol:
            return RemoteEventType::UnsupportedProtocol;
        case CommandDeliveryStatus::WifiNotConfigured:
            return RemoteEventType::WifiNotConfigured;
        case CommandDeliveryStatus::WifiNotReady:
            return RemoteEventType::WifiNotReady;
        case CommandDeliveryStatus::Failed:
            return RemoteEventType::TransportError;
    }

    return RemoteEventType::TransportError;
}
}  // namespace

RemoteApplication::RemoteApplication(
    const TvProfile& profile,
    TvCommandSender& commandSender,
    std::uint32_t initialRepeatDelayMs,
    std::uint32_t repeatIntervalMs
)
    : profile_(&profile),
      commandSender_(commandSender),
      initialRepeatDelayMs_(initialRepeatDelayMs),
      repeatIntervalMs_(repeatIntervalMs),
      commandHeld_(false),
      activeCommand_{false, TvCommand::Power, "", false},
      commandPressedAtMs_(0),
      lastCommandSentAtMs_(0) {}

RemoteEvent RemoteApplication::update(
    const CommandBinding& binding,
    bool pressed,
    bool inputChanged,
    std::uint32_t nowMs
) {
    if (!pressed) {
        reset();
        return noEvent();
    }

    if (!binding.matched) {
        reset();
        return inputChanged ? unmappedEvent() : noEvent();
    }

    if (!commandHeld_ || binding.command != activeCommand_.command) {
        activeCommand_ = binding;
        commandHeld_ = true;
        commandPressedAtMs_ = nowMs;
        lastCommandSentAtMs_ = nowMs;
        return send(binding, false);
    }

    if (
        binding.repeatable &&
        hasElapsed(nowMs, commandPressedAtMs_, initialRepeatDelayMs_) &&
        hasElapsed(nowMs, lastCommandSentAtMs_, repeatIntervalMs_)
    ) {
        lastCommandSentAtMs_ = nowMs;
        return send(binding, true);
    }

    return noEvent();
}

void RemoteApplication::setProfile(const TvProfile& profile) {
    profile_ = &profile;
    reset();
}

void RemoteApplication::reset() {
    commandHeld_ = false;
    activeCommand_ = {false, TvCommand::Power, "", false};
    commandPressedAtMs_ = 0;
    lastCommandSentAtMs_ = 0;
}

RemoteEvent RemoteApplication::send(const CommandBinding& binding, bool repeated) {
    const CommandDeliveryResult delivery = commandSender_.send(
        *profile_,
        binding.command
    );

    return event(
        eventTypeFor(delivery.status),
        binding.command,
        binding.label,
        delivery.route,
        delivery.verification,
        delivery.code,
        repeated
    );
}

bool RemoteApplication::hasElapsed(
    std::uint32_t now,
    std::uint32_t since,
    std::uint32_t duration
) {
    return static_cast<std::uint32_t>(now - since) >= duration;
}

RemoteEvent RemoteApplication::noEvent() {
    return event(
        RemoteEventType::None,
        TvCommand::Power,
        "",
        TvCommandRoute::Infrared,
        CodeVerification::Provisional,
        kEmptyCode,
        false
    );
}

RemoteEvent RemoteApplication::unmappedEvent() {
    return event(
        RemoteEventType::UnmappedInput,
        TvCommand::Power,
        "",
        TvCommandRoute::Infrared,
        CodeVerification::Provisional,
        kEmptyCode,
        false
    );
}
