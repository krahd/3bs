// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// External diagnostic: no product source or published preset is changed.
#include "core/MusicEngine.h"
#include "GeneratedFixtures.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace threebs;
constexpr double sampleRate = 48000.0;
constexpr double tempo = 120.0;
constexpr double beats = 60.0;
constexpr std::uint32_t blockSize = 512;
constexpr std::uint64_t totalSamples = 1440000;

struct Trace {
    static constexpr std::size_t capacity = MusicEngine::maximumEventsPerSample * blockSize
        + MusicEngine::maximumTransitionEvents + bodyCount;
    std::array<MidiEvent, capacity> events{};
    std::array<std::array<std::uint32_t, 128>, 16> ledger{};
    std::array<std::array<std::uint64_t, 3>, bodyCount> counts{};
    std::size_t size{};
    std::uint32_t allowedSamples{};
    std::uint64_t base{}, previousSample{}, totalEvents{}, rejected{}, invalid{}, unmatchedOffs{};
    std::uint64_t active{}, peakActive{}, cleanupEvents{};
    bool hasPrevious{}, cleanup{};

    void begin(std::uint64_t absoluteSample, std::uint32_t sampleCount, bool stopping = false) {
        size = 0;
        base = absoluteSample;
        allowedSamples = sampleCount;
        cleanup = stopping;
    }
    EventSink sink() noexcept {
        return {this, [](void* context, const MidiEvent& event) noexcept {
            auto& trace = *static_cast<Trace*>(context);
            if (trace.size == trace.events.size()) { ++trace.rejected; return; }
            trace.events[trace.size++] = event;
            const auto at = trace.base + event.sampleOffset;
            const auto type = static_cast<unsigned>(event.type);
            if (event.sampleOffset >= trace.allowedSamples || event.channel < 1 || event.channel > 16
                || event.data1 > 127 || event.data2 > 127 || event.sourceBody >= bodyCount
                || type > 2 || (trace.hasPrevious && at < trace.previousSample)) {
                ++trace.invalid;
                return;
            }
            trace.previousSample = at;
            trace.hasPrevious = true;
            ++trace.totalEvents;
            ++trace.counts[event.sourceBody][type];
            if (trace.cleanup) ++trace.cleanupEvents;
            auto& held = trace.ledger[event.channel - 1U][event.data1];
            if (event.type == MidiEventType::NoteOn) { ++held; ++trace.active; }
            if (event.type == MidiEventType::NoteOff) {
                if (held == 0U) ++trace.unmatchedOffs;
                else { --held; --trace.active; }
            }
            trace.peakActive = std::max(trace.peakActive, trace.active);
        }};
    }
    void write(std::ofstream& file) const {
        for (std::size_t i = 0; i < size; ++i) {
            const auto& event = events[i];
            file << (cleanup ? "cleanup" : "performance") << ',' << base + event.sampleOffset << ','
                 << static_cast<unsigned>(event.type) << ',' << static_cast<unsigned>(event.channel) << ','
                 << static_cast<unsigned>(event.data1) << ',' << static_cast<unsigned>(event.data2) << ','
                 << static_cast<unsigned>(event.sourceBody) << '\n';
        }
    }
};

bool finite(const SimulationState& state) {
    for (const auto& body : state.bodies)
        if (!isFinite(body.position) || !isFinite(body.velocity) || !std::isfinite(body.mass)) return false;
    return std::isfinite(state.elapsed);
}
void vector(std::ostream& out, const Vec3& value) {
    out << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}
