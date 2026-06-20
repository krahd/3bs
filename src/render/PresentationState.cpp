// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "render/PresentationState.h"

#include "core/Random.h"

#include <algorithm>
#include <cmath>

namespace threebs {
namespace {

using Palette = std::array<Colour3, 6>;

constexpr Palette paletteFor(PaletteId id) noexcept {
    switch (id) {
    case PaletteId::Glass:
        return {{{0.025F, 0.075F, 0.12F}, {0.08F, 0.36F, 0.46F}, {0.26F, 0.78F, 0.82F},
                 {0.76F, 0.95F, 0.94F}, {0.31F, 0.46F, 0.64F}, {0.64F, 0.76F, 0.91F}}};
    case PaletteId::Ember:
        return {{{0.055F, 0.012F, 0.016F}, {0.34F, 0.045F, 0.025F}, {0.82F, 0.21F, 0.055F},
                 {1.00F, 0.68F, 0.19F}, {0.29F, 0.08F, 0.12F}, {0.75F, 0.25F, 0.17F}}};
    case PaletteId::Nocturne:
        return {{{0.008F, 0.012F, 0.045F}, {0.035F, 0.09F, 0.23F}, {0.19F, 0.32F, 0.57F},
                 {0.65F, 0.72F, 0.91F}, {0.12F, 0.035F, 0.19F}, {0.48F, 0.27F, 0.68F}}};
    case PaletteId::Cyan:
        return {{{0.006F, 0.035F, 0.055F}, {0.01F, 0.22F, 0.29F}, {0.02F, 0.69F, 0.76F},
                 {0.57F, 0.98F, 0.96F}, {0.06F, 0.18F, 0.35F}, {0.18F, 0.72F, 0.93F}}};
    case PaletteId::Violet:
        return {{{0.026F, 0.012F, 0.065F}, {0.16F, 0.05F, 0.31F}, {0.48F, 0.20F, 0.74F},
                 {0.88F, 0.61F, 0.98F}, {0.14F, 0.10F, 0.38F}, {0.45F, 0.37F, 0.89F}}};
    case PaletteId::Eclipse:
    default:
        return {{{0.025F, 0.018F, 0.035F}, {0.23F, 0.12F, 0.045F}, {0.93F, 0.55F, 0.13F},
                 {1.00F, 0.86F, 0.45F}, {0.015F, 0.22F, 0.29F}, {0.42F, 0.17F, 0.67F}}};
    }
}

Colour3 mix(Colour3 a, Colour3 b, float amount) noexcept {
    return {a.r + (b.r - a.r) * amount,
            a.g + (b.g - a.g) * amount,
            a.b + (b.b - a.b) * amount};
}

} // namespace

CameraState sanitizedCameraState(CameraState state) noexcept {
    const CameraState defaults;
    if (!std::isfinite(state.yaw))
        state.yaw = defaults.yaw;
    if (!std::isfinite(state.pitch))
        state.pitch = defaults.pitch;
    if (!std::isfinite(state.distance))
        state.distance = defaults.distance;
    if (!std::isfinite(state.minimumDistance))
        state.minimumDistance = defaults.minimumDistance;
    if (!std::isfinite(state.maximumDistance))
        state.maximumDistance = defaults.maximumDistance;
    if (!std::isfinite(state.autoOrbit))
        state.autoOrbit = defaults.autoOrbit;

    state.pitch = std::clamp(state.pitch, -1.4835298F, 1.4835298F);
    state.minimumDistance = std::clamp(state.minimumDistance, 1.0F, 20.0F);
    state.maximumDistance = std::clamp(state.maximumDistance, 5.0F, 80.0F);
    if (state.maximumDistance < state.minimumDistance + 0.5F)
        state.maximumDistance = std::min(80.0F, state.minimumDistance + 0.5F);
    if (state.minimumDistance > state.maximumDistance - 0.5F)
        state.minimumDistance = std::max(1.0F, state.maximumDistance - 0.5F);
    state.distance = std::clamp(state.distance, state.minimumDistance, state.maximumDistance);
    state.autoOrbit = std::clamp(state.autoOrbit, -2.0F, 2.0F);
    state.focusBody = std::clamp(state.focusBody, -1, static_cast<int>(bodyCount) - 1);
    return state;
}

std::array<PlanetVisualStyle, bodyCount> makePlanetVisualStyles(PaletteId palette,
                                                               std::uint64_t seed) noexcept {
    const auto colours = paletteFor(palette);
    Pcg32 random(seed ^ 0x504c414e455453ULL, 0x334253ULL);
    std::array<PlanetVisualStyle, bodyCount> result{};
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& style = result[body];
        style.seed = random.nextUInt();
        const auto accent = static_cast<std::size_t>((body * 2U) % colours.size());
        style.colours[0] = mix(colours[0], colours[(accent + 1U) % colours.size()], 0.28F);
        style.colours[1] = colours[(accent + 1U) % colours.size()];
        style.colours[2] = colours[(accent + 2U) % colours.size()];
        style.colours[3] = colours[(accent + 3U) % colours.size()];
        style.terrainScale = 1.7F + static_cast<float>(random.nextUnit()) * 2.3F;
        style.oceanLevel = 0.38F + static_cast<float>(random.nextUnit()) * 0.22F;
        style.rotationRate = (0.018F + static_cast<float>(random.nextUnit()) * 0.042F)
            * (body == 1U ? -1.0F : 1.0F);
        style.cloudRate = style.rotationRate * (1.35F + static_cast<float>(random.nextUnit()) * 0.8F);
        style.axialTilt = -0.42F + static_cast<float>(random.nextUnit()) * 0.84F;
    }
    return result;
}

} // namespace threebs
