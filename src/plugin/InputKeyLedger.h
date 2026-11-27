// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace threebs {

// Intrusive lists keep the most recently pressed held key and each channel's
// held keys. Note on/off is O(1); channel cleanup visits only its held keys.
// Across a callback, cleanup work is bounded by the initial held keys plus
// accepted presses, rather than all 2048 keys for every incoming release.
class InputKeyLedger {
public:
    InputKeyLedger() noexcept { clear(); }
    void clear() noexcept {
        nodes_ = {};
        channelHeads_.fill(none);
        latest_ = none;
        heldNotes_ = 0;
        transpose_ = 0;
    }
    int heldNotes() const noexcept { return heldNotes_; }
    std::int8_t transpose() const noexcept { return transpose_; }

    void noteOn(std::uint8_t channel, std::uint8_t note) noexcept {
        if (channel >= 16U || note >= 128U) return;
        const auto key = static_cast<std::uint16_t>(channel * 128U + note);
        auto& node = nodes_[key];
        if (node.count == 0) {
            node.channelPrevious = none;
            node.channelNext = channelHeads_[channel];
            if (node.channelNext != none) nodes_[node.channelNext].channelPrevious = key;
            channelHeads_[channel] = key;
        } else unlinkLatest(key);
        if (node.count < std::numeric_limits<std::uint16_t>::max()) {
            ++node.count;
            ++heldNotes_;
        }
        node.previous = none;
        node.next = latest_;
        if (latest_ != none) nodes_[latest_].previous = key;
        latest_ = key;
        transpose_ = static_cast<std::int8_t>(note % 12U);
    }
    void noteOff(std::uint8_t channel, std::uint8_t note) noexcept {
        if (channel >= 16U || note >= 128U) return;
        const auto key = static_cast<std::uint16_t>(channel * 128U + note);
        auto& node = nodes_[key];
        if (node.count == 0) return;
        --node.count;
        --heldNotes_;
        if (node.count == 0) unlinkKey(key);
        updateTranspose();
    }
    void allNotesOff(std::uint8_t channel) noexcept {
        if (channel >= 16U) return;
        while (channelHeads_[channel] != none) {
            const auto key = channelHeads_[channel];
            heldNotes_ -= nodes_[key].count;
            nodes_[key].count = 0;
            unlinkKey(key);
        }
        updateTranspose();
    }

private:
    static constexpr std::uint16_t none = 2048;
    struct Node {
        std::uint16_t count{};
        std::uint16_t previous{none}, next{none};
        std::uint16_t channelPrevious{none}, channelNext{none};
    };
    void unlinkLatest(std::uint16_t key) noexcept {
        const auto& node = nodes_[key];
        if (node.previous == none) latest_ = node.next;
        else nodes_[node.previous].next = node.next;
        if (node.next != none) nodes_[node.next].previous = node.previous;
    }
    void unlinkKey(std::uint16_t key) noexcept {
        const auto& node = nodes_[key];
        unlinkLatest(key);
        if (node.channelPrevious == none) channelHeads_[key / 128U] = node.channelNext;
        else nodes_[node.channelPrevious].channelNext = node.channelNext;
        if (node.channelNext != none) nodes_[node.channelNext].channelPrevious = node.channelPrevious;
    }
    void updateTranspose() noexcept {
        // Hold the last transpose after the final release, preserving the
        // established instrument behaviour. Gate uses the independent count.
        if (latest_ != none) transpose_ = static_cast<std::int8_t>((latest_ % 128U) % 12U);
    }
    std::array<Node, 2048> nodes_{};
    std::array<std::uint16_t, 16> channelHeads_{};
    std::uint16_t latest_{none};
    int heldNotes_{};
    std::int8_t transpose_{};
};

} // namespace threebs
