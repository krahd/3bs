// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#pragma once
#include <juce_core/juce_core.h>
#include <charconv>
#include <cmath>
namespace threebs {
inline bool parseFiniteNumber(const juce::String& source, double& value) {
    auto text = source.trim().toStdString();
    if (text.empty()) return false;
    if (text.front() == '+') text.erase(text.begin());
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value,
                                       std::chars_format::general);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && std::isfinite(value);
}
} // namespace threebs
