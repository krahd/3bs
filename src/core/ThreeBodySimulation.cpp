// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/ThreeBodySimulation.h"

#include "core/Random.h"

#include <algorithm>
#include <cmath>

namespace threebs {
namespace {

constexpr double pi = 3.14159265358979323846;

Vec3 randomDirection(Pcg32& random) noexcept {
    const auto z = random.symmetric();
    const auto azimuth = random.nextUnit() * 2.0 * pi;
    const auto radial = std::sqrt(std::max(0.0, 1.0 - z * z));
    return {radial * std::cos(azimuth), radial * std::sin(azimuth), z};
}

Vec3 rotateAroundAxis(Vec3 value, Vec3 axis, double angle) noexcept {
    const auto axisLength = length(axis);
    if (axisLength <= 1.0e-12 || std::abs(angle) <= 1.0e-12)
        return value;
    axis = axis / axisLength;
    const auto sine = std::sin(angle);
    const auto cosine = std::cos(angle);
    return value * cosine + cross(axis, value) * sine + axis * (dot(axis, value) * (1.0 - cosine));
}

void removeCenterOfMassMotion(SimulationState& state) noexcept {
    double massSum{};
    Vec3 weightedPosition{};
    Vec3 weightedVelocity{};
    for (const auto& body : state.bodies) {
        massSum += body.mass;
        weightedPosition += body.position * body.mass;
        weightedVelocity += body.velocity * body.mass;
    }
    if (massSum <= 0.0)
        return;
    const auto center = weightedPosition / massSum;
    const auto velocity = weightedVelocity / massSum;
    for (auto& body : state.bodies) {
        body.position -= center;
        body.velocity -= velocity;
    }
}

void addPerturbation(SimulationState& state, Pcg32& random, double amount) noexcept {
    for (auto& body : state.bodies) {
        body.position += randomDirection(random) * amount;
        body.velocity += randomDirection(random) * (amount * 0.35);
        body.mass *= 1.0 + random.symmetric() * amount * 0.15;
        body.mass = std::max(0.05, body.mass);
    }
    removeCenterOfMassMotion(state);
}

std::array<double, bodyCount> defaultPlaneTilts(InitialSystem system) noexcept {
    switch (system) {
    case InitialSystem::FigureEight:
        return {-8.0, 5.0, 13.0};
    case InitialSystem::Hierarchical:
        return {0.0, 14.0, -19.0};
    case InitialSystem::Stable:
        return {-10.0, 7.0, 16.0};
    case InitialSystem::ControlledChaos:
        return {5.0, -8.0, 12.0};
    case InitialSystem::Unbound:
        return {0.0, 0.0, 0.0};
    }
    return {0.0, 0.0, 0.0};
}

} // namespace

SimulationState applyInitialPlaneTilts(SimulationState state,
                                       const std::array<double, bodyCount>& tiltDegrees) noexcept {
    constexpr std::array<Vec3, bodyCount> axes{{{1.0, 0.0, 0.0},
                                                {0.5, 0.8660254037844386, 0.0},
                                                {-0.766044443118978, 0.6427876096865394, 0.0}}};
    constexpr auto radians = pi / 180.0;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        const auto angle = std::clamp(tiltDegrees[body], -75.0, 75.0) * radians;
        state.bodies[body].position = rotateAroundAxis(state.bodies[body].position, axes[body], angle);
        state.bodies[body].velocity = rotateAroundAxis(state.bodies[body].velocity, axes[body], angle);
    }
    removeCenterOfMassMotion(state);
    return state;
}

SimulationState makeInitialState(InitialSystem system, std::uint64_t seed, double chaos) noexcept {
    SimulationState result;
    result.seed = seed;
    Pcg32 random(seed, 0x3B5ULL);
    chaos = std::clamp(chaos, 0.0, 1.0);

    switch (system) {
    case InitialSystem::FigureEight:
        result.bodies = {{{1.0, {-0.97000436, 0.24308753, 0.0}, {0.466203685, 0.43236573, 0.0}},
                          {1.0, {0.97000436, -0.24308753, 0.0}, {0.466203685, 0.43236573, 0.0}},
                          {1.0, {0.0, 0.0, 0.0}, {-0.93240737, -0.86473146, 0.0}}}};
        addPerturbation(result, random, chaos * 0.015);
        break;
    case InitialSystem::Hierarchical:
        result.bodies = {{{3.4, {-0.18, 0.0, 0.0}, {0.0, -0.18, 0.02}},
                          {0.55, {1.25, 0.0, 0.08}, {0.0, 1.20, 0.04}},
                          {0.12, {1.55, 0.0, -0.04}, {0.0, 1.82, -0.08}}}};
        addPerturbation(result, random, chaos * 0.06);
        break;
    case InitialSystem::Stable:
        result.bodies = {{{1.0, {-1.0, -0.577350269, 0.0}, {0.34, -0.59, 0.02}},
                          {1.0, {1.0, -0.577350269, 0.0}, {0.34, 0.59, -0.02}},
                          {1.0, {0.0, 1.154700538, 0.0}, {-0.68, 0.0, 0.0}}}};
        addPerturbation(result, random, chaos * 0.04);
        break;
    case InitialSystem::ControlledChaos:
        result.bodies = {{{1.0, {-1.1, 0.1, -0.1}, {0.08, -0.45, 0.16}},
                          {0.82, {0.9, -0.2, 0.2}, {0.12, 0.58, -0.07}},
                          {0.64, {0.1, 1.05, -0.15}, {-0.62, -0.08, 0.11}}}};
        addPerturbation(result, random, 0.08 + chaos * 0.28);
        break;
    case InitialSystem::Unbound:
        for (auto& body : result.bodies) {
            body.mass = 0.1 + random.nextUnit() * 4.9;
            body.position = randomDirection(random) * (0.2 + random.nextUnit() * 3.8);
            body.velocity = randomDirection(random) * (0.1 + random.nextUnit() * 2.6);
        }
        removeCenterOfMassMotion(result);
        break;
    }

    result = applyInitialPlaneTilts(result, defaultPlaneTilts(system));
    return result;
}