void configuration(std::ostream& out, const diagnostic::Fixture& fixture, const EngineConfig& config) {
    out << "{\"id\":\"" << fixture.id << "\",\"system\":" << static_cast<unsigned>(fixture.system)
        << ",\"seed\":" << fixture.seed << ",\"chaos\":" << fixture.chaos
        << ",\"loop_policy\":" << static_cast<unsigned>(fixture.loopPolicy)
        << ",\"voicing_mode\":" << static_cast<unsigned>(config.voicingMode)
        << ",\"simulation\":{\"gravity\":" << config.simulation.gravitationalConstant
        << ",\"softening\":" << config.simulation.softening << ",\"speed\":" << config.simulation.speed
        << ",\"fixed_step\":" << config.simulation.fixedStep
        << ",\"escape_policy\":" << static_cast<unsigned>(config.simulation.escapePolicy)
        << ",\"escape_radius\":" << config.simulation.escapeRadius
        << ",\"max_steps_per_advance\":" << config.simulation.maxStepsPerAdvance << "},\"voices\":[";
    for (std::size_t i = 0; i < bodyCount; ++i) {
        const auto& voice = config.voices[i];
        if (i) out << ',';
        out << "{\"enabled\":" << (voice.enabled ? "true" : "false")
            << ",\"channel\":" << static_cast<unsigned>(voice.channel)
            << ",\"root\":" << static_cast<unsigned>(voice.root)
            << ",\"octave\":" << static_cast<int>(voice.octave)
            << ",\"scale\":" << static_cast<unsigned>(voice.scale)
            << ",\"pitch\":" << static_cast<unsigned>(voice.pitchMapping)
            << ",\"trigger\":" << static_cast<unsigned>(voice.triggerMapping)
            << ",\"minimum\":" << static_cast<unsigned>(voice.minimumNote)
            << ",\"maximum\":" << static_cast<unsigned>(voice.maximumNote)
            << ",\"division\":" << voice.clockDivisionBeats
            << ",\"probability\":" << voice.probability
            << ",\"duration_min\":" << voice.minimumDurationBeats
            << ",\"duration_max\":" << voice.maximumDurationBeats
            << ",\"duration_map\":" << static_cast<unsigned>(voice.durationMapping)
            << ",\"duration_grid\":" << static_cast<unsigned>(voice.durationGrid)
            << ",\"minimum_trigger_interval\":" << voice.minimumTriggerIntervalBeats
            << ",\"close_approach_distance\":" << voice.closeApproachDistance
            << ",\"minimum_velocity\":" << static_cast<unsigned>(voice.minimumVelocity)
            << ",\"maximum_velocity\":" << static_cast<unsigned>(voice.maximumVelocity)
            << ",\"cc_enabled\":[" << (voice.ccLanes[0].enabled ? "true" : "false") << ','
            << (voice.ccLanes[1].enabled ? "true" : "false") << "]}";
    }
    out << "]}\n";
}

