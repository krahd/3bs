// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/ArtworkPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

namespace threebs {

class ThreeBSEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ThreeBSEditor(ThreeBSProcessor& owner);
    ~ThreeBSEditor() override;
    void resized() override;

private:
    void timerCallback() override;

    ThreeBSProcessor& processor_;
    ArtworkPanel panel_;
    PresetCatalog presets_;
    PresentationState presentation_{};
    std::array<double, bodyCount> lastPlaneTilts_{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 11> sliderAttachments_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voicingModeAttachment_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, bodyCount> voiceEnableAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, bodyCount> voiceScaleAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, bodyCount> voiceRootAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, bodyCount> voicePitchAttachments_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, bodyCount> voiceTriggerAttachments_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreeBSEditor)
};

} // namespace threebs
