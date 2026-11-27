// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"
#include "ui/ArtworkPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace {
using namespace threebs;
int failures{};
void check(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void setParameter(ThreeBSProcessor& processor, const char* id, float value) {
    if (auto* parameter = processor.parameters().getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
struct Message {
    int sample{};
    std::array<juce::uint8, 3> data{};
    bool operator==(const Message&) const = default;
};
struct Host final : juce::AudioPlayHead {
    PositionInfo position;
    juce::Optional<PositionInfo> getPosition() const override { return position; }
};
std::unique_ptr<ThreeBSProcessor> makeProcessor(double rate = 48000.0) {
    auto processor = std::make_unique<ThreeBSProcessor>();
    processor->setPlayConfigDetails(0, 2, rate, 16384);
    processor->prepareToPlay(rate, 16384);
    return processor;
}
UserConfiguration clockConfiguration(ThreeBSProcessor& processor, bool gate = false) {
    auto configuration = processor.currentUserConfiguration();
    configuration.sync = false;
    configuration.run = true;
    configuration.engine.simulation.speed = 0.0;
    configuration.engine.inputGateEnabled = gate;
    configuration.engine.inputTransposeEnabled = false;
    configuration.engine.voicingMode = VoicingMode::Independent;
    configuration.engine.autoResetEnabled = false;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& voice = configuration.engine.voices[body];
        voice.enabled = body == 0;
        voice.channel = static_cast<std::uint8_t>(body + 1U);
        voice.triggerMapping = TriggerMapping::Clock;
        voice.clockDivisionBeats = 1.0 / 128.0;
        voice.minimumTriggerIntervalBeats = 0.0;
        voice.probability = 1.0;
        voice.durationGrid = DurationGrid::ContinuousLegacy;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 0.25;
        for (auto& lane : voice.ccLanes) lane.enabled = false;
    }
    return configuration;
}
std::vector<Message> renderTimeline(double rate, int blockSize, bool strum = false,
                                    double bpm = 120.0, bool transpose = false) {
    Host host;
    host.position.setBpm(bpm);
    host.position.setIsPlaying(true);
    host.position.setTimeSignature(juce::AudioPlayHead::TimeSignature{4, 4});
    auto processor = makeProcessor(rate);
    processor->setPlayHead(&host);
    auto configuration = clockConfiguration(*processor, !transpose);
    configuration.sync = true;
    configuration.engine.inputTransposeEnabled = transpose;
    if (strum) {
        configuration.engine.voicingMode = VoicingMode::Strum;
        configuration.engine.chordStrumMilliseconds = 120.0;
        configuration.engine.minimumChordIntervalBeats = 1.0;
        for (auto& voice : configuration.engine.voices) voice.enabled = true;
    }
    if (bpm > 120.0) {
        configuration.engine.inputGateEnabled = false;
        for (auto& voice : configuration.engine.voices) {
            voice.enabled = true;
            voice.minimumDurationBeats = voice.maximumDurationBeats = 1.0 / 1024.0;
        }
    }
    check(processor->applyUserConfiguration(configuration), "timeline configuration must be accepted");
    const std::array<Message, 13> input{{
        {100, {0x90, 60, 100}}, {900, {0x80, 60, 0}},
        {1100, {0x90, 60, 100}}, {1400, {0x91, 67, 100}},
        {1500, {0x80, 61, 0}}, {2000, {0xB0, 123, 0}},
        {2400, {0x81, 67, 0}}, {3000, {0x90, 60, 100}},
        {3001, {0x90, 60, 100}}, {3500, {0x90, 60, 0}},
        {4000, {0x80, 60, 0}}, {5000, {0x90, 65, 100}},
        {5100, {0xB0, 120, 0}}
    }};
    constexpr int length = 16384;
    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    midi.ensureSize(4 * 1024 * 1024);
    std::vector<Message> result;
    for (int offset = 0; offset < length;) {
        const auto frames = std::min(blockSize, length - offset);
        audio.setSize(2, frames, false, false, true);
        midi.clear();
        host.position.setPpqPosition(static_cast<double>(offset) * bpm / (60.0 * rate));
        for (const auto& event : input)
            if (event.sample >= offset && event.sample < offset + frames)
                midi.addEvent(event.data.data(), 3, event.sample - offset);
        processor->processBlock(audio, midi);
        for (const auto metadata : midi) {
            check(metadata.numBytes == 3 && metadata.samplePosition >= 0
                      && metadata.samplePosition < frames, "generated MIDI has valid in-block offsets");
            if (metadata.numBytes == 3)
                result.push_back({offset + metadata.samplePosition,
                    {metadata.data[0], metadata.data[1], metadata.data[2]}});
        }
        offset += frames;
    }
    check(processor->realtimeFaults() == 0, "valid timeline does not raise an RT input fault");
    return result;
}
void inputAndStreamingTests() {
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        const auto reference = renderTimeline(rate, 16384);
        check(!reference.empty(), "same-block gate press/release must generate notes");
        for (const int block : {1, 32, 64, 127, 128, 512, 1024, 2048})
            check(renderTimeline(rate, block) == reference,
                  "sample-offset gate/key ledger must be independent of callback partition");
        bool firstGateOff{}, channelGateStillOpen{}, repeatedKeyStillOpen{};
        for (const auto& event : reference) {
            const bool on = (event.data[0] & 0xF0U) == 0x90U;
            if (on) {
                const bool open = (event.sample >= 100 && event.sample < 900)
                    || (event.sample >= 1100 && event.sample < 2400)
                    || (event.sample >= 3000 && event.sample < 4000)
                    || (event.sample >= 5000 && event.sample < 5100);
                check(open, "closed gate must not emit generated notes");
                channelGateStillOpen |= event.sample >= 2000 && event.sample < 2400;
                repeatedKeyStillOpen |= event.sample >= 3500 && event.sample < 4000;
            } else firstGateOff |= event.sample == 900;
        }
        check(firstGateOff, "gate closure emits the active note-off at the input offset");
        check(channelGateStillOpen, "CC123 clears its input channel without releasing another channel");
        check(repeatedKeyStillOpen, "one release retains a duplicated held key");
    }
    check(renderTimeline(48000, 127, true) == renderTimeline(48000, 16384, true),
          "strum cancellation preserves sample-offset block invariance");
    check(renderTimeline(48000, 64, false, 120, true) == renderTimeline(48000, 16384, false, 120, true),
          "transpose follows the per-channel key ledger at sample offsets");
    const auto dense = renderTimeline(48000, 16384, false, 960);
    check(dense.size() > 1024, "dense host test must exceed the former event ceiling");
    check(dense == renderTimeline(48000, 127, false, 960),
          "streamed host output preserves every event above 1024 events per callback");
}
void unaffectedVoiceTest() {
    auto processor = makeProcessor();
    auto configuration = clockConfiguration(*processor);
    configuration.engine.inputTransposeEnabled = true;
    for (std::size_t body = 0; body < 2; ++body) {
        auto& voice = configuration.engine.voices[body];
        voice.enabled = true;
        voice.clockDivisionBeats = 16.0;
        voice.minimumDurationBeats = voice.maximumDurationBeats = 8.0;
    }
    check(processor->applyUserConfiguration(configuration), "two-voice configuration is accepted");
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    midi.ensureSize(65536);
    const std::array<juce::uint8, 3> input{0x90, 65, 100};
    midi.addEvent(input.data(), 3, 0);
    processor->processBlock(audio, midi);
    int notes{};
    for (const auto metadata : midi) notes += (metadata.data[0] & 0xF0U) == 0x90U;
    check(notes == 2, "unaffected-voice test establishes two active notes");
    auto advanced = processor->currentUserConfiguration().engine;
    advanced.voices[0].channel = 9;
    check(processor->applyAdvancedConfiguration(advanced), "single-voice advanced edit is accepted");
    midi.clear(); processor->processBlock(audio, midi);
    bool affectedOff{}, unaffectedOff{};
    for (const auto metadata : midi) {
        affectedOff |= metadata.data[0] == 0x80U;
        unaffectedOff |= metadata.data[0] == 0x81U;
    }
    check(affectedOff && !unaffectedOff,
          "advanced channel edit cleans its old channel and preserves an unaffected sounding voice");
    processor->requestPanic();
    midi.clear(); processor->processBlock(audio, midi);
    bool retainedOff{};
    for (const auto metadata : midi) retainedOff |= metadata.data[0] == 0x81U;
    check(retainedOff, "Panic confirms the unaffected voice retained its sounding-note ledger");
}

void portableValidationTests() {
    auto processor = makeProcessor();
    auto configuration = processor->currentUserConfiguration();
    configuration.initial.seed = configuration.baseInitial.seed = std::numeric_limits<std::uint64_t>::max();
    configuration.presentation.visualSeed = configuration.initial.seed;
    configuration.initial.bodies[0].mass = configuration.baseInitial.bodies[0].mass = 1000.0;
    configuration.engine.minimumChordIntervalBeats = 64.0;
    configuration.engine.voices[0].minimumDurationBeats = 32;
    configuration.engine.voices[0].maximumDurationBeats = 64;
    configuration.engine.voices[0].probability = 0.17;
    configuration.timeSignatureSource = TimeSignatureSource::Manual;
    for (const auto meter : {std::array<int, 2>{3, 4}, {5, 8}, {7, 8}}) {
        configuration.timeSignatureNumerator = meter[0]; configuration.timeSignatureDenominator = meter[1];
        UserConfiguration decoded;
        juce::String error;
        check(deserializeUserConfiguration(serializeUserConfiguration(configuration), decoded, error),
              "portable schema 4 must round trip the complete supported value range");
        check(decoded.initial.seed == configuration.initial.seed
                  && decoded.presentation.visualSeed == configuration.presentation.visualSeed
                  && decoded.timeSignatureSource == TimeSignatureSource::Manual
                  && decoded.timeSignatureNumerator == meter[0] && decoded.timeSignatureDenominator == meter[1]
                  && decoded.initial.bodies[0].mass == 1000.0
                  && decoded.engine.voices[0].maximumDurationBeats == 64.0,
              "portable round trip preserves unsigned seeds, exact mass/duration, and meter");
    }
    const auto validJson = serializeUserConfiguration(configuration);
    const auto reject = [&](const std::function<void(juce::var&)>& mutate) {
        auto source = juce::JSON::parse(validJson);
        mutate(source);
        auto output = configuration;
        juce::String error;
        const bool accepted = deserializeUserConfiguration(juce::JSON::toString(source), output, error);
        check(!accepted && error.isNotEmpty() && serializeUserConfiguration(output) == validJson,
              "invalid portable input must report failure without mutating the destination");
    };
    reject([](juce::var& v) { v.getDynamicObject()->setProperty("densityPercent", "80"); });
    reject([](juce::var& v) { v.getDynamicObject()->setProperty("densityPercent", juce::var{}); });
    reject([](juce::var& v) { v.getDynamicObject()->setProperty("run", 1); });
    reject([](juce::var& v) { v.getDynamicObject()->setProperty("schemaVersion", 4.5); });
    reject([](juce::var& v) { v.getDynamicObject()->setProperty("timeSignatureDenominator", 3); });
    reject([](juce::var& v) {
        v["initialState"].getDynamicObject()->setProperty("seed", "18446744073709551616");
    });
    reject([](juce::var& v) {
        v["initialState"]["bodies"].getArray()->getReference(0).getDynamicObject()->setProperty("mass", "NaN");
    });
    reject([](juce::var& v) {
        auto* object = v["engine"]["voices"].getArray()->getReference(0).getDynamicObject();
        object->setProperty("minimumNote", 80); object->setProperty("maximumNote", 40);
    });
    reject([](juce::var& v) {
        v["engine"]["voices"].getArray()->getReference(0).getDynamicObject()->setProperty("ccLanes", juce::var{});
    });
    reject([](juce::var& v) {
        auto* object = v["engine"]["voices"].getArray()->getReference(0).getDynamicObject();
        object->setProperty("scale", static_cast<int>(ScaleId::Custom));
        juce::Array<juce::var> mask;
        for (int i = 0; i < 12; ++i) mask.add(false);
        object->setProperty("customScale", mask);
    });
    for (int schema = 1; schema <= 3; ++schema) {
        auto source = juce::JSON::parse(validJson);
        source.getDynamicObject()->setProperty("schemaVersion", schema);
        for (const auto* name : {"timeSignatureSource", "timeSignatureNumerator", "timeSignatureDenominator"})
            source.getDynamicObject()->removeProperty(name);
        UserConfiguration output;
        juce::String error;
        check(deserializeUserConfiguration(juce::JSON::toString(source), output, error)
                  && output.timeSignatureSource == TimeSignatureSource::Host
                  && output.timeSignatureNumerator == 4 && output.timeSignatureDenominator == 4,
              "portable schemas 1-3 migrate to established host/4:4 meter defaults");
    }
}
void ownershipAndRecallTests() {
    auto processor = makeProcessor();
    auto configuration = clockConfiguration(*processor);
    configuration.initial.bodies[0].mass = configuration.baseInitial.bodies[0].mass = 1000.0;
    configuration.engine.voices[0].probability = 0.17;
    configuration.engine.voices[1].probability = 0.83;
    configuration.engine.voices[0].minimumDurationBeats = 16.0;
    configuration.engine.voices[0].maximumDurationBeats = 32.0;
    configuration.presentation.camera.yaw = 0.72F;
    configuration.presentation.notePaneMinimized = true;
    configuration.timeSignatureSource = TimeSignatureSource::Manual;
    configuration.timeSignatureNumerator = 7; configuration.timeSignatureDenominator = 8;
    check(processor->applyUserConfiguration(configuration), "exact configuration is accepted");
    juce::MemoryBlock state;
    processor->getStateInformation(state);
    auto restored = makeProcessor();
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(restored->lastStateError().isEmpty(), "current host state restores without a validation error");
    check(serializeUserConfiguration(restored->currentUserConfiguration())
              == serializeUserConfiguration(configuration), "restore is visible before the first processing callback");
    juce::MemoryBlock resaved;
    restored->getStateInformation(resaved);
    auto twice = makeProcessor();
    twice->setStateInformation(resaved.getData(), static_cast<int>(resaved.getSize()));
    check(serializeUserConfiguration(twice->currentUserConfiguration())
              == serializeUserConfiguration(restored->currentUserConfiguration()),
          "immediate restore/save preserves complete state before audio consumption");
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    midi.ensureSize(1024 * 1024);
    restored->processBlock(audio, midi);
    auto after = restored->currentUserConfiguration();
    check(after.initial.bodies[0].mass == 1000.0 && after.engine.voices[0].probability == 0.17
              && after.engine.voices[1].probability == 0.83 && after.engine.voices[0].maximumDurationBeats == 32.0,
          "callback preserves exact authored values beyond coarse controls");
    auto* mass = restored->parameters().getParameter("mass1");
    check(mass != nullptr && mass->convertFrom0to1(1.0F) == 8.0F,
          "published mass parameter keeps its original normalised automation mapping");
    setParameter(*restored, "mass1", 4.0F);
    setParameter(*restored, "density", 50.0F);
    midi.clear(); restored->processBlock(audio, midi);
    after = restored->currentUserConfiguration();
    check(std::abs(after.initial.bodies[0].mass - 4.0) < 0.001
              && after.engine.voices[0].probability == 0.5 && after.engine.voices[1].probability == 0.5,
          "changing a coarse control explicitly takes ownership of its authored value");
    // Queue saturation cannot lose the final accepted state or an earlier reset.
    for (int index = 0; index < 256; ++index) {
        configuration.initial.bodies[0].mass = configuration.baseInitial.bodies[0].mass = 9.0 + index;
        configuration.initial.seed = configuration.baseInitial.seed = static_cast<std::uint64_t>(1000 + index);
        check(processor->applyUserConfiguration(configuration), "every coalesced configuration is accepted");
        if ((index % 5) == 0) processor->requestReset();
    }
    midi.clear(); processor->processBlock(audio, midi);
    check(processor->appliedConfigurationRevision() == processor->configurationRevision(),
          "callback applies the latest accepted revision after command saturation");
    RenderSnapshot latest;
    while (processor->snapshots().pop(latest)) {}
    check(latest.bodies[0].mass == 264.0, "latest accepted exact mass reaches the simulation after coalescing");
    for (int block = 0; block < 96; ++block) { midi.clear(); processor->processBlock(audio, midi); }
    check(processor->droppedSnapshots() > 0, "slow visual consumer uses observable drop-new policy");
    std::uint64_t sequence{};
    while (processor->snapshots().pop(latest)) {
        check(latest.sequence > sequence, "snapshot consumer sees monotonic complete records");
        sequence = latest.sequence;
    }
    // A stale editor cache cannot overwrite a newer host recall.
    const auto staleRevision = restored->configurationRevision();
    const auto stalePresentation = restored->presentationState();
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(!restored->setPresentationState(stalePresentation, staleRevision),
          "presentation writes from an old configuration revision are rejected");
    std::unique_ptr<juce::AudioProcessorEditor> editor(restored->createEditor());
    auto changed = configuration;
    changed.presentation.camera.yaw = 1.125F;
    changed.presentation.notePaneMinimized = false;
    changed.presentation.notePaneStyle = NotePaneStyle::Vertical;
    check(processor->applyUserConfiguration(changed), "editor recall source is accepted");
    processor->getStateInformation(state);
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    // Exercise the real timer path on the message thread, without a manual UI.
    juce::Thread::sleep(80);
    juce::Timer::callPendingTimersSynchronously();
    ArtworkPanel* panel{};
    for (int child = 0; child < editor->getNumChildComponents(); ++child)
        if (auto* candidate = dynamic_cast<ArtworkPanel*>(editor->getChildComponent(child))) panel = candidate;
    check(panel != nullptr && std::abs(panel->presentationState().camera.yaw - 1.125F) < 1.0e-6F
              && !panel->presentationState().notePaneMinimized
              && panel->presentationState().notePaneStyle == NotePaneStyle::Vertical,
          "already-open editor refreshes presentation after host recall");
    check(std::abs(restored->presentationState().camera.yaw - 1.125F) < 1.0e-6F,
          "editor timer does not write stale presentation back into restored state");
}
void hostValidationAndMigrationTests() {
    auto processor = makeProcessor();
    juce::MemoryBlock source;
    processor->getStateInformation(source);
    const auto original = serializeUserConfiguration(processor->currentUserConfiguration());
    const auto revision = processor->configurationRevision();
    const auto reject = [&](const std::function<void(juce::XmlElement&)>& mutate) {
        auto xml = juce::AudioProcessor::getXmlFromBinary(source.getData(), static_cast<int>(source.getSize()));
        check(xml != nullptr, "host fixture must decode");
        if (xml == nullptr) return;
        mutate(*xml);
        juce::MemoryBlock corrupted;
        juce::AudioProcessor::copyXmlToBinary(*xml, corrupted);
        processor->setStateInformation(corrupted.getData(), static_cast<int>(corrupted.getSize()));
        check(processor->configurationRevision() == revision && processor->lastStateError().isNotEmpty()
                  && serializeUserConfiguration(processor->currentUserConfiguration()) == original,
              "malformed host state is rejected atomically before publication");
    };
    reject([](juce::XmlElement& xml) { xml.setAttribute("body0_px", "NaN"); });
    reject([](juce::XmlElement& xml) { xml.setAttribute("body0_px", "1e99999999999999999999"); });
    reject([](juce::XmlElement& xml) { xml.setAttribute("body1_mass", "1.0junk"); });
    reject([](juce::XmlElement& xml) { xml.setAttribute("schemaVersion", "8.5"); });
    reject([](juce::XmlElement& xml) { xml.setAttribute("schemaVersion", 99); });
    reject([](juce::XmlElement& xml) { xml.setAttribute("cameraMinimumDistance", 19); xml.setAttribute("cameraMaximumDistance", 8); });
    reject([](juce::XmlElement& xml) { xml.setAttribute("configurationTemplateJson", "{}"); });
    reject([](juce::XmlElement& xml) {
        for (auto* child : xml.getChildIterator()) {
            if (child->getStringAttribute("id") == "voiceDurMin1") child->setAttribute("value", 7);
            if (child->getStringAttribute("id") == "voiceDurMax1") child->setAttribute("value", 1);
        }
    });
    reject([](juce::XmlElement& xml) {
        auto* duplicate = xml.createNewChildElement("PARAM");
        duplicate->setAttribute("id", "mass1"); duplicate->setAttribute("value", 1);
    });
    for (int schema = 1; schema <= 7; ++schema) {
        auto xml = juce::AudioProcessor::getXmlFromBinary(source.getData(), static_cast<int>(source.getSize()));
        xml->setAttribute("schemaVersion", schema);
        xml->removeAttribute("configurationTemplateJson");
        if (schema < 4)
            for (const auto* name : {"cameraMinimumDistance", "cameraMaximumDistance", "cameraAutoFrame", "notePaneMinimized"})
                xml->removeAttribute(name);
        for (auto* child : xml->getChildIterator())
            if (child->getStringAttribute("id") == "trail" && schema == 1) child->setAttribute("value", 0.5);
        juce::MemoryBlock migrated;
        juce::AudioProcessor::copyXmlToBinary(*xml, migrated);
        auto restored = makeProcessor();
        restored->setStateInformation(migrated.getData(), static_cast<int>(migrated.getSize()));
        const auto configuration = restored->currentUserConfiguration();
        check(restored->lastStateError().isEmpty(), "published host schema 1-7 fixture must migrate");
        if (schema < 7)
            check(configuration.engine.voices[0].durationGrid == DurationGrid::ContinuousLegacy,
                  "pre-v7 host duration grid migrates to continuous legacy");
        if (schema < 6)
            check(configuration.timeSignatureSource == TimeSignatureSource::Host
                      && configuration.timeSignatureNumerator == 4 && configuration.timeSignatureDenominator == 4,
                  "pre-v6 host meter migrates to host 4:4 defaults");
        if (schema == 1)
            check(std::abs(configuration.presentation.visual.trailSeconds - 20.0F) < 1.0e-6F,
                  "schema 1 normalised trail migrates to seconds");
    }
}
void transportAndOverflowTests() {
    Host host;
    host.position.setBpm(120.0); host.position.setIsPlaying(true);
    host.position.setPpqPosition(0.0);
    auto processor = makeProcessor(); processor->setPlayHead(&host);
    auto configuration = clockConfiguration(*processor);
    configuration.sync = true; configuration.loopPolicy = LoopPolicy::Continue;
    configuration.engine.voices[0].clockDivisionBeats = 1.0;
    configuration.engine.voices[0].minimumDurationBeats = 2.0;
    configuration.engine.voices[0].maximumDurationBeats = 2.0;
    check(processor->applyUserConfiguration(configuration), "transport test configuration accepted");
    juce::AudioBuffer<float> audio(2, 480);
    juce::MidiBuffer midi; midi.ensureSize(1024 * 1024);
    processor->processBlock(audio, midi);
    RenderSnapshot before, after;
    while (processor->snapshots().pop(before)) {}
    host.position.setIsLooping(true);
    host.position.setLoopPoints(juce::AudioPlayHead::LoopPoints{0.0, 0.02});
    host.position.setPpqPosition(0.0);
    midi.clear(); processor->processBlock(audio, midi);
    while (processor->snapshots().pop(after)) {}
    check(after.trajectoryRevision == before.trajectoryRevision,
          "verified host loop wrap obeys Continue without an accidental seek reset");
    host.position.setIsLooping(false); host.position.setPpqPosition(0.0);
    midi.clear(); processor->processBlock(audio, midi);
    while (processor->snapshots().pop(after)) {}
    check(after.trajectoryRevision > before.trajectoryRevision, "backwards seek creates a new trajectory");
    host.position.setIsPlaying(false);
    midi.clear(); processor->processBlock(audio, midi);
    bool off{};
    for (const auto metadata : midi) off |= (metadata.data[0] & 0xF0U) == 0x80U;
    check(off, "transport stop flushes active notes");
    host.position.setIsPlaying(true); host.position.setBpm(std::numeric_limits<double>::quiet_NaN());
    midi.clear(); processor->processBlock(audio, midi);
    check((processor->realtimeFaults() & 8U) != 0, "non-finite host timing is rejected before engine processing");
    auto overflowProcessor = makeProcessor();
    for (int index = 0; index < 32769; ++index) {
        const std::array<juce::uint8, 3> note{0x90, 60, 100};
        midi.addEvent(note.data(), 3, 0);
    }
    overflowProcessor->processBlock(audio, midi);
    check((overflowProcessor->realtimeFaults() & 4U) != 0 && midi.isEmpty(),
          "input record overflow takes explicit cleanup path without partially generating a block");
}
}

int runPluginRegressions() {
    inputAndStreamingTests();
    unaffectedVoiceTest();
    portableValidationTests();
    ownershipAndRecallTests();
    hostValidationAndMigrationTests();
    transportAndOverflowTests();
    if (failures == 0) std::cout << "All plugin reliability regressions passed\n";
    return failures;
}
