// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/MusicEngine.h"

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace threebs {

// One producer and one consumer. The producer owns the back slot, the consumer
// owns the front slot; only a completed value is exchanged through the middle.
// Superseded control edits may coalesce. Reset requests are cumulative revisions.
template <typename Value>
class LatestValueMailbox {
    static_assert(std::is_trivially_copyable_v<Value>);
public:
    void publish(const Value& value) noexcept {
        slots_[back_] = value;
        back_ = middle_.exchange(back_ | dirty, std::memory_order_acq_rel) & indexMask;
    }
    bool consume(Value& value) noexcept {
        if ((middle_.load(std::memory_order_acquire) & dirty) == 0U)
            return false;
        front_ = middle_.exchange(front_, std::memory_order_acq_rel) & indexMask;
        value = slots_[front_];
        return true;
    }
private:
    static constexpr unsigned dirty = 4U;
    static constexpr unsigned indexMask = 3U;
    std::array<Value, 3> slots_{};
    unsigned back_{1U};
    unsigned front_{};
    alignas(64) std::atomic<unsigned> middle_{2U};
};

// All deadlines derive from the same absolute sample origin. A delayed wakeup
// changes lateness, never tempo; UI callback counts do not enter this clock.
class StandaloneTimeline {
public:
    static constexpr double sampleRate = 48000.0;
    static constexpr double tempo = 120.0;
    static constexpr double lookAheadSeconds = 0.020;
    static constexpr double maximumLatenessSeconds = 0.100;
    static constexpr std::uint32_t blockSamples = 96U;

    explicit StandaloneTimeline(double nowSeconds = 0.0) noexcept
        : origin_(nowSeconds + lookAheadSeconds) {}
    double deadline(std::uint64_t sample) const noexcept {
        return origin_ + static_cast<double>(sample) / sampleRate;
    }
    double seconds() const noexcept { return static_cast<double>(cursor_) / sampleRate; }
    double beat() const noexcept { return static_cast<double>(cursor_) * tempo / (60.0 * sampleRate); }
    std::uint64_t cursor() const noexcept { return cursor_; }
    bool blockDue(double nowSeconds) const noexcept {
        return deadline(cursor_ + blockSamples) <= nowSeconds + lookAheadSeconds;
    }
    bool isLate(double nowSeconds) const noexcept {
        return nowSeconds - deadline(cursor_) > maximumLatenessSeconds;
    }
    void advance() noexcept { cursor_ += blockSamples; }
    void recoverAt(double nowSeconds) noexcept {
        const auto sample = std::max(0.0, (nowSeconds + lookAheadSeconds - origin_) * sampleRate);
        cursor_ = static_cast<std::uint64_t>(std::ceil(sample / blockSamples)) * blockSamples;
    }
private:
    double origin_{};
    std::uint64_t cursor_{};
};

inline std::array<bool, bodyCount> cancelledStandaloneVoices(const EngineConfig& previous,
                                                             const EngineConfig& next) noexcept {
    const bool global = previous.voicingMode != next.voicingMode
        || previous.chordSystem.root != next.chordSystem.root
        || previous.chordSystem.scale != next.chordSystem.scale
        || previous.chordSystem.customScale != next.chordSystem.customScale
        || previous.chordStrumUnit != next.chordStrumUnit
        || previous.chordStrumValue != next.chordStrumValue
        || previous.chordStrumMilliseconds != next.chordStrumMilliseconds
        || previous.inputTranspose != next.inputTranspose
        || (next.inputGateEnabled && !next.inputGateOpen);
    std::array<bool, bodyCount> result{};
    for (std::size_t body = 0; body < bodyCount; ++body)
        result[body] = global || previous.voices[body] != next.voices[body];
    return result;
}

// The destination ledger records only messages actually handed to that output.
// Overlapping voices sharing a channel/key are counted, including repeated-ons.
class DeliveredNoteLedger {
public:
    void record(const MidiEvent& event) noexcept {
        if (event.channel < 1U || event.channel > 16U || event.data1 > 127U)
            return;
        auto& count = notes_[event.channel - 1U][event.data1];
        if (event.type == MidiEventType::NoteOn && event.data2 > 0U) {
            if (count < 255U) ++count;
        } else if (event.type == MidiEventType::NoteOff
                   || (event.type == MidiEventType::NoteOn && event.data2 == 0U)) {
            if (count != 0U) --count;
        }
    }
    void flush(EventSink sink) noexcept {
        for (std::size_t channel = 0; channel < notes_.size(); ++channel)
            for (std::size_t note = 0; note < notes_[channel].size(); ++note)
                while (notes_[channel][note] != 0U) {
                    sink.push({0, MidiEventType::NoteOff, static_cast<std::uint8_t>(channel + 1U),
                               static_cast<std::uint8_t>(note), 0, 0});
                    --notes_[channel][note];
                }
    }
    bool contains(const MidiEvent& event) const noexcept {
        return event.channel >= 1U && event.channel <= 16U && event.data1 <= 127U
            && notes_[event.channel - 1U][event.data1] != 0U;
    }
    std::size_t activeCount() const noexcept {
        std::size_t result{};
        for (const auto& channel : notes_)
            for (const auto count : channel) result += count;
        return result;
    }
private:
    std::array<std::array<std::uint8_t, 128>, 16> notes_{};
};

} // namespace threebs
