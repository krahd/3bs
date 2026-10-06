// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
std::atomic<bool> trackAllocations{false};
std::atomic<std::size_t> allocationCount{0};

void noteAllocation() noexcept {
    if (trackAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
}
}

void* operator new(std::size_t size) {
    noteAllocation();
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    noteAllocation();
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

void* operator new(std::size_t size, std::align_val_t alignment) {
    noteAllocation();
    void* memory{};
    if (posix_memalign(&memory, static_cast<std::size_t>(alignment), size) == 0)
        return memory;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void operator delete(void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }

int main() {
    juce::ScopedJuceInitialiser_GUI juce;
    threebs::ThreeBSProcessor processor;
    constexpr int blockSize = 512;
    constexpr double sampleRate = 48000.0;
    processor.setPlayConfigDetails(0, 2, sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    midi.ensureSize(64 * 1024);

    for (int block = 0; block < 256; ++block) {
        midi.clear();
        processor.processBlock(audio, midi);
    }

    constexpr int measuredBlocks = 2048;
    std::chrono::nanoseconds total{};
    std::chrono::nanoseconds maximum{};
    allocationCount.store(0, std::memory_order_relaxed);

    for (int block = 0; block < measuredBlocks; ++block) {
        midi.clear();
        const auto start = std::chrono::steady_clock::now();
        trackAllocations.store(true, std::memory_order_relaxed);
        processor.processBlock(audio, midi);
        trackAllocations.store(false, std::memory_order_relaxed);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        total += elapsed;
        if (elapsed > maximum)
            maximum = elapsed;
    }

    const auto allocations = allocationCount.load(std::memory_order_relaxed);
    const auto averageUs =
        std::chrono::duration<double, std::micro>(total).count() / measuredBlocks;
    const auto maximumUs = std::chrono::duration<double, std::micro>(maximum).count();
    const auto budgetUs = 1.0e6 * blockSize / sampleRate;

    std::cout << "steady-state processBlock profile: blocks=" << measuredBlocks
              << " allocations=" << allocations
              << " average_us=" << averageUs
              << " max_us=" << maximumUs
              << " block_budget_us=" << budgetUs << '\\n';

    if (allocations != 0) {
        std::cerr << "FAIL: steady-state processBlock allocated memory\\n";
        return 1;
    }
    return 0;
}
