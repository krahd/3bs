// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/SnapshotQueue.h"
#include "render/MetalSceneComponent.h"

#include <juce_events/juce_events.h>

#import <Metal/Metal.h>

#include <cstdlib>
#include <iostream>

int main() {
    juce::ScopedJuceInitialiser_GUI juce;
    if (MTLCreateSystemDefaultDevice() == nil) {
        std::cerr << "Metal device unavailable in this execution environment; skipping\n";
        return 77;
    }
    threebs::SpscQueue<threebs::RenderSnapshot, 64> snapshots;
    threebs::NoteVisualizationQueue noteEvents;
    threebs::MetalSceneComponent scene(snapshots, noteEvents);
    if (!scene.rendererAvailable()) {
        std::cerr << "Metal renderer or shader pipeline unavailable\n";
        return EXIT_FAILURE;
    }
    std::cout << "Metal renderer and shaders available\n";
    return EXIT_SUCCESS;
}
