// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using namespace threebs;
int failures{};
void check(bool condition, const char* description) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}

EngineConfig voices(bool all = false) {
    EngineConfig config;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.channel = static_cast<std::uint8_t>(body + 1);
        voice.enabled = all || body == 0;
        voice.clockDivisionBeats = 16.0;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 8.0;
    }
    return config;
}

ProcessContext context(std::uint32_t count, bool start = false) {
    ProcessContext value;
    value.sampleCount = count;
    value.transportStarted = start;
    return value;
}

struct Recorded {
    std::uint64_t sample;
    MidiEvent event;
};

struct Stream {
    std::vector<Recorded> events;
    std::uint64_t blockStart{};
    bool overflow{};
    Stream() { events.reserve(50000); }
    EventSink sink() noexcept {
        return {this, [](void* opaque, const MidiEvent& event) noexcept {
            auto& stream = *static_cast<Stream*>(opaque);
            if (stream.events.size() == stream.events.capacity()) { stream.overflow = true; return; }
            stream.events.push_back({stream.blockStart + event.sampleOffset, event});
        }};
    }
};

std::array<std::array<int, 128>, 16> noteLedger(const Stream& stream) {
    std::array<std::array<int, 128>, 16> ledger{};
    for (const auto& record : stream.events) {
        const auto& event = record.event;
        check(event.channel >= 1 && event.channel <= 16 && event.data1 <= 127,
              "emitted MIDI fields remain in range");
        if (event.channel < 1 || event.channel > 16 || event.data1 > 127) continue;
        auto& count = ledger[event.channel - 1][event.data1];
        if (event.type == MidiEventType::NoteOn) ++count;
        if (event.type == MidiEventType::NoteOff) {
            check(count > 0, "NoteOff matches a delivered sounding identity");
            --count;
        }
    }
    return ledger;
}

bool emptyLedger(const Stream& stream) {
    for (const auto& channel : noteLedger(stream))
        for (const auto count : channel)
            if (count != 0) return false;
    return true;
}

void configurationAndCleanup() {
    const auto initial = makeInitialState(InitialSystem::FigureEight, 4242, 0.0);
    auto config = voices();
    for (auto& voice : config.voices) voice.channel = 1;
    config.voices[0].octave = 2;
    MusicEngine explicitConfig(initial, config);
    check(explicitConfig.config().voices == config.voices, "explicit C/channel-1 configuration is preserved");
    for (auto& voice : config.voices) voice.enabled = false;
    MusicEngine silent(initial, config);
    Stream silence;
    silent.process(context(512, true), silence.sink());
    check(silence.events.empty(), "an explicitly silent configuration stays silent");

    config = voices();
    MusicEngine engine(initial, config);
    Stream stream;
    engine.process(context(1, true), stream.sink());
    config.voices[0].channel = 7;
    check(engine.setConfig(config), "channel change accepted");
    stream.blockStart = 1;
    engine.process(context(1), stream.sink());
    check(stream.events.size() == 2 && stream.events[0].event.channel == 1
              && stream.events[1].event.channel == 1, "channel change cleans the old channel first");
    check(emptyLedger(stream), "channel replacement leaves no unmatched notes");

    MusicEngine resetEngine(initial, voices());
    Stream resetStream;
    resetEngine.process(context(1, true), resetStream.sink());
    check(resetEngine.reset(initial) && resetEngine.reset(initial), "repeated deferred resets accepted");
    auto stopped = context(1); stopped.playing = false;
    resetStream.blockStart = 1;
    resetEngine.process(stopped, resetStream.sink());
    check(resetStream.events.size() == 2 && emptyLedger(resetStream), "reset without sink retains exactly one cleanup");

    MusicEngine immediate(initial, voices());
    Stream immediateStream;
    immediate.process(context(1, true), immediateStream.sink());
    check(immediate.reset(initial, immediateStream.sink()), "streaming reset accepted");
    check(emptyLedger(immediateStream), "streaming reset cleans notes immediately");
}

