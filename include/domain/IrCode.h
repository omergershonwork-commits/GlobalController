#pragma once

#include <cstdint>

namespace global_controller {

enum class IrProtocol : std::uint8_t {
    Nec,
};

struct IrCode {
    IrProtocol protocol;
    std::uint16_t address;
    std::uint16_t command;
    std::int8_t repeats;
};

}  // namespace global_controller
