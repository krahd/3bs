// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"
#include "core/SnapshotQueue.h"
#include "render/CameraController.h"
#include "render/TrailHistory.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace {

int failures{};

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(double a, double b, double tolerance = 1.0e-10) {
    return std::abs(a - b) <= tolerance;
}

void testPcgSequence() {
    threebs::Pcg32 random(42, 54);
    check(random.nextUInt() == 2707161783U, "PCG first reference value");
    check(random.nextUInt() == 2068313097U, "PCG second reference value");
    check(random.nextUInt() == 3122475824U, "PCG third reference value");
}

void testDeterministicSimulation() {
    const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 20260619);
    threebs::ThreeBodySimulation first(initial);
    threebs::ThreeBodySimulation second(initial);
    first.advance(2.0);
    for (int i = 0; i < 200; ++i)
        second.advance(0.01);

    for (std::size_t i = 0; i < threebs::bodyCount; ++i) {
        check(near(first.state().bodies[i].position.x, second.state().bodies[i].position.x, 1.0e-9),
              "simulation chunking preserves x");
        check(near(first.state().bodies[i].velocity.y, second.state().bodies[i].velocity.y, 1.0e-9),
              "simulation chunking preserves velocity");
    }
}

void testEnergyAndSoftening() {
    auto config = threebs::SimulationConfig{};
    config.gravitationalConstant = 1.0;
    config.softening = 0.02;
    const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 7, 0.0);
    threebs::ThreeBodySimulation simulation(initial, config);
    const auto energyBefore = simulation.totalEnergy();
    simulation.advance(4.0);
    const auto drift = std::abs(simulation.totalEnergy() - energyBefore) / std::max(1.0, std::abs(energyBefore));
    check(drift < 0.002, "velocity Verlet energy drift remains bounded");
    for (const auto& body : simulation.state().bodies)
        check(threebs::isFinite(body.position) && threebs::isFinite(body.velocity),
              "softened simulation remains finite");
}

void testConstrainedSystems() {
    for (std::uint64_t seed = 1; seed < 64; ++seed) {
        const auto state = threebs::makeInitialState(threebs::InitialSystem::ControlledChaos, seed, 1.0);
        for (const auto& body : state.bodies) {
            check(body.mass >= 0.05 && body.mass < 6.0, "constrained mass is bounded");
            check(threebs::length(body.position) < 5.0, "constrained position is bounded");
            check(threebs::length(body.velocity) < 5.0, "constrained velocity is bounded");
        }
    }
}

void testInitialPlaneTilts() {
    const auto state = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 123, 0.0);
    double zExtent{};
    double totalMass{};
    threebs::Vec3 center{};
    threebs::Vec3 velocity{};
    for (const auto& body : state.bodies) {
        zExtent += std::abs(body.position.z) + std::abs(body.velocity.z);
        totalMass += body.mass;
        center += body.position * body.mass;
        velocity += body.velocity * body.mass;
    }
    check(zExtent > 0.05, "curated initial systems are not visually coplanar");
    check(threebs::length(center / totalMass) < 1.0e-12
              && threebs::length(velocity / totalMass) < 1.0e-12,
          "initial plane tilts preserve center-of-mass frame");

    const auto extraTilt = threebs::applyInitialPlaneTilts(state, {20.0, -25.0, 35.0});
    for (const auto& body : extraTilt.bodies)
        check(threebs::isFinite(body.position) && threebs::isFinite(body.velocity),
              "configurable plane tilts preserve finite vectors");
    const auto normalA = threebs::normalized(threebs::cross(extraTilt.bodies[0].position,
                                                           extraTilt.bodies[0].velocity));
    const auto normalB = threebs::normalized(threebs::cross(extraTilt.bodies[1].position,
                                                           extraTilt.bodies[1].velocity));
    check(std::abs(threebs::dot(normalA, normalB)) < 0.995,
          "configurable tilts produce distinct orbital planes");
}

