#pragma once

#include <cstdint>

enum class IrProtocol : std::uint8_t {
    Nec,
    Samsung,
    Sony,
    Lg,
    Panasonic,
    Rc5,
    Rc6,
    XiaomiRcmm,
    Raw,
};

struct IrCode {
    IrProtocol protocol;
    std::uint16_t address;
    std::uint16_t command;
    std::int8_t repeats;
};
