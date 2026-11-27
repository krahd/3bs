// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace threebs {

// One logical producer (external writers serialise before calling publish),
// one audio consumer. Each side exclusively owns its current slot; exchange
// transfers the unused middle slot. The consumer never waits for a writer.
// Publications deliberately coalesce. Commands must contain complete state
// and a monotonic reset revision so that coalescing cannot discard cleanup.
template <typename Value>
class LatestCommandMailbox {
    static_assert(std::is_trivially_copyable_v<Value>);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
public:
    void publish(const Value& value) noexcept {
        slots_[producer_] = value;
        producer_ = middle_.exchange(producer_ | dirty, std::memory_order_acq_rel) & indexMask;
    }

    bool consume(Value& value) noexcept {
        if ((middle_.load(std::memory_order_acquire) & dirty) == 0U)
            return false;
        consumer_ = middle_.exchange(consumer_, std::memory_order_acq_rel) & indexMask;
        value = slots_[consumer_];
        return true;
    }

private:
    static constexpr std::uint32_t dirty = 4U;
    static constexpr std::uint32_t indexMask = 3U;
    std::array<Value, 3> slots_{};
    std::uint32_t producer_{};
    std::uint32_t consumer_{1U};
    std::atomic<std::uint32_t> middle_{2U};
};

} // namespace threebs