ThreeBodySimulation::ThreeBodySimulation()
    : ThreeBodySimulation(makeInitialState(InitialSystem::FigureEight, 0x334253ULL)) {}

ThreeBodySimulation::ThreeBodySimulation(SimulationState initial, SimulationConfig config)
    : initial_(initial), state_(initial), config_(config) {
    sanitizeConfig();
}

void ThreeBodySimulation::reset(const SimulationState& initial) noexcept {
    initial_ = initial;
    state_ = initial;
    accumulator_ = 0.0;
}

void ThreeBodySimulation::setConfig(const SimulationConfig& config) noexcept {
    config_ = config;
    sanitizeConfig();
}

void ThreeBodySimulation::setBodyMass(std::size_t bodyIndex, double mass) noexcept {
    if (bodyIndex >= bodyCount || !std::isfinite(mass))
        return;
    const auto validated = std::clamp(mass, 0.05, 1000.0);
    initial_.bodies[bodyIndex].mass = validated;
    state_.bodies[bodyIndex].mass = validated;
}

void ThreeBodySimulation::sanitizeConfig() noexcept {
    config_.gravitationalConstant = std::clamp(config_.gravitationalConstant, 0.000001, 1000.0);
    config_.softening = std::clamp(config_.softening, 0.000001, 4.0);
    config_.fixedStep = std::clamp(config_.fixedStep, 1.0 / 96000.0, 0.25);
    config_.speed = std::clamp(config_.speed, 0.0, 64.0);
    config_.escapeRadius = std::clamp(config_.escapeRadius, 1.0, 100000.0);
    config_.maxStepsPerAdvance = std::clamp(config_.maxStepsPerAdvance, 1U, 1000000U);
}

void ThreeBodySimulation::advance(double deltaTime) noexcept {
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0 || config_.speed <= 0.0)
        return;

    accumulator_ += static_cast<long double>(deltaTime) * static_cast<long double>(config_.speed);
    const auto fixedStep = static_cast<long double>(config_.fixedStep);
    const auto available = static_cast<std::uint64_t>(
        std::floor(accumulator_ / fixedStep + 1.0e-12L));
    const auto steps = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(available, config_.maxStepsPerAdvance));
    for (std::uint32_t index = 0; index < steps; ++index) {
        step();
        accumulator_ -= fixedStep;
    }
    if (std::abs(accumulator_) < fixedStep * 1.0e-12L)
        accumulator_ = 0.0L;
    if (steps == config_.maxStepsPerAdvance)
        accumulator_ = std::fmod(accumulator_, fixedStep);
}

std::array<Vec3, bodyCount> ThreeBodySimulation::accelerations() const noexcept {
    std::array<Vec3, bodyCount> result{};
    const auto epsilonSquared = config_.softening * config_.softening;
    for (std::size_t i = 0; i < bodyCount; ++i) {
        for (std::size_t j = 0; j < bodyCount; ++j) {
            if (i == j)
                continue;
            const auto displacement = state_.bodies[j].position - state_.bodies[i].position;
            const auto softenedDistanceSquared = lengthSquared(displacement) + epsilonSquared;
            const auto denominator = softenedDistanceSquared * std::sqrt(softenedDistanceSquared);
            result[i] += displacement * (config_.gravitationalConstant * state_.bodies[j].mass / denominator);
        }
    }
    return result;
}

void ThreeBodySimulation::step() noexcept {
    const auto before = accelerations();
    const auto dt = config_.fixedStep;
    const auto halfDtSquared = 0.5 * dt * dt;
    for (std::size_t i = 0; i < bodyCount; ++i)
        state_.bodies[i].position += state_.bodies[i].velocity * dt + before[i] * halfDtSquared;

    const auto after = accelerations();
    for (std::size_t i = 0; i < bodyCount; ++i)
        state_.bodies[i].velocity += (before[i] + after[i]) * (0.5 * dt);

    state_.elapsed += dt;
    ++state_.completedSteps;
    detectEscapes();
}

