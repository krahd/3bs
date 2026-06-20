// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Types.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace threebs {

enum class PaletteId : std::uint8_t {
    Eclipse,
    Glass,
    Ember,
    Nocturne,
    Cyan,
    Violet,
};

enum class GraphicsQuality : std::uint8_t {
    Low,
    Medium,
    High,
};

struct Colour3 {
    float r{};
    float g{};
    float b{};
};

struct CameraState {
    float yaw{};
    float pitch{-0.34F};
    float distance{7.0F};
    float autoOrbit{0.035F};
    int focusBody{-1};

    bool operator==(const CameraState&) const = default;
};

struct VisualSettings {
    float trailSeconds{30.0F};
    float trailWidth{1.2F};
    float extrusion{0.12F};
    float bloom{0.34F};
    float starDensity{0.62F};
    PaletteId palette{PaletteId::Eclipse};
    GraphicsQuality quality{GraphicsQuality::High};

    bool operator==(const VisualSettings&) const = default;
};

struct PlanetVisualStyle {
    std::uint32_t seed{};
    std::array<Colour3, 4> colours{};
    float terrainScale{2.4F};
    float oceanLevel{0.48F};
    float rotationRate{0.035F};
    float cloudRate{0.015F};
    float axialTilt{};
};

struct PresentationState {
    VisualSettings visual{};
    CameraState camera{};
    std::uint64_t visualSeed{0x334253ULL};

    bool operator==(const PresentationState&) const = default;
};

constexpr float migrateV1TrailLength(float normalizedLength) noexcept {
    return 5.0F + 30.0F * std::clamp(normalizedLength, 0.0F, 1.0F);
}

std::array<PlanetVisualStyle, bodyCount> makePlanetVisualStyles(PaletteId palette,
                                                               std::uint64_t seed) noexcept;

constexpr double planetRadius(double mass) noexcept {
    const auto safeMass = mass < 0.05 ? 0.05 : mass;
    return 0.22 + 0.10 * std::cbrt(safeMass);
}

} // namespace threebs
