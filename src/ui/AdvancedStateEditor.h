// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Types.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace threebs {

void showAdvancedStateEditor(const SimulationState& state, juce::Rectangle<int> targetArea,
                             juce::Component& parent,
                             std::function<void(const SimulationState&)> onApply);

} // namespace threebs
