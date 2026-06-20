// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/Types.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace threebs {

struct RenderSnapshot {
    std::array<BodyState, bodyCount> bodies{};
    std::array<bool, bodyCount> escaped{};
    std::uint64_t sequence{};
    std::uint64_t trajectoryRevision{};
    double interpolationAlpha{};
};

enum class NoteVisualizationType : std::uint8_t { On, Off };

struct NoteVisualizationEvent {
    NoteVisualizationType type{NoteVisualizationType::On};
    std::uint8_t body{};
    std::uint8_t note{};
    std::uint8_t velocity{};
    std::uint64_t sequence{};
};

template <typename Value, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2);
    static_assert(std::is_trivially_copyable_v<Value>);

public:
    bool push(const Value& value) noexcept {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == read_.load(std::memory_order_acquire))
            return false;
        storage_[write] = value;
        write_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(Value& value) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire))
            return false;
        value = storage_[read];
        read_.store(increment(read), std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t increment(std::size_t value) noexcept {
        return (value + 1U) % Capacity;
    }

    std::array<Value, Capacity> storage_{};
    alignas(64) std::atomic<std::size_t> read_{};
    alignas(64) std::atomic<std::size_t> write_{};
};

using NoteVisualizationQueue = SpscQueue<NoteVisualizationEvent, 2048>;

} // namespace threebs
