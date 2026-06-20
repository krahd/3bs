// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Random.h"
#include "core/Scale.h"
#include "core/ThreeBodySimulation.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace threebs {

enum class MidiEventType : std::uint8_t { NoteOn, NoteOff, ControlChange };

struct MidiEvent {
    std::uint32_t sampleOffset{};
    MidiEventType type{MidiEventType::NoteOn};
    std::uint8_t channel{1};
    std::uint8_t data1{};
    std::uint8_t data2{};
    std::uint8_t sourceBody{};
};

template <std::size_t Capacity>
class FixedEventBuffer {
public:
    bool push(const MidiEvent& event) noexcept {
        if (size_ >= Capacity)
            return false;
        events_[size_++] = event;
        return true;
    }
    void clear() noexcept { size_ = 0; }
    std::size_t size() const noexcept { return size_; }
    const MidiEvent* begin() const noexcept { return events_.data(); }
    const MidiEvent* end() const noexcept { return events_.data() + size_; }
    const MidiEvent& operator[](std::size_t index) const noexcept { return events_[index]; }

private:
    std::array<MidiEvent, Capacity> events_{};
    std::size_t size_{};
};

struct ProcessContext {
    std::uint32_t sampleCount{};
    double sampleRate{48000.0};
    double beatAtStart{};
    double beatsPerSample{2.0 / 48000.0};
    bool playing{true};
    bool transportStarted{};
    bool seeked{};
    bool loopWrapped{};
    LoopPolicy loopPolicy{LoopPolicy::Restart};
};

struct EngineConfig {
    SimulationConfig simulation{};
    std::array<VoiceConfig, bodyCount> voices{};
    bool inputTransposeEnabled{};
    bool inputGateEnabled{};
    std::int8_t inputTranspose{};
    bool inputGateOpen{true};
};

struct BodyMeasurements {
    double barycentricRadius{};
    double nearestBodyDistance{};
    double signedPlaneDistance{};
    double speed{};
    double orbitalPhase{};
    double radialVelocity{};
};

class MusicEngine {
public:
    static constexpr std::size_t maximumEventsPerBlock = 1024;
    using EventBuffer = FixedEventBuffer<maximumEventsPerBlock>;

    MusicEngine();
    explicit MusicEngine(SimulationState initial, EngineConfig config = {});

    void prepare(double sampleRate) noexcept;
    void reset(const SimulationState& initial, EventBuffer* noteOffs = nullptr) noexcept;
    void setConfig(const EngineConfig& config) noexcept;
    void setBodyMass(std::size_t bodyIndex, double mass) noexcept;
    void process(const ProcessContext& context, EventBuffer& output) noexcept;
    void allNotesOff(std::uint32_t sampleOffset, EventBuffer& output) noexcept;

    const ThreeBodySimulation& simulation() const noexcept { return simulation_; }
    ThreeBodySimulation& simulation() noexcept { return simulation_; }
    const EngineConfig& config() const noexcept { return config_; }
    std::array<BodyMeasurements, bodyCount> measurements() const noexcept;

private:
    struct VoiceRuntime {
        bool noteActive{};
        std::uint8_t activeNote{};
        double noteOffBeat{};
        double lastTriggerBeat{-1.0e12};
        double lastClockIndex{-1.0};
        BodyMeasurements previous{};
        std::array<double, 2> smoothedCc{};
        std::array<std::uint8_t, 2> lastCc{255, 255};
        std::array<std::uint64_t, 2> lastCcSample{};
    };

    double mappingValue(std::size_t bodyIndex, PitchMapping mapping,
                        const std::array<BodyMeasurements, bodyCount>& values) const noexcept;
    bool shouldTrigger(std::size_t bodyIndex, const VoiceConfig& voice,
                       const BodyMeasurements& current, double absoluteBeat) noexcept;
    void triggerVoice(std::size_t bodyIndex, std::uint32_t sampleOffset, double beat,
                      const BodyMeasurements& measurement, EventBuffer& output) noexcept;
    void emitContinuousControllers(std::size_t bodyIndex, std::uint32_t sampleOffset,
                                   const std::array<BodyMeasurements, bodyCount>& values,
                                   EventBuffer& output) noexcept;
    void applyTransportReset(std::uint32_t sampleOffset, EventBuffer& output) noexcept;

    SimulationState initial_{};
    ThreeBodySimulation simulation_{};
    EngineConfig config_{};
    std::array<VoiceRuntime, bodyCount> runtime_{};
    Pcg32 random_{};
    double sampleRate_{48000.0};
    bool wasPlaying_{};
    bool hasTimelineBeat_{};
    double timelineBeat_{};
    std::uint64_t processedSamples_{};
};

} // namespace threebs
