#pragma once

#include "domain/IrCode.h"

namespace global_controller {

enum class SendStatus {
    Success,
    UnsupportedProtocol,
    InvalidCode,
};

class IrTransmitter {
public:
    virtual ~IrTransmitter() = default;

    virtual void begin() = 0;
    virtual SendStatus send(const IrCode& code) = 0;
};

}  // namespace global_controller
