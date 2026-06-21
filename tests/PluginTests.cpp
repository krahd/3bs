// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"
#include "ui/UserConfiguration.h"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

int failures{};

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void setParameter(threebs::ThreeBSProcessor& processor, const juce::String& id, float value) {
    if (auto* parameter = processor.parameters().getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float parameterValue(threebs::ThreeBSProcessor& processor, const juce::String& id) {
    if (auto* value = processor.parameters().getRawParameterValue(id))
        return value->load();
    return -1.0F;
}

struct CapturedMidi {
    int sample{};
    std::vector<std::uint8_t> bytes;
    bool operator==(const CapturedMidi&) const = default;
};

std::vector<CapturedMidi> renderBlocks(threebs::ThreeBSProcessor& processor, int blockCount) {
    std::vector<CapturedMidi> result;
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < blockCount; ++block) {
        processor.processBlock(audio, midi);
        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();
            result.push_back({block * 512 + metadata.samplePosition,
                              {message.getRawData(), message.getRawData() + message.getRawDataSize()}});
        }
        midi.clear();
    }
    return result;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juce;
    threebs::ThreeBSProcessor processor;
    processor.setPlayConfigDetails(0, 2, 48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

    auto fileConfiguration = processor.currentUserConfiguration();
    fileConfiguration.initial.bodies[0].position = {1.25, -2.5, 3.75};
    fileConfiguration.engine.voicingMode = threebs::VoicingMode::Strum;
    fileConfiguration.engine.voices[1].triggerMapping = threebs::TriggerMapping::Apsis;
    fileConfiguration.engine.voices[1].octave = -2;
    fileConfiguration.engine.voices[1].minimumDurationBeats = 0.77;
    fileConfiguration.engine.voices[1].maximumDurationBeats = 0.77;
    fileConfiguration.engine.chordSystem.root = 5;
    fileConfiguration.engine.chordStrumUnit = threebs::StrumUnit::BarFraction;
    fileConfiguration.presentation.camera.yaw = 0.73F;
    fileConfiguration.planeTilts = {12.0, -8.0, 4.0};
    const auto configurationJson = threebs::serializeUserConfiguration(fileConfiguration);
    threebs::UserConfiguration decodedConfiguration;
    juce::String configurationError;
    check(threebs::deserializeUserConfiguration(configurationJson, decodedConfiguration,
                                                configurationError),
          ".3bs JSON configuration must round trip");
    check(std::abs(decodedConfiguration.initial.bodies[0].position.y + 2.5) < 1.0e-9
              && decodedConfiguration.engine.voicingMode == threebs::VoicingMode::Strum
              && decodedConfiguration.engine.voices[1].triggerMapping == threebs::TriggerMapping::Apsis
              && decodedConfiguration.engine.voices[1].octave == -2
              && decodedConfiguration.engine.chordSystem.root == 5
              && decodedConfiguration.engine.chordStrumUnit == threebs::StrumUnit::BarFraction
              && std::abs(decodedConfiguration.presentation.camera.yaw - 0.73F) < 1.0e-6F,
          ".3bs JSON must preserve body, music, and presentation settings");
    check(!threebs::deserializeUserConfiguration("{\"format\":\"wrong\"}",
                                                 decodedConfiguration, configurationError),
          "malformed .3bs JSON must be rejected");
    processor.applyUserConfiguration(decodedConfiguration);
    check(std::abs(processor.currentUserConfiguration().engine.voices[1].maximumDurationBeats - 0.77) < 1.0e-9,
          "loaded hidden voice fields must survive a subsequent save");

    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    bool generatedNote{};
    bool audioWasSilent = true;
    for (int block = 0; block < 96 && !generatedNote; ++block) {
        audio.clear();
        audio.addSample(0, 0, 1.0F);
        midi.clear();
        processor.processBlock(audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
            if (audio.getMagnitude(channel, 0, audio.getNumSamples()) != 0.0F)
                audioWasSilent = false;
        }
        for (const auto metadata : midi)
            generatedNote = generatedNote || metadata.getMessage().isNoteOn();
    }
    check(audioWasSilent, "VST3 processor must produce silent audio");
    check(generatedNote, "processor must generate MIDI notes");
    bool visualizedNote{};
    threebs::NoteVisualizationEvent visualEvent;
    while (processor.noteVisualizationEvents().pop(visualEvent)) {
        visualizedNote = visualizedNote
            || (visualEvent.type == threebs::NoteVisualizationType::On
                && visualEvent.body < threebs::bodyCount && visualEvent.velocity > 0);
    }
    check(visualizedNote, "generated notes must retain source-body visualization metadata");

    midi.clear();
    processor.processBlockBypassed(audio, midi);
    bool noteOff{};
    for (const auto metadata : midi)
        noteOff = noteOff || metadata.getMessage().isNoteOff();
    check(noteOff, "bypass must emit active note-offs");

    auto presentation = processor.presentationState();
    presentation.camera.focusBody = 2;
    presentation.camera.yaw = 1.125F;
    presentation.camera.pitch = -0.42F;
    presentation.camera.distance = 9.5F;
    presentation.camera.minimumDistance = 1.8F;
    presentation.camera.maximumDistance = 64.0F;
    presentation.camera.autoFrame = false;
    presentation.visual.trailSeconds = 30.0F;
    presentation.visual.palette = threebs::PaletteId::Violet;
    presentation.visualSeed = 20260620;
    presentation.notePaneMinimized = true;
    processor.setPresentationState(presentation);
    processor.requestPlaneTilts({14.0, -18.0, 9.5});
    setParameter(processor, "voiceRoot1", 9.0F);
    setParameter(processor, "voiceRoot2", 10.0F);
    setParameter(processor, "voiceRoot3", 11.0F);
    setParameter(processor, "voicingMode", 2.0F);
    setParameter(processor, "chordStrum", 37.0F);

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    check(state.getSize() > 0, "processor state must serialize");
    threebs::ThreeBSProcessor restored;
    restored.setPlayConfigDetails(0, 2, 48000.0, 512);
    restored.prepareToPlay(48000.0, 512);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto restoredPresentation = restored.presentationState();
    check(restoredPresentation.camera.focusBody == 2, "camera focus must survive state recall");
    check(std::abs(restoredPresentation.camera.yaw - 1.125F) < 1.0e-6F
              && std::abs(restoredPresentation.camera.pitch + 0.42F) < 1.0e-6F
              && std::abs(restoredPresentation.camera.distance - 9.5F) < 1.0e-6F,
          "camera orbit and zoom must survive state recall");
    check(std::abs(restoredPresentation.camera.minimumDistance - 1.8F) < 1.0e-6F
              && std::abs(restoredPresentation.camera.maximumDistance - 64.0F) < 1.0e-6F
              && !restoredPresentation.camera.autoFrame,
          "camera framing settings must survive state recall");
    check(restoredPresentation.notePaneMinimized,
          "note-pane minimization must survive state recall");
    check(parameterValue(restored, "voiceRoot1") == 9.0F
              && parameterValue(restored, "voiceRoot2") == 10.0F
              && parameterValue(restored, "voiceRoot3") == 11.0F,
          "per-planet roots must survive schema-v5 recall");
    check(parameterValue(restored, "voicingMode") == 2.0F
              && parameterValue(restored, "chordStrum") == 37.0F,
          "chord mode controls must survive schema-v5 recall");
    check(restoredPresentation.visual.palette == threebs::PaletteId::Violet
              && restoredPresentation.visualSeed == 20260620,
          "planet appearance must survive state recall");
    const auto restoredTilts = restored.initialPlaneTilts();
    check(std::abs(restoredTilts[0] - 14.0) < 1.0e-6
              && std::abs(restoredTilts[1] + 18.0) < 1.0e-6
              && std::abs(restoredTilts[2] - 9.5) < 1.0e-6,
          "initial plane tilt controls must survive state recall");
    juce::MemoryBlock roundTrip;
    restored.getStateInformation(roundTrip);
    check(roundTrip.getSize() > 0, "processor state must deserialize and serialize again");

    auto legacyXml = juce::AudioProcessor::getXmlFromBinary(
        state.getData(), static_cast<int>(state.getSize()));
    check(legacyXml != nullptr, "schema-v5 state must decode as XML for migration test");
    if (legacyXml != nullptr) {
        legacyXml->setAttribute("schemaVersion", 3);
        legacyXml->removeAttribute("cameraMinimumDistance");
        legacyXml->removeAttribute("cameraMaximumDistance");
        legacyXml->removeAttribute("cameraAutoFrame");
        legacyXml->removeAttribute("notePaneMinimized");
        for (int index = legacyXml->getNumChildElements() - 1; index >= 0; --index) {
            auto* child = legacyXml->getChildElement(index);
            const auto id = child->getStringAttribute("id");
            if (id.startsWith("voiceRoot") || id == "voicingMode" || id == "chordStrum")
                legacyXml->removeChildElement(child, true);
        }
        juce::MemoryBlock legacyState;
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyState);
        threebs::ThreeBSProcessor legacyRestored;
        legacyRestored.setStateInformation(legacyState.getData(), static_cast<int>(legacyState.getSize()));
        const auto legacyPresentation = legacyRestored.presentationState();
        check(std::abs(legacyPresentation.camera.minimumDistance - 2.5F) < 1.0e-6F
                  && std::abs(legacyPresentation.camera.maximumDistance - 40.0F) < 1.0e-6F
                  && legacyPresentation.camera.autoFrame && !legacyPresentation.notePaneMinimized,
              "schema-v3 state must receive schema-v4 presentation defaults");
        check(parameterValue(legacyRestored, "voicingMode") == 0.0F
                  && parameterValue(legacyRestored, "chordStrum") == 24.0F,
              "schema-v3 state must receive schema-v5 chord defaults");
    }
    processor.requestReset();
    const auto originalSequence = renderBlocks(processor, 32);
    const auto restoredSequence = renderBlocks(restored, 32);
    check(originalSequence == restoredSequence, "state recall must reproduce the MIDI sequence");

    processor.releaseResources();
    if (failures == 0)
        std::cout << "All plugin tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
