// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/MusicEngine.h"
#include "render/PresentationState.h"

#include <juce_core/juce_core.h>
#include <cstddef>

namespace threebs {

inline constexpr int userConfigurationSchemaVersion = 4;
inline constexpr std::size_t maximumUserConfigurationBytes = 2U * 1024U * 1024U;

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
    int voicingPresetIndex{-1};
    bool run{true};
    bool sync{true};
    TimeSignatureSource timeSignatureSource{TimeSignatureSource::Host};
    int timeSignatureNumerator{4};
    int timeSignatureDenominator{4};
};

bool validateUserConfiguration(const UserConfiguration& configuration, juce::String& error);
// Bounded file input, including files that grow after their size is inspected.
// The destination is unchanged unless a complete valid configuration is read.
bool loadUserConfiguration(const juce::File& file, UserConfiguration& configuration,
                           juce::String& error);
juce::String serializeUserConfiguration(const UserConfiguration& configuration);
bool deserializeUserConfiguration(const juce::String& json, UserConfiguration& configuration,
                                  juce::String& error);

} // namespace threebs
