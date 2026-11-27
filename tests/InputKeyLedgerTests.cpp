// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#include "plugin/InputKeyLedger.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
// Deliberately simple scan-based reference, independent of the intrusive lists.
struct ReferenceKeys {
    std::array<std::uint16_t, 2048> counts{};
    std::array<std::uint64_t, 2048> order{};
    std::uint64_t serial{};
    int held{};
    std::int8_t transpose{};
    void on(std::uint8_t channel, std::uint8_t note) {
        const auto key = static_cast<std::size_t>(channel) * 128U + note;
        if (counts[key] < std::numeric_limits<std::uint16_t>::max()) { ++counts[key]; ++held; }
        order[key] = ++serial;
        transpose = static_cast<std::int8_t>(note % 12U);
    }
    void off(std::uint8_t channel, std::uint8_t note) {
        const auto key = static_cast<std::size_t>(channel) * 128U + note;
        if (counts[key] > 0) { --counts[key]; --held; }
        update();
    }
    void clearChannel(std::uint8_t channel) {
        for (std::size_t note = 0; note < 128; ++note) {
            auto& count = counts[channel * 128U + note];
            held -= count; count = 0;
        }
        update();
    }
    void update() {
        std::uint64_t latest{};
        for (std::size_t key = 0; key < counts.size(); ++key)
            if (counts[key] != 0 && order[key] > latest) {
                latest = order[key]; transpose = static_cast<std::int8_t>((key % 128U) % 12U);
            }
    }
};
bool inputLedgerTest() {
    threebs::InputKeyLedger ledger;
    ReferenceKeys reference;
    std::uint64_t random = 0x334253ULL;
    for (int event = 0; event < 100000; ++event) {
        random ^= random << 13U; random ^= random >> 7U; random ^= random << 17U;
        const auto channel = static_cast<std::uint8_t>(random & 15U);
        const auto note = static_cast<std::uint8_t>((random >> 4U) & 127U);
        const auto operation = (random >> 11U) % 7U;
        if (operation < 3U) { ledger.noteOn(channel, note); reference.on(channel, note); }
        else if (operation < 6U) { ledger.noteOff(channel, note); reference.off(channel, note); }
        else { ledger.allNotesOff(channel); reference.clearChannel(channel); }
        if (ledger.heldNotes() != reference.held || ledger.transpose() != reference.transpose) return false;
    }
    ledger.clear();
    for (int i = 0; i < 65537; ++i) ledger.noteOn(1, 65);
    if (ledger.heldNotes() != 65535 || ledger.transpose() != 5) return false;
    for (int i = 0; i < 65535; ++i) ledger.noteOff(1, 65);
    if (ledger.heldNotes() != 0 || ledger.transpose() != 5) return false;
    ledger.clear(); reference = {};
    ledger.noteOn(1, 65); reference.on(1, 65);
    constexpr int releases = 32768;
    const auto oldStart = std::chrono::steady_clock::now();
    for (int event = 0; event < releases; ++event)
        reference.off(0, static_cast<std::uint8_t>(event % 128));
    const auto oldEnd = std::chrono::steady_clock::now();
    for (int event = 0; event < releases; ++event)
        ledger.noteOff(0, static_cast<std::uint8_t>(event % 128));
    const auto newEnd = std::chrono::steady_clock::now();
    std::cout << "input ledger random_events=100000 dense_releases=" << releases
              << " scan_reference_us=" << std::chrono::duration<double, std::micro>(oldEnd - oldStart).count()
              << " linked_ledger_us=" << std::chrono::duration<double, std::micro>(newEnd - oldEnd).count()
              << " (portable diagnostic; not a native callback deadline)\n";
    return ledger.heldNotes() == reference.held && ledger.transpose() == reference.transpose;
}
}

int main() {
    if (inputLedgerTest()) return 0;
    std::cerr << "FAIL: MIDI input ledger\n";
    return 1;
}
