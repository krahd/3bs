// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/SnapshotQueue.h"
#include "standalone/StandaloneTimeline.h"
#include "standalone/CoreMidiOutput.h"
#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <map>
#include <string>
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
    std::uint64_t outputFailures() const noexcept { return outputFailures_.load(std::memory_order_relaxed); }
    std::uint64_t deliveredEvents() const noexcept { return deliveredEvents_.load(std::memory_order_relaxed); }
    double latenessPercentileMilliseconds(double percentile) const noexcept {
        const auto total = deliveredEvents();
        if (total == 0U) return 0.0;
        const auto target = static_cast<std::uint64_t>(std::ceil(std::clamp(percentile, 0.0, 1.0) * static_cast<double>(total)));
        std::uint64_t accumulated{};
        for (std::size_t bucket = 0; bucket < latenessHistogram_.size(); ++bucket) {
            accumulated += latenessHistogram_[bucket].load(std::memory_order_relaxed);
            if (accumulated >= target) return static_cast<double>(bucket + 1U) * 0.1;
        }
        return maximumLatenessMilliseconds();
    }
    double maximumLatenessMilliseconds() const noexcept {
        return static_cast<double>(maximumLatenessMicros_.load(std::memory_order_relaxed)) / 1000.0;
    }

private:
    friend struct StandaloneRuntimeTestPeer;
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
        std::uint64_t voiceEpoch{};
    };
    struct Emission {
        StandaloneRuntime* runtime{};
        const StandaloneTimeline* timeline{};
        const MusicEngine* engine{};
        std::array<std::uint64_t, bodyCount> voiceEpochs{};
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
        const auto body = std::min<std::size_t>(event.sourceBody, bodyCount - 1U);
        if (!self.events_.push({event, emission.timeline->deadline(sample), emission.epoch, emission.voiceEpochs[body]})) {
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
            visual.trajectoryRevision = emission.trajectoryRevision + emission.engine->trajectoryRevision();
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
        std::array<std::uint64_t, bodyCount> voiceEpochs{};
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
                const auto cancelled = cancelledStandaloneVoices(controls.engine, incoming.engine);
                for (std::size_t body = 0; body < bodyCount; ++body)
                    if (cancelled[body]) voiceEpochs_[body].fetch_add(1U, std::memory_order_acq_rel);
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
            for (std::size_t body = 0; body < bodyCount; ++body)
                voiceEpochs[body] = voiceEpochs_[body].load(std::memory_order_acquire);
            Emission emission{this, &timeline, &engine, voiceEpochs, knownEpoch, trajectoryRevision, false};
            engine.process(context, EventSink{&emission, enqueue});
            timeline.advance();
            wasPlaying = context.playing;
            if (++sequence % 5U == 0U) {
                RenderSnapshot snapshot;
                snapshot.bodies = engine.simulation().state().bodies;
                snapshot.escaped = engine.simulation().state().escaped;
                snapshot.sequence = sequence;
                snapshot.trajectoryRevision = trajectoryRevision + engine.trajectoryRevision();
                snapshot.interpolationAlpha = engine.simulation().interpolationAlpha();
                snapshot.timelineSeconds = timeline.seconds();
                if (!snapshots_.push(snapshot)) visualDrops_.fetch_add(1U, std::memory_order_relaxed);
            }
        }
        epoch_.fetch_add(1U, std::memory_order_acq_rel);
    }
    void deliver() {
        std::unique_ptr<CoreMidiOutput> output;
        std::array<DeliveredNoteLedger, bodyCount> ledgers;
        std::array<std::uint64_t, bodyCount> knownVoiceEpochs{};
        const auto flushAll = [&] {
            for (auto& ledger : ledgers)
                if (!ledger.tryFlush([&](const MidiEvent& event) { return output != nullptr && output->send(event); })) return false;
            return true;
        };
        std::uint64_t knownEpoch{}, maximumLateness{};
        Route currentRoute;
        bool hasRoute{};
        double nextDeviceCheck{};
        std::map<std::string, std::array<DeliveredNoteLedger, bodyCount>> disconnectedNotes;
        const auto devicePresent = [](const Route& route) {
            return route.virtualOutput || CoreMidiOutput::isAvailable(juce::String::fromUTF8(route.identifier.data()));
        };
        const auto routeKey = [&] { return currentRoute.virtualOutput ? std::string("virtual:3bs")
            : std::string(currentRoute.identifier.data()); };
        const auto closeOutput = [&](bool disconnected) {
            const auto outstanding = ledgers;
            const auto cleared = flushAll();
            if (hasRoute && (disconnected || !cleared))
                disconnectedNotes[routeKey()] = disconnected ? outstanding : ledgers;
            ledgers = {};
            if (!cleared) outputFailures_.fetch_add(1U, std::memory_order_relaxed);
            epoch_.fetch_add(1U, std::memory_order_acq_rel);
            output.reset();
            outputReady_.store(false, std::memory_order_relaxed);
        };
        const auto openOutput = [&] {
            output = currentRoute.virtualOutput ? CoreMidiOutput::createVirtual("The Three Body Solution")
                : CoreMidiOutput::openDevice(juce::String::fromUTF8(currentRoute.identifier.data()));
            if (output != nullptr) {
                const auto pendingCleanup = disconnectedNotes.find(routeKey());
                if (pendingCleanup != disconnectedNotes.end()) {
                    bool cleared = true;
                    for (auto& ledger : pendingCleanup->second)
                        cleared = ledger.tryFlush([&](const MidiEvent& event) { return output->send(event); }) && cleared;
                    if (cleared) disconnectedNotes.erase(pendingCleanup);
                    else {
                        outputFailures_.fetch_add(1U, std::memory_order_relaxed);
                        output.reset();
                    }
                }
            }
            epoch_.fetch_add(1U, std::memory_order_acq_rel);
            outputReady_.store(output != nullptr, std::memory_order_relaxed);
        };
        ScheduledEvent pending;
        bool hasPending{};
        while (!stopping_.load(std::memory_order_acquire)) {
            Route route;
            if (routes_.consume(route)) {
                closeOutput(output != nullptr && !devicePresent(currentRoute));
                currentRoute = route;
                hasRoute = true;
                openOutput();
                appliedRouteRevision_.store(route.revision, std::memory_order_release);
                nextDeviceCheck = now() + 1.0;
            }
            if (hasRoute && now() >= nextDeviceCheck) {
                const auto available = devicePresent(currentRoute);
                if (output != nullptr && !available) closeOutput(true);
                else if (output == nullptr && available) openOutput();
                nextDeviceCheck = now() + 1.0;
            }
            const auto epoch = epoch_.load(std::memory_order_acquire);
            if (knownEpoch != epoch) {
                if (!flushAll()) {
                    outputFailures_.fetch_add(1U, std::memory_order_relaxed);
                    closeOutput(false);
                }
                knownEpoch = epoch;
            }
            for (std::size_t body = 0; body < bodyCount; ++body) {
                const auto revision = voiceEpochs_[body].load(std::memory_order_acquire);
                if (revision != knownVoiceEpochs[body]) {
                    if (!ledgers[body].tryFlush([&](const MidiEvent& event) { return output != nullptr && output->send(event); })) {
                        outputFailures_.fetch_add(1U, std::memory_order_relaxed);
                        closeOutput(false);
                    }
                    knownVoiceEpochs[body] = revision;
                }
            }
            if (!hasPending) hasPending = events_.pop(pending);
            if (!hasPending) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            const auto body = std::min<std::size_t>(pending.event.sourceBody, bodyCount - 1U);
            if (pending.epoch != epoch || pending.voiceEpoch != knownVoiceEpochs[body]) { hasPending = false; continue; }
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
            if (pending.epoch == epoch_.load(std::memory_order_acquire)
                && pending.voiceEpoch == voiceEpochs_[body].load(std::memory_order_acquire) && output != nullptr) {
                if (pending.event.type != MidiEventType::NoteOff || ledgers[body].contains(pending.event)) {
                    if (!output->send(pending.event)) {
                        outputFailures_.fetch_add(1U, std::memory_order_relaxed);
                        closeOutput(false);
                        hasPending = false;
                        continue;
                    }
                    ledgers[body].record(pending.event);
                    const auto bucket = std::min<std::size_t>(latenessHistogram_.size() - 1U,
                        static_cast<std::size_t>(lateness * 10000.0));
                    latenessHistogram_[bucket].fetch_add(1U, std::memory_order_relaxed);
                    deliveredEvents_.fetch_add(1U, std::memory_order_relaxed);
                }
            }
            hasPending = false;
        }
        if (!flushAll()) outputFailures_.fetch_add(1U, std::memory_order_relaxed);
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
    std::array<std::atomic<std::uint64_t>, bodyCount> voiceEpochs_{};
    std::atomic<std::uint64_t> deliveredEvents_{}, outputFailures_{};
    std::atomic<std::uint64_t> appliedRouteRevision_{};
    std::array<std::atomic<std::uint64_t>, 1001> latenessHistogram_{};
    std::uint64_t routeRevision_{}, noteSequence_{};
};

} // namespace threebs