void testSingleBodyRespawn() {
    auto initial = threebs::makeInitialState(threebs::InitialSystem::Stable, 99, 0.0);
    const auto unchangedOne = initial.bodies[1];
    const auto unchangedTwo = initial.bodies[2];
    initial.bodies[0].position = {30.0, 0.0, 0.0};
    initial.bodies[0].velocity = {2.0, 0.0, 0.0};
    auto config = threebs::SimulationConfig{};
    config.escapeRadius = 10.0;
    config.escapePolicy = threebs::EscapePolicy::RespawnBody;
    threebs::ThreeBodySimulation simulation(initial, config);
    simulation.step();
    check(simulation.state().respawnCount[0] == 1, "escaped body respawned once");
    check(simulation.state().respawnCount[1] == 0 && simulation.state().respawnCount[2] == 0,
          "other bodies were not respawned");
    check(threebs::length(simulation.state().bodies[0].position) < 10.0,
          "respawned body returns inside escape radius");
    check(threebs::length(simulation.state().bodies[1].position - unchangedOne.position) < 0.01,
          "first unaffected body continues normally");
    check(threebs::length(simulation.state().bodies[2].position - unchangedTwo.position) < 0.01,
          "second unaffected body continues normally");
}

void testScales() {
    threebs::VoiceConfig voice;
    voice.root = 2;
    voice.scale = threebs::ScaleId::HungarianMinor;
    voice.minimumNote = 36;
    voice.maximumNote = 84;
    for (int i = 0; i <= 1000; ++i) {
        const auto note = threebs::quantizeNormalizedPitch(static_cast<double>(i) / 1000.0, voice);
        check(note >= voice.minimumNote && note <= voice.maximumNote, "quantized note stays in range");
        check(threebs::noteIsInScale(note, voice.root, voice.scale, voice.customScale),
              "quantized note stays in scale");
    }
}

struct TimedEvent {
    std::uint64_t sample{};
    threebs::MidiEvent event{};
};

std::vector<TimedEvent> renderWithBlockSize(std::uint32_t blockSize) {
    auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 1234, 0.0);
    threebs::MusicEngine engine(initial);
    engine.prepare(48000.0);
    std::vector<TimedEvent> rendered;
    constexpr std::uint64_t totalSamples = 48000;
    for (std::uint64_t position = 0; position < totalSamples; position += blockSize) {
        const auto count = static_cast<std::uint32_t>(std::min<std::uint64_t>(blockSize, totalSamples - position));
        threebs::ProcessContext context;
        context.sampleCount = count;
        context.sampleRate = 48000.0;
        context.beatAtStart = static_cast<double>(position) * 2.0 / 48000.0;
        context.beatsPerSample = 2.0 / 48000.0;
        context.playing = true;
        context.transportStarted = position == 0;
        threebs::MusicEngine::EventBuffer buffer;
        engine.process(context, buffer);
        for (const auto& event : buffer)
            rendered.push_back({position + event.sampleOffset, event});
    }
    return rendered;
}

void testBlockSizeIndependentMidi() {
    const auto small = renderWithBlockSize(64);
    const auto large = renderWithBlockSize(511);
    check(!small.empty(), "engine generates MIDI");
    check(small.size() == large.size(), "MIDI event count is block-size independent");
    if (small.size() != large.size())
        return;
    for (std::size_t i = 0; i < small.size(); ++i) {
        check(small[i].sample == large[i].sample, "MIDI sample time is block-size independent");
        check(small[i].event.type == large[i].event.type
                  && small[i].event.channel == large[i].event.channel
                  && small[i].event.data1 == large[i].event.data1
                  && small[i].event.data2 == large[i].event.data2,
              "MIDI event data is block-size independent");
    }
}

void testNoteCleanup() {
    auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 55, 0.0);
    threebs::MusicEngine engine(initial);
    threebs::ProcessContext playing;
    playing.sampleCount = 1024;
    playing.transportStarted = true;
    playing.beatAtStart = 0.0;
    playing.beatsPerSample = 2.0 / 48000.0;
    threebs::MusicEngine::EventBuffer output;
    engine.process(playing, output);

    threebs::ProcessContext stopped = playing;
    stopped.playing = false;
    stopped.transportStarted = false;
    engine.process(stopped, output);
    bool foundNoteOff{};
    for (const auto& event : output)
        foundNoteOff = foundNoteOff || event.type == threebs::MidiEventType::NoteOff;
    check(foundNoteOff, "transport stop emits active note-offs");
}

