// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Types.h"

#include <array>
#include <cstdint>
#include <span>

namespace threebs {

std::span<const std::uint8_t> scaleIntervals(ScaleId scale) noexcept;
bool noteIsInScale(std::uint8_t note, std::uint8_t root, ScaleId scale,
                   const std::array<bool, 12>& custom) noexcept;
std::uint8_t quantizeNormalizedPitch(double normalizedValue, const VoiceConfig& voice) noexcept;
std::uint8_t quantizeScaleDegree(double normalizedValue, int degreeOffset,
                                 const VoiceConfig& voice) noexcept;

} // namespace threebs
