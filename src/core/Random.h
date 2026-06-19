// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include <cstdint>

namespace threebs {

class Pcg32 {
public:
    explicit Pcg32(std::uint64_t seed = 0x334253ULL, std::uint64_t sequence = 1ULL) noexcept {
        seedGenerator(seed, sequence);
    }

    void seedGenerator(std::uint64_t seed, std::uint64_t sequence = 1ULL) noexcept {
        state_ = 0ULL;
        increment_ = (sequence << 1U) | 1U;
        nextUInt();
        state_ += seed;
        nextUInt();
    }

    std::uint32_t nextUInt() noexcept {
        const auto previous = state_;
        state_ = previous * 6364136223846793005ULL + increment_;
        const auto xorShifted = static_cast<std::uint32_t>(((previous >> 18U) ^ previous) >> 27U);
        const auto rotation = static_cast<std::uint32_t>(previous >> 59U);
        return (xorShifted >> rotation) | (xorShifted << ((0U - rotation) & 31U));
    }

    double nextUnit() noexcept {
        return static_cast<double>(nextUInt()) / 4294967296.0;
    }

    double symmetric() noexcept {
        return nextUnit() * 2.0 - 1.0;
    }

private:
    std::uint64_t state_{};
    std::uint64_t increment_{};
};

} // namespace threebs
