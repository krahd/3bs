// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#pragma once
#include "core/MusicEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
namespace threebs {
void showAdvancedConfigurationEditor(const EngineConfig&, juce::Component&,
                                     std::function<void(const EngineConfig&)>);
// Only the fields available in this editor are transferred. Main controls may
// continue to receive host automation while this nonmodal editor is open.
void copyAdvancedConfigurationFields(const EngineConfig& source, EngineConfig& destination) noexcept;
} // namespace threebs
