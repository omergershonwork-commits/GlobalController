#pragma once

#include "TvCommandSender.h"
#include "ir/IrTransmitter.h"
#include "wifi/AndroidTvRemoteRuntime.h"

class HybridTvCommandSender final : public TvCommandSender {
public:
    HybridTvCommandSender(
        IrTransmitter& infraredTransmitter,
        AndroidTvRemoteRuntime& wifiRemote
    );

    CommandDeliveryResult send(
        const TvProfile& profile,
        TvCommand command
    ) override;

private:
    CommandDeliveryResult sendInfrared(
        const TvProfile& profile,
        TvCommand command
    );

    CommandDeliveryResult sendWifi(TvCommand command);

    IrTransmitter& infraredTransmitter_;
    AndroidTvRemoteRuntime& wifiRemote_;
};
