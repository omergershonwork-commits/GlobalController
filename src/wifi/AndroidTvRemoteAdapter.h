#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "domain/TvCommand.h"
#include "pairing/PairingManager.h"
#include "remote/RemoteManager.h"

enum class AndroidTvRemoteState : std::uint8_t {
    Disabled,
    WifiConnecting,
    RemoteConnecting,
    PairingConnecting,
    PairingCodeRequired,
    PairingSubmitting,
    Ready,
    Error,
};

class AndroidTvRemoteAdapter final {
public:
    AndroidTvRemoteAdapter();

    void begin(const char* ssid, const char* password, const IPAddress& tvIp);
    void loop();

    bool submitPairingCode(const String& code);
    bool send(TvCommand command);

    AndroidTvRemoteState state() const;
    bool configured() const;
    bool ready() const;
    bool pairingCodeRequired() const;
    const char* stateLabel() const;

private:
    void connectRemote();
    void startPairing();
    static bool toRemoteKey(TvCommand command, Remote__RemoteKeyCode& keyCode);

    RemoteManager remoteManager_;
    PairingManager pairingManager_;
    AndroidTvRemoteState state_;
    IPAddress tvIp_;
    bool configured_;
    bool pairingCodeSubmitted_;
    char pairingServiceName_[24];
};
