// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#include "standalone/StandaloneTimeline.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>

namespace {
int failures{};
void check(bool condition, const char* name) {
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (!condition) ++failures;
}
void clockTest() {
    threebs::StandaloneTimeline clock(100.0);
    // Simulate thirty minutes of uneven worker wakeups. UI timing is absent.
    double now = 100.0;
    std::uint64_t blocks{};
    while (now < 1900.0) {
        now = std::min(1900.0, now + (blocks % 17U == 0U ? 0.017 : 0.003));
        while (clock.blockDue(now)) { clock.advance(); ++blocks; }
    }
    check(std::abs(clock.seconds() - 1800.0) <= 0.00200001,
          "30-minute absolute sample timeline has no accumulated wakeup drift");
    check(std::abs(clock.deadline(48000U) - 101.020) < 1.0e-10,
          "sample deadline includes fixed look-ahead exactly once");
    check(!clock.isLate(now), "bounded worker jitter does not fault");
    now += 1.0;
    check(clock.isLate(now), "unbounded late wakeup is detected");
    clock.recoverAt(now);
    check(!clock.isLate(now) && clock.deadline(clock.cursor()) >= now,
          "late recovery never schedules cancelled history in the past");
}
void ledgerTest() {
    threebs::DeliveredNoteLedger ledger;
    ledger.record({0, threebs::MidiEventType::NoteOn, 1, 60, 100, 0});
    ledger.record({0, threebs::MidiEventType::NoteOn, 1, 60, 100, 1});
    ledger.record({0, threebs::MidiEventType::NoteOn, 16, 127, 100, 2});
    ledger.record({0, threebs::MidiEventType::NoteOff, 1, 60, 0, 0});
    check(ledger.activeCount() == 2U, "destination ledger counts overlapping channel/key identities");
    threebs::FixedEventBuffer<8> sent;
    ledger.flush(sent.sink());
    check(ledger.activeCount() == 0U && sent.size() == 2U
          && sent[0].channel == 1U && sent[0].data1 == 60U
          && sent[1].channel == 16U && sent[1].data1 == 127U,
          "reset/output replacement/quit cleanup targets actually delivered notes");
    ledger.flush(sent.sink());
    check(sent.size() == 2U, "repeated cleanup is idempotent");
}
void mailboxTest() {
    struct Payload { std::uint64_t revision{}; std::array<std::uint64_t, 128> values{}; };
    threebs::LatestValueMailbox<Payload> mailbox;
    std::atomic<bool> done{};
    std::thread producer([&] {
        Payload value;
        for (std::uint64_t i = 1; i <= 250000; ++i) {
            value.revision = i; value.values.fill(i); mailbox.publish(value);
        }
        done.store(true, std::memory_order_release);
    });
    std::uint64_t last{}; bool coherent = true;
    Payload value;
    while (!done.load(std::memory_order_acquire) || last < 250000) {
        if (!mailbox.consume(value)) { std::this_thread::yield(); continue; }
        coherent = coherent && value.revision > last;
        for (auto field : value.values) coherent = coherent && field == value.revision;
        last = value.revision;
    }
    producer.join();
    check(coherent && last == 250000, "saturated mailbox publishes coherent increasing revisions and final edit");
}

void pendingPanicTest() {
    using namespace threebs;
    EngineConfig config;
    config.voicingMode = VoicingMode::Strum;
    config.chordStrumMilliseconds = 100;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& voice = config.voices[body];
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.clockDivisionBeats = 16;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 8;
    }
    const auto initial = makeInitialState(InitialSystem::FigureEight, 42);
    const EventSink discardAfterDestinationFlush{nullptr, [](void*, const MidiEvent&) noexcept {}};
    for (const bool reset : {false, true}) {
        MusicEngine engine(initial, config);
        DeliveredNoteLedger destination;
        FixedEventBuffer<32> delivered;
        struct Delivery { DeliveredNoteLedger* ledger; FixedEventBuffer<32>* events; } delivery{&destination, &delivered};
        const EventSink send{&delivery, [](void* opaque, const MidiEvent& event) noexcept {
            auto& target = *static_cast<Delivery*>(opaque);
            target.ledger->record(event);
            (void)target.events->push(event);
        }};
        ProcessContext first; first.sampleCount = 1; first.transportStarted = true;
        engine.process(first, send);
        check(destination.activeCount() == 1U && delivered.size() == 1U,
              "runtime-style strum has one delivered note and two pending notes");

        // A generation/route epoch invalidates queued output. Only the delivery
        // owner flushes the physical destination; the engine then discards its
        // corresponding scheduling ledger through an explicit consuming sink.
        destination.flush(delivered.sink());
        engine.allNotesOff(0, discardAfterDestinationFlush);
        if (reset) (void)engine.reset(initial, discardAfterDestinationFlush);
        ProcessContext after; after.sampleCount = 12000; after.playing = !reset;
        engine.process(after, send);
        std::size_t noteOns{};
        for (const auto& event : delivered) noteOns += event.type == MidiEventType::NoteOn;
        check(noteOns == 1U && destination.activeCount() == 0U && !delivered.overflowed(),
              "epoch panic/reset contract cannot replay cancelled strum notes or strand delivered notes");
    }
}
}
int main() { clockTest(); ledgerTest(); mailboxTest(); pendingPanicTest(); return failures == 0 ? 0 : 1; }
