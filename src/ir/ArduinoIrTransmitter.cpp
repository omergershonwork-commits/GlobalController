#define DISABLE_CODE_FOR_RECEIVER
#define SEND_PWM_BY_TIMER

#include <IRremote.hpp>

#include "ArduinoIrTransmitter.h"

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

        default:
            return SendResult::UnsupportedProtocol;
    }
}
