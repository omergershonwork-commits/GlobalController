#include "AndroidTvRemoteRuntime.h"

#include <cstring>

namespace {
constexpr std::size_t kQueueLength = 12;
constexpr std::uint32_t kTaskStackBytes = 32768;
constexpr std::uint32_t kStackWarningBytes = 4096;
constexpr UBaseType_t kTaskPriority = tskIDLE_PRIORITY;
constexpr BaseType_t kNetworkCore = 0;
constexpr TickType_t kWorkerDelay = pdMS_TO_TICKS(5);

void copyText(char* destination, std::size_t capacity, const char* source) {
    if (destination == nullptr || capacity == 0) {
        return;
    }

    const char* safeSource = source == nullptr ? "" : source;
    std::strncpy(destination, safeSource, capacity - 1);
    destination[capacity - 1] = '\0';
}
}  // namespace

AndroidTvRemoteRuntime::AndroidTvRemoteRuntime(AndroidTvRemoteAdapter& adapter)
    : adapter_(adapter),
      queue_(nullptr),
      task_(nullptr),
      requested_(false),
      configured_(false),
      workerActive_(false),
      maxTickDurationMs_(0),
      minimumFreeStackBytes_(kTaskStackBytes),
      pairingCodePending_(false),
      pendingPairingCode_{} {}

bool AndroidTvRemoteRuntime::begin() {
    if (task_ != nullptr) {
        return true;
    }

    queue_ = xQueueCreate(kQueueLength, sizeof(Message));
    if (queue_ == nullptr) {
        Serial.println("Failed to create Android TV runtime queue");
        return false;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry,
        "android-tv",
        kTaskStackBytes,
        this,
        kTaskPriority,
        &task_,
        kNetworkCore
    );

    if (created != pdPASS) {
        vQueueDelete(queue_);
        queue_ = nullptr;
        task_ = nullptr;
        Serial.println("Failed to create Android TV runtime task");
        return false;
    }

    Serial.printf(
        "Android TV runtime task started on core %d with %lu-byte stack at idle priority\n",
        static_cast<int>(kNetworkCore),
        static_cast<unsigned long>(kTaskStackBytes)
    );
    return true;
}

bool AndroidTvRemoteRuntime::start(const char* ssid, const char* password) {
    if (queue_ == nullptr || ssid == nullptr || ssid[0] == '\0') {
        configured_ = false;
        requested_ = false;
        return false;
    }

    Message message{};
    message.type = MessageType::Start;
    copyText(message.ssid, sizeof(message.ssid), ssid);
    copyText(message.password, sizeof(message.password), password);

    configured_ = true;
    requested_ = true;

    if (!enqueue(message)) {
        requested_ = false;
        return false;
    }

    return true;
}

bool AndroidTvRemoteRuntime::cancel() {
    requested_ = false;

    if (queue_ == nullptr) {
        return false;
    }

    Message message{};
    message.type = MessageType::Cancel;
    return enqueue(message, true);
}

bool AndroidTvRemoteRuntime::send(TvCommand command) {
    if (!ready() || queue_ == nullptr) {
        return false;
    }

    Message message{};
    message.type = MessageType::SendCommand;
    message.command = command;
    return enqueue(message);
}

bool AndroidTvRemoteRuntime::submitPairingCode(const String& code) {
    if (!requested_ || queue_ == nullptr || code.length() != 6) {
        return false;
    }

    Message message{};
    message.type = MessageType::PairingCode;
    copyText(message.pairingCode, sizeof(message.pairingCode), code.c_str());
    return enqueue(message);
}

AndroidTvRemoteState AndroidTvRemoteRuntime::state() const {
    return requested_ ? adapter_.state() : AndroidTvRemoteState::Disabled;
}

bool AndroidTvRemoteRuntime::configured() const {
    return configured_;
}

bool AndroidTvRemoteRuntime::ready() const {
    return requested_ && adapter_.ready();
}

bool AndroidTvRemoteRuntime::pairingCodeRequired() const {
    return requested_ && adapter_.pairingCodeRequired();
}

bool AndroidTvRemoteRuntime::requested() const {
    return requested_;
}

