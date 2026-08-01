#pragma once

#include <M5Cardputer.h>

#include "domain/TvCommand.h"

struct KeyboardCommand {
    bool matched;
    TvCommand command;
    const char* label;
    bool repeatable;
};

class KeyboardCommandMapper final {
public:
    static KeyboardCommand map(const Keyboard_Class::KeysState& state);
};
