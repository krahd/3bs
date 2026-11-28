// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/SnapshotQueue.h"
#include "render/PresentationState.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace threebs {

struct RendererPerformanceMetrics {
    double framesPerSecond{};
    double cpuFrameMilliseconds{};
    double maxCpuFrameMilliseconds{};
    std::uint64_t frameCount{};
    std::uint64_t completedFrameCount{};
    std::uint64_t failedFrameCount{};
    std::uint64_t skippedFrameCount{};
    std::uint64_t maxGlyphCount{};
    std::uint64_t maxGlyphBytes{};
    double gpuFrameMilliseconds{};
};

struct RendererFrameCapture {
    std::size_t width{};
    std::size_t height{};
    std::vector<std::uint8_t> bgraPixels;
};

class MetalSceneComponent final : public juce::NSViewComponent {
public:
    MetalSceneComponent(SpscQueue<RenderSnapshot, 64>& snapshots,
                        NoteVisualizationQueue& noteVisualizationEvents);
    ~MetalSceneComponent() override;

    void setPresentationState(const PresentationState& state) noexcept;
    PresentationState presentationState() const noexcept;
    bool rendererAvailable() const noexcept;
    RendererPerformanceMetrics performanceMetrics() const noexcept;

    std::function<void(const CameraState&)> onCameraInteractionComplete;
    std::function<void(bool)> onNotePaneMinimizedChanged;
    std::function<void(NotePaneStyle)> onNotePaneStyleChanged;

private:
    friend class MetalSceneTestAccess;
    // Only the native integration test can inject a particular buffer allocation
    // failure. Normal construction always passes -1.
    MetalSceneComponent(SpscQueue<RenderSnapshot, 64>& snapshots,
                        NoteVisualizationQueue& noteVisualizationEvents,
                        int failedBufferAllocation);
    void requestFrameCapture();
    RendererFrameCapture frameCapture() const;
    class Impl;
    std::unique_ptr<Impl> impl_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetalSceneComponent)
};

} // namespace threebs
