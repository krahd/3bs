// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "render/FrameResourcePool.h"
#include "render/NoteHistory.h"
#include "render/TrailHistory.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1.0e-8; }

void testFrameOwnership() {
    threebs::FrameResourcePool<3> pool;
    auto first = pool.tryAcquire();
    auto second = pool.tryAcquire();
    auto third = pool.tryAcquire();
    check(first && second && third, "all preallocated frame sets must be usable");
    check(first.index() != second.index() && first.index() != third.index()
              && second.index() != third.index(), "in-flight frames must never alias resources");
    check(!pool.tryAcquire(), "exhausted GPU resource pool must skip instead of reusing live memory");
    const auto gpuSlot = first.handOff();
    check(!pool.tryAcquire(), "committed resource remains owned until GPU completion");
    check(pool.complete(gpuSlot), "GPU completion releases exactly its frame resource");
    {
        auto abandoned = pool.tryAcquire();
        check(abandoned && abandoned.index() == gpuSlot, "only the completed slot may be reused");
    }
    auto recovered = pool.tryAcquire();
    check(static_cast<bool>(recovered), "early return before submission releases the CPU lease");
    auto moved = std::move(recovered);
    check(!recovered && moved, "moving a lease preserves single ownership");
}

void testConcurrentFrameCompletion() {
    threebs::FrameResourcePool<3> pool;
    struct Work { std::size_t slot{}; std::uint64_t generation{}; };
    threebs::SpscQueue<Work, 16> submitted;
    std::array<std::uint64_t, 3> payload{};
    std::atomic<bool> correct{true};
    constexpr std::uint64_t iterations = 10000;
    std::thread gpu([&] {
        for (std::uint64_t consumed = 0; consumed < iterations;) {
            Work work;
            if (!submitted.pop(work)) {
                std::this_thread::yield();
                continue;
            }
            // Deliberately read twice before releasing ownership. A premature
            // reuse would race with this access and corrupt a generation.
            const auto before = payload[work.slot];
            std::this_thread::yield();
            if (before != work.generation || payload[work.slot] != work.generation)
                correct.store(false, std::memory_order_relaxed);
            if (!pool.complete(work.slot))
                correct.store(false, std::memory_order_relaxed);
            ++consumed;
        }
    });
    for (std::uint64_t generation = 1; generation <= iterations; ++generation) {
        auto lease = pool.tryAcquire();
        while (!lease) {
            std::this_thread::yield();
            lease = pool.tryAcquire();
        }
        const auto slot = lease.index();
        payload[slot] = generation;
        (void)lease.handOff();
        while (!submitted.push({slot, generation}))
            std::this_thread::yield();
    }
    gpu.join();
    check(correct.load() && pool.inFlight() == 0,
          "asynchronous completion must preserve data and release every submitted slot");
}

void testTrailRetentionAndTessellation() {
    for (const double duration : {5.0, 30.0, 60.0}) {
        for (const int fps : {30, 60, 240, 1000}) {
            for (const double angularSpeed : {0.2, 12.0}) {
                threebs::TrailHistory<4096> history;
                for (int index = 0; index <= fps * 120; ++index) {
                    const double time = static_cast<double>(index) / fps;
                    history.append({10.0 * std::cos(time * angularSpeed),
                                    10.0 * std::sin(time * angularSpeed), 0.0},
                                   time, 7, duration);
                }
                check(history.size() > 1 && history.size() <= 4096,
                      "long-running trails must keep a bounded history");
                const auto retained = 120.0 - history[0].time;
                const auto tolerance = 2.0 / fps + duration / 4094.0 + 0.01;
                check(retained <= duration + 1.0e-8 && retained >= duration - tolerance,
                      "trail duration must survive fast motion and high callback rates");
                std::array<threebs::TrailHistory<4096>::Sample, 8192> geometry;
                const auto count = history.tessellate(geometry);
                check(count >= 2 && count <= geometry.size(), "tessellation respects fixed geometry budget");
                check(near(geometry[0].time, history[0].time) && near(geometry[count - 1].time, 120.0),
                      "tessellation preserves the retained history and current endpoint");
                for (std::size_t i = 1; i < count; ++i)
                    check(geometry[i].time >= geometry[i - 1].time
                              && threebs::isFinite(geometry[i].position),
                          "smooth geometry remains finite and temporally ordered");
            }
        }
    }
    threebs::TrailHistory<64> sparse;
    sparse.append({0.0, 0.0, 0.0}, 0.0, 1);
    sparse.append({1.0, 0.0, 0.0}, 1.0, 1);
    std::array<threebs::TrailHistory<64>::Sample, 8> geometry;
    check(sparse.size() == 2 && sparse.tessellate(geometry) == 8,
          "smooth subdivisions belong to geometry rather than stored history");
    sparse.append({3.0, 0.0, 0.0}, 2.0, 2);
    check(sparse.size() == 1, "trajectory revision clears the old trail");
    sparse.append({std::numeric_limits<double>::infinity(), 0.0, 0.0}, 3.0, 2);
    check(sparse.size() == 1, "non-finite trail input must not reach geometry conversion");
}

