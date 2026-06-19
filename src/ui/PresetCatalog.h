// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Types.h"
#include "render/MetalSceneComponent.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace threebs {

struct ArtworkPreset {
    juce::String id;
    juce::String name;
    juce::String description;
    InitialSystem system{InitialSystem::FigureEight};
    std::uint64_t seed{};
    double chaos{};
    SimulationConfig simulation{};
    LoopPolicy loopPolicy{LoopPolicy::Restart};
    std::array<VoiceConfig, bodyCount> voices{};
    VisualSettings visual{};
};

class PresetCatalog {
public:
    PresetCatalog();

    bool valid() const noexcept { return valid_; }
    std::size_t size() const noexcept { return presets_.size(); }
    const ArtworkPreset& operator[](std::size_t index) const;
    juce::StringArray names() const;

private:
    std::vector<ArtworkPreset> presets_;
    bool valid_{};
};

} // namespace threebs
