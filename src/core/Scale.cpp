// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/Scale.h"

#include <algorithm>
#include <array>

namespace threebs {
namespace {

constexpr std::array<std::uint8_t, 7> major{0, 2, 4, 5, 7, 9, 11};
constexpr std::array<std::uint8_t, 7> naturalMinor{0, 2, 3, 5, 7, 8, 10};
constexpr std::array<std::uint8_t, 7> dorian{0, 2, 3, 5, 7, 9, 10};
constexpr std::array<std::uint8_t, 7> phrygian{0, 1, 3, 5, 7, 8, 10};
constexpr std::array<std::uint8_t, 7> lydian{0, 2, 4, 6, 7, 9, 11};
constexpr std::array<std::uint8_t, 7> mixolydian{0, 2, 4, 5, 7, 9, 10};
constexpr std::array<std::uint8_t, 7> locrian{0, 1, 3, 5, 6, 8, 10};
constexpr std::array<std::uint8_t, 5> majorPentatonic{0, 2, 4, 7, 9};
constexpr std::array<std::uint8_t, 5> minorPentatonic{0, 3, 5, 7, 10};
constexpr std::array<std::uint8_t, 6> blues{0, 3, 5, 6, 7, 10};
constexpr std::array<std::uint8_t, 12> chromatic{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr std::array<std::uint8_t, 7> hungarianMinor{0, 2, 3, 6, 7, 8, 11};
constexpr std::array<std::uint8_t, 6> wholeTone{0, 2, 4, 6, 8, 10};
constexpr std::array<std::uint8_t, 8> diminished{0, 2, 3, 5, 6, 8, 9, 11};
constexpr std::array<std::uint8_t, 0> custom{};

} // namespace

std::span<const std::uint8_t> scaleIntervals(ScaleId scale) noexcept {
    switch (scale) {
    case ScaleId::Major: return major;
    case ScaleId::NaturalMinor: return naturalMinor;
    case ScaleId::Dorian: return dorian;
    case ScaleId::Phrygian: return phrygian;
    case ScaleId::Lydian: return lydian;
    case ScaleId::Mixolydian: return mixolydian;
    case ScaleId::Locrian: return locrian;
    case ScaleId::MajorPentatonic: return majorPentatonic;
    case ScaleId::MinorPentatonic: return minorPentatonic;
    case ScaleId::Blues: return blues;
    case ScaleId::Chromatic: return chromatic;
    case ScaleId::HungarianMinor: return hungarianMinor;
    case ScaleId::WholeTone: return wholeTone;
    case ScaleId::Diminished: return diminished;
    case ScaleId::Custom: return custom;
    }
    return minorPentatonic;
}

bool noteIsInScale(std::uint8_t note, std::uint8_t root, ScaleId scale,
                   const std::array<bool, 12>& customNotes) noexcept {
    const auto pitchClass = static_cast<std::uint8_t>((note + 12U - (root % 12U)) % 12U);
    if (scale == ScaleId::Custom)
        return customNotes[pitchClass];
    const auto intervals = scaleIntervals(scale);
    return std::find(intervals.begin(), intervals.end(), pitchClass) != intervals.end();
}

std::uint8_t quantizeNormalizedPitch(double normalizedValue, const VoiceConfig& voice) noexcept {
    const auto low = std::min(voice.minimumNote, voice.maximumNote);
    const auto high = std::max(voice.minimumNote, voice.maximumNote);
    std::array<std::uint8_t, 128> candidates{};
    std::size_t count{};
    for (std::uint16_t note = low; note <= high; ++note) {
        const auto midiNote = static_cast<std::uint8_t>(note);
        if (noteIsInScale(midiNote, voice.root, voice.scale, voice.customScale))
            candidates[count++] = midiNote;
    }
    if (count == 0)
        return low;
    const auto value = clamp01(normalizedValue);
    const auto index = std::min(count - 1U, static_cast<std::size_t>(value * static_cast<double>(count)));
    return candidates[index];
}

std::uint8_t quantizeScaleDegree(double normalizedValue, int degreeOffset,
                                 const VoiceConfig& voice) noexcept {
    const auto low = std::min(voice.minimumNote, voice.maximumNote);
    const auto high = std::max(voice.minimumNote, voice.maximumNote);
    std::array<std::uint8_t, 128> candidates{};
    int count{};
    for (std::uint16_t note = low; note <= high; ++note) {
        const auto midiNote = static_cast<std::uint8_t>(note);
        if (noteIsInScale(midiNote, voice.root, voice.scale, voice.customScale))
            candidates[static_cast<std::size_t>(count++)] = midiNote;
    }
    if (count == 0)
        return low;
    const auto base = std::min(count - 1, static_cast<int>(clamp01(normalizedValue) * count));
    return candidates[static_cast<std::size_t>(std::clamp(base + degreeOffset, 0, count - 1))];
}

} // namespace threebs
