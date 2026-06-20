// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"

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
    presentation.visual.trailSeconds = 30.0F;
    presentation.visual.palette = threebs::PaletteId::Violet;
    presentation.visualSeed = 20260620;
    processor.setPresentationState(presentation);

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
    check(restoredPresentation.visual.palette == threebs::PaletteId::Violet
              && restoredPresentation.visualSeed == 20260620,
          "planet appearance must survive state recall");
    juce::MemoryBlock roundTrip;
    restored.getStateInformation(roundTrip);
    check(roundTrip.getSize() > 0, "processor state must deserialize and serialize again");
    processor.requestReset();
    const auto originalSequence = renderBlocks(processor, 32);
    const auto restoredSequence = renderBlocks(restored, 32);
    check(originalSequence == restoredSequence, "state recall must reproduce the MIDI sequence");

    processor.releaseResources();
    if (failures == 0)
        std::cout << "All plugin tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