Vec3 ThreeBodySimulation::barycenter() const noexcept {
    double totalMass{};
    Vec3 center{};
    for (const auto& body : state_.bodies) {
        totalMass += body.mass;
        center += body.position * body.mass;
    }
    return totalMass > 0.0 ? center / totalMass : Vec3{};
}

Vec3 ThreeBodySimulation::barycentricVelocity() const noexcept {
    double totalMass{};
    Vec3 velocity{};
    for (const auto& body : state_.bodies) {
        totalMass += body.mass;
        velocity += body.velocity * body.mass;
    }
    return totalMass > 0.0 ? velocity / totalMass : Vec3{};
}

void ThreeBodySimulation::detectEscapes() noexcept {
    for (std::size_t i = 0; i < bodyCount; ++i) {
        std::size_t nearestIndex = i == 0 ? 1 : 0;
        auto nearestDisplacement = state_.bodies[i].position - state_.bodies[nearestIndex].position;
        auto nearestDistanceSquared = lengthSquared(nearestDisplacement);
        for (std::size_t j = 0; j < bodyCount; ++j) {
            if (i == j)
                continue;
            const auto displacement = state_.bodies[i].position - state_.bodies[j].position;
            const auto distanceSquared = lengthSquared(displacement);
            if (distanceSquared < nearestDistanceSquared) {
                nearestIndex = j;
                nearestDisplacement = displacement;
                nearestDistanceSquared = distanceSquared;
            }
        }
        const auto relativeVelocity = state_.bodies[i].velocity - state_.bodies[nearestIndex].velocity;
        const auto outwardVelocity = dot(relativeVelocity, nearestDisplacement);
        const auto escaped = std::sqrt(nearestDistanceSquared) > config_.escapeRadius
                             && outwardVelocity > 0.0;
        state_.escaped[i] = escaped;
        if (escaped && config_.escapePolicy == EscapePolicy::RespawnBody)
            respawn(i);
    }
}

void ThreeBodySimulation::respawn(std::size_t bodyIndex) noexcept {
    if (bodyIndex >= bodyCount)
        return;

    auto& count = state_.respawnCount[bodyIndex];
    ++count;
    const auto mixedSeed = state_.seed ^ (0x9E3779B97F4A7C15ULL * (bodyIndex + 1U))
                           ^ (0xD1B54A32D192ED03ULL * count);
    Pcg32 random(mixedSeed, bodyIndex + 1U);

    double otherMass{};
    Vec3 otherCenter{};
    Vec3 otherVelocity{};
    for (std::size_t i = 0; i < bodyCount; ++i) {
        if (i == bodyIndex)
            continue;
        otherMass += state_.bodies[i].mass;
        otherCenter += state_.bodies[i].position * state_.bodies[i].mass;
        otherVelocity += state_.bodies[i].velocity * state_.bodies[i].mass;
    }
    otherCenter = otherMass > 0.0 ? otherCenter / otherMass : Vec3{};
    otherVelocity = otherMass > 0.0 ? otherVelocity / otherMass : Vec3{};

    const auto direction = randomDirection(random);
    const auto radius = config_.escapeRadius * (0.62 + random.nextUnit() * 0.12);
    auto tangent = normalized(cross(direction, randomDirection(random)), {0.0, 1.0, 0.0});
    const auto orbitalSpeed = std::sqrt(config_.gravitationalConstant * std::max(0.1, otherMass) / radius);
    auto& body = state_.bodies[bodyIndex];
    body.position = otherCenter + direction * radius;
    body.velocity = otherVelocity + tangent * orbitalSpeed - direction * (orbitalSpeed * 0.18);
    state_.escaped[bodyIndex] = false;
}

double ThreeBodySimulation::totalEnergy() const noexcept {
    double kinetic{};
    double potential{};
    for (const auto& body : state_.bodies)
        kinetic += 0.5 * body.mass * lengthSquared(body.velocity);

    const auto epsilonSquared = config_.softening * config_.softening;
    for (std::size_t i = 0; i < bodyCount; ++i) {
        for (std::size_t j = i + 1; j < bodyCount; ++j) {
            const auto distance = std::sqrt(lengthSquared(state_.bodies[j].position - state_.bodies[i].position)
                                            + epsilonSquared);
            potential -= config_.gravitationalConstant * state_.bodies[i].mass * state_.bodies[j].mass / distance;
        }
    }
    return kinetic + potential;
}

double ThreeBodySimulation::interpolationAlpha() const noexcept {
    return config_.fixedStep > 0.0
               ? clamp01(static_cast<double>(accumulator_ / static_cast<long double>(config_.fixedStep)))
               : 0.0;
}

} // namespace threebs
