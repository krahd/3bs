// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/ThreeBodySimulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace threebs;
Vec3 angularMomentum(const SimulationState& state) {
    Vec3 result{};
    for (const auto& body : state.bodies) result += cross(body.position, body.velocity * body.mass);
    return result;
}
double separation(const SimulationState& state) {
    double result = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < bodyCount; ++i)
        for (std::size_t j = i + 1; j < bodyCount; ++j)
            result = std::min(result, length(state.bodies[i].position - state.bodies[j].position));
    return result;
}
bool finite(const SimulationState& state) {
    for (const auto& body : state.bodies)
        if (!isFinite(body.position) || !isFinite(body.velocity) || !std::isfinite(body.mass)) return false;
    return std::isfinite(state.elapsed);
}
double distance(const SimulationState& a, const SimulationState& b) {
    double result{};
    for (std::size_t i = 0; i < bodyCount; ++i) {
        result = std::max(result, length(a.bodies[i].position - b.bodies[i].position));
        result = std::max(result, length(a.bodies[i].velocity - b.bodies[i].velocity));
    }
    return result;
}

bool referenceConvergence() {
    constexpr double root3 = 1.7320508075688772935;
    SimulationConfig config;
    config.gravitationalConstant = 1.0;
    config.softening = 0.04;
    config.escapePolicy = EscapePolicy::Leave;
    const auto omega = std::sqrt(3.0 / std::pow(3.0 + config.softening * config.softening, 1.5));
    SimulationState initial;
    initial.bodies[0].position = {1, 0, 0};
    initial.bodies[1].position = {-0.5, root3 * 0.5, 0};
    initial.bodies[2].position = {-0.5, -root3 * 0.5, 0};
    for (auto& body : initial.bodies) body.velocity = cross({0, 0, omega}, body.position);
    std::array<double, 3> errors{};
    std::array<double, 3> energyErrors{};
    bool ok = true;
    for (std::size_t level = 0; level < errors.size(); ++level) {
        const auto stepsPerUnit = 120U << level;
        config.fixedStep = 1.0 / stepsPerUnit;
        ThreeBodySimulation simulation(initial, config);
        const auto energy = simulation.totalEnergy();
        const auto momentum = angularMomentum(initial);
        for (unsigned i = 0; i < 4U * stepsPerUnit; ++i) {
            simulation.step();
            energyErrors[level] = std::max(energyErrors[level], std::abs(simulation.totalEnergy() - energy));
        }
        const auto angle = omega * 4.0;
        for (std::size_t body = 0; body < bodyCount; ++body) {
            const auto p = initial.bodies[body].position;
            const Vec3 exact{p.x * std::cos(angle) - p.y * std::sin(angle),
                             p.x * std::sin(angle) + p.y * std::cos(angle), 0};
            errors[level] = std::max(errors[level], length(simulation.state().bodies[body].position - exact));
        }
        ok = ok && finite(simulation.state()) && !simulation.faulted()
            && length(angularMomentum(simulation.state()) - momentum) < 1.0e-10
            && length(simulation.barycenter()) < 1.0e-10;
    }
    const auto firstRatio = errors[0] / errors[1];
    const auto secondRatio = errors[1] / errors[2];
    ok = ok && firstRatio > 3.8 && firstRatio < 4.2 && secondRatio > 3.8 && secondRatio < 4.2
        && energyErrors[2] < energyErrors[1] && energyErrors[1] < energyErrors[0];
    std::cout << "{\"check\":\"softened_equilateral_convergence\",\"position_errors\":["
              << errors[0] << ',' << errors[1] << ',' << errors[2]
              << "],\"refinement_ratios\":[" << firstRatio << ',' << secondRatio
              << "],\"passed\":" << (ok ? "true" : "false") << "}\n";

    auto extreme = initial;
    for (auto& body : extreme.bodies) body.mass = 1000.0;
    config.gravitationalConstant = 1000.0;
    config.softening = 0.000001;
    config.fixedStep = 1.0 / 96000.0;
    config.speed = 64.0;
    config.maxStepsPerAdvance = 128;
    ThreeBodySimulation bounded(extreme, config);
    bounded.advance(1000.0);
    const bool boundedOk = finite(bounded.state()) && !bounded.faulted()
        && bounded.state().completedSteps == 128 && bounded.truncatedAdvances() == 1;
    std::cout << "{\"check\":\"extreme_configuration_work_bound\",\"steps\":"
              << bounded.state().completedSteps << ",\"truncated_advances\":" << bounded.truncatedAdvances()
              << ",\"passed\":" << (boundedOk ? "true" : "false") << "}\n";
    return ok && boundedOk;
}

