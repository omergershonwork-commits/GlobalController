#pragma once

#include <cstdint>

namespace global_controller {

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
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
};

inline const char* tvCommandName(TvCommand command) {
    switch (command) {
        case TvCommand::Power:
            return "Power";
        case TvCommand::VolumeUp:
            return "Volume Up";
        case TvCommand::VolumeDown:
            return "Volume Down";
        case TvCommand::Mute:
            return "Mute";
        case TvCommand::ChannelUp:
            return "Channel Up";
        case TvCommand::ChannelDown:
            return "Channel Down";
        case TvCommand::NavigateUp:
            return "Up";
        case TvCommand::NavigateDown:
            return "Down";
        case TvCommand::NavigateLeft:
            return "Left";
        case TvCommand::NavigateRight:
            return "Right";
        case TvCommand::Ok:
            return "OK";
        case TvCommand::Back:
            return "Back";
        case TvCommand::Home:
            return "Home";
        case TvCommand::Input:
            return "Input";
        case TvCommand::Digit0:
            return "0";
        case TvCommand::Digit1:
            return "1";
        case TvCommand::Digit2:
            return "2";
        case TvCommand::Digit3:
            return "3";
        case TvCommand::Digit4:
            return "4";
        case TvCommand::Digit5:
            return "5";
        case TvCommand::Digit6:
            return "6";
        case TvCommand::Digit7:
            return "7";
        case TvCommand::Digit8:
            return "8";
        case TvCommand::Digit9:
            return "9";
    }

    return "Unknown";
}

}  // namespace global_controller