std::array<int, threebs::bodyCount> countNoteOnsPerBody(const threebs::EngineConfig& config) {
    auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 4242, 0.0);
    threebs::MusicEngine engine(initial, config);
    engine.prepare(48000.0);
    std::array<int, threebs::bodyCount> counts{};
    threebs::ProcessContext context;
    context.sampleCount = 48000;
    context.sampleRate = 48000.0;
    context.beatsPerSample = 2.0 / 48000.0;
    context.playing = true;
    for (int block = 0; block < 4; ++block) {
        context.beatAtStart = static_cast<double>(block) * 2.0;
        context.transportStarted = block == 0;
        threebs::MusicEngine::EventBuffer buffer;
        engine.process(context, buffer);
        for (const auto& event : buffer)
            if (event.type == threebs::MidiEventType::NoteOn && event.sourceBody < threebs::bodyCount)
                ++counts[event.sourceBody];
    }
    return counts;
}

void testPerVoiceGeneration() {
    threebs::EngineConfig config;
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.enabled = true;
        voice.channel = static_cast<std::uint8_t>(body + 1);
        voice.triggerMapping = threebs::TriggerMapping::Clock;
        voice.clockDivisionBeats = 0.25;
        voice.probability = 1.0;
        voice.pitchMapping = threebs::PitchMapping::BarycentricRadius;
    }
    const auto counts = countNoteOnsPerBody(config);
    check(counts[0] > 0 && counts[1] > 0 && counts[2] > 0,
          "all three planets generate notes");

    config.voices[1].enabled = false;
    const auto disabled = countNoteOnsPerBody(config);
    check(disabled[1] == 0, "disabled planet generates no notes");
    check(disabled[0] > 0 && disabled[2] > 0,
          "other planets still generate when one is disabled");
}

void testChordAndStrumModes() {
    threebs::EngineConfig config;
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.enabled = true;
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.root = static_cast<std::uint8_t>(body * 2U);
        voice.scale = threebs::ScaleId::Major;
        voice.triggerMapping = threebs::TriggerMapping::Clock;
        voice.pitchMapping = threebs::PitchMapping::OrbitalPhase;
        voice.clockDivisionBeats = 0.25;
        voice.probability = 1.0;
    }
    // Chord and Strum modes harmonize against one global tonal frame.
    config.chordSystem.root = 2;
    config.chordSystem.scale = threebs::ScaleId::Major;
    const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 5150, 0.0);
    const auto render = [&](threebs::VoicingMode mode, double strumMilliseconds) {
        config.voicingMode = mode;
        config.chordStrumMilliseconds = strumMilliseconds;
        threebs::MusicEngine engine(initial, config);
        engine.prepare(48000.0);
        std::array<std::uint64_t, threebs::bodyCount> firstOn{
            std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max(),
            std::numeric_limits<std::uint64_t>::max()};
        for (std::uint64_t position = 0; position < 2048; position += 127) {
            threebs::ProcessContext context;
            context.sampleCount = static_cast<std::uint32_t>(std::min<std::uint64_t>(127, 2048 - position));
            context.sampleRate = 48000.0;
            context.beatAtStart = static_cast<double>(position) * 2.0 / 48000.0;
            context.beatsPerSample = 2.0 / 48000.0;
            context.transportStarted = position == 0;
            threebs::MusicEngine::EventBuffer events;
            engine.process(context, events);
            for (const auto& event : events) {
                if (event.type != threebs::MidiEventType::NoteOn || event.sourceBody >= threebs::bodyCount)
                    continue;
                firstOn[event.sourceBody] = std::min(firstOn[event.sourceBody], position + event.sampleOffset);
                check(threebs::noteIsInScale(event.data1, config.chordSystem.root,
                                             config.chordSystem.scale,
                                             config.chordSystem.customScale),
                      "chord notes respect the global chord root and scale");
            }
        }
        return firstOn;
    };
    const auto chord = render(threebs::VoicingMode::Chord, 0.0);
    check(chord[0] == chord[1] && chord[1] == chord[2],
          "chord mode starts all enabled planets together");
    const auto initialNoteCount = [&](threebs::VoicingMode mode) {
        config.voicingMode = mode;
        config.chordStrumMilliseconds = 0.0;
        threebs::MusicEngine engine(initial, config);
        threebs::ProcessContext context;
        context.sampleCount = 1;
        context.sampleRate = 48000.0;
        context.beatsPerSample = 2.0 / 48000.0;
        context.transportStarted = true;
        threebs::MusicEngine::EventBuffer events;
        engine.process(context, events);
        return static_cast<std::size_t>(std::count_if(
            events.begin(), events.end(), [](const auto& event) {
                return event.type == threebs::MidiEventType::NoteOn;
            }));
    };
    check(initialNoteCount(threebs::VoicingMode::Independent) == threebs::bodyCount
              && initialNoteCount(threebs::VoicingMode::Chord) == threebs::bodyCount,
          "chord mode replaces rather than layers over independent note generation");
    const auto strum = render(threebs::VoicingMode::Strum, 10.0);
    check(strum[0] == 0 && strum[1] == 480 && strum[2] == 960,
          "strum mode schedules deterministic notes across processing blocks");
    for (auto& voice : config.voices)
        voice.triggerMapping = threebs::TriggerMapping::PhaseStep;
    const auto physicalChord = render(threebs::VoicingMode::Chord, 0.0);
    check(physicalChord[0] != std::numeric_limits<std::uint64_t>::max()
              && physicalChord[0] == physicalChord[1] && physicalChord[1] == physicalChord[2],
          "planet phase criteria can trigger a complete chord");
}

