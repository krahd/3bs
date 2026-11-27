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
void failedCleanupRetryTest() {
    using namespace threebs;
    DeliveredNoteLedger ledger;
    const MidiEvent first{0, MidiEventType::NoteOn, 1, 60, 100, 0};
    const MidiEvent second{0, MidiEventType::NoteOn, 2, 61, 100, 1};
    const MidiEvent last{0, MidiEventType::NoteOn, 16, 127, 100, 2};
    ledger.record(first); ledger.record(first); ledger.record(second); ledger.record(last);

    std::size_t attempts{};
    check(!ledger.tryFlush([&](const MidiEvent&) noexcept { ++attempts; return false; })
          && attempts == 1U && ledger.activeCount() == 4U,
          "failed output cleanup retains every delivered note and stops at the first rejection");

    FixedEventBuffer<8> accepted;
    attempts = 0;
    check(!ledger.tryFlush([&](const MidiEvent& event) noexcept {
        ++attempts;
        if (accepted.size() == 2U) return false;
        return accepted.push(event);
    }) && attempts == 3U && accepted.size() == 2U && ledger.activeCount() == 2U,
          "partial cleanup removes only note-offs accepted by the destination");
    check(!ledger.contains(first) && ledger.contains(second) && ledger.contains(last),
          "partial cleanup retains the original channel/key of each unsent note-off");

    attempts = 0;
    check(!ledger.tryFlush([&](const MidiEvent&) noexcept { ++attempts; return false; })
          && attempts == 1U && ledger.activeCount() == 2U && accepted.size() == 2U,
          "repeated output failure cannot forget remaining cleanup obligations");
    check(ledger.tryFlush([&](const MidiEvent& event) noexcept { return accepted.push(event); })
          && ledger.activeCount() == 0U && accepted.size() == 4U
          && accepted[0].channel == 1U && accepted[0].data1 == 60U
          && accepted[1].channel == 1U && accepted[1].data1 == 60U
          && accepted[2].channel == 2U && accepted[2].data1 == 61U
          && accepted[3].channel == 16U && accepted[3].data1 == 127U,
          "reconnection retries exactly the outstanding note-offs without resending accepted cleanup");
    attempts = 0;
    check(ledger.tryFlush([&](const MidiEvent&) noexcept { ++attempts; return false; }) && attempts == 0U,
          "a fully cleaned destination needs no further send attempt");
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

void voiceCancellationTest() {
    using namespace threebs;
    EngineConfig before;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        before.voices[body].channel = static_cast<std::uint8_t>(body + 1U);
        before.voices[body].clockDivisionBeats = 16;
        before.voices[body].minimumDurationBeats = before.voices[body].maximumDurationBeats = 8;
    }
    const std::array<bool, bodyCount> none{}, all{true, true, true};
    check(cancelledStandaloneVoices(before, before) == none,
          "unchanged controls preserve all scheduled voice epochs");
    auto next = before;
    next.simulation.gravitationalConstant = 2;
    check(cancelledStandaloneVoices(before, next) == none,
          "physical controls preserve existing note lifetimes");
    next = before;
    next.voices[1].enabled = false;
    check(cancelledStandaloneVoices(before, next) == std::array<bool, bodyCount>{false, true, false},
          "disabling one voice cancels only that voice's scheduled epoch");
    next = before;
    next.voices[0].channel = 16;
    const auto channelMask = cancelledStandaloneVoices(before, next);
    check(channelMask == std::array<bool, bodyCount>{true, false, false},
          "channel reassignment cancels only the reassigned voice");

    // Exercise the mask with real sounding identities. The destination owner
    // flushes only the changed voice; the engine's subsequent cleanup remains
    // addressed to its old channel and must not terminate unrelated voices.
    MusicEngine engine(makeInitialState(InitialSystem::FigureEight, 42), before);
    std::array<DeliveredNoteLedger, bodyCount> destinations;
    struct Delivery { std::array<DeliveredNoteLedger, bodyCount>* ledgers; } delivery{&destinations};
    const EventSink send{&delivery, [](void* opaque, const MidiEvent& event) noexcept {
        auto& target = *static_cast<Delivery*>(opaque);
        (*target.ledgers)[event.sourceBody].record(event);
    }};
    ProcessContext first; first.sampleCount = 1; first.transportStarted = true;
    engine.process(first, send);
    check(destinations[0].activeCount() == 1U && destinations[1].activeCount() == 1U
          && destinations[2].activeCount() == 1U,
          "independent voices establish distinct delivered note identities");
    FixedEventBuffer<16> cleanup;
    for (std::size_t body = 0; body < bodyCount; ++body)
        if (channelMask[body]) destinations[body].flush(cleanup.sink());
    check(cleanup.size() == 1U && cleanup[0].channel == 1U
          && destinations[0].activeCount() == 0U && destinations[1].activeCount() == 1U
          && destinations[2].activeCount() == 1U,
          "per-voice destination cleanup uses the old channel and preserves unaffected notes");
    check(engine.setConfig(next), "channel reassignment is accepted by the engine");
    ProcessContext after; after.sampleCount = 1; after.beatAtStart = first.beatsPerSample;
    FixedEventBuffer<16> generated;
    engine.process(after, generated.sink());
    check(generated.size() == 1U && generated[0].type == MidiEventType::NoteOff
          && generated[0].sourceBody == 0U && generated[0].channel == 1U,
          "engine reconfiguration emits only the changed voice's original-channel cleanup");

    next = before;
    next.voicingMode = VoicingMode::Chord;
    check(cancelledStandaloneVoices(before, next) == all,
          "mode transition invalidates every queued voicing event");
    next = before;
    next.inputGateEnabled = true;
    next.inputGateOpen = true;
    check(cancelledStandaloneVoices(before, next) == none,
          "enabling an open gate preserves sounding voices");
    auto closed = next;
    closed.inputGateOpen = false;
    check(cancelledStandaloneVoices(next, closed) == all,
          "gate closure invalidates every queued voice event");
    check(cancelledStandaloneVoices(closed, next) == none,
          "gate reopening does not cancel newly eligible voice epochs");
    next = before;
    next.chordSystem.root = 2;
    check(cancelledStandaloneVoices(before, next) == all,
          "global chord-frame edits invalidate the complete queued voicing");
}
}
int main() {
    clockTest(); ledgerTest(); failedCleanupRetryTest(); mailboxTest(); pendingPanicTest(); voiceCancellationTest();
    return failures == 0 ? 0 : 1;
}