void pendingTransitions() {
    const auto initial = makeInitialState(InitialSystem::FigureEight, 42, 0.0);
    for (int transition = 0; transition < 4; ++transition) {
        auto config = voices(true);
        config.voicingMode = VoicingMode::Strum;
        config.chordStrumMilliseconds = 100.0;
        MusicEngine engine(initial, config);
        Stream stream;
        engine.process(context(1, true), stream.sink());
        if (transition == 0) for (auto& voice : config.voices) voice.enabled = false;
        if (transition == 1) { config.inputGateEnabled = true; config.inputGateOpen = false; }
        if (transition == 2) config.voicingMode = VoicingMode::Independent;
        if (transition == 3) check(engine.reset(initial), "pending reset accepted");
        if (transition != 3) check(engine.setConfig(config), "pending transition accepted");
        stream.blockStart = 1;
        auto next = context(12000);
        if (transition == 3) next.playing = false;
        engine.process(next, stream.sink());
        engine.allNotesOff(12000, stream.sink());
        std::size_t ons{};
        for (const auto& event : stream.events) ons += event.event.type == MidiEventType::NoteOn;
        check(ons == 1, "pending strum cannot sound after disable, gate close, mode change or reset");
        check(emptyLedger(stream), "pending transition cleans original note");
    }
    auto config = voices(true);
    MusicEngine engine(initial, config);
    Stream stream;
    engine.process(context(1, true), stream.sink());
    config.voices[1].enabled = false;
    check(engine.setConfig(config), "single-voice disable accepted");
    stream.blockStart = 1;
    engine.process(context(1), stream.sink());
    check(stream.events.size() == 4 && stream.events.back().event.channel == 2,
          "disabling one voice leaves the other sounding identities alone");
    engine.allNotesOff(1, stream.sink());
    check(emptyLedger(stream), "final cleanup closes unaffected voices");
}

EngineConfig denseConfig(VoicingMode mode = VoicingMode::Independent) {
    auto config = voices(true);
    config.voicingMode = mode;
    config.minimumChordIntervalBeats = 0.0;
    config.chordStrumMilliseconds = 2.0;
    for (auto& voice : config.voices) {
        voice.clockDivisionBeats = 1.0 / 128.0;
        voice.minimumTriggerIntervalBeats = 0;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 1.0 / 1024.0;
        voice.durationGrid = DurationGrid::ContinuousLegacy;
    }
    return config;
}

Stream render(const EngineConfig& config, std::uint32_t block, double sampleRate,
              bool variable = false) {
    const auto initial = makeInitialState(InitialSystem::FigureEight, 1);
    MusicEngine engine(initial, config);
    Stream stream;
    std::uint32_t position{};
    while (position < 16384) {
        const auto requested = variable ? 1U + (position * 17U + 31U) % 2048U : block;
        auto process = context(std::min(requested, 16384U - position), position == 0);
        process.sampleRate = sampleRate;
        process.beatsPerSample = 4.0 / sampleRate;
        process.beatAtStart = position * process.beatsPerSample;
        stream.blockStart = position;
        engine.process(process, stream.sink());
        position += process.sampleCount;
    }
    stream.blockStart = position;
    engine.allNotesOff(0, stream.sink());
    check(!stream.overflow && emptyLedger(stream), "streaming output delivered all notes and their cleanup");
    return stream;
}

bool sameStream(const Stream& a, const Stream& b) {
    if (a.events.size() != b.events.size()) return false;
    for (std::size_t i = 0; i < a.events.size(); ++i) {
        const auto& x = a.events[i]; const auto& y = b.events[i];
        if (x.sample != y.sample || x.event.type != y.event.type || x.event.channel != y.event.channel
            || x.event.data1 != y.event.data1 || x.event.data2 != y.event.data2
            || x.event.sourceBody != y.event.sourceBody) return false;
    }
    return true;
}

