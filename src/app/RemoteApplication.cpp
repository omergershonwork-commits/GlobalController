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
    CodeVerification verification,
    const IrCode& code,
    bool repeated
) {
    return {type, command, label, verification, code, repeated};
}
}  // namespace

RemoteApplication::RemoteApplication(
    const TvProfile& profile,
    IrTransmitter& transmitter,
    std::uint32_t initialRepeatDelayMs,
    std::uint32_t repeatIntervalMs
)
    : profile_(&profile),
      transmitter_(transmitter),
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
    const TvProfileEntry* entry = profile_->find(binding.command);
    if (entry == nullptr) {
        return event(
            RemoteEventType::CommandUnavailable,
            binding.command,
            binding.label,
            CodeVerification::Provisional,
            kEmptyCode,
            repeated
        );
    }

    const SendResult result = transmitter_.send(entry->code);
    if (result != SendResult::Success) {
        return event(
            RemoteEventType::UnsupportedProtocol,
            binding.command,
            binding.label,
            entry->verification,
            entry->code,
            repeated
        );
    }

    return event(
        RemoteEventType::CommandSent,
        binding.command,
        binding.label,
        entry->verification,
        entry->code,
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
        CodeVerification::Provisional,
        kEmptyCode,
        false
    );
}
