#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "AndroidTvRemoteAdapter.h"

class AndroidTvRemoteRuntime final {
public:
    explicit AndroidTvRemoteRuntime(AndroidTvRemoteAdapter& adapter);

    bool begin();
    bool start(const char* ssid, const char* password);
    bool cancel();
    bool requestPairing();
    bool skipCandidate();
    bool send(TvCommand command);
    bool submitPairingCode(const String& code);

    AndroidTvRemoteState state() const;
    bool configured() const;
    bool ready() const;
    bool pairingCodeRequired() const;
    bool requested() const;
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

    unsigned long maxTickDurationMs();
    std::uint32_t minimumFreeStackBytes() const;
    std::uint32_t configuredStackBytes() const;

private:
    enum class MessageType : std::uint8_t {
        Start,
        Cancel,
        BeginPairing,
        SkipCandidate,
        SendCommand,
        PairingCode,
    };

    struct Message {
        MessageType type;
        TvCommand command;
        char ssid[33];
        char password[65];
        char pairingCode[7];
    };

    static void taskEntry(void* context);
    void run();
    void handleMessage(const Message& message);
    bool enqueue(const Message& message, bool urgent = false);
    void sampleStackWatermark();

    AndroidTvRemoteAdapter& adapter_;
    QueueHandle_t queue_;
    TaskHandle_t task_;
    volatile bool requested_;
    volatile bool configured_;
    volatile bool workerActive_;
    volatile unsigned long maxTickDurationMs_;
    volatile std::uint32_t minimumFreeStackBytes_;
};
