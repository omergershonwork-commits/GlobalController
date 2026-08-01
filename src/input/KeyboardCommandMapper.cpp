#include "KeyboardCommandMapper.h"

namespace {
CommandBinding noMatch() {
    return {false, TvCommand::Power, "", false};
}

CommandBinding binding(TvCommand command, const char* label, bool repeatable) {
    return {true, command, label, repeatable};
}

char toLowerAscii(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}
}  // namespace

CommandBinding KeyboardCommandMapper::map(const Keyboard_Class::KeysState& state) {
    if (state.enter) {
        return binding(TvCommand::Ok, "OK", false);
    }

    if (state.del) {
        return binding(TvCommand::Back, "Back", false);
    }

    for (const char rawCharacter : state.word) {
        switch (toLowerAscii(rawCharacter)) {
            case 'p':
                return binding(TvCommand::Power, "Power", false);
            case 'm':
                return binding(TvCommand::Mute, "Mute", false);
            case 'u':
                return binding(TvCommand::VolumeUp, "Volume up", true);
            case 'j':
                return binding(TvCommand::VolumeDown, "Volume down", true);
            case 'r':
                return binding(TvCommand::ChannelUp, "Channel up", true);
            case 'f':
                return binding(TvCommand::ChannelDown, "Channel down", true);
            case 'w':
                return binding(TvCommand::NavigateUp, "Navigate up", true);
            case 's':
                return binding(TvCommand::NavigateDown, "Navigate down", true);
            case 'a':
                return binding(TvCommand::NavigateLeft, "Navigate left", true);
            case 'd':
                return binding(TvCommand::NavigateRight, "Navigate right", true);
            case 'o':
                return binding(TvCommand::Ok, "OK", false);
            case 'b':
                return binding(TvCommand::Back, "Back", false);
            case 'h':
                return binding(TvCommand::Home, "Home", false);
            case 'i':
                return binding(TvCommand::Input, "Input", false);
            default:
                break;
        }
    }

    return noMatch();
}
