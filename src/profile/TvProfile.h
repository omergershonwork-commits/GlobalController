#pragma once

#include <cstdint>

#include "domain/TvCommand.h"
#include "ir/IrCode.h"

enum class CodeVerification : std::uint8_t {
    VerifiedOnDevice,
    Provisional,
};

enum class TvCommandRoute : std::uint8_t {
    Infrared,
    Wifi,
};

struct TvProfileEntry {
    TvCommand command;
    IrCode code;
    CodeVerification verification;
};

class TvProfile {
public:
    virtual ~TvProfile() = default;

    virtual const char* brand() const = 0;
    virtual const char* model() const = 0;
    virtual TvCommandRoute routeFor(TvCommand command) const = 0;
    virtual const TvProfileEntry* find(TvCommand command) const = 0;
};
