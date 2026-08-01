#define DISABLE_CODE_FOR_RECEIVER
#define SEND_PWM_BY_TIMER

#include <IRremote.hpp>

#include "ir/ArduinoIrTransmitter.h"

namespace global_controller {

ArduinoIrTransmitter::ArduinoIrTransmitter(std::uint8_t pin) : pin_(pin) {}

void ArduinoIrTransmitter::begin() {
    IrSender.begin(DISABLE_LED_FEEDBACK);
    IrSender.setSendPin(pin_);
}

SendStatus ArduinoIrTransmitter::send(const IrCode& code) {
    if (code.repeats < 0) {
        return SendStatus::InvalidCode;
    }

    switch (code.protocol) {
        case IrProtocol::Nec:
            IrSender.sendNEC(code.address, code.command, code.repeats);
            return SendStatus::Success;
    }

    return SendStatus::UnsupportedProtocol;
}

std::uint8_t ArduinoIrTransmitter::pin() const {
    return pin_;
}

}  // namespace global_controller
