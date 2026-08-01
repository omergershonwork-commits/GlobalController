#pragma once

#include "TvCommand.h"

struct CommandBinding {
    bool matched;
    TvCommand command;
    const char* label;
    bool repeatable;
};