void run(const diagnostic::Fixture& fixture, unsigned refinement, const std::filesystem::path& directory,
         std::ofstream& results, std::ofstream& configurations) {
    EngineConfig config;
    config.simulation = fixture.simulation;
    config.voices = fixture.voices;
    config.simulation.fixedStep /= refinement;
    if (!isValidEngineConfig(config)) throw std::runtime_error("invalid generated engine configuration");
    const auto initial = makeInitialState(fixture.system, fixture.seed, fixture.chaos);
    if (!isValidSimulationState(initial)) throw std::runtime_error("invalid generated initial state");
    if (refinement == 1U) configuration(configurations, fixture, config);
    MusicEngine engine(initial, config);
    engine.prepare(sampleRate);
    const auto suffix = refinement == 1U ? "published" : "half-step";
    const auto filename = std::string(fixture.id) + "." + suffix + ".csv";
    std::ofstream file(directory / "traces" / filename);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << "phase,absolute_sample,type,channel,data1,data2,source_body\n";
    Trace trace;
    ProcessContext context;
    context.sampleRate = sampleRate;
    context.beatsPerSample = tempo / 60.0 / sampleRate;
    context.timeSigNumerator = 4;
    context.timeSigDenominator = 4;
    context.loopPolicy = fixture.loopPolicy;
    for (std::uint64_t base = 0; base < totalSamples;) {
        context.sampleCount = static_cast<std::uint32_t>(std::min<std::uint64_t>(blockSize, totalSamples - base));
        context.beatAtStart = static_cast<double>(base) * context.beatsPerSample;
        context.transportStarted = base == 0U;
        trace.begin(base, context.sampleCount);
        engine.process(context, trace.sink());
        trace.write(file);
        if (!finite(engine.simulation().state()) || engine.simulation().faulted())
            throw std::runtime_error("nonfinite or faulted simulation");
        base += context.sampleCount;
    }
    const auto finalState = engine.simulation().state();
    const auto activeBefore = trace.active;
    trace.begin(totalSamples, 1U, true);
    engine.allNotesOff(0, trace.sink());
    trace.write(file);
    file.close();
    const auto passed = trace.rejected == 0U && trace.invalid == 0U && trace.unmatchedOffs == 0U
        && trace.active == 0U && engine.simulation().truncatedAdvances() == 0U;
    results << "{\"preset\":\"" << fixture.id << "\",\"step\":\"" << suffix
        << "\",\"fixed_step\":" << config.simulation.fixedStep
        << ",\"beat_units\":" << beats << ",\"samples\":" << totalSamples
        << ",\"events\":" << trace.totalEvents << ",\"active_before_cleanup\":" << activeBefore
        << ",\"active_after_cleanup\":" << trace.active << ",\"cleanup_events\":" << trace.cleanupEvents
        << ",\"peak_active\":" << trace.peakActive << ",\"unmatched_offs\":" << trace.unmatchedOffs
        << ",\"rejected_events\":" << trace.rejected << ",\"invalid_events\":" << trace.invalid
        << ",\"truncated_advances\":" << engine.simulation().truncatedAdvances()
        << ",\"completed_physics_steps\":" << finalState.completedSteps
        << ",\"simulation_elapsed\":" << finalState.elapsed << ",\"voice_events\":[";
    for (std::size_t i = 0; i < bodyCount; ++i) {
        if (i) results << ',';
        results << "{\"on\":" << trace.counts[i][0] << ",\"off\":" << trace.counts[i][1]
            << ",\"cc\":" << trace.counts[i][2] << '}';
    }
    results << "],\"final_bodies\":[";
    for (std::size_t i = 0; i < bodyCount; ++i) {
        if (i) results << ',';
        results << "{\"mass\":" << finalState.bodies[i].mass << ",\"position\":";
        vector(results, finalState.bodies[i].position);
        results << ",\"velocity\":";
        vector(results, finalState.bodies[i].velocity);
        results << ",\"escaped\":" << (finalState.escaped[i] ? "true" : "false")
            << ",\"respawns\":" << finalState.respawnCount[i] << '}';
    }
    results << "],\"passed\":" << (passed ? "true" : "false") << "}\n";
    results.flush();
    std::cout << (passed ? "PASS " : "FAIL ") << fixture.id << ' ' << suffix << " events="
              << trace.totalEvents << " remaining_notes=" << trace.active << '\n' << std::flush;
    if (!passed) throw std::runtime_error("event integrity or work bound failed");
}
}

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: refinement OUTPUT_DIRECTORY\n"; return 2; }
    try {
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory / "traces");
        std::ofstream results(directory / "runs.jsonl");
        std::ofstream configurations(directory / "effective-configurations.jsonl");
        results.exceptions(std::ios::badbit | std::ios::failbit);
        configurations.exceptions(std::ios::badbit | std::ios::failbit);
        results << std::setprecision(17);
        configurations << std::setprecision(17);
        const auto fixtures = diagnostic::makeFixtures();
        if (fixtures.size() != 24U) throw std::runtime_error("expected all 24 canonical fixtures");
        for (const auto& fixture : fixtures) {
            run(fixture, 1U, directory, results, configurations);
            run(fixture, 2U, directory, results, configurations);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
