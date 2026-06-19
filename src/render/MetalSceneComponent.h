// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/SnapshotQueue.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

namespace threebs {

struct VisualSettings {
    float trailLength{0.82F};
    float trailWidth{1.2F};
    float extrusion{0.12F};
    float bloom{0.34F};
    float starDensity{0.62F};
    float autoOrbit{0.035F};
    int focusBody{-1};
};

class MetalSceneComponent final : public juce::NSViewComponent {
public:
    explicit MetalSceneComponent(SpscQueue<RenderSnapshot, 64>& snapshots);
    ~MetalSceneComponent() override;

    void setVisualSettings(const VisualSettings& settings) noexcept;
    bool rendererAvailable() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetalSceneComponent)
};

} // namespace threebs