const char* AndroidTvRemoteRuntime::stateLabel() const {
    return requested_ ? adapter_.stateLabel() : "Wi-Fi remote stopped";
}

unsigned long AndroidTvRemoteRuntime::maxTickDurationMs() {
    const unsigned long duration = maxTickDurationMs_;
    maxTickDurationMs_ = 0;
    return duration;
}

std::uint32_t AndroidTvRemoteRuntime::minimumFreeStackBytes() const {
    return minimumFreeStackBytes_;
}

std::uint32_t AndroidTvRemoteRuntime::configuredStackBytes() const {
    return kTaskStackBytes;
}

void AndroidTvRemoteRuntime::taskEntry(void* context) {
    auto* runtime = static_cast<AndroidTvRemoteRuntime*>(context);
    runtime->run();
}

void AndroidTvRemoteRuntime::run() {
    Message message{};
    sampleStackWatermark();

    for (;;) {
        while (xQueueReceive(queue_, &message, 0) == pdTRUE) {
            handleMessage(message);
            sampleStackWatermark();
        }

        if (workerActive_ && requested_) {
            const unsigned long startedMs = millis();
            adapter_.loop();
            processPendingPairingCode();
            const unsigned long durationMs = millis() - startedMs;
            if (durationMs > maxTickDurationMs_) {
                maxTickDurationMs_ = durationMs;
            }
            sampleStackWatermark();
        }

        vTaskDelay(kWorkerDelay);
    }
}

void AndroidTvRemoteRuntime::handleMessage(const Message& message) {
    switch (message.type) {
        case MessageType::Start:
            if (!requested_) {
                return;
            }
            if (workerActive_) {
                adapter_.cancel();
            }
            clearPendingPairingCode();
            adapter_.begin(message.ssid, message.password);
            workerActive_ = adapter_.configured();
            return;

        case MessageType::Cancel:
            clearPendingPairingCode();
            if (workerActive_) {
                adapter_.cancel();
            }
            workerActive_ = false;
            return;

        case MessageType::SendCommand:
            if (workerActive_ && requested_) {
                adapter_.send(message.command);
            }
            return;

        case MessageType::PairingCode:
            if (workerActive_ && requested_) {
                copyText(
                    pendingPairingCode_,
                    sizeof(pendingPairingCode_),
                    message.pairingCode
                );
                pairingCodePending_ = true;
                Serial.println(
                    "Pairing code queued; it will be submitted when the TV handshake is ready"
                );
                processPendingPairingCode();
            }
            return;
    }
}

bool AndroidTvRemoteRuntime::enqueue(const Message& message, bool urgent) {
    if (queue_ == nullptr) {
        return false;
    }

    const BaseType_t sent = urgent
        ? xQueueSendToFront(queue_, &message, 0)
        : xQueueSendToBack(queue_, &message, 0);

    if (sent != pdTRUE) {
        Serial.println("Android TV runtime queue is full");
        return false;
    }

    return true;
}

void AndroidTvRemoteRuntime::processPendingPairingCode() {
    if (
        !pairingCodePending_ ||
        !workerActive_ ||
        !requested_ ||
        !adapter_.pairingCodeRequired()
    ) {
        return;
    }

    if (adapter_.submitPairingCode(String(pendingPairingCode_))) {
        Serial.println("Queued pairing code submitted to the television");
        clearPendingPairingCode();
    } else {
        Serial.println("Pairing code submission failed; keeping it queued for retry");
    }
}

void AndroidTvRemoteRuntime::clearPendingPairingCode() {
    pairingCodePending_ = false;
    pendingPairingCode_[0] = '\0';
}

void AndroidTvRemoteRuntime::sampleStackWatermark() {
    const std::uint32_t freeBytes = static_cast<std::uint32_t>(
        uxTaskGetStackHighWaterMark(nullptr)
    );

    if (freeBytes >= minimumFreeStackBytes_) {
        return;
    }

    minimumFreeStackBytes_ = freeBytes;
    Serial.printf(
        "Android TV task stack watermark: %lu bytes free\n",
        static_cast<unsigned long>(freeBytes)
    );

    if (freeBytes < kStackWarningBytes) {
        Serial.println("WARNING: Android TV task stack is close to exhaustion");
    }
}