void boundedStreaming() {
    for (const auto rate : {44100.0, 48000.0, 96000.0}) {
        for (const auto mode : {VoicingMode::Independent, VoicingMode::Chord, VoicingMode::Strum}) {
            auto config = denseConfig(mode);
            const auto reference = render(config, 512, rate);
            for (const auto size : {1U, 32U, 64U, 127U, 128U, 1024U, 2048U, 16384U})
                check(sameStream(reference, render(config, size, rate)),
                      "dense MIDI is exactly invariant to block partition");
            check(sameStream(reference, render(config, 512, rate, true)),
                  "dense MIDI is exactly invariant to varying block sizes");
        }
    }
    const auto reference = render(denseConfig(), 16384, 48000);
    check(reference.events.size() == 1050, "original 1050-event reproduction is delivered without truncation");

    MusicEngine bounded(makeInitialState(InitialSystem::FigureEight, 1), denseConfig());
    MusicEngine::EventBuffer output;
    auto process = context(16384, true); process.beatsPerSample = 4.0 / 48000.0;
    const auto result = bounded.process(process, output);
    check(!result.complete() && result.delivered == 1024 && result.rejected == 26
              && output.overflowed(), "bounded diagnostic collection reports exact truncation explicitly");

    auto stress = denseConfig(VoicingMode::Strum);
    stress.autoResetEnabled = true;
    stress.autoResetBars = 1.0 / 16.0;
    for (auto& voice : stress.voices) for (auto& lane : voice.ccLanes) lane.enabled = true;
    const auto events = render(stress, 16384, 48000);
    std::size_t atSample{}; std::uint64_t sample = std::numeric_limits<std::uint64_t>::max();
    for (const auto& record : events.events) {
        if (record.sample != sample) { sample = record.sample; atSample = 0; }
        ++atSample;
        check(atSample <= MusicEngine::maximumEventsPerSample + MusicEngine::maximumTransitionEvents,
              "per-sample storage bound covers reset, pending, retrigger and controller traffic");
    }
}

void speedExtrema() {
    for (const bool receding : {false, true}) {
        SimulationState initial;
        initial.bodies[0].position = {-1, 0, 0};
        initial.bodies[1].position = {1, 0, 0};
        initial.bodies[2].position = {0, 2, 0};
        if (receding) {
            initial.bodies[0].velocity = {-0.2, 0, 0};
            initial.bodies[1].velocity = {0.2, 0, 0};
        }
        auto config = voices();
        config.voices[0].triggerMapping = TriggerMapping::SpeedPeak;
        MusicEngine engine(initial, config);
        Stream stream;
        engine.process(context(4800, true), stream.sink());
        check(stream.events.empty(), "monotonic acceleration/deceleration contains no artificial speed peak");
    }
    auto config = voices();
    config.voices[0].triggerMapping = TriggerMapping::SpeedPeak;
    config.voices[0].minimumTriggerIntervalBeats = 0;
    MusicEngine engine(makeInitialState(InitialSystem::FigureEight, 4242, 0.0), config);
    Stream stream;
    auto previous = engine.measurements()[0].speed;
    double slope{};
    int extrema{};
    for (std::uint32_t sample = 0; sample < 96000; ++sample) {
        stream.blockStart = sample;
        engine.process(context(1, sample == 0), stream.sink());
        const auto speed = engine.measurements()[0].speed;
        const auto delta = speed - previous;
        if (delta < 0 && slope > 0) ++extrema;
        if (delta != 0) slope = delta;
        previous = speed;
    }
    int ons{};
    for (const auto& record : stream.events) ons += record.event.type == MidiEventType::NoteOn;
    check(extrema > 0 && ons == extrema, "each measured positive-to-negative speed slope emits one peak");
    engine.allNotesOff(1, stream.sink());
    check(emptyLedger(stream), "physical speed-peak notes clean up");
}

void strumTimingChanges() {
    auto config = voices(true);
    config.voicingMode = VoicingMode::Strum;
    config.chordStrumUnit = StrumUnit::Beats;
    config.chordStrumValue = 0.25;
    const auto initial = makeInitialState(InitialSystem::FigureEight, 42);
    MusicEngine engine(initial, config);
    Stream stream;
    engine.process(context(3000, true), stream.sink()); // 0.125 beat at 120 BPM.
    stream.blockStart = 3000;
    auto faster = context(6000); faster.beatsPerSample = 4.0 / 48000.0;
    engine.process(faster, stream.sink());
    std::array<std::uint64_t, 3> onsets{};
    for (const auto& record : stream.events)
        if (record.event.type == MidiEventType::NoteOn) onsets[record.event.sourceBody] = record.sample;
    check(onsets == std::array<std::uint64_t, 3>{0, 4500, 7500},
          "pending beat strums follow a later tempo change");
    engine.allNotesOff(6000, stream.sink());
    check(emptyLedger(stream), "tempo-changing strum cleans all voices");

    config.chordStrumUnit = StrumUnit::Milliseconds;
    config.chordStrumMilliseconds = 100;
    MusicEngine changingRate(initial, config);
    Stream timed;
    changingRate.process(context(2400, true), timed.sink()); // First 50 ms.
    timed.blockStart = 2400;
    auto highRate = context(16000); highRate.sampleRate = 96000; highRate.beatsPerSample = 2.0 / 96000.0;
    changingRate.process(highRate, timed.sink());
    onsets.fill(0);
    for (const auto& record : timed.events)
        if (record.event.type == MidiEventType::NoteOn) onsets[record.event.sourceBody] = record.sample;
    check(onsets == std::array<std::uint64_t, 3>{0, 7200, 16800},
          "pending millisecond strums retain elapsed time across sample-rate changes");
    changingRate.allNotesOff(16000, timed.sink());
    check(emptyLedger(timed), "sample-rate-changing strum cleans all voices");
}