threebs::NoteVisualizationEvent note(threebs::NoteVisualizationType type, std::uint8_t body,
                                      std::uint8_t pitch, std::uint64_t sequence,
                                      double time, std::uint64_t revision) {
    threebs::NoteVisualizationEvent result;
    result.type = type;
    result.body = body;
    result.note = pitch;
    result.velocity = 100;
    result.sequence = sequence;
    result.timelineSeconds = time;
    result.trajectoryRevision = revision;
    return result;
}

void testNoteTimelineAndEpochs() {
    using Type = threebs::NoteVisualizationType;
    threebs::NoteHistory<> history;
    threebs::RenderSnapshot snapshot;
    snapshot.trajectoryRevision = 1;
    snapshot.timelineSeconds = 10.0;
    history.append(note(Type::On, 0, 60, 1, 8.0, 1));
    history.append(note(Type::Off, 0, 60, 2, 8.25, 1));
    history.append(note(Type::On, 1, 64, 3, 9.0, 1));
    history.advance(snapshot, 100.0);
    const auto& first = history.history()[0][0];
    const auto& second = history.history()[1][0];
    check(first.valid && !first.active && near(first.startTime, 98.0)
              && near(first.endTime, 98.25) && near(second.startTime, 99.0),
          "queued notes preserve onset spacing and duration after a rendering stall");
    history.append(note(Type::On, 2, 67, 4, 11.0, 2));
    history.advance(snapshot, 100.2);
    check(history.pendingCount() == 1 && !history.history()[2][0].valid,
          "future-epoch event waits for its matching snapshot");
    history.append(note(Type::On, 0, 36, 5, 9.5, 1));
    snapshot.trajectoryRevision = 2;
    snapshot.timelineSeconds = 12.0;
    history.advance(snapshot, 102.0);
    check(!history.history()[0][0].valid && !history.history()[1][0].valid
              && history.history()[2][0].valid && near(history.history()[2][0].startTime, 101.0),
          "reset drops stale queued notes while retaining the new trajectory");
    history.append(note(Type::Off, 2, 67, 7, 12.0, 2));
    history.advance(snapshot, 102.0);
    check(history.droppedEvents() == 1 && !history.history()[2][0].valid,
          "lost visual events clear uncertain active notes instead of leaving them stuck");

    threebs::NoteHistory<2, 2> bounded;
    bounded.append(note(Type::On, 0, 60, 1, 20.0, 3));
    bounded.append(note(Type::On, 0, 61, 2, 21.0, 3));
    bounded.append(note(Type::On, 0, 62, 3, 22.0, 3));
    snapshot.trajectoryRevision = 3;
    snapshot.timelineSeconds = 23.0;
    bounded.advance(snapshot, 30.0);
    check(bounded.pendingCount() == 0 && bounded.droppedEvents() == 2
              && bounded.history()[0][0].note == 62,
          "pending visual events have bounded, explicit overflow recovery");
}
} // namespace

int main() {
    try {
        testFrameOwnership();
        testConcurrentFrameCompletion();
        testTrailRetentionAndTessellation();
        testNoteTimelineAndEpochs();
        std::cout << "Renderer ownership, trails and note timeline tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
