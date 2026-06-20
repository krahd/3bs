// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Math.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace threebs {

inline constexpr std::size_t bodyCount = 3;
inline constexpr std::uint32_t stateSchemaVersion = 2;

enum class InitialSystem : std::uint8_t {
    FigureEight,
    Hierarchical,
    Stable,
    ControlledChaos,
    Unbound,
};

enum class EscapePolicy : std::uint8_t {
    Leave,
    RespawnBody,
    Prompt,
};

enum class LoopPolicy : std::uint8_t {
    Restart,
    Continue,
};

enum class PitchMapping : std::uint8_t {
    BarycentricRadius,
    NearestBodyDistance,
    SignedPlaneDistance,
    Speed,
    OrbitalPhase,
};

enum class TriggerMapping : std::uint8_t {
    Clock,
    PlaneCrossing,
    CloseApproach,
    TurningPoint,
};

enum class ScaleId : std::uint8_t {
    Major,
    NaturalMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    MajorPentatonic,
    MinorPentatonic,
    Blues,
    Chromatic,
    HungarianMinor,
    WholeTone,
    Diminished,
    Custom,
};

struct BodyState {
    double mass{1.0};
    Vec3 position{};
    Vec3 velocity{};
};

struct SimulationConfig {
    double gravitationalConstant{0.65};
    double softening{0.04};
    double fixedStep{1.0 / 480.0};
    double speed{1.0};
    double escapeRadius{12.0};
    std::uint32_t maxStepsPerAdvance{16384};
    EscapePolicy escapePolicy{EscapePolicy::Leave};
};

struct SimulationState {
    std::array<BodyState, bodyCount> bodies{};
    std::array<bool, bodyCount> escaped{};
    std::array<std::uint32_t, bodyCount> respawnCount{};
    std::uint64_t seed{0x334253ULL};
    std::uint64_t completedSteps{};
    double elapsed{};
};

struct CcLaneConfig {
    bool enabled{};
    PitchMapping source{PitchMapping::Speed};
    std::uint8_t controller{1};
    double smoothing{0.85};
};

struct VoiceConfig {
    bool enabled{true};
    std::uint8_t channel{1};
    std::uint8_t root{0};
    std::uint8_t minimumNote{36};
    std::uint8_t maximumNote{84};
    ScaleId scale{ScaleId::MinorPentatonic};
    std::array<bool, 12> customScale{true, false, true, false, true, true,
                                      false, true, false, true, false, true};
    PitchMapping pitchMapping{PitchMapping::BarycentricRadius};
    TriggerMapping triggerMapping{TriggerMapping::Clock};
    double clockDivisionBeats{0.25};
    double probability{1.0};
    double durationBeats{0.20};
    double minimumTriggerIntervalBeats{0.0625};
    double closeApproachDistance{1.0};
    std::uint8_t minimumVelocity{44};
    std::uint8_t maximumVelocity{112};
    std::array<CcLaneConfig, 2> ccLanes{};
};

} // namespace threebs
