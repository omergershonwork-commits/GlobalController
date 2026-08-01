#include "Lg37Ld450Profile.h"

namespace {
constexpr std::uint16_t kLgAddress = 0x04;
constexpr std::int8_t kNoRepeats = 0;

constexpr TvProfileEntry kEntries[] = {
    {TvCommand::Power, {IrProtocol::Nec, kLgAddress, 0x08, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::VolumeUp, {IrProtocol::Nec, kLgAddress, 0x02, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::VolumeDown, {IrProtocol::Nec, kLgAddress, 0x03, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::Mute, {IrProtocol::Nec, kLgAddress, 0x09, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::ChannelUp, {IrProtocol::Nec, kLgAddress, 0x00, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::ChannelDown, {IrProtocol::Nec, kLgAddress, 0x01, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::NavigateUp, {IrProtocol::Nec, kLgAddress, 0x40, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::NavigateDown, {IrProtocol::Nec, kLgAddress, 0x41, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::NavigateLeft, {IrProtocol::Nec, kLgAddress, 0x07, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::NavigateRight, {IrProtocol::Nec, kLgAddress, 0x06, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::Ok, {IrProtocol::Nec, kLgAddress, 0x44, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::Back, {IrProtocol::Nec, kLgAddress, 0x28, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::Home, {IrProtocol::Nec, kLgAddress, 0x43, kNoRepeats}, CodeVerification::VerifiedOnDevice},
    {TvCommand::Input, {IrProtocol::Nec, kLgAddress, 0x0B, kNoRepeats}, CodeVerification::VerifiedOnDevice},
};
}  // namespace

const char* Lg37Ld450Profile::brand() const {
    return "LG";
}

const char* Lg37Ld450Profile::model() const {
    return "37LD450-ZA";
}

const TvProfileEntry* Lg37Ld450Profile::find(TvCommand command) const {
    for (const auto& entry : kEntries) {
        if (entry.command == command) {
            return &entry;
        }
    }

    return nullptr;
}
