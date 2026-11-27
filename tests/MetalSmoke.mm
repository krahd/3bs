// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/SnapshotQueue.h"
#include "render/MetalSceneComponent.h"

#include <juce_events/juce_events.h>
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace threebs {
// Test-only access does not add failure controls to product UI or public API.
class MetalSceneTestAccess {
public:
    static std::unique_ptr<MetalSceneComponent> failBuffer(
        SpscQueue<RenderSnapshot, 64>& snapshots, NoteVisualizationQueue& notes, int index) {
        return std::unique_ptr<MetalSceneComponent>(new MetalSceneComponent(snapshots, notes, index));
    }
    static void requestCapture(MetalSceneComponent& scene) { scene.requestFrameCapture(); }
    static RendererFrameCapture capture(const MetalSceneComponent& scene) { return scene.frameCapture(); }
};
} // namespace threebs

namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void pumpMessages() {
    @autoreleasepool {
        while (auto* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode dequeue:YES])
            [NSApp sendEvent:event];
        [NSApp updateWindows];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                               beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.005]];
    }
}

template <typename Predicate>
void await(threebs::MetalSceneComponent& scene, Predicate predicate, const char* failure) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!predicate()) {
        check(scene.rendererAvailable() && scene.performanceMetrics().failedFrameCount == 0,
              "Metal resource or GPU command failure during actual-frame test");
        check(std::chrono::steady_clock::now() < deadline, failure);
        pumpMessages();
    }
}

class SmokeWindow final : public juce::DocumentWindow {
public:
    explicit SmokeWindow(threebs::MetalSceneComponent& scene)
        : DocumentWindow("3bs renderer acceptance", juce::Colours::black, closeButton) {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setContentNonOwned(&scene, false);
        centreWithSize(900, 600);
        setVisible(true);
    }
    void closeButtonPressed() override {}
};

void drawAndWait(threebs::MetalSceneComponent& scene, MTKView* view) {
    std::uint64_t submittedTarget{};
    await(scene, [&] {
        if (submittedTarget == 0) {
            const auto previous = scene.performanceMetrics().frameCount;
            [view draw];
            const auto submitted = scene.performanceMetrics().frameCount;
            if (submitted > previous)
                submittedTarget = submitted;
        }
        return submittedTarget != 0
            && scene.performanceMetrics().completedFrameCount >= submittedTarget;
    }, "Metal device exists but no drawable frame completed within eight seconds");
}

void fillHistory(threebs::SpscQueue<threebs::RenderSnapshot, 64>& snapshots,
                 threebs::NoteVisualizationQueue& notes, std::uint64_t& sequence,
                 std::size_t count, std::uint64_t revision, double timeline) {
    threebs::RenderSnapshot snapshot;
    snapshot.bodies = {{{1.0, {-1.0, 0.0, 0.0}, {}}, {1.0, {1.0, 0.0, 0.0}, {}},
                        {1.0, {0.0, 1.1, 0.0}, {}}}};
    snapshot.trajectoryRevision = revision;
    snapshot.sequence = revision;
    snapshot.timelineSeconds = timeline;
    check(snapshots.push(snapshot), "snapshot queue overflow in native renderer test");
    for (std::size_t index = 0; index < count; ++index) {
        threebs::NoteVisualizationEvent event;
        event.type = threebs::NoteVisualizationType::On;
        event.body = static_cast<std::uint8_t>(index % 3);
        event.note = static_cast<std::uint8_t>(48 + index % 24);
        event.velocity = 100;
        event.sequence = ++sequence;
        event.trajectoryRevision = revision;
        event.timelineSeconds = timeline - 5.5 + 5.4 * static_cast<double>(index)
                                                       / static_cast<double>(count - 1U);
        check(notes.push(event), "note-on queue overflow in native renderer test");
        event.type = threebs::NoteVisualizationType::Off;
        event.sequence = ++sequence;
        event.timelineSeconds += 0.025;
        check(notes.push(event), "note-off queue overflow in native renderer test");
    }
}

