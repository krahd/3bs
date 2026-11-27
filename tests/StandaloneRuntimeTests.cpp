// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "standalone/StandaloneRuntime.h"
#include <juce_events/juce_events.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace {
using namespace threebs;
using Clock = std::chrono::steady_clock;
int failures{};

double now() noexcept {
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}
void check(bool condition, const char* message) {
    std::cout << (condition ? "PASS " : "FAIL ") << message << '\n';
    if (!condition) ++failures;
}

// The CoreMIDI callback performs no allocation or locking. Only this test's
// message thread reads the queue and updates its observed destination ledger.
class Capture final : public juce::MidiInputCallback {
public:
    struct TimedEvent { MidiEvent event{}; double received{}; };
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override {
        if (message.getRawDataSize() != 3) return;
        const auto* bytes = message.getRawData();
        const auto kind = bytes[0] & 0xf0U;
        if (kind != 0x80U && kind != 0x90U) return;
        MidiEvent event;
        event.type = kind == 0x90U && bytes[2] != 0U ? MidiEventType::NoteOn : MidiEventType::NoteOff;
        event.channel = static_cast<std::uint8_t>((bytes[0] & 0x0fU) + 1U);
        event.data1 = bytes[1]; event.data2 = bytes[2];
        if (!events_.push({event, now()})) dropped_.fetch_add(1U, std::memory_order_relaxed);
        if (event.type == MidiEventType::NoteOn) receivedOns_.fetch_add(1U, std::memory_order_relaxed);
    }
    void drain() {
        TimedEvent value;
        while (events_.pop(value)) {
            const auto& event = value.event;
            if (event.type == MidiEventType::NoteOn) {
                ++ons;
                if (measureIntervals && event.channel == 1U) {
                    if (intervalNotes == 0U) firstOn = value.received;
                    else maximumIntervalError = std::max(maximumIntervalError,
                        std::abs(value.received - lastOn - expectedPeriod));
                    lastOn = value.received;
                    ++intervalNotes;
                }
            } else {
                ++offs;
                if (!ledger.contains(event)) ++unmatchedOffs;
            }
            ledger.record(event);
            peakActive = std::max(peakActive, ledger.activeCount());
        }
    }
    std::uint64_t receivedOns() const noexcept { return receivedOns_.load(std::memory_order_relaxed); }
    std::uint64_t dropped() const noexcept { return dropped_.load(std::memory_order_relaxed); }
    static constexpr double expectedPeriod = 0.125; // 1/4 beat at 120 BPM.
    DeliveredNoteLedger ledger;
    std::uint64_t ons{}, offs{}, unmatchedOffs{}, intervalNotes{};
    std::size_t peakActive{};
    double firstOn{}, lastOn{}, maximumIntervalError{};
    bool measureIntervals{true};
private:
    SpscQueue<TimedEvent, 32768> events_;
    std::atomic<std::uint64_t> receivedOns_{}, dropped_{};
};

