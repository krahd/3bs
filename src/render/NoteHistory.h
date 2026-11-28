// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/SnapshotQueue.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace threebs {

struct NoteHistoryEntry {
    double startTime{};
    double endTime{};
    std::uint8_t note{};
    std::uint8_t velocity{};
    bool active{};
    bool valid{};
};

// The producer stamps events and end-of-block snapshots with the same monotonic
// processing timeline. Events may arrive before their snapshot; retain them in
// this bounded presentation-only queue until the snapshot catches up. An epoch
// change discards old trajectories and a sequence gap clears uncertain voices.
template <std::size_t HistoryCapacity = 32, std::size_t PendingCapacity = 2048>
class NoteHistory {
    static_assert(HistoryCapacity > 0 && PendingCapacity > 0);
public:
    using History = std::array<std::array<NoteHistoryEntry, HistoryCapacity>, bodyCount>;

    bool append(const NoteVisualizationEvent& event) noexcept {
        if (event.body >= bodyCount || !std::isfinite(event.timelineSeconds))
            return false;
        if (lastSequence_ != 0 && event.sequence != 0) {
            if (event.sequence <= lastSequence_)
                return false;
            if (event.sequence != lastSequence_ + 1U) {
                droppedEvents_ += event.sequence - lastSequence_ - 1U;
                clearHistory();
                pendingCount_ = 0;
            }
        }
        lastSequence_ = event.sequence;
        if (pendingCount_ == PendingCapacity) {
            droppedEvents_ += pendingCount_;
            clearHistory();
            pendingCount_ = 0;
        }
        pending_[pendingCount_++] = event;
        return true;
    }

    void advance(const RenderSnapshot& snapshot, double displayNow) noexcept {
        if (!std::isfinite(snapshot.timelineSeconds) || !std::isfinite(displayNow))
            return;
        if (haveSnapshot_ && snapshot.trajectoryRevision < revision_)
            return;
        if (!haveSnapshot_ || snapshot.trajectoryRevision != revision_) {
            clearHistory();
            revision_ = snapshot.trajectoryRevision;
            haveSnapshot_ = true;
        }
        std::size_t retained{};
        for (std::size_t index = 0; index < pendingCount_; ++index) {
            const auto event = pending_[index];
            if (event.trajectoryRevision < revision_)
                continue;
            if (event.trajectoryRevision > revision_
                || event.timelineSeconds > snapshot.timelineSeconds + 1.0e-9) {
                pending_[retained++] = event;
                continue;
            }
            const auto displayTime = displayNow + event.timelineSeconds - snapshot.timelineSeconds;
            if (std::isfinite(displayTime))
                consume(event, displayTime);
        }
        pendingCount_ = retained;
    }

    const History& history() const noexcept { return history_; }
    std::size_t pendingCount() const noexcept { return pendingCount_; }
    std::uint64_t droppedEvents() const noexcept { return droppedEvents_; }

private:
    void clearHistory() noexcept {
        history_ = {};
        next_ = {};
    }

    void consume(const NoteVisualizationEvent& event, double displayTime) noexcept {
        auto& voice = history_[event.body];
        if (event.type == NoteVisualizationType::On) {
            for (auto& entry : voice) {
                if (entry.valid && entry.active) {
                    entry.endTime = std::max(entry.startTime, displayTime);
                    entry.active = false;
                }
            }
            voice[next_[event.body]] = {displayTime, displayTime, event.note, event.velocity, true, true};
            next_[event.body] = (next_[event.body] + 1U) % HistoryCapacity;
        } else {
            for (auto& entry : voice) {
                if (entry.valid && entry.active && entry.note == event.note) {
                    entry.endTime = std::max(entry.startTime, displayTime);
                    entry.active = false;
                }
            }
        }
    }

    History history_{};
    std::array<std::size_t, bodyCount> next_{};
    std::array<NoteVisualizationEvent, PendingCapacity> pending_{};
    std::size_t pendingCount_{};
    std::uint64_t lastSequence_{};
    std::uint64_t revision_{};
    std::uint64_t droppedEvents_{};
    bool haveSnapshot_{};
};

} // namespace threebs