void writeCapture(const threebs::RendererFrameCapture& capture, const char* path) {
    auto* bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nullptr pixelsWide:static_cast<NSInteger>(capture.width)
        pixelsHigh:static_cast<NSInteger>(capture.height) bitsPerSample:8 samplesPerPixel:4
        hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
        bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
        bytesPerRow:static_cast<NSInteger>(capture.width * 4U) bitsPerPixel:32];
    check(bitmap != nil, "unable to allocate PNG capture bitmap");
    auto* rgba = bitmap.bitmapData;
    for (std::size_t pixel = 0; pixel < capture.width * capture.height; ++pixel) {
        const auto offset = pixel * 4U;
        rgba[offset] = capture.bgraPixels[offset + 2U];
        rgba[offset + 1U] = capture.bgraPixels[offset + 1U];
        rgba[offset + 2U] = capture.bgraPixels[offset];
        rgba[offset + 3U] = capture.bgraPixels[offset + 3U];
    }
    NSData* png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    const BOOL saved = png != nil && [png writeToFile:[NSString stringWithUTF8String:path] atomically:YES];
    [bitmap release];
    check(saved, "unable to write requested Metal frame capture");
}
} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juce;
    @autoreleasepool {
        auto device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            std::cerr << "Metal device unavailable in this execution environment; skipping\n";
            return 77;
        }
        [device release];
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [NSApp finishLaunching];
        try {
            // Three immutable buffers plus six resources for each of three
            // in-flight frames. Every allocation is independently required.
            for (int allocation = 0; allocation < 21; ++allocation) {
                @autoreleasepool {
                    threebs::SpscQueue<threebs::RenderSnapshot, 64> snapshots;
                    threebs::NoteVisualizationQueue notes;
                    auto failed = threebs::MetalSceneTestAccess::failBuffer(snapshots, notes, allocation);
                    check(!failed->rendererAvailable(), "missing buffer incorrectly reported renderer ready");
                    auto* view = static_cast<MTKView*>(failed->getView());
                    if (view != nil)
                        [view draw]; // Must return before dereferencing missing resources.
                }
            }

            threebs::SpscQueue<threebs::RenderSnapshot, 64> snapshots;
            threebs::NoteVisualizationQueue notes;
            threebs::MetalSceneComponent scene(snapshots, notes);
            check(scene.rendererAvailable(), "Metal renderer or shader pipeline unavailable");
            SmokeWindow window(scene);
            auto* view = static_cast<MTKView*>(scene.getView());
            check(view != nil, "native Metal view unavailable");
            view.paused = YES;
            view.enableSetNeedsDisplay = NO;
            threebs::PresentationState presentation;
            presentation.notePaneStyle = threebs::NotePaneStyle::Vertical;
            presentation.camera.autoOrbit = 0.0F;
            scene.setPresentationState(presentation);
            std::uint64_t sequence{};
            std::uint64_t revision{1};
            for (const std::size_t count : {28U, 29U, 96U}) {
                fillHistory(snapshots, notes, sequence, count, revision, 10.0 * static_cast<double>(revision));
                drawAndWait(scene, view);
                const auto metrics = scene.performanceMetrics();
                check(metrics.maxGlyphCount >= count * 3U,
                      "vertical note history omitted glyphs at a tested upload boundary");
                check(metrics.failedFrameCount == 0, "GPU command reported a rendering error");
                ++revision;
            }
            check(scene.performanceMetrics().maxGlyphBytes >= 13824,
                  "dense note history must exercise an upload larger than the inline 4-KB limit");

            for (const auto size : {juce::Point<int>{640, 480}, {1280, 720}, {1600, 900}}) {
                window.setSize(size.x, size.y);
                pumpMessages();
                for (int frame = 0; frame < 6; ++frame)
                    [view draw];
                drawAndWait(scene, view);
            }
            window.setVisible(false);
            pumpMessages();
            window.setVisible(true);
            drawAndWait(scene, view);

            fillHistory(snapshots, notes, sequence, 96, revision, 10.0 * static_cast<double>(revision));
            threebs::MetalSceneTestAccess::requestCapture(scene);
            drawAndWait(scene, view);
            threebs::RendererFrameCapture capture;
            await(scene, [&] {
                capture = threebs::MetalSceneTestAccess::capture(scene);
                return !capture.bgraPixels.empty();
            }, "GPU frame readback did not complete");
            check(capture.width >= 320 && capture.height >= 200,
                  "captured native frame has invalid dimensions");
            unsigned minimum = 255, maximum = 0;
            std::size_t brightPixels{};
            for (std::size_t pixel = 0; pixel < capture.width * capture.height; ++pixel) {
                unsigned brightest{};
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    const unsigned value = capture.bgraPixels[pixel * 4U + channel];
                    minimum = std::min(minimum, value);
                    maximum = std::max(maximum, value);
                    brightest = std::max(brightest, value);
                }
                brightPixels += static_cast<std::size_t>(brightest > 48);
            }
            check(maximum - minimum > 32 && brightPixels > 64,
                  "captured frame contains no meaningful rendered content");
            if (const auto* path = std::getenv("THREEBS_RENDER_CAPTURE_PATH"))
                writeCapture(capture, path);

            await(scene, [&] {
                const auto metrics = scene.performanceMetrics();
                return metrics.completedFrameCount == metrics.frameCount;
            }, "submitted GPU frames were not all completed");
            const auto metrics = scene.performanceMetrics();
            check(metrics.failedFrameCount == 0, "native frame integration reported failures");
            std::cout << "Metal drawable integration passed: " << metrics.completedFrameCount
                      << " completed frames, " << metrics.maxGlyphBytes
                      << " glyph bytes, zero GPU errors; all 21 allocation failures handled\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