void testSnapshotQueue() {
    threebs::SpscQueue<threebs::RenderSnapshot, 4> queue;
    threebs::RenderSnapshot input;
    input.sequence = 42;
    check(queue.push(input), "snapshot queue accepts value");
    threebs::RenderSnapshot output;
    check(queue.pop(output) && output.sequence == 42, "snapshot queue preserves value");
}

void testNoteVisualizationQueue() {
    threebs::NoteVisualizationQueue queue;
    threebs::NoteVisualizationEvent input;
    for (std::size_t index = 0; index < 2047; ++index) {
        input.sequence = index;
        check(queue.push(input), "note visualization queue accepts fixed capacity");
    }
    check(!queue.push(input), "note visualization queue drops on overflow");
    threebs::NoteVisualizationEvent output;
    check(queue.pop(output) && output.sequence == 0,
          "note visualization queue preserves event order");
}

void testCameraTargetAndHitTesting() {
    std::array<threebs::BodyState, threebs::bodyCount> bodies{{
        {2.0, {-1.0, 0.0, 0.0}, {}},
        {1.0, {2.0, 0.0, 0.0}, {}},
        {1.0, {0.0, 2.0, 0.0}, {}},
    }};
    const auto barycenter = threebs::CameraController::barycenter(bodies);
    check(near(barycenter.x, 0.0) && near(barycenter.y, 0.5), "camera uses mass-weighted barycenter");

    threebs::CameraController camera;
    threebs::CameraState state;
    state.yaw = 0.0F;
    state.pitch = 0.0F;
    state.distance = 7.0F;
    state.autoOrbit = 0.0F;
    camera.setState(state, bodies, 0.0);

    std::array<threebs::BodyState, threebs::bodyCount> overlapping{{
        {1.0, {0.0, 0.0, 0.0}, {}},
        {1.0, {0.0, 0.0, -2.0}, {}},
        {1.0, {20.0, 20.0, 20.0}, {}},
    }};
    state.focusBody = 0;
    camera.setState(state, overlapping, 0.0);
    check(camera.hitTest(600.0, 380.0, 1200.0, 760.0, overlapping) == 0,
          "camera selects nearest overlapping planet");
    check(camera.hitTest(10.0, 10.0, 1200.0, 760.0, overlapping) == -1,
          "camera background hit returns barycenter target");
    check(threebs::CameraController::isClick(2.0, 2.0)
              && !threebs::CameraController::isClick(4.0, 0.0),
          "camera click threshold is four pixels");
}

