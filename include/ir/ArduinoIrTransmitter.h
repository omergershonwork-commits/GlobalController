#pragma once

#include <cstdint>

#include "ir/IrTransmitter.h"

namespace global_controller {

class ArduinoIrTransmitter final : public IrTransmitter {
public:
    explicit ArduinoIrTransmitter(std::uint8_t pin);

    void begin() override;
    SendStatus send(const IrCode& code) override;

    std::uint8_t pin() const;

private:
    std::uint8_t pin_;
};

}  // namespace global_controller
