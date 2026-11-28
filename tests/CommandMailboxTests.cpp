// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/LatestCommandMailbox.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {
struct Publication {
    std::uint64_t revision{};
    std::uint64_t resetRevision{};
    std::array<std::uint64_t, 128> payload{};
};
Publication makePublication(std::uint64_t revision) {
    Publication result;
    result.revision = revision;
    result.resetRevision = revision - revision % 17U;
    for (std::size_t i = 0; i < result.payload.size(); ++i)
        result.payload[i] = revision * 131U + i;
    return result;
}
bool valid(const Publication& value) {
    if (value.resetRevision != value.revision - value.revision % 17U) return false;
    for (std::size_t i = 0; i < value.payload.size(); ++i)
        if (value.payload[i] != value.revision * 131U + i) return false;
    return true;
}
}

int main() {
    threebs::LatestCommandMailbox<Publication> mailbox;
    Publication received;
    if (mailbox.consume(received)) return 1;
    // Saturation deliberately coalesces to the final complete publication,
    // including its most recent required reset.
    for (std::uint64_t revision = 1; revision <= 1024; ++revision)
        mailbox.publish(makePublication(revision));
    if (!mailbox.consume(received) || received.revision != 1024
        || !valid(received) || mailbox.consume(received)) return 1;

    constexpr std::uint64_t finalRevision = 251024;
    std::atomic<bool> finished{};
    std::thread producer([&] {
        for (std::uint64_t revision = 1025; revision <= finalRevision; ++revision)
            mailbox.publish(makePublication(revision));
        finished.store(true, std::memory_order_release);
    });
    std::uint64_t previous = 1024, consumed{};
    bool coherent = true;
    while (!finished.load(std::memory_order_acquire) || previous != finalRevision) {
        if (!mailbox.consume(received)) { std::this_thread::yield(); continue; }
        coherent = coherent && valid(received) && received.revision > previous;
        previous = received.revision;
        ++consumed;
    }
    producer.join();
    std::cout << "mailbox publications=250000 consumed=" << consumed
              << " final=" << previous << " coherent=" << coherent << '\n';
    return coherent ? 0 : 1;
}
