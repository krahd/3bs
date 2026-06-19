// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Types.h"

#include <array>

namespace threebs {

SimulationState makeInitialState(InitialSystem system, std::uint64_t seed, double chaos = 0.35) noexcept;

class ThreeBodySimulation {
public:
    ThreeBodySimulation();
    explicit ThreeBodySimulation(SimulationState initial, SimulationConfig config = {});

    void reset(const SimulationState& initial) noexcept;
    void setConfig(const SimulationConfig& config) noexcept;
    void setBodyMass(std::size_t bodyIndex, double mass) noexcept;
    void advance(double deltaTime) noexcept;
    void step() noexcept;
    void respawn(std::size_t bodyIndex) noexcept;

    const SimulationState& state() const noexcept { return state_; }
    const SimulationState& initialState() const noexcept { return initial_; }
    const SimulationConfig& config() const noexcept { return config_; }
    double interpolationAlpha() const noexcept;

    Vec3 barycenter() const noexcept;
    Vec3 barycentricVelocity() const noexcept;
    double totalEnergy() const noexcept;

private:
    std::array<Vec3, bodyCount> accelerations() const noexcept;
    void detectEscapes() noexcept;
    void sanitizeConfig() noexcept;

    SimulationState initial_{};
    SimulationState state_{};
    SimulationConfig config_{};
    long double accumulator_{};
};

} // namespace threebs
