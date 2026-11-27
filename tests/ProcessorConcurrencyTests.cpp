// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

namespace {
using namespace threebs;

UserConfiguration fixture(ThreeBSProcessor& processor, int index) {
    auto value = processor.currentUserConfiguration();
    const std::array<std::array<double, bodyCount>, 2> masses{{{50.0, 500.0, 750.0}, {100.0, 600.0, 900.0}}};
    for (std::size_t body = 0; body < bodyCount; ++body) {
        value.initial.bodies[body].mass = masses[static_cast<std::size_t>(index)][body];
        value.baseInitial.bodies[body].mass = masses[static_cast<std::size_t>(index)][body];
        auto& voice = value.engine.voices[body];
        voice.enabled = true;
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.triggerMapping = TriggerMapping::Clock;
        voice.clockDivisionBeats = 0.25;
        voice.minimumTriggerIntervalBeats = 0.0;
        voice.probability = 1.0;
        voice.durationGrid = DurationGrid::ContinuousLegacy;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 4.0;
        for (auto& lane : voice.ccLanes) lane.enabled = false;
    }
    value.initial.seed = value.baseInitial.seed = index == 0 ? 0xABC123ULL : 0xDEF456ULL;
    value.presentation.visualSeed = value.initial.seed + 3U;
    value.engine.voices[0].channel = index == 0 ? 1 : 9;
    value.engine.simulation.speed = 0.0;
    value.engine.voicingMode = VoicingMode::Independent;
    value.engine.inputGateEnabled = value.engine.inputTransposeEnabled = false;
    value.engine.autoResetEnabled = false;
    value.densityPercent = 100.0;
    value.run = true;
    value.sync = false;
    value.timeSignatureSource = TimeSignatureSource::Manual;
    value.timeSignatureNumerator = index == 0 ? 7 : 5;
    value.timeSignatureDenominator = index == 0 ? 8 : 4;
    return value;
}

bool completeFixture(const UserConfiguration& value, const std::array<UserConfiguration, 2>& fixtures) {
    for (const auto& expected : fixtures) {
        bool matches = value.initial.seed == expected.initial.seed
            && value.baseInitial.seed == expected.baseInitial.seed
            && value.presentation.visualSeed == expected.presentation.visualSeed
            && value.engine.voices[0].channel == expected.engine.voices[0].channel
            && value.timeSignatureSource == expected.timeSignatureSource
            && value.timeSignatureNumerator == expected.timeSignatureNumerator
            && value.timeSignatureDenominator == expected.timeSignatureDenominator
            && value.run && !value.sync && value.engine.simulation.speed == 0.0;
        for (std::size_t body = 0; body < bodyCount; ++body)
            matches = matches
                && value.initial.bodies[body].mass == expected.initial.bodies[body].mass
                && value.baseInitial.bodies[body].mass == expected.baseInitial.bodies[body].mass;
        if (matches) return true;
    }
    return false;
}

bool completeSnapshot(const RenderSnapshot& snapshot, const std::array<UserConfiguration, 2>& fixtures) {
    for (const auto& expected : fixtures) {
        bool matches = true;
        for (std::size_t body = 0; body < bodyCount; ++body)
            matches = matches && snapshot.bodies[body].mass == expected.initial.bodies[body].mass;
        if (matches) return true;
    }
    return false;
}

int exerciseProcessor() {
    // No editor, window, renderer, or device is instantiated. The event manager
    // only supplies the JUCE parameter/timer infrastructure used by a processor.
    auto processor = std::make_unique<ThreeBSProcessor>();
    auto savedStateVerifier = std::make_unique<ThreeBSProcessor>();
    auto fixtureWriter = std::make_unique<ThreeBSProcessor>();
    const std::array<UserConfiguration, 2> fixtures{fixture(*fixtureWriter, 0), fixture(*fixtureWriter, 1)};
    std::array<juce::MemoryBlock, 2> hostStates;
    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        if (!fixtureWriter->applyUserConfiguration(fixtures[index])) return 1;
        fixtureWriter->getStateInformation(hostStates[index]);
    }
    fixtureWriter.reset();
    if (!processor->applyUserConfiguration(fixtures[0])) return 1;
    processor->setPlayConfigDetails(0, 2, 48000.0, 128);
    processor->prepareToPlay(48000.0, 128);
    std::atomic<bool> start{}, aborted{}, audioAcknowledged{};
    std::atomic<std::uint64_t> finalRevision{};
    std::atomic<unsigned> failures{}, publications{}, saves{}, callbackCount{}, noteOns{}, snapshots{};
    constexpr unsigned publicationCount = 384, saveCount = 768;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    const auto timedOut = [&] {
        if (std::chrono::steady_clock::now() < deadline) return false;
        aborted.store(true, std::memory_order_release);
        return true;
    };
    const auto waitForStart = [&] {
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
    };
    std::thread audio([&] {
        juce::AudioBuffer<float> audioBuffer(2, 128);
        juce::MidiBuffer midi;
        midi.ensureSize(2 * 1024 * 1024);
        std::array<std::array<int, 128>, 16> sounding{};
        waitForStart();
        while (!aborted.load(std::memory_order_acquire)) {
            midi.clear();
            processor->processBlock(audioBuffer, midi);
            callbackCount.fetch_add(1, std::memory_order_relaxed);
            for (const auto metadata : midi) {
                if (metadata.numBytes != 3 || metadata.samplePosition < 0 || metadata.samplePosition >= 128
                    || metadata.data[1] > 127U || metadata.data[2] > 127U) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                auto& active = sounding[metadata.data[0] & 15U][metadata.data[1]];
                const auto kind = metadata.data[0] & 0xF0U;
                if (kind == 0x90U && metadata.data[2] != 0) {
                    if (active != 0) failures.fetch_add(1, std::memory_order_relaxed);
                    ++active;
                    noteOns.fetch_add(1, std::memory_order_relaxed);
                } else if (kind == 0x80U || (kind == 0x90U && metadata.data[2] == 0)) {
                    if (active == 0) failures.fetch_add(1, std::memory_order_relaxed);
                    else --active;
                }
            }
            const auto target = finalRevision.load(std::memory_order_acquire);
            if (target != 0 && processor->appliedConfigurationRevision() == target) {
                for (const auto& channel : sounding)
                    for (const auto active : channel)
                        if (active != 0) failures.fetch_add(1, std::memory_order_relaxed);
                audioAcknowledged.store(true, std::memory_order_release);
                return;
            }
            if (timedOut()) return;
            std::this_thread::yield();
        }
    });
    std::thread writer([&] {
        waitForStart();
        for (unsigned iteration = 0; iteration < publicationCount && !aborted.load(std::memory_order_acquire); ++iteration) {
            const auto index = static_cast<std::size_t>(iteration & 1U);
            if ((iteration & 2U) == 0) {
                if (!processor->applyUserConfiguration(fixtures[index])) failures.fetch_add(1, std::memory_order_relaxed);
            } else {
                processor->setStateInformation(hostStates[index].getData(), static_cast<int>(hostStates[index].getSize()));
                if (processor->lastStateError().isNotEmpty()) failures.fetch_add(1, std::memory_order_relaxed);
            }
            publications.fetch_add(1, std::memory_order_relaxed);
            if (timedOut()) return;
            std::this_thread::yield();
        }
    });
    std::thread reader([&] {
        waitForStart();
        for (unsigned iteration = 0; iteration < saveCount && !aborted.load(std::memory_order_acquire); ++iteration) {
            if (!completeFixture(processor->currentUserConfiguration(), fixtures))
                failures.fetch_add(1, std::memory_order_relaxed);
            juce::MemoryBlock saved;
            processor->getStateInformation(saved);
            savedStateVerifier->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
            if (savedStateVerifier->lastStateError().isNotEmpty()
                || !completeFixture(savedStateVerifier->currentUserConfiguration(), fixtures))
                failures.fetch_add(1, std::memory_order_relaxed);
            RenderSnapshot snapshot;
            for (int count = 0; count < 64 && processor->snapshots().pop(snapshot); ++count) {
                if (!completeSnapshot(snapshot, fixtures)) failures.fetch_add(1, std::memory_order_relaxed);
                snapshots.fetch_add(1, std::memory_order_relaxed);
            }
            saves.fetch_add(1, std::memory_order_relaxed);
            if (timedOut()) return;
            std::this_thread::yield();
        }
    });
    start.store(true, std::memory_order_release);
    writer.join();
    reader.join();
    if (!aborted.load(std::memory_order_acquire)) {
        auto stopped = fixtures[1];
        stopped.run = false;
        if (!processor->applyUserConfiguration(stopped)) failures.fetch_add(1, std::memory_order_relaxed);
        finalRevision.store(processor->configurationRevision(), std::memory_order_release);
    }
    audio.join();
    processor->releaseResources();
    const bool completed = !aborted.load() && audioAcknowledged.load()
        && publications.load() == publicationCount && saves.load() == saveCount
        && callbackCount.load() > 0 && noteOns.load() > 0 && snapshots.load() > 0
        && processor->realtimeFaults() == 0
        && processor->appliedConfigurationRevision() == processor->configurationRevision();
    std::cout << "processor concurrency publications=" << publications.load()
              << " save_restore_pairs=" << saves.load() << " callbacks=" << callbackCount.load()
              << " notes=" << noteOns.load() << " snapshots=" << snapshots.load()
              << " errors=" << failures.load() << " final_ack=" << audioAcknowledged.load()
              << " timed_out=" << aborted.load() << '\n';
    return completed && failures.load() == 0 ? 0 : 1;
}
}

int main() {
    juce::MessageManager::getInstance();
    const auto result = exerciseProcessor();
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    return result;
}
