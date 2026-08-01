#pragma once

#include <cstdint>

#include "IrTransmitter.h"

class ArduinoIrTransmitter final : public IrTransmitter {
public:
    explicit ArduinoIrTransmitter(std::uint8_t pin);

    void begin() override;
    SendResult send(const IrCode& code) override;

private:
    std::uint8_t pin_;
};
