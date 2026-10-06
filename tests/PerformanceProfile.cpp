// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <string_view>

namespace {

std::atomic<std::size_t> trackedAllocations{};
thread_local bool trackAllocations{};

void countAllocation() noexcept {
    if (trackAllocations)
        trackedAllocations.fetch_add(1, std::memory_order_relaxed);
}

constexpr std::size_t measuredBlocks = 20000;
constexpr std::size_t warmupBlocks = 512;
constexpr std::uint32_t samplesPerBlock = 256;
constexpr double sampleRate = 48000.0;
constexpr double beatsPerSample = 2.0 / sampleRate;

struct ProfileResult {
    std::string_view name;
    double meanMicroseconds{};
    double p95Microseconds{};
    double maximumMicroseconds{};
    std::size_t allocations{};
    std::size_t midiEvents{};
};

threebs::EngineConfig makeConfig(threebs::VoicingMode mode) {
    threebs::EngineConfig config;
    config.voicingMode = mode;
    config.chordStrumMilliseconds = 24.0;
    config.chordStrumUnit = threebs::StrumUnit::Beats;
    config.chordStrumValue = 1.0 / 32.0;
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.enabled = true;
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.triggerMapping = threebs::TriggerMapping::Clock;
        voice.clockDivisionBeats = 0.125;
        voice.probability = 1.0;
        voice.pitchMapping = threebs::PitchMapping::Speed;
        voice.durationMapping = threebs::PitchMapping::Speed;
        voice.durationGrid = threebs::DurationGrid::Straight;
        voice.minimumDurationBeats = 0.125;
        voice.maximumDurationBeats = 1.0;
    }
    return config;
}

ProfileResult runProfile(std::string_view name, threebs::VoicingMode mode) {
    auto initial = threebs::makeInitialState(threebs::InitialSystem::FigureEight, 0x3b5ULL, 0.0);
    threebs::MusicEngine engine(initial, makeConfig(mode));
    engine.prepare(sampleRate);

    threebs::ProcessContext context;
    context.sampleCount = samplesPerBlock;
    context.sampleRate = sampleRate;
    context.beatsPerSample = beatsPerSample;
    context.playing = true;

    double beat{};
    for (std::size_t block = 0; block < warmupBlocks; ++block) {
        context.beatAtStart = beat;
        context.transportStarted = block == 0;
        threebs::MusicEngine::EventBuffer events;
        engine.process(context, events);
        beat += static_cast<double>(samplesPerBlock) * beatsPerSample;
    }

    std::array<double, measuredBlocks> timings{};
    trackedAllocations.store(0, std::memory_order_relaxed);
    std::size_t midiEvents{};
    for (std::size_t block = 0; block < measuredBlocks; ++block) {
        context.beatAtStart = beat;
        context.transportStarted = false;
        threebs::MusicEngine::EventBuffer events;
        trackAllocations = true;
        const auto start = std::chrono::steady_clock::now();
        engine.process(context, events);
        const auto stop = std::chrono::steady_clock::now();
        trackAllocations = false;
        timings[block] = std::chrono::duration<double, std::micro>(stop - start).count();
        midiEvents += events.size();
        beat += static_cast<double>(samplesPerBlock) * beatsPerSample;
    }

    std::sort(timings.begin(), timings.end());
    double total{};
    for (const auto value : timings)
        total += value;
    const auto p95Index = static_cast<std::size_t>(
        std::floor(0.95 * static_cast<double>(measuredBlocks - 1U)));
    return {name, total / static_cast<double>(measuredBlocks), timings[p95Index], timings.back(),
            trackedAllocations.load(std::memory_order_relaxed), midiEvents};
}

} // namespace

void* operator new(std::size_t size) {
    countAllocation();
    if (auto* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) {
    countAllocation();
    if (auto* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    const std::array results{
        runProfile("independent", threebs::VoicingMode::Independent),
        runProfile("chord", threebs::VoicingMode::Chord),
        runProfile("strum", threebs::VoicingMode::Strum)};

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "scenario,mean_us,p95_us,max_us,allocations,midi_events\n";
    std::size_t allocations{};
    for (const auto& result : results) {
        std::cout << result.name << ',' << result.meanMicroseconds << ',' << result.p95Microseconds
                  << ',' << result.maximumMicroseconds << ',' << result.allocations << ','
                  << result.midiEvents << '\n';
        allocations += result.allocations;
    }
    if (allocations != 0U) {
        std::cerr << "FAIL: MusicEngine::process allocated " << allocations
                  << " times in measured blocks\n";
        return 1;
    }
    return 0;
}