void invalidCandidates() {
    const auto initial = makeInitialState(InitialSystem::FigureEight, 1);
    const auto config = voices();
    MusicEngine engine(initial, config);
    Stream stream;
    engine.process(context(1, true), stream.sink());
    for (const auto value : {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        auto state = initial; state.bodies[0].position.x = value;
        check(!isValidSimulationState(state) && !engine.reset(state, stream.sink()),
              "non-finite initial state is rejected without touching sounding state");
        auto invalid = config; invalid.simulation.fixedStep = value;
        check(!isValidEngineConfig(invalid) && !engine.setConfig(invalid), "non-finite step is rejected");
        invalid = config; invalid.voices[0].probability = value;
        check(!engine.setConfig(invalid), "non-finite voice mapping is rejected");
        check(quantizeNormalizedPitch(value, config.voices[0]) <= 127,
              "quantizer's public boundary cannot convert a non-finite index");
        check(std::isfinite(mappedDurationBeats(value, value, value, DurationGrid::Straight)),
              "duration helper remains finite for malformed direct calls");
    }
    check(stream.events.size() == 1, "rejected state did not flush a still-valid sounding note");
    auto inverted = config; inverted.voices[0].minimumNote = 100; inverted.voices[0].maximumNote = 10;
    check(!engine.setConfig(inverted), "inverted note range rejected");
    auto custom = config; custom.voices[0].scale = ScaleId::Custom; custom.voices[0].customScale.fill(false);
    check(!engine.setConfig(custom), "empty selected custom scale rejected");
    auto invalidEnum = config; invalidEnum.voicingMode = static_cast<VoicingMode>(255);
    check(!engine.setConfig(invalidEnum), "invalid enum rejected");
    auto malformed = initial; malformed.bodies[0].position.x = std::numeric_limits<double>::quiet_NaN();
    MusicEngine constructed(malformed, invalidEnum);
    check(isValidSimulationState(constructed.simulation().state()) && isValidEngineConfig(constructed.config()),
          "constructor fallback is a complete valid configuration");
    auto invalidTime = context(128); invalidTime.beatsPerSample = std::numeric_limits<double>::quiet_NaN();
    const auto before = engine.simulation().state().completedSteps;
    engine.process(invalidTime, stream.sink());
    check(emptyLedger(stream) && engine.simulation().state().completedSteps == before,
          "invalid host timing cleans notes without advancing invalid physics");
    auto boundedConfig = config.simulation; boundedConfig.maxStepsPerAdvance = 8;
    ThreeBodySimulation simulation(initial, boundedConfig);
    simulation.advance(1.0e100);
    check(simulation.state().completedSteps == 8 && simulation.truncatedAdvances() == 1
              && !simulation.faulted(), "oversized advance is bounded before integer conversion and reported");
}
}

int main() {
    const auto run = [](const char* name, void (*test)()) {
        const auto before = failures; test();
        std::cout << (before == failures ? "PASS " : "FAIL ") << name << '\n';
    };
    run("configuration and sounding identity", configurationAndCleanup);
    run("pending transition cancellation", pendingTransitions);
    run("streaming and partition invariance", boundedStreaming);
    run("physical speed extrema", speedExtrema);
    run("strum tempo and sample-rate changes", strumTimingChanges);
    run("invalid candidate rejection", invalidCandidates);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
