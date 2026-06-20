// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Math.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace threebs {

template <std::size_t Capacity>
class TrailHistory {
public:
    struct Sample {
        Vec3 position{};
        double time{};
    };

    void append(Vec3 position, double time, std::uint64_t revision) noexcept {
        if (!haveRevision_ || revision != revision_) {
            clear();
            revision_ = revision;
            haveRevision_ = true;
        }
        const auto destination = (start_ + size_) % Capacity;
        samples_[destination] = {position, time};
        if (size_ < Capacity) {
            ++size_;
        } else {
            start_ = (start_ + 1U) % Capacity;
        }
    }

    void prune(double now, double seconds) noexcept {
        const auto cutoff = now - seconds;
        while (size_ > 0U && samples_[start_].time < cutoff) {
            start_ = (start_ + 1U) % Capacity;
            --size_;
        }
    }

    void clear() noexcept {
        start_ = 0U;
        size_ = 0U;
    }

    std::size_t size() const noexcept { return size_; }
    const Sample& operator[](std::size_t index) const noexcept {
        return samples_[(start_ + index) % Capacity];
    }

private:
    std::array<Sample, Capacity> samples_{};
    std::size_t start_{};
    std::size_t size_{};
    std::uint64_t revision_{};
    bool haveRevision_{};
};

} // namespace threebs
