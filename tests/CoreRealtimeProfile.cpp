// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>

namespace {
std::atomic<bool> measuring{false};
std::atomic<std::size_t> allocations{}, frees{};
void allocated() noexcept { if (measuring.load(std::memory_order_relaxed)) ++allocations; }
void freed(void* value) noexcept { if (value && measuring.load(std::memory_order_relaxed)) ++frees; }
}
void* operator new(std::size_t size) {
    allocated(); if (auto* value = std::malloc(std::max<std::size_t>(size, 1))) return value;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { freed(value); std::free(value); }
void operator delete[](void* value) noexcept { ::operator delete(value); }
void operator delete(void* value, std::size_t) noexcept { ::operator delete(value); }
void operator delete[](void* value, std::size_t) noexcept { ::operator delete(value); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    allocated(); void* value{};
    if (posix_memalign(&value, static_cast<std::size_t>(alignment), std::max<std::size_t>(size, 1)) == 0) return value;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
void operator delete(void* value, std::align_val_t) noexcept { ::operator delete(value); }
void operator delete[](void* value, std::align_val_t) noexcept { ::operator delete(value); }
void operator delete(void* value, std::size_t, std::align_val_t) noexcept { ::operator delete(value); }
void operator delete[](void* value, std::size_t, std::align_val_t) noexcept { ::operator delete(value); }

namespace {
using namespace threebs;
struct Count {
    std::uint64_t events{};
    bool valid{true};
    EventSink sink() noexcept {
        return {this, [](void* opaque, const MidiEvent& event) noexcept {
            auto& count = *static_cast<Count*>(opaque);
            ++count.events;
            count.valid = count.valid && event.channel >= 1 && event.channel <= 16 && event.data1 <= 127;
        }};
    }
};

bool profile(VoicingMode mode, double sampleRate, std::uint32_t blockSize, bool transitions = false) {
    EngineConfig config;
    config.voicingMode = mode;
    config.minimumChordIntervalBeats = 0;
    config.chordStrumMilliseconds = 1;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.channel = static_cast<std::uint8_t>(body + 1);
        voice.clockDivisionBeats = 1.0 / 128.0;
        voice.minimumTriggerIntervalBeats = 0;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 1.0 / 1024.0;
        voice.durationGrid = DurationGrid::ContinuousLegacy;
        for (auto& lane : voice.ccLanes) lane.enabled = true;
    }
    if (transitions) { config.simulation.speed = 8; config.simulation.gravitationalConstant = 3; }
    const auto initial = makeInitialState(InitialSystem::ControlledChaos, 20261127, 0.6);
    MusicEngine engine(initial, config);
    Count count;
    ProcessContext context;
    context.sampleRate = sampleRate;
    context.sampleCount = blockSize;
    context.beatsPerSample = 4.0 / sampleRate;
    context.transportStarted = true;
    allocations.store(0); frees.store(0);
    measuring.store(true, std::memory_order_relaxed);
    engine.process(context, count.sink());
    measuring.store(false, std::memory_order_relaxed);
    const auto coldAllocations = allocations.load();
    const auto coldFrees = frees.load();
    context.transportStarted = false;
    for (unsigned i = 0; i < 32; ++i) engine.process(context, count.sink());
    const std::size_t blocks = blockSize > 2048 ? 32U : 128U;
    std::array<double, 128> microseconds{};
    std::size_t overruns{};
    const auto budget = 1.0e6 * static_cast<double>(blockSize) / sampleRate;
    allocations.store(0); frees.store(0);
    for (std::size_t block = 0; block < blocks; ++block) {
        auto candidate = config;
        if (transitions) candidate.voices[1].enabled = (block & 1U) == 0;
        const auto start = std::chrono::steady_clock::now();
        measuring.store(true, std::memory_order_relaxed);
        engine.setConfig(candidate);
        if (transitions && block % 31U == 0) engine.reset(initial, count.sink());
        engine.process(context, count.sink());
        measuring.store(false, std::memory_order_relaxed);
        microseconds[block] = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        overruns += microseconds[block] > budget;
    }
    const auto warmAllocations = allocations.load();
    const auto warmFrees = frees.load();
    std::sort(microseconds.begin(), microseconds.begin() + static_cast<std::ptrdiff_t>(blocks));
    const bool passed = count.valid && !engine.simulation().faulted()
        && coldAllocations == 0 && coldFrees == 0 && warmAllocations == 0 && warmFrees == 0;
    std::cout << "{\"mode\":" << static_cast<int>(mode) << ",\"sample_rate\":" << sampleRate
              << ",\"block_size\":" << blockSize << ",\"transitions\":" << (transitions ? "true" : "false")
              << ",\"measured_blocks\":" << blocks << ",\"cold_cpp_allocations\":" << coldAllocations
              << ",\"cold_cpp_frees\":" << coldFrees << ",\"warm_cpp_allocations\":" << warmAllocations
              << ",\"warm_cpp_frees\":" << warmFrees
              << ",\"p50_us\":" << microseconds[(blocks - 1) * 50 / 100]
              << ",\"p95_us\":" << microseconds[(blocks - 1) * 95 / 100]
              << ",\"p99_us\":" << microseconds[(blocks - 1) * 99 / 100]
              << ",\"max_us\":" << microseconds[blocks - 1] << ",\"budget_us\":" << budget
              << ",\"overruns\":" << overruns << ",\"passed\":" << (passed ? "true" : "false") << "}\n";
    return passed;
}
}

int main() {
    std::cout << std::setprecision(8);
    bool ok = true;
    for (const auto mode : {VoicingMode::Independent, VoicingMode::Chord, VoicingMode::Strum}) {
        for (const auto rate : {44100.0, 48000.0, 96000.0})
            for (const auto block : {64U, 512U}) ok = profile(mode, rate, block) && ok;
        ok = profile(mode, 48000, 16384) && ok;
        ok = profile(mode, 48000, 512, true) && ok;
    }
    // Timing remains a machine/build-specific report. Allocation and numerical
    // failure are test failures; native host/GPU qualification is separate.
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