using Identifiers = std::vector<juce::String>;
Identifiers inputIdentifiers() {
    Identifiers result;
    for (const auto& device : juce::MidiInput::getAvailableDevices()) result.push_back(device.identifier);
    return result;
}
std::unique_ptr<juce::MidiInput> connectCreatedSource(const Identifiers& before, Capture& capture) {
    const auto deadline = now() + 3.0;
    while (now() < deadline) {
        for (const auto& device : juce::MidiInput::getAvailableDevices()) {
            if (device.name != "The Three Body Solution"
                || std::find(before.begin(), before.end(), device.identifier) != before.end()) continue;
            // Only the source that appeared after this test created it is used.
            // Existing virtual sources and physical destinations are untouched.
            auto input = juce::MidiInput::openDevice(device.identifier, &capture);
            if (input != nullptr) { input->start(); return input; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return {};
}

struct Harness {
    SpscQueue<RenderSnapshot, 64> snapshots;
    NoteVisualizationQueue notes;
    std::unique_ptr<StandaloneRuntime> runtime{std::make_unique<StandaloneRuntime>(snapshots, notes)};
    void drainVisuals() {
        RenderSnapshot snapshot;
        while (snapshots.pop(snapshot)) {}
        NoteVisualizationEvent event;
        while (notes.pop(event)) {}
    }
    void pump(Capture& capture) { capture.drain(); drainVisuals(); }
    template <typename Predicate>
    bool until(Capture& capture, Predicate predicate, double timeout = 1.0) {
        const auto deadline = now() + timeout;
        do {
            pump(capture);
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } while (now() < deadline);
        pump(capture);
        return predicate();
    }
    void settle(Capture& capture, double seconds = 0.15) {
        const auto deadline = now() + seconds;
        while (now() < deadline) {
            pump(capture);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        pump(capture);
    }
};

StandaloneControls testControls() {
    StandaloneControls result;
    result.initial = makeInitialState(InitialSystem::FigureEight, 20261127);
    result.playing = false;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& voice = result.engine.voices[body];
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.triggerMapping = TriggerMapping::Clock;
        voice.clockDivisionBeats = 0.25;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 8;
        voice.minimumTriggerIntervalBeats = 0;
        voice.probability = 1;
    }
    return result;
}
double stressSeconds() {
    if (const auto* text = std::getenv("THREEBS_STANDALONE_STRESS_SECONDS")) {
        char* end{};
        const auto value = std::strtod(text, &end);
        if (end != text && *end == '\0' && std::isfinite(value) && value >= 1.0 && value <= 1800.0)
            return value;
        std::cerr << "Invalid THREEBS_STANDALONE_STRESS_SECONDS (expected 1..1800).\n";
        ++failures;
    }
    return 2.0;
}
} // namespace

int main() {
#if !JUCE_MAC
    std::cout << "SKIP: native CoreMIDI loopback requires macOS.\n";
    return 77;
#else
    juce::ScopedJuceInitialiser_GUI initialise;
    // Establish whether the environment offers CoreMIDI at all. Once this
    // succeeds, a runtime routing/capture failure is a failure, never a skip.
    auto capability = juce::MidiOutput::createNewDevice("ThreeBS CI capability probe");
    if (capability == nullptr) {
        std::cout << "SKIP: environment did not permit creation of a virtual CoreMIDI source.\n";
        return 77;
    }
    capability.reset();
    check(juce::MessageManager::getInstance()->isThisTheMessageThread(),
          "test runs on the JUCE message thread");

    Harness harness;
    Capture original;
    auto controls = testControls();
    const auto initialInputs = inputIdentifiers();
    harness.runtime->publish(controls);
    check(harness.runtime->selectOutput(true, {}), "runtime accepts its virtual output route");
    harness.runtime->start();
    check(harness.until(original, [&] { return harness.runtime->outputReady(); }, 3.0),
          "worker creates the virtual CoreMIDI output");
    auto originalInput = connectCreatedSource(initialInputs, original);
    check(originalInput != nullptr, "loopback connects only to the test-created virtual source");
    if (originalInput == nullptr) { harness.runtime->stop(); return 1; }

    controls.playing = true;
    ++controls.resetRevision;
    harness.runtime->publish(controls);
    check(harness.until(original, [&] { return original.ledger.activeCount() == bodyCount; }),
          "physical loopback receives the three generated voice notes");
    const auto onsBeforeStall = original.receivedOns();
    const auto duration = stressSeconds();
    const auto stressStart = now();
    auto nextReport = stressStart + 30.0;
    while (now() - stressStart < duration) {
        // No dispatch loop and no UI timer run during this interval. Generation
        // and MIDI delivery must continue solely on their owning worker threads.
        std::this_thread::sleep_for(std::chrono::duration<double>(
            std::min(1.0, duration - (now() - stressStart))));
        harness.pump(original);
        if (now() >= nextReport) {
            std::cout << "STRESS elapsed=" << now() - stressStart
                      << " received_note_ons=" << original.ons << '\n';
            nextReport += 30.0;
        }
    }
    check(original.receivedOns() >= onsBeforeStall + bodyCount,
          "MIDI generation continues while the message thread is stalled");
    original.measureIntervals = false;
    const auto drift = original.intervalNotes > 1U
        ? (original.lastOn - original.firstOn)
            - static_cast<double>(original.intervalNotes - 1U) * Capture::expectedPeriod
        : 1.0e9;
    check(original.intervalNotes > 2U && std::abs(drift) < 0.2,
          "captured MIDI period does not accumulate message-thread timing drift");
    check(!harness.runtime->timingFault() && harness.runtime->overruns() == 0U
          && harness.runtime->maximumLatenessMilliseconds()
              <= StandaloneTimeline::maximumLatenessSeconds * 1000.0,
          "worker remains within its explicit lateness and overrun contract");

    // Long durations and a 32-second refractory interval separate transition
    // cleanup from natural note endings. Each deadline below is one second.
    for (auto& voice : controls.engine.voices) {
        voice.clockDivisionBeats = 16;
        voice.minimumTriggerIntervalBeats = 64;
    }
    ++controls.resetRevision;
    const auto offsBeforeReset = original.offs;
    harness.runtime->publish(controls);
    check(harness.until(original, [&] {
        return original.offs >= offsBeforeReset + bodyCount && original.ledger.activeCount() == bodyCount;
    }), "reset cleans the previous delivered identities before restarting voices");

    ++controls.panicRevision;
    harness.runtime->publish(controls);
    check(harness.until(original, [&] { return original.ledger.activeCount() == 0U; }),
          "panic terminates physically delivered notes before their natural endings");
    const auto onsAfterPanic = original.ons;
    harness.settle(original);
    check(original.ons == onsAfterPanic && original.ledger.activeCount() == 0U,
          "panic cannot replay cancelled pending output");

    ++controls.resetRevision;
    harness.runtime->publish(controls);
    check(harness.until(original, [&] { return original.ledger.activeCount() == bodyCount; }),
          "reset resumes generation after panic");
    controls.playing = false;
    harness.runtime->publish(controls);
    check(harness.until(original, [&] { return original.ledger.activeCount() == 0U; }),
          "stop terminates the original destination's active notes");
    const auto onsAfterStop = original.ons;
    harness.settle(original);
    check(original.ons == onsAfterStop, "stopped runtime does not deliver queued note-ons");

    controls.playing = true;
    ++controls.resetRevision;
    harness.runtime->publish(controls);
    check(harness.until(original, [&] { return original.ledger.activeCount() == bodyCount; }),
          "resume establishes notes before output replacement");
    const auto beforeReplacement = inputIdentifiers();
    check(harness.runtime->selectOutput(true, {}), "virtual output replacement is accepted");
    check(harness.until(original, [&] { return original.ledger.activeCount() == 0U; }),
          "output replacement cleans notes on the old physical source");
    Capture replacement;
    replacement.measureIntervals = false;
    auto replacementInput = connectCreatedSource(beforeReplacement, replacement);
    check(replacementInput != nullptr, "loopback reconnects only to the newly created replacement source");
    if (replacementInput == nullptr) {
        harness.runtime->stop(); originalInput->stop(); return 1;
    }
    ++controls.resetRevision;
    harness.runtime->publish(controls);
    check(harness.until(replacement, [&] { return replacement.ledger.activeCount() == bodyCount; }),
          "replacement destination receives only its own new note generation");

    const auto maximumLateness = harness.runtime->maximumLatenessMilliseconds();
    const auto overruns = harness.runtime->overruns();
    const auto visualDrops = harness.runtime->visualDrops();
    harness.runtime.reset(); // Destructor must join owners and clean the active output.
    check(harness.until(replacement, [&] { return replacement.ledger.activeCount() == 0U; }),
          "runtime destruction flushes the delivered-note ledger before closing output");
    harness.settle(original);
    harness.settle(replacement);
    originalInput->stop(); replacementInput->stop();
    original.drain(); replacement.drain();
    check(original.ledger.activeCount() == 0U && replacement.ledger.activeCount() == 0U
          && original.peakActive <= bodyCount && replacement.peakActive <= bodyCount,
          "reset, routing and shutdown leave no duplicate or stranded captured notes");
    check(original.unmatchedOffs == 0U && replacement.unmatchedOffs == 0U
          && original.dropped() == 0U && replacement.dropped() == 0U,
          "loopback capture observes balanced identities without dropped events");
    check(overruns == 0U, "all transition scenarios complete without worker overruns");
    std::cout << "{\"standalone_runtime\":{\"stress_seconds\":" << duration
              << ",\"captured_note_ons\":" << original.ons + replacement.ons
              << ",\"period_drift_seconds\":" << drift
              << ",\"max_interval_error_seconds\":" << original.maximumIntervalError
              << ",\"max_delivery_lateness_ms\":" << maximumLateness
              << ",\"overruns\":" << overruns << ",\"visual_drops_during_stall\":" << visualDrops
              << ",\"failures\":" << failures << "}}\n";
    return failures == 0 ? 0 : 1;
#endif
}
