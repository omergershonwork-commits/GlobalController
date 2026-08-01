#include "Lg37Ld450Profile.h"

namespace {
constexpr std::uint16_t kLgAddress = 0x04;
constexpr std::uint16_t kXiaomiDevice = 0x86;
constexpr std::int8_t kNoRepeats = 0;

constexpr TvProfileEntry kLgEntries[] = {
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

constexpr TvProfileEntry kXiaomiEntries[] = {
    {TvCommand::Power, {IrProtocol::XiaomiRcmm, 0x3C, 0xCC, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::VolumeUp, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x0E, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::VolumeDown, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x0F, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::Mute, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0xA1, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::NavigateUp, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x05, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::NavigateDown, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x06, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::NavigateLeft, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x0B, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::NavigateRight, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x0C, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::Ok, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x0D, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::Back, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x07, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::Home, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x08, kNoRepeats}, CodeVerification::Provisional},
    {TvCommand::Input, {IrProtocol::XiaomiRcmm, kXiaomiDevice, 0x01, kNoRepeats}, CodeVerification::Provisional},
};

template <std::size_t EntryCount>
const TvProfileEntry* findEntry(
    const TvProfileEntry (&entries)[EntryCount],
    TvCommand command
) {
    for (const auto& entry : entries) {
        if (entry.command == command) {
            return &entry;
        }
    }

    return nullptr;
}
}  // namespace

const char* Lg37Ld450Profile::brand() const {
    return "LG";
}

const char* Lg37Ld450Profile::model() const {
    return "37LD450-ZA";
}

const TvProfileEntry* Lg37Ld450Profile::find(TvCommand command) const {
    return findEntry(kLgEntries, command);
}

const char* XiaomiMiTvMssp3Profile::brand() const {
    return "Xiaomi";
}

const char* XiaomiMiTvMssp3Profile::model() const {
    return "MiTV-MSSP3";
}

const TvProfileEntry* XiaomiMiTvMssp3Profile::find(TvCommand command) const {
    return findEntry(kXiaomiEntries, command);
}
