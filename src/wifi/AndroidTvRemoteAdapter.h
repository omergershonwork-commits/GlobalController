#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "domain/TvCommand.h"
#include "pairing/PairingManager.h"
#include "remote/RemoteManager.h"

enum class AndroidTvRemoteState : std::uint8_t {
    Disabled,
    WifiConnecting,
    TvDiscovering,
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

    void begin(const char* ssid, const char* password);
    void cancel();
    void loop();

    bool requestPairing();
    bool skipCurrentCandidate();
    bool submitPairingCode(const String& code);
    bool send(TvCommand command);

    AndroidTvRemoteState state() const;
    bool configured() const;
    bool ready() const;
    bool pairingCodeRequired() const;
    const char* stateLabel() const;

    void candidateStatus(
        char* hostname,
        std::size_t hostnameCapacity,
        char* ipAddress,
        std::size_t ipAddressCapacity,
        std::uint8_t& candidateNumber,
        std::uint8_t& candidateCount,
        std::uint32_t& revision
    ) const;

private:
    static constexpr std::uint8_t kMaxCandidates = 8;

    struct Candidate {
        IPAddress address;
        std::uint16_t remotePort;
        char hostname[33];
    };

    void startDiscovery();
    bool discoverTv();
    bool connectSelectedCandidate();
    bool returnToCandidateSelection(const char* reason);
    bool connectRemote();
    bool startPairing();
    void resetCandidates();
    void publishCurrentCandidate();
    void sortCandidates();
    void setPairingKnown(bool known);
    void setPreferredHostname(const char* hostname);
    static bool toRemoteKey(TvCommand command, Remote__RemoteKeyCode& keyCode);

    RemoteManager remoteManager_;
    PairingManager pairingManager_;
    Preferences preferences_;
    volatile AndroidTvRemoteState state_;
    IPAddress tvIp_;
    std::uint16_t remotePort_;
    volatile bool configured_;
    bool pairingCodeSubmitted_;
    bool mdnsStarted_;
    bool preferencesOpened_;
    bool pairingKnown_;
    bool candidateSelectionReady_;
    unsigned long lastDiscoveryAttemptMs_;
    unsigned long pairingAttemptStartedMs_;
    char pairingServiceName_[24];
    char preferredHostname_[33];
    Candidate candidates_[kMaxCandidates];
    volatile std::uint8_t candidateCount_;
    volatile std::uint8_t candidateIndex_;
    volatile std::uint32_t candidateRevision_;
    char currentCandidateHostname_[33];
    char currentCandidateIp_[16];
};
