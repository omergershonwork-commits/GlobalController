#define DISABLE_CODE_FOR_RECEIVER
#define SEND_PWM_BY_TIMER

#include <IRremote.hpp>

#include "ArduinoIrTransmitter.h"

namespace {
constexpr std::uint16_t kXiaomiUnitUs = 290;
constexpr std::uint16_t kXiaomiHeaderMarkUs = 1000;
constexpr std::uint16_t kXiaomiSymbolMarkUs = 2 * kXiaomiUnitUs;
constexpr std::uint8_t kXiaomiFrequencyKHz = 36;
constexpr std::uint8_t kXiaomiRawLength = 23;
constexpr unsigned long kXiaomiRepeatGapMs = 30;

std::uint8_t xiaomiChecksum(std::uint8_t device, std::uint8_t function) {
    return static_cast<std::uint8_t>(
        ((device >> 4) ^ device ^ (function >> 4) ^ function) & 0x0F
    );
}

void sendXiaomiRcmm(const IrCode& code) {
    const std::uint8_t device = static_cast<std::uint8_t>(code.address);
    const std::uint8_t function = static_cast<std::uint8_t>(code.command);
    const std::uint32_t payload =
        (static_cast<std::uint32_t>(device) << 12) |
        (static_cast<std::uint32_t>(function) << 4) |
        xiaomiChecksum(device, function);

    std::uint16_t raw[kXiaomiRawLength];
    std::uint8_t index = 0;
    raw[index++] = kXiaomiHeaderMarkUs;
    raw[index++] = kXiaomiSymbolMarkUs;

    for (std::int8_t shift = 18; shift >= 0; shift -= 2) {
        const std::uint8_t symbol = static_cast<std::uint8_t>((payload >> shift) & 0x03);
        raw[index++] = kXiaomiSymbolMarkUs;
        raw[index++] = static_cast<std::uint16_t>((2 + symbol) * kXiaomiUnitUs);
    }

    raw[index++] = kXiaomiSymbolMarkUs;

    const std::uint8_t repeatCount = code.repeats > 0
        ? static_cast<std::uint8_t>(code.repeats)
        : 0;

    for (std::uint8_t transmission = 0; transmission <= repeatCount; ++transmission) {
        IrSender.sendRaw(raw, index, kXiaomiFrequencyKHz);
        if (transmission < repeatCount) {
            delay(kXiaomiRepeatGapMs);
        }
    }
}
}  // namespace

ArduinoIrTransmitter::ArduinoIrTransmitter(std::uint8_t pin) : pin_(pin) {}

void ArduinoIrTransmitter::begin() {
    IrSender.begin(DISABLE_LED_FEEDBACK);
    IrSender.setSendPin(pin_);
}

SendResult ArduinoIrTransmitter::send(const IrCode& code) {
    switch (code.protocol) {
        case IrProtocol::Nec:
            IrSender.sendNEC(code.address, code.command, code.repeats);
            return SendResult::Success;

        case IrProtocol::XiaomiRcmm:
            sendXiaomiRcmm(code);
            return SendResult::Success;

        default:
            return SendResult::UnsupportedProtocol;
    }
}
