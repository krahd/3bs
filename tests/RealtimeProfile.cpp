// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"
#include "AllocationProbe.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
void noteCppAllocation() noexcept { threebs::allocationProbe::noteCppAllocation(); }
void noteFree(void* pointer) noexcept { threebs::allocationProbe::noteCppFree(pointer); }
}

void* operator new(std::size_t size) {
    noteCppAllocation();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { noteFree(pointer); std::free(pointer); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    noteCppAllocation();
    void* memory{};
    if (posix_memalign(&memory, static_cast<std::size_t>(alignment), size == 0 ? 1 : size) == 0) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
void operator delete(void* pointer, std::align_val_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { ::operator delete(pointer); }

int main() {
    juce::ScopedJuceInitialiser_GUI initialiser;
#if !defined(__APPLE__)
    std::cerr << "Full C allocator instrumentation requires the supported macOS runtime.\n";
    return 77;
#else
    // Prove the instrumentation is active; an absent interposition must fail,
    // never certify zero allocations. Use a volatile call to avoid elision.
    void* (*volatile allocate)(std::size_t) = &std::malloc;
    threebs::allocationProbe::begin();
    auto* calibration = allocate(17);
    const auto calibrationCounts = threebs::allocationProbe::end();
    std::free(calibration);
    if (calibrationCounts.cAllocations == 0) {
        std::cerr << "FAIL: C allocator instrumentation is inactive\n";
        return 1;
    }
    threebs::ThreeBSProcessor processor;
    constexpr double sampleRate = 48000.0;
    processor.setPlayConfigDetails(0, 2, sampleRate, 16384);
    processor.prepareToPlay(sampleRate, 16384);
    juce::AudioBuffer<float> audio(2, 16384);
    juce::MidiBuffer midi;
    // Host-owned capacity is an explicit condition of this test. It is not an
    // assertion that every DAW provides this amount or never reallocates it.
    midi.ensureSize(4 * 1024 * 1024);
    const std::array<juce::uint8, 1024> sysexPayload{};
    const auto sysex = juce::MidiMessage::createSysExMessage(sysexPayload.data(), static_cast<int>(sysexPayload.size()));
    juce::MidiBuffer denseInput;
    denseInput.ensureSize(512 * 1024);
    for (int event = 0; event < 32768; ++event) {
        const std::array<juce::uint8, 3> release{0x80, static_cast<juce::uint8>(event % 128), 0};
        denseInput.addEvent(release.data(), 3, 0);
    }
    std::chrono::nanoseconds total{}, maximum{};
    std::size_t callbacks{}, failures{};
    const auto measure = [&](const char* scenario, int frames, int count, int input, bool bypass) {
        audio.setSize(2, frames, false, false, true);
        threebs::allocationProbe::Counts counts;
        std::chrono::nanoseconds scenarioTotal{}, scenarioMaximum{};
        for (int block = 0; block < count; ++block) {
            midi.clear();
            if (input == 2) { midi = denseInput; midi.ensureSize(4 * 1024 * 1024); }
            if (input == 1) {
                midi.addEvent(sysex, 0);
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), frames / 4);
                midi.addEvent(juce::MidiMessage::noteOff(1, 60), frames * 3 / 4);
            }
            const auto start = std::chrono::steady_clock::now();
            threebs::allocationProbe::begin();
            if (bypass) processor.processBlockBypassed(audio, midi);
            else processor.processBlock(audio, midi);
            const auto observed = threebs::allocationProbe::end();
            counts.cppAllocations += observed.cppAllocations;
            counts.cAllocations += observed.cAllocations;
            counts.frees += observed.frees;
            const auto elapsed = std::chrono::steady_clock::now() - start;
            total += elapsed; maximum = std::max(maximum, elapsed); ++callbacks;
            scenarioTotal += elapsed; scenarioMaximum = std::max(scenarioMaximum, elapsed);
        }
        std::cout << scenario << " blocks=" << count << " frames=" << frames
                  << " cpp_allocations=" << counts.cppAllocations << " c_allocations=" << counts.cAllocations
                  << " frees=" << counts.frees
                  << " average_us=" << std::chrono::duration<double, std::micro>(scenarioTotal).count() / count
                  << " maximum_us=" << std::chrono::duration<double, std::micro>(scenarioMaximum).count() << '\n';
        failures += counts.cppAllocations != 0 || counts.cAllocations != 0 || counts.frees != 0;
    };
    measure("cold-command-consumption", 512, 1, false, false);
    measure("short-input-and-long-sysex", 128, 128, true, false);
    measure("dense-input-releases", 64, 4, 2, false);
    measure("steady-state", 512, 2048, false, false);
    processor.requestReset();
    measure("reset-with-input", 64, 1, true, false);
    auto configuration = processor.currentUserConfiguration();
    configuration.engine.voicingMode = threebs::VoicingMode::Strum;
    processor.applyUserConfiguration(configuration);
    measure("state-replacement-large-block", 16384, 4, true, false);
    measure("bypass-cleanup", 512, 1, false, true);
    processor.requestPanic();
    measure("panic-cleanup", 512, 1, false, false);
    std::cout << "callbacks=" << callbacks
              << " average_us=" << std::chrono::duration<double, std::micro>(total).count() / static_cast<double>(callbacks)
              << " maximum_us=" << std::chrono::duration<double, std::micro>(maximum).count()
              << " (mixed sizes; timing diagnostic only)\n";
    return failures == 0 ? 0 : 1;
#endif
}
