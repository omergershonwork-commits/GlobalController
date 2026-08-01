#pragma once

#include "TvProfile.h"

class Lg37Ld450Profile final : public TvProfile {
public:
    const char* brand() const override;
    const char* model() const override;
    TvCommandRoute routeFor(TvCommand command) const override;
    const TvProfileEntry* find(TvCommand command) const override;
};

class XiaomiMiTvMssp3Profile final : public TvProfile {
public:
    const char* brand() const override;
    const char* model() const override;
    TvCommandRoute routeFor(TvCommand command) const override;
    const TvProfileEntry* find(TvCommand command) const override;
};
