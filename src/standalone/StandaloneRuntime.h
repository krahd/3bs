// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/SnapshotQueue.h"
#include "standalone/StandaloneTimeline.h"
#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

namespace threebs {

struct StandaloneControls {
    EngineConfig engine{};
    SimulationState initial{};
    std::uint64_t resetRevision{1};
    std::uint64_t panicRevision{};
    bool playing{true};
    int numerator{4};
    int denominator{4};
};

class StandaloneRuntime {
public:
    StandaloneRuntime(SpscQueue<RenderSnapshot, 64>& snapshots, NoteVisualizationQueue& notes)
        : snapshots_(snapshots), notes_(notes) {}
    ~StandaloneRuntime() { stop(); }
    void start() {
        if (generation_.joinable()) return;
        stopping_.store(false);
        delivery_ = std::thread([this] { deliver(); });
        generation_ = std::thread([this] { generate(); });
    }
    void stop() {
        stopping_.store(true, std::memory_order_release);
        if (generation_.joinable()) generation_.join();
        if (delivery_.joinable()) delivery_.join();
    }
    void publish(const StandaloneControls& controls) noexcept { controls_.publish(controls); }
    bool selectOutput(bool virtualOutput, const juce::String& identifier) {
        Route route;
        route.virtualOutput = virtualOutput;
        const auto utf8 = identifier.toRawUTF8();
        const auto length = std::strlen(utf8);
        if (length >= route.identifier.size()) return false;
        std::memcpy(route.identifier.data(), utf8, length + 1U);
        route.revision = ++routeRevision_;
        routes_.publish(route);
        return true;
    }
    bool outputReady() const noexcept { return outputReady_.load(std::memory_order_relaxed); }
    bool timingFault() const noexcept { return timingFault_.load(std::memory_order_relaxed); }
    std::uint64_t overruns() const noexcept { return overruns_.load(std::memory_order_relaxed); }
    std::uint64_t visualDrops() const noexcept { return visualDrops_.load(std::memory_order_relaxed); }
    double maximumLatenessMilliseconds() const noexcept {
        return static_cast<double>(maximumLatenessMicros_.load(std::memory_order_relaxed)) / 1000.0;
    }

private:
    using Clock = std::chrono::steady_clock;
    static double now() noexcept {
        return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
    }
    struct Route {
        std::array<char, 512> identifier{};
        std::uint64_t revision{};
        bool virtualOutput{true};
    };
    struct ScheduledEvent {
        MidiEvent event{};
        double due{};
        std::uint64_t epoch{};
    };
    struct Emission {
        StandaloneRuntime* runtime{};
        const StandaloneTimeline* timeline{};
        std::uint64_t epoch{};
        std::uint64_t trajectoryRevision{};
        bool failed{};
    };
    // Destination cleanup is guaranteed by the independent epoch/ledger path.
    static EventSink discardAfterEpochCleanup() noexcept {
        return {nullptr, [](void*, const MidiEvent&) noexcept {}};
    }
    static void enqueue(void* context, const MidiEvent& event) noexcept {
        auto& emission = *static_cast<Emission*>(context);
        auto& self = *emission.runtime;
        if (emission.failed) return;
        const auto sample = emission.timeline->cursor() + event.sampleOffset;
        if (!self.events_.push({event, emission.timeline->deadline(sample), emission.epoch})) {
            emission.failed = true;
            self.timingFault_.store(true, std::memory_order_release);
            self.epoch_.fetch_add(1U, std::memory_order_acq_rel);
            self.overruns_.fetch_add(1U, std::memory_order_relaxed);
            return;
        }
        if (event.type != MidiEventType::ControlChange) {
            NoteVisualizationEvent visual;
            visual.type = event.type == MidiEventType::NoteOn ? NoteVisualizationType::On : NoteVisualizationType::Off;
            visual.body = event.sourceBody;
            visual.note = event.data1;
            visual.velocity = event.data2;
            visual.sequence = ++self.noteSequence_;
            visual.timelineSeconds = static_cast<double>(sample) / StandaloneTimeline::sampleRate;
            visual.trajectoryRevision = emission.trajectoryRevision;
            if (!self.notes_.push(visual)) self.visualDrops_.fetch_add(1U, std::memory_order_relaxed);
        }
    }
    void generate() {
        MusicEngine engine;
        engine.prepare(StandaloneTimeline::sampleRate);
        StandaloneTimeline timeline(now());
        StandaloneControls controls;
        std::uint64_t resetRevision{}, panicRevision{}, trajectoryRevision{1}, sequence{}, knownEpoch{};
        bool wasPlaying{};
        std::array<std::uint32_t, bodyCount> respawns{};
        while (!stopping_.load(std::memory_order_acquire)) {
            StandaloneControls incoming;
            if (controls_.consume(incoming)) {
                if (!isValidEngineConfig(incoming.engine) || !isValidSimulationState(incoming.initial))
                    continue;
                const bool resetting = incoming.resetRevision != resetRevision;
                const bool panic = incoming.panicRevision != panicRevision;
                const bool stopping = controls.playing && !incoming.playing;
                if (resetting || panic || stopping) {
                    epoch_.fetch_add(1U, std::memory_order_acq_rel);
                    engine.allNotesOff(0, discardAfterEpochCleanup());
                    ++trajectoryRevision;
                }
                if (resetting || panic || (!controls.playing && incoming.playing))
                    timingFault_.store(false, std::memory_order_release);
                controls = incoming;
                (void)engine.setConfig(controls.engine);
                if (resetting) (void)engine.reset(controls.initial, discardAfterEpochCleanup());
                else for (std::size_t body = 0; body < bodyCount; ++body)
                    engine.setBodyMass(body, controls.initial.bodies[body].mass);
                resetRevision = controls.resetRevision;
                panicRevision = controls.panicRevision;
            }
            const auto epoch = epoch_.load(std::memory_order_acquire);
            if (knownEpoch != epoch) {
                engine.allNotesOff(0, discardAfterEpochCleanup());
                knownEpoch = epoch;
                ++trajectoryRevision;
            }
            const auto clockNow = now();
            if (timeline.isLate(clockNow)) {
                timingFault_.store(true, std::memory_order_release);
                epoch_.fetch_add(1U, std::memory_order_acq_rel);
                overruns_.fetch_add(1U, std::memory_order_relaxed);
                engine.allNotesOff(0, discardAfterEpochCleanup());
                timeline.recoverAt(clockNow);
                ++trajectoryRevision;
            }
            if (!timeline.blockDue(clockNow)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            ProcessContext context;
            context.sampleCount = StandaloneTimeline::blockSamples;
            context.sampleRate = StandaloneTimeline::sampleRate;
            context.beatAtStart = timeline.beat();
            context.beatsPerSample = StandaloneTimeline::tempo / (60.0 * StandaloneTimeline::sampleRate);
            context.timeSigNumerator = controls.numerator;
            context.timeSigDenominator = controls.denominator;
            context.playing = controls.playing && !timingFault_.load(std::memory_order_acquire);
            context.transportStarted = context.playing && !wasPlaying;
            if (context.transportStarted) ++trajectoryRevision;
            Emission emission{this, &timeline, knownEpoch, trajectoryRevision, false};
            engine.process(context, EventSink{&emission, enqueue});
            timeline.advance();
            wasPlaying = context.playing;
            if (engine.simulation().state().respawnCount != respawns) {
                respawns = engine.simulation().state().respawnCount;
                ++trajectoryRevision;
            }
            if (++sequence % 5U == 0U) {
                RenderSnapshot snapshot;
                snapshot.bodies = engine.simulation().state().bodies;
                snapshot.escaped = engine.simulation().state().escaped;
                snapshot.sequence = sequence;
                snapshot.trajectoryRevision = trajectoryRevision;
                snapshot.interpolationAlpha = engine.simulation().interpolationAlpha();
                snapshot.timelineSeconds = timeline.seconds();
                if (!snapshots_.push(snapshot)) visualDrops_.fetch_add(1U, std::memory_order_relaxed);
            }
        }
        epoch_.fetch_add(1U, std::memory_order_acq_rel);
    }
    static void send(void* context, const MidiEvent& event) noexcept {
        auto* output = static_cast<juce::MidiOutput*>(context);
        if (output == nullptr) return;
        if (event.type == MidiEventType::NoteOn)
            output->sendMessageNow(juce::MidiMessage::noteOn(event.channel, event.data1, event.data2));
        else if (event.type == MidiEventType::NoteOff)
            output->sendMessageNow(juce::MidiMessage::noteOff(event.channel, event.data1));
        else
            output->sendMessageNow(juce::MidiMessage::controllerEvent(event.channel, event.data1, event.data2));
    }
    void deliver() {
        std::unique_ptr<juce::MidiOutput> output;
        DeliveredNoteLedger ledger;
        std::uint64_t knownEpoch{}, maximumLateness{};
        ScheduledEvent pending;
        bool hasPending{};
        while (!stopping_.load(std::memory_order_acquire)) {
            Route route;
            if (routes_.consume(route)) {
                ledger.flush({output.get(), send});
                epoch_.fetch_add(1U, std::memory_order_acq_rel);
                output.reset();
                output = route.virtualOutput ? juce::MidiOutput::createNewDevice("The Three Body Solution")
                    : juce::MidiOutput::openDevice(juce::String::fromUTF8(route.identifier.data()));
                outputReady_.store(output != nullptr, std::memory_order_relaxed);
            }
            const auto epoch = epoch_.load(std::memory_order_acquire);
            if (knownEpoch != epoch) {
                ledger.flush({output.get(), send});
                knownEpoch = epoch;
            }
            if (!hasPending) hasPending = events_.pop(pending);
            if (!hasPending) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (pending.epoch != epoch) { hasPending = false; continue; }
            const auto lateness = now() - pending.due;
            if (lateness < 0.0) {
                const auto sleep = std::min(-lateness, 0.001);
                std::this_thread::sleep_for(std::chrono::duration<double>(sleep));
                continue;
            }
            if (lateness > StandaloneTimeline::maximumLatenessSeconds) {
                timingFault_.store(true, std::memory_order_release);
                epoch_.fetch_add(1U, std::memory_order_acq_rel);
                overruns_.fetch_add(1U, std::memory_order_relaxed);
                hasPending = false;
                continue;
            }
            maximumLateness = std::max(maximumLateness, static_cast<std::uint64_t>(lateness * 1.0e6));
            maximumLatenessMicros_.store(maximumLateness, std::memory_order_relaxed);
            // Epoch is rechecked immediately before handoff; a transition racing
            // this call is followed by the destination-owned cleanup next loop.
            if (pending.epoch == epoch_.load(std::memory_order_acquire) && output != nullptr) {
                send(output.get(), pending.event);
                ledger.record(pending.event);
            }
            hasPending = false;
        }
        ledger.flush({output.get(), send});
        outputReady_.store(false, std::memory_order_relaxed);
    }

    SpscQueue<RenderSnapshot, 64>& snapshots_;
    NoteVisualizationQueue& notes_;
    LatestValueMailbox<StandaloneControls> controls_;
    LatestValueMailbox<Route> routes_;
    SpscQueue<ScheduledEvent, 65536> events_;
    std::thread generation_, delivery_;
    std::atomic<bool> stopping_{true}, outputReady_{}, timingFault_{};
    std::atomic<std::uint64_t> epoch_{1}, overruns_{}, visualDrops_{}, maximumLatenessMicros_{};
    std::uint64_t routeRevision_{}, noteSequence_{};
};

} // namespace threebs