void testCameraMotion() {
    std::array<threebs::BodyState, threebs::bodyCount> bodies{};
    bodies[0].mass = bodies[1].mass = bodies[2].mass = 1.0;
    bodies[0].position = {1.0, 0.0, 0.0};
    threebs::CameraController camera;
    threebs::CameraState state;
    state.autoOrbit = 1.0F;
    camera.setState(state, bodies, 0.0);
    camera.beginInteraction(0.0);
    camera.update(2.9, bodies);
    check(near(camera.state().yaw, 0.0, 1.0e-6), "auto orbit pauses after interaction");
    camera.update(3.5, bodies);
    check(camera.state().yaw > 0.0F, "auto orbit resumes after idle delay");

    camera.orbit(0.0, 100000.0, 1000.0, 4.0);
    check(camera.state().pitch < 1.49F, "camera pitch is clamped");
    camera.zoom(1000.0, bodies, 4.0);
    check(near(camera.state().distance, 2.5, 1.0e-6), "camera zoom has a near bound");
    camera.zoom(-1000.0, bodies, 4.0);
    check(near(camera.state().distance, 40.0, 1.0e-6), "camera zoom has a configurable far bound");
    check(!camera.state().autoFrame, "manual zoom disables automatic framing");

    camera.selectFocus(0, bodies, 5.0);
    camera.update(5.35, bodies);
    const auto halfway = camera.basis().target.x;
    check(halfway > 0.0 && halfway < 1.0, "camera focus transition eases between targets");
    camera.update(5.8, bodies);
    check(near(camera.basis().target.x, 1.0, 1.0e-6), "camera focus transition completes");
    camera.zoom(1.0, bodies, 6.0);
    camera.update(6.8, bodies);
    check(camera.state().focusBody == -1 && near(camera.basis().target.x, 1.0 / 3.0, 1.0e-6),
          "manual zoom returns selected focus to the barycenter");

    const auto yawBeforeReset = camera.state().yaw;
    const auto pitchBeforeReset = camera.state().pitch;
    camera.resetView(bodies, 16.0 / 9.0, 7.0);
    check(camera.state().autoFrame && camera.state().focusBody == -1
              && near(camera.basis().target.x, 1.0 / 3.0, 1.0e-6),
          "camera view reset immediately restores fitted barycenter framing");
    check(near(camera.state().yaw, yawBeforeReset, 1.0e-6)
              && near(camera.state().pitch, pitchBeforeReset, 1.0e-6),
          "camera view reset preserves the viewing plane");
}

void testCameraAutoFraming() {
    std::array<threebs::BodyState, threebs::bodyCount> bodies{{
        {1.0, {-5.0, 0.0, 0.0}, {}},
        {1.0, {5.0, 0.0, 0.0}, {}},
        {1.0, {0.0, 1.0, 0.0}, {}},
    }};
    const auto wide = threebs::CameraController::framingDistance(bodies, 16.0 / 9.0);
    const auto narrow = threebs::CameraController::framingDistance(bodies, 0.5);
    check(narrow > wide && wide > 5.0, "camera framing accounts for viewport aspect ratio");

    threebs::CameraState state;
    state.distance = 2.5F;
    state.maximumDistance = 40.0F;
    state.autoOrbit = 0.0F;
    threebs::CameraController camera;
    camera.setState(state, bodies, 0.0);
    for (int step = 1; step <= 100; ++step)
        camera.update(static_cast<double>(step) * 0.1, bodies, 16.0 / 9.0);
    check(std::abs(camera.state().distance - wide) < 0.01,
          "barycenter camera smoothly reaches the fitted distance");

    state.distance = 7.0F;
    state.focusBody = 0;
    camera.setState(state, bodies, 0.0);
    camera.update(1.0, bodies, 16.0 / 9.0);
    check(near(camera.state().distance, 7.0, 1.0e-6),
          "selected-body focus bypasses automatic framing");
}

void testCameraSanitization() {
    threebs::CameraState state;
    state.yaw = std::numeric_limits<float>::infinity();
    state.pitch = std::numeric_limits<float>::quiet_NaN();
    state.distance = -200.0F;
    state.minimumDistance = 30.0F;
    state.maximumDistance = 2.0F;
    state.autoOrbit = std::numeric_limits<float>::quiet_NaN();
    state.focusBody = 99;
    const auto sanitized = threebs::sanitizedCameraState(state);
    check(std::isfinite(sanitized.yaw) && std::isfinite(sanitized.pitch)
              && std::isfinite(sanitized.autoOrbit),
          "camera state replaces non-finite values");
    check(sanitized.minimumDistance >= 1.0F && sanitized.maximumDistance <= 80.0F
              && sanitized.maximumDistance >= sanitized.minimumDistance + 0.5F,
          "camera state repairs crossed zoom limits");
    check(sanitized.distance >= sanitized.minimumDistance
              && sanitized.distance <= sanitized.maximumDistance
              && sanitized.focusBody == 2,
          "camera state clamps distance and focus");
}

