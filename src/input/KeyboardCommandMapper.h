#pragma once

#include <M5Cardputer.h>

#include "domain/CommandBinding.h"

class KeyboardCommandMapper final {
public:
    static CommandBinding map(const Keyboard_Class::KeysState& state);
};
