// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/MusicEngine.h"
#include "render/PresentationState.h"

#include <juce_core/juce_core.h>

namespace threebs {

inline constexpr int userConfigurationSchemaVersion = 2;

struct UserConfiguration {
    SimulationState initial{};
    SimulationState baseInitial{};
    std::array<double, bodyCount> planeTilts{};
    EngineConfig engine{};
    PresentationState presentation{};
    LoopPolicy loopPolicy{LoopPolicy::Restart};
    double chaosPercent{20.0};
    double densityPercent{80.0};
    int presetIndex{};
    bool run{true};
    bool sync{true};
};

juce::String serializeUserConfiguration(const UserConfiguration& configuration);
bool deserializeUserConfiguration(const juce::String& json, UserConfiguration& configuration,
                                  juce::String& error);

} // namespace threebs
