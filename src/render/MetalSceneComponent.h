// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/SnapshotQueue.h"
#include "render/PresentationState.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <memory>

namespace threebs {

class MetalSceneComponent final : public juce::NSViewComponent {
public:
    MetalSceneComponent(SpscQueue<RenderSnapshot, 64>& snapshots,
                        NoteVisualizationQueue& noteVisualizationEvents);
    ~MetalSceneComponent() override;

    void setPresentationState(const PresentationState& state) noexcept;
    PresentationState presentationState() const noexcept;
    bool rendererAvailable() const noexcept;

    std::function<void(const CameraState&)> onCameraInteractionComplete;
    std::function<void(bool)> onNotePaneMinimizedChanged;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetalSceneComponent)
};

} // namespace threebs