void testTrailHistory() {
    threebs::TrailHistory<4> trail;
    for (int i = 0; i < 6; ++i)
        trail.append({static_cast<double>(i) * 0.01, 0.0, 0.0}, static_cast<double>(i), 1);
    check(trail.size() == 4 && near(trail[0].position.x, 0.02), "trail ring preserves newest samples");
    trail.prune(6.0, 2.5);
    check(trail.size() == 2 && near(trail[0].position.x, 0.04), "trail prunes samples by age");
    threebs::TrailHistory<64> smoothed;
    smoothed.append({0.0, 0.0, 0.0}, 0.0, 1);
    smoothed.append({1.0, 0.0, 0.0}, 1.0, 1);
    check(smoothed.size() > 2U, "fast trail motion is subdivided into smooth ribbon samples");
    trail.append({10.0, 0.0, 0.0}, 7.0, 2);
    check(trail.size() == 1 && near(trail[0].position.x, 10.0), "trail revision clears old trajectory");
}

threebs::EngineConfig singleClockVoiceConfig() {
    threebs::EngineConfig config;
    for (auto& voice : config.voices)
        voice.enabled = false;
    auto& voice = config.voices[0];
    voice.enabled = true;
    voice.channel = 1;
    voice.triggerMapping = threebs::TriggerMapping::Clock;
    voice.clockDivisionBeats = 0.5;
    voice.probability = 1.0;
    voice.pitchMapping = threebs::PitchMapping::BarycentricRadius;
    voice.minimumNote = 36;
    voice.maximumNote = 60;
    return config;
}

void testOctaveTranspose() {
    const auto firstNote = [](int octave) {
        auto config = singleClockVoiceConfig();
        config.voices[0].octave = static_cast<std::int8_t>(octave);
        const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 4242, 0.0);
        threebs::MusicEngine engine(initial, config);
        engine.prepare(48000.0);
        threebs::ProcessContext context;
        context.sampleCount = 48000;
        context.sampleRate = 48000.0;
        context.beatsPerSample = 2.0 / 48000.0;
        context.playing = true;
        context.transportStarted = true;
        threebs::MusicEngine::EventBuffer buffer;
        engine.process(context, buffer);
        for (const auto& event : buffer)
            if (event.type == threebs::MidiEventType::NoteOn)
                return static_cast<int>(event.data1);
        return -1;
    };
    const auto base = firstNote(0);
    check(base >= 0 && firstNote(1) == base + 12,
          "per-voice octave shifts notes up by twelve semitones");
    check(firstNote(-1) == base - 12,
          "per-voice octave shifts notes down by twelve semitones");
}

std::vector<int> noteDurationsInSamples(double minDuration, double maxDuration,
                                        threebs::DurationGrid grid) {
    auto config = singleClockVoiceConfig();
    config.voices[0].minimumDurationBeats = minDuration;
    config.voices[0].maximumDurationBeats = maxDuration;
    config.voices[0].durationMapping = threebs::PitchMapping::Speed;
    config.voices[0].durationGrid = grid;
    const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 4242, 0.0);
    threebs::MusicEngine engine(initial, config);
    engine.prepare(48000.0);
    std::vector<int> durations;
    std::uint64_t globalSample{};
    bool active{};
    std::uint64_t onSample{};
    for (int block = 0; block < 6; ++block) {
        threebs::ProcessContext context;
        context.sampleCount = 24000;
        context.sampleRate = 48000.0;
        context.beatsPerSample = 2.0 / 48000.0;
        context.playing = true;
        context.transportStarted = block == 0;
        threebs::MusicEngine::EventBuffer buffer;
        engine.process(context, buffer);
        for (const auto& event : buffer) {
            if (event.sourceBody != 0)
                continue;
            const auto at = globalSample + event.sampleOffset;
            if (event.type == threebs::MidiEventType::NoteOn) {
                active = true;
                onSample = at;
            } else if (event.type == threebs::MidiEventType::NoteOff && active) {
                durations.push_back(static_cast<int>(at - onSample));
                active = false;
            }
        }
        globalSample += context.sampleCount;
    }
    return durations;
}

