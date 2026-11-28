// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdlib>
#include <locale.h>
#if JUCE_MAC || JUCE_IOS
#include <xlocale.h>
#endif
#include <locale>
#include <iomanip>
#include <limits>
#include <sstream>
namespace threebs {
inline juce::String formatExactNumber(double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return juce::String(output.str());
}
// Strict decimal grammar and classic-locale conversion for non-real-time text inputs.
// Apple's deployment-target libc++ lacks floating-point std::from_chars.
inline bool parseFiniteNumber(const juce::String& source, double& value) {
    const auto text = source.trim();
    const auto bytes = text.toRawUTF8();
    const auto length = static_cast<std::size_t>(text.getNumBytesAsUTF8());
    if (length == 0U) return false;
    std::size_t i{};
    if (bytes[i] == '+' || bytes[i] == '-') ++i;
    bool digit{};
    while (i < length && bytes[i] >= '0' && bytes[i] <= '9') { digit = true; ++i; }
    if (i < length && bytes[i] == '.') {
        ++i;
        while (i < length && bytes[i] >= '0' && bytes[i] <= '9') { digit = true; ++i; }
    }
    if (!digit) return false;
    if (i < length && (bytes[i] == 'e' || bytes[i] == 'E')) {
        ++i;
        if (i < length && (bytes[i] == '+' || bytes[i] == '-')) ++i;
        const auto start = i;
        while (i < length && bytes[i] >= '0' && bytes[i] <= '9') ++i;
        if (start == i) return false;
    }
    if (i != length) return false;
    // libc++ streams set failbit on ERANGE even when strtod produced a valid
    // finite subnormal. The explicit C locale keeps decimal syntax independent
    // of the host locale without treating representable underflow as invalid.
    // This locale is process-lifetime storage, initialised only by non-real-time
    // text parsing; no processing callback invokes this function.
    char* end{};
#if JUCE_WINDOWS
    static const auto numericLocale = _create_locale(LC_NUMERIC, "C");
    if (numericLocale == nullptr) return false;
    value = _strtod_l(bytes, &end, numericLocale);
#else
    static const auto numericLocale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
    if (numericLocale == nullptr) return false;
    value = strtod_l(bytes, &end, numericLocale);
#endif
    return end == bytes + length && std::isfinite(value);
}
} // namespace threebs
