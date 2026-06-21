// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Math.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace threebs {

template <std::size_t Capacity>
class TrailHistory {
public:
    static constexpr double targetSegmentLength = 0.075;
    static constexpr std::size_t maximumSubdivisions = 16;
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
        if (size_ == 0U) {
            push({position, time});
            return;
        }
        const auto previous = (*this)[size_ - 1U];
        const auto distance = length(position - previous.position);
        const auto subdivisions = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::ceil(distance / targetSegmentLength)),
            1U, maximumSubdivisions);
        const auto tangent = size_ > 1U
            ? previous.position - (*this)[size_ - 2U].position : position - previous.position;
        const auto destinationTangent = position - previous.position;
        for (std::size_t step = 1U; step <= subdivisions; ++step) {
            const auto amount = static_cast<double>(step) / static_cast<double>(subdivisions);
            const auto amount2 = amount * amount;
            const auto amount3 = amount2 * amount;
            push({previous.position * (2.0 * amount3 - 3.0 * amount2 + 1.0)
                      + tangent * (amount3 - 2.0 * amount2 + amount)
                      + position * (-2.0 * amount3 + 3.0 * amount2)
                      + destinationTangent * (amount3 - amount2),
                  previous.time + (time - previous.time) * amount});
        }
    }

private:
    void push(Sample sample) noexcept {
        const auto destination = (start_ + size_) % Capacity;
        samples_[destination] = sample;
        if (size_ < Capacity) {
            ++size_;
        } else {
            start_ = (start_ + 1U) % Capacity;
        }
    }

public:
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