void testDurationMapping() {
    const auto fixed = noteDurationsInSamples(0.5, 0.5, threebs::DurationGrid::Straight);
    check(fixed.size() > 3, "fixed-length voice produces multiple notes");
    bool allEqual = true;
    for (const auto duration : fixed)
        allEqual = allEqual && std::abs(duration - fixed.front()) <= 1;
    check(allEqual, "equal min/max duration yields constant note lengths");

    const auto varying = noteDurationsInSamples(0.1, 0.4, threebs::DurationGrid::ContinuousLegacy);
    int minimum = std::numeric_limits<int>::max();
    int maximum = 0;
    for (const auto duration : varying) {
        minimum = std::min(minimum, duration);
        maximum = std::max(maximum, duration);
    }
    check(varying.size() > 3 && maximum - minimum > 1000,
          "legacy continuous duration mapping preserves varied note lengths");

    for (const auto value : {0.0, 0.1, 0.3, 0.5, 0.7, 0.9, 1.0}) {
        const auto duration = threebs::mappedDurationBeats(
            value, 0.125, 8.0, threebs::DurationGrid::Straight);
        check(std::find(threebs::straightDurationBeats.begin(),
                        threebs::straightDurationBeats.end(), duration)
                  != threebs::straightDurationBeats.end(),
              "straight duration mapping emits only rhythmic grid values");
    }
    check(threebs::mappedDurationBeats(0.0, 0.5, 2.0, threebs::DurationGrid::Straight) == 0.5
              && threebs::mappedDurationBeats(1.0, 0.5, 2.0,
                                              threebs::DurationGrid::Straight) == 2.0,
          "straight duration mapping respects its minimum and maximum");
    check(threebs::mappedDurationBeats(0.73, 1.0, 1.0,
                                      threebs::DurationGrid::Straight) == 1.0,
          "equal rhythmic duration bounds remain fixed");
}

void testGlobalChordFrame() {
    threebs::EngineConfig config;
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.enabled = true;
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.root = 1;  // deliberately not in the global frame
        voice.scale = threebs::ScaleId::Chromatic;
        voice.triggerMapping = threebs::TriggerMapping::Clock;
        voice.pitchMapping = threebs::PitchMapping::OrbitalPhase;
        voice.clockDivisionBeats = 0.25;
        voice.probability = 1.0;
    }
    config.voicingMode = threebs::VoicingMode::Chord;
    config.chordSystem.root = 7;
    config.chordSystem.scale = threebs::ScaleId::MajorPentatonic;
    const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 5150, 0.0);
    threebs::MusicEngine engine(initial, config);
    engine.prepare(48000.0);
    int notes{};
    for (int block = 0; block < 8; ++block) {
        threebs::ProcessContext context;
        context.sampleCount = 12000;
        context.sampleRate = 48000.0;
        context.beatsPerSample = 2.0 / 48000.0;
        context.playing = true;
        context.transportStarted = block == 0;
        threebs::MusicEngine::EventBuffer buffer;
        engine.process(context, buffer);
        for (const auto& event : buffer) {
            if (event.type != threebs::MidiEventType::NoteOn)
                continue;
            ++notes;
            check(threebs::noteIsInScale(event.data1, config.chordSystem.root,
                                         config.chordSystem.scale, config.chordSystem.customScale),
                  "chord mode quantizes to the global frame, ignoring per-planet root/scale");
        }
    }
    check(notes > 0, "chord mode still generates notes under the global frame");
}

std::array<std::uint64_t, threebs::bodyCount> strumOnsets(threebs::StrumUnit unit, double value) {
    threebs::EngineConfig config;
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.enabled = true;
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.triggerMapping = threebs::TriggerMapping::Clock;
        voice.pitchMapping = threebs::PitchMapping::OrbitalPhase;
        voice.clockDivisionBeats = 0.25;
        voice.probability = 1.0;
    }
    config.voicingMode = threebs::VoicingMode::Strum;
    config.chordStrumUnit = unit;
    config.chordStrumValue = value;
    config.minimumChordIntervalBeats = 1.0;
    const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 5150, 0.0);
    threebs::MusicEngine engine(initial, config);
    engine.prepare(48000.0);
    std::array<std::uint64_t, threebs::bodyCount> firstOn{
        std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max(),
        std::numeric_limits<std::uint64_t>::max()};
    for (std::uint64_t position = 0; position < 24000; position += 127) {
        threebs::ProcessContext context;
        context.sampleCount = static_cast<std::uint32_t>(std::min<std::uint64_t>(127, 24000 - position));
        context.sampleRate = 48000.0;
        context.beatsPerSample = 2.0 / 48000.0;
        context.timeSigNumerator = 4;
        context.timeSigDenominator = 4;
        context.transportStarted = position == 0;
        threebs::MusicEngine::EventBuffer events;
        engine.process(context, events);
        for (const auto& event : events)
            if (event.type == threebs::MidiEventType::NoteOn && event.sourceBody < threebs::bodyCount)
                firstOn[event.sourceBody] = std::min(firstOn[event.sourceBody], position + event.sampleOffset);
    }
    return firstOn;
}

