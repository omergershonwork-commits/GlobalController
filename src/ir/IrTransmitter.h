#pragma once

#include "IrCode.h"

enum class SendResult {
    Success,
    UnsupportedProtocol,
};

class IrTransmitter {
public:
    virtual ~IrTransmitter() = default;
    virtual void begin() = 0;
    virtual SendResult send(const IrCode& code) = 0;
};