bool presetDiagnostic(char** args) {
    const std::string id(args[2]);
    if (id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos) return false;
    const auto system = static_cast<InitialSystem>(std::stoul(args[3]));
    const auto seed = static_cast<std::uint64_t>(std::stoull(args[4]));
    const auto chaos = std::stod(args[5]);
    SimulationConfig config;
    config.gravitationalConstant = std::stod(args[6]);
    config.softening = std::stod(args[7]);
    config.speed = std::stod(args[8]);
    config.escapePolicy = static_cast<EscapePolicy>(std::stoul(args[9]));
    if (!isValidSimulationConfig(config)) return false;
    const auto initial = makeInitialState(system, seed, chaos);
    ThreeBodySimulation published(initial, config);
    auto conservativeConfig = config; conservativeConfig.escapePolicy = EscapePolicy::Leave;
    ThreeBodySimulation conservative(initial, conservativeConfig);
    ThreeBodySimulation repeated(initial, conservativeConfig);
    auto refinedConfig = conservativeConfig; refinedConfig.fixedStep *= 0.5;
    ThreeBodySimulation refined(initial, refinedConfig);
    const auto initialEnergy = conservative.totalEnergy();
    const auto initialMomentum = angularMomentum(initial);
    double energyExcursion{}, refinedEnergyExcursion{}, momentumExcursion{};
    double minimumSeparation = separation(initial);
    bool ok = true;
    for (unsigned tick = 0; tick < 6000; ++tick) {
        published.advance(0.01);
        conservative.advance(0.01);
        repeated.advance(0.01);
        refined.advance(0.01);
        energyExcursion = std::max(energyExcursion,
            std::abs(conservative.totalEnergy() - initialEnergy) / std::max(1.0, std::abs(initialEnergy)));
        refinedEnergyExcursion = std::max(refinedEnergyExcursion,
            std::abs(refined.totalEnergy() - initialEnergy) / std::max(1.0, std::abs(initialEnergy)));
        momentumExcursion = std::max(momentumExcursion,
            length(angularMomentum(conservative.state()) - initialMomentum) / std::max(1.0, length(initialMomentum)));
        minimumSeparation = std::min(minimumSeparation, separation(conservative.state()));
        ok = ok && finite(published.state()) && finite(conservative.state()) && finite(refined.state())
            && !published.faulted() && !conservative.faulted() && !refined.faulted();
    }
    const auto repeatDifference = distance(conservative.state(), repeated.state());
    const auto refinementDifference = distance(conservative.state(), refined.state());
    std::uint64_t respawns{};
    for (const auto value : published.state().respawnCount) respawns += value;
    ok = ok && repeatDifference == 0.0 && momentumExcursion < 1.0e-8
        && published.truncatedAdvances() == 0 && conservative.truncatedAdvances() == 0
        && refined.truncatedAdvances() == 0;
    // Energy and trajectory differences under refinement are diagnostics, not
    // universal chaos/respawn tolerances or permission to replace preset values.
    std::cout << "{\"preset\":\"" << id << "\",\"beat_units\":60,\"sampled_every_beats\":0.01"
              << ",\"conservative_energy_excursion\":" << energyExcursion
              << ",\"half_step_energy_excursion\":" << refinedEnergyExcursion
              << ",\"relative_angular_momentum_excursion\":" << momentumExcursion
              << ",\"minimum_sampled_separation\":" << minimumSeparation
              << ",\"repeat_max_state_difference\":" << repeatDifference
              << ",\"half_step_final_state_difference\":" << refinementDifference
              << ",\"published_policy_respawns\":" << respawns
              << ",\"passed\":" << (ok ? "true" : "false") << "}\n";
    return ok;
}
}

int main(int argc, char** argv) {
    std::cout << std::setprecision(12);
    try {
        if (argc == 1) return referenceConvergence() ? EXIT_SUCCESS : EXIT_FAILURE;
        if (argc == 10 && std::string(argv[1]) == "--preset")
            return presetDiagnostic(argv) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    std::cerr << "Usage: numerical-tests [--preset id system seed chaos gravity softening speed escape-policy]\n";
    return EXIT_FAILURE;
}