void testStrumBeatUnit() {
    // 0.25 beat at 2/48000 beats per sample is 6000 samples between voices.
    const auto beats = strumOnsets(threebs::StrumUnit::Beats, 0.25);
    check(beats[0] == 0 && beats[1] == 6000 && beats[2] == 12000,
          "beat-relative strum spacing matches the expected sample delay");
    // 1/8 bar of 4/4 is 0.5 beats, i.e. 12000 samples between voices.
    const auto bars = strumOnsets(threebs::StrumUnit::BarFraction, 0.125);
    check(bars[0] == 0 && bars[1] == 12000 && bars[2] == 24000,
          "bar-fraction strum spacing scales by the time signature");
}

std::vector<std::pair<int, int>> collectOnsets(bool autoReset, std::uint64_t totalSamples) {
    auto config = singleClockVoiceConfig();
    config.voices[0].clockDivisionBeats = 0.25;
    config.autoResetEnabled = autoReset;
    config.autoResetBars = 1.0;
    const auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 4242, 0.0);
    threebs::MusicEngine engine(initial, config);
    engine.prepare(48000.0);
    std::vector<std::pair<int, int>> stream;
    std::uint64_t globalSample{};
    const std::uint32_t blockSize = 4800;
    while (globalSample < totalSamples) {
        threebs::ProcessContext context;
        context.sampleCount = blockSize;
        context.sampleRate = 48000.0;
        context.beatsPerSample = 2.0 / 48000.0;
        context.timeSigNumerator = 4;
        context.timeSigDenominator = 4;
        context.playing = true;
        context.transportStarted = globalSample == 0;
        threebs::MusicEngine::EventBuffer buffer;
        engine.process(context, buffer);
        for (const auto& event : buffer) {
            if (event.type != threebs::MidiEventType::NoteOn)
                continue;
            if (globalSample + event.sampleOffset < totalSamples)
                stream.emplace_back(static_cast<int>(event.sourceBody), static_cast<int>(event.data1));
        }
        globalSample += blockSize;
    }
    return stream;
}

void testBarAutoReset() {
    // One bar of 4/4 at 2/48000 beats per sample spans 96000 samples.
    const auto opening = collectOnsets(true, 96000);
    const auto twoBars = collectOnsets(true, 192000);
    auto expected = opening;
    expected.insert(expected.end(), opening.begin(), opening.end());
    check(!opening.empty() && twoBars == expected,
          "auto-reset every bar replays the deterministic opening note stream");
    const auto freeTwoBars = collectOnsets(false, 192000);
    check(freeTwoBars != expected,
          "without auto-reset the timeline evolves past the opening");
}

void testPresentationMigration() {
    check(near(threebs::migrateV1TrailLength(0.82F), 29.6, 1.0e-5),
          "schema-v1 trail length migrates to seconds");
    check(near(threebs::migrateV1TrailLength(-1.0F), 5.0)
              && near(threebs::migrateV1TrailLength(2.0F), 35.0),
          "schema-v1 trail migration clamps malformed values");
}

} // namespace

int main() {
    testPcgSequence();
    testDeterministicSimulation();
    testEnergyAndSoftening();
    testConstrainedSystems();
    testInitialPlaneTilts();
    testSingleBodyRespawn();
    testScales();
    testBlockSizeIndependentMidi();
    testNoteCleanup();
    testPerVoiceGeneration();
    testChordAndStrumModes();
    testSnapshotQueue();
    testNoteVisualizationQueue();
    testCameraTargetAndHitTesting();
    testCameraMotion();
    testCameraAutoFraming();
    testCameraSanitization();
    testTrailHistory();
    testPresentationMigration();
    if (failures == 0)
        std::cout << "All core tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
