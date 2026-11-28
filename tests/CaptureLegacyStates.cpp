// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// This source is compiled ONLY against the pinned pre-repair source checkout.
#include "plugin/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    if (argc != 2) return EXIT_FAILURE;
    const juce::File directory(juce::String::fromUTF8(argv[1]));
    if (directory.createDirectory().failed()) return EXIT_FAILURE;
    for (int scenario = 0; scenario < 2; ++scenario) {
        threebs::ThreeBSProcessor processor;
        if (scenario != 0) {
            auto state = processor.currentUserConfiguration();
            state.initial.bodies[0].mass = 2.75;
            state.baseInitial.bodies[0].mass = 2.75;
            state.initial.bodies[1].position.z = 0.12345678;
            state.baseInitial.bodies[1].position.z = 0.12345678;
            state.engine.voices[0].channel = 9;
            state.engine.voices[0].root = 11;
            state.engine.voices[0].minimumNote = 41;
            state.engine.voices[0].maximumNote = 89;
            state.engine.voices[0].customScale = {true, false, false, true, false, false, true, true, false, false, true, false};
            state.engine.voices[0].ccLanes[0] = {true, threebs::PitchMapping::Speed, 74, 0.6};
            state.engine.voicingMode = threebs::VoicingMode::Strum;
            state.engine.chordStrumUnit = threebs::StrumUnit::Beats;
            state.engine.chordStrumValue = 0.125;
            state.densityPercent = 37.5;
            state.sync = false;
            state.presentation.notePaneMinimized = true;
            (void)processor.applyUserConfiguration(state);
            for (const auto& setting : {std::pair{"timeSigSource", 1.0F},
                                       std::pair{"timeSigNum", 6.0F}, std::pair{"timeSigDenom", 3.0F}}) {
                auto* parameter = processor.parameters().getParameter(setting.first);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
            }
        }
        processor.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> audio(2, 512);
        juce::MidiBuffer midi;
        midi.ensureSize(65536);
        for (int i = 0; i < 8; ++i) { midi.clear(); processor.processBlock(audio, midi); }
        const auto name = scenario == 0 ? juce::String("default") : juce::String("edited");
        juce::MemoryBlock binary;
        processor.getStateInformation(binary);
        if (!directory.getChildFile(name + ".bin").replaceWithData(binary.getData(), binary.getSize())
            || !directory.getChildFile(name + ".3bs").replaceWithText(
                threebs::serializeUserConfiguration(processor.currentUserConfiguration()))) return EXIT_FAILURE;
        std::cout << "Captured prior-executable host7/portable3 scenario " << name << '\n';
    }
    return EXIT_SUCCESS;
}
