// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Math.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace threebs {

template <std::size_t Capacity>
class TrailHistory {
    static_assert(Capacity >= 2);
public:
    static constexpr double targetSegmentLength = 0.075;
    static constexpr std::size_t maximumSubdivisions = 16;
    struct Sample {
        Vec3 position{};
        double time{};
    };

    void append(Vec3 position, double time, std::uint64_t revision,
                double retainSeconds = 0.0) noexcept {
        if (!isFinite(position) || !std::isfinite(time))
            return;
        if (!haveRevision_ || revision != revision_) {
            clear();
            revision_ = revision;
            haveRevision_ = true;
        }
        if (size_ > 0U && time < (*this)[size_ - 1U].time)
            clear();
        if (retainSeconds > 0.0 && std::isfinite(retainSeconds)) {
            prune(time, retainSeconds);
            const auto interval = retainSeconds / static_cast<double>(std::max<std::size_t>(1U, Capacity - 2U));
            if (size_ > 0U && time - (*this)[size_ - 1U].time < interval) {
                // A render callback faster than the history budget updates a
                // separate live endpoint; it cannot evict the requested past.
                liveEndpoint_ = {position, time};
                haveLiveEndpoint_ = true;
                return;
            }
            if (size_ == Capacity)
                decimate();
        }
        haveLiveEndpoint_ = false;
        if (size_ > 0U && time == (*this)[size_ - 1U].time) {
            samples_[(start_ + size_ - 1U) % Capacity] = {position, time};
        } else {
            push({position, time});
        }
    }

    // History stores time samples only. Smooth geometry is generated separately
    // within the caller's fixed budget, always retaining the oldest/newest ends.
    std::size_t tessellate(std::span<Sample> output) const noexcept {
        const auto historyCount = size_ + static_cast<std::size_t>(haveLiveEndpoint_);
        if (historyCount == 0U || output.empty())
            return 0;
        const auto sample = [&](std::size_t index) -> const Sample& {
            return index < size_ ? (*this)[index] : liveEndpoint_;
        };
        const auto baseCount = std::min(historyCount, output.size());
        if (baseCount == 1U) {
            output[0] = sample(historyCount - 1U);
            return 1;
        }
        const auto sourceIndex = [&](std::size_t index) {
            return index * (historyCount - 1U) / (baseCount - 1U);
        };
        const auto extraFor = [&](std::size_t index) {
            const auto distance = length(sample(sourceIndex(index)).position
                                         - sample(sourceIndex(index - 1U)).position);
            const auto subdivisions = std::clamp(std::ceil(distance / targetSegmentLength),
                                                 1.0, static_cast<double>(maximumSubdivisions));
            return static_cast<std::size_t>(subdivisions) - 1U;
        };
        std::size_t wantedExtra{};
        for (std::size_t index = 1; index < baseCount; ++index)
            wantedExtra += extraFor(index);
        const auto extraBudget = std::min(wantedExtra, output.size() - baseCount);
        std::size_t cumulativeExtra{}, allocatedExtra{}, count{1};
        output[0] = sample(0);
        for (std::size_t index = 1; index < baseCount; ++index) {
            cumulativeExtra += extraFor(index);
            const auto allocated = wantedExtra == 0U ? 0U
                : cumulativeExtra * extraBudget / wantedExtra;
            const auto subdivisions = 1U + allocated - allocatedExtra;
            allocatedExtra = allocated;
            const auto& previous = sample(sourceIndex(index - 1U));
            const auto& destination = sample(sourceIndex(index));
            auto tangent = destination.position - previous.position;
            if (index > 1U) {
                const auto& prior = sample(sourceIndex(index - 2U));
                const auto priorDuration = previous.time - prior.time;
                if (priorDuration > 0.0)
                    tangent = (previous.position - prior.position)
                              * ((destination.time - previous.time) / priorDuration);
            }
            const auto destinationTangent = destination.position - previous.position;
            for (std::size_t step = 1U; step <= subdivisions; ++step) {
                const auto amount = static_cast<double>(step) / static_cast<double>(subdivisions);
                const auto amount2 = amount * amount;
                const auto amount3 = amount2 * amount;
                output[count++] = {
                    previous.position * (2.0 * amount3 - 3.0 * amount2 + 1.0)
                        + tangent * (amount3 - 2.0 * amount2 + amount)
                        + destination.position * (-2.0 * amount3 + 3.0 * amount2)
                        + destinationTangent * (amount3 - amount2),
                    previous.time + (destination.time - previous.time) * amount};
            }
        }
        return count;
    }

private:
    void decimate() noexcept {
        if constexpr (Capacity <= 2)
            return;
        std::size_t destination{};
        for (std::size_t source = 0; source < size_; source += 2U)
            samples_[(start_ + destination++) % Capacity] = (*this)[source];
        if ((size_ - 1U) % 2U != 0U)
            samples_[(start_ + destination++) % Capacity] = (*this)[size_ - 1U];
        size_ = destination;
    }

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
        if (haveLiveEndpoint_ && liveEndpoint_.time < cutoff)
            haveLiveEndpoint_ = false;
        while (size_ > 0U && samples_[start_].time < cutoff) {
            start_ = (start_ + 1U) % Capacity;
            --size_;
        }
    }

    void clear() noexcept {
        start_ = 0U;
        size_ = 0U;
        haveLiveEndpoint_ = false;
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
    Sample liveEndpoint_{};
    bool haveLiveEndpoint_{};
};

} // namespace threebs
