#pragma once

#include <cstdint>

enum class TvCommand : std::uint8_t {
    Power,
    VolumeUp,
    VolumeDown,
    Mute,
    ChannelUp,
    ChannelDown,
    NavigateUp,
    NavigateDown,
    NavigateLeft,
    NavigateRight,
    Ok,
    Back,
    Home,
    Input,
};
