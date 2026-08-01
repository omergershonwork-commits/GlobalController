#include "HybridTvCommandSender.h"

namespace {
constexpr IrCode kEmptyCode{
    IrProtocol::Nec,
    0,
    0,
    0,
};

CommandDeliveryResult result(
    CommandDeliveryStatus status,
    TvCommandRoute route,
    CodeVerification verification,
    const IrCode& code
) {
    return {status, route, verification, code};
}
}  // namespace

HybridTvCommandSender::HybridTvCommandSender(
    IrTransmitter& infraredTransmitter,
    AndroidTvRemoteAdapter& wifiRemote
)
    : infraredTransmitter_(infraredTransmitter),
      wifiRemote_(wifiRemote) {}

CommandDeliveryResult HybridTvCommandSender::send(
    const TvProfile& profile,
    TvCommand command
) {
    return profile.routeFor(command) == TvCommandRoute::Wifi
        ? sendWifi(command)
        : sendInfrared(profile, command);
}

CommandDeliveryResult HybridTvCommandSender::sendInfrared(
    const TvProfile& profile,
    TvCommand command
) {
    const TvProfileEntry* entry = profile.find(command);
    if (entry == nullptr) {
        return result(
            CommandDeliveryStatus::Unavailable,
            TvCommandRoute::Infrared,
            CodeVerification::Provisional,
            kEmptyCode
        );
    }

    const SendResult sendResult = infraredTransmitter_.send(entry->code);
    if (sendResult != SendResult::Success) {
        return result(
            CommandDeliveryStatus::UnsupportedProtocol,
            TvCommandRoute::Infrared,
            entry->verification,
            entry->code
        );
    }

    return result(
        CommandDeliveryStatus::Sent,
        TvCommandRoute::Infrared,
        entry->verification,
        entry->code
    );
}

CommandDeliveryResult HybridTvCommandSender::sendWifi(TvCommand command) {
    if (!wifiRemote_.configured()) {
        return result(
            CommandDeliveryStatus::WifiNotConfigured,
            TvCommandRoute::Wifi,
            CodeVerification::Provisional,
            kEmptyCode
        );
    }

    if (!wifiRemote_.ready()) {
        return result(
            CommandDeliveryStatus::WifiNotReady,
            TvCommandRoute::Wifi,
            CodeVerification::Provisional,
            kEmptyCode
        );
    }

    if (!wifiRemote_.send(command)) {
        return result(
            CommandDeliveryStatus::Failed,
            TvCommandRoute::Wifi,
            CodeVerification::Provisional,
            kEmptyCode
        );
    }

    return result(
        CommandDeliveryStatus::Sent,
        TvCommandRoute::Wifi,
        CodeVerification::Provisional,
        kEmptyCode
    );
}
