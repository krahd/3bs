// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"
#include "core/SnapshotQueue.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
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

void testSnapshotQueue() {
    threebs::SpscQueue<threebs::RenderSnapshot, 4> queue;
    threebs::RenderSnapshot input;
    input.sequence = 42;
    check(queue.push(input), "snapshot queue accepts value");
    threebs::RenderSnapshot output;
    check(queue.pop(output) && output.sequence == 42, "snapshot queue preserves value");
}

} // namespace

int main() {
    testPcgSequence();
    testDeterministicSimulation();
    testEnergyAndSoftening();
    testConstrainedSystems();
    testSingleBodyRespawn();
    testScales();
    testBlockSizeIndependentMidi();
    testNoteCleanup();
    testSnapshotQueue();
    if (failures == 0)
        std::cout << "All core tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
