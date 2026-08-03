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
    bool send(TvCommand command);
    bool submitPairingCode(const String& code);

    AndroidTvRemoteState state() const;
    bool configured() const;
    bool ready() const;
    bool pairingCodeRequired() const;
    bool requested() const;
    const char* stateLabel() const;

    unsigned long maxTickDurationMs();

private:
    enum class MessageType : std::uint8_t {
        Start,
        Cancel,
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

    AndroidTvRemoteAdapter& adapter_;
    QueueHandle_t queue_;
    TaskHandle_t task_;
    volatile bool requested_;
    volatile bool configured_;
    volatile bool workerActive_;
    volatile unsigned long maxTickDurationMs_;
};
