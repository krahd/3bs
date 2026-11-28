// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace threebs {

// A resource set belongs to the CPU until handOff(), then to its GPU command
// until complete(). A failed/abandoned encoding releases its lease automatically.
// No wait is needed when every set is in flight: the caller can skip that frame.
template <std::size_t Capacity>
class FrameResourcePool {
    static_assert(Capacity > 0 && Capacity < 64);
public:
    class Lease {
    public:
        Lease() noexcept = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)), index_(other.index_) {}
        Lease& operator=(Lease&& other) noexcept {
            if (this != &other) {
                release();
                owner_ = std::exchange(other.owner_, nullptr);
                index_ = other.index_;
            }
            return *this;
        }
        ~Lease() { release(); }
        explicit operator bool() const noexcept { return owner_ != nullptr; }
        std::size_t index() const noexcept { return index_; }
        std::size_t handOff() noexcept {
            owner_ = nullptr;
            return index_;
        }
    private:
        friend class FrameResourcePool;
        Lease(FrameResourcePool* owner, std::size_t index) noexcept
            : owner_(owner), index_(index) {}
        void release() noexcept {
            if (owner_ != nullptr) {
                (void)owner_->complete(index_);
                owner_ = nullptr;
            }
        }
        FrameResourcePool* owner_{};
        std::size_t index_{};
    };

    Lease tryAcquire() noexcept {
        auto occupied = occupied_.load(std::memory_order_acquire);
        for (;;) {
            const auto available = (~occupied) & mask;
            if (available == 0)
                return {};
            const auto index = static_cast<std::size_t>(std::countr_zero(available));
            if (occupied_.compare_exchange_weak(occupied, occupied | (std::uint64_t{1} << index),
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed))
                return Lease(this, index);
        }
    }

    bool complete(std::size_t index) noexcept {
        if (index >= Capacity)
            return false;
        const auto bit = std::uint64_t{1} << index;
        return (occupied_.fetch_and(~bit, std::memory_order_release) & bit) != 0;
    }

    std::size_t inFlight() const noexcept {
        return static_cast<std::size_t>(std::popcount(occupied_.load(std::memory_order_acquire)));
    }

private:
    static constexpr auto mask = (std::uint64_t{1} << Capacity) - 1;
    std::atomic<std::uint64_t> occupied_{};
};

} // namespace threebs
