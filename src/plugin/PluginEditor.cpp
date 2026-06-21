// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginEditor.h"

#include "ui/AdvancedStateEditor.h"

#include <cmath>

namespace threebs {

ThreeBSEditor::ThreeBSEditor(ThreeBSProcessor& owner)
    : AudioProcessorEditor(owner), processor_(owner),
      panel_(owner.snapshots(), owner.noteVisualizationEvents()) {
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(900, 600, 1800, 1200);
    setSize(1200, 760);
    addAndMakeVisible(panel_);

    auto& state = processor_.parameters();
    runAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "run", panel_.runButton());
    syncAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "sync", panel_.syncButton());
    const auto masses = panel_.massSliders();
    std::array<std::pair<const char*, juce::Slider*>, 11> sliders{{
        {"speed", &panel_.speedSlider()}, {"gravity", &panel_.gravitySlider()},
        {"softening", &panel_.softeningSlider()}, {"chaos", &panel_.chaosSlider()},
        {"density", &panel_.densitySlider()}, {"trail", &panel_.trailSlider()},
        {"bloom", &panel_.bloomSlider()}, {"chordStrum", &panel_.chordStrumSlider()}, {"mass1", masses[0]},
        {"mass2", masses[1]}, {"mass3", masses[2]}}};
    for (std::size_t i = 0; i < sliders.size(); ++i)
        sliderAttachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, sliders[i].first, *sliders[i].second);

    const auto enables = panel_.voiceEnableButtons();
    const auto scales = panel_.voiceScaleSelectors();
    const auto roots = panel_.voiceRootSelectors();
    const auto pitches = panel_.voicePitchSelectors();
    const auto triggers = panel_.voiceTriggerSelectors();
    for (std::size_t body = 0; body < bodyCount; ++body) {
        const auto suffix = juce::String(body + 1);
        voiceEnableAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            state, "voiceEnabled" + suffix, *enables[body]);
        voiceScaleAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state, "voiceScale" + suffix, *scales[body]);
        voiceRootAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state, "voiceRoot" + suffix, *roots[body]);
        voicePitchAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state, "voicePitch" + suffix, *pitches[body]);
        voiceTriggerAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state, "voiceTrigger" + suffix, *triggers[body]);
    }

    voicingModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "voicingMode", panel_.voicingModeSelector());

    panel_.setPresetCatalog(presets_);
    panel_.onPresetSelected = [this](int index) {
        if (!presets_.valid() || index < 0 || static_cast<std::size_t>(index) >= presets_.size())
            return;
        const auto& preset = presets_[static_cast<std::size_t>(index)];
        presentation_ = preset.presentation;
        panel_.setPresentationState(presentation_);
        lastPlaneTilts_.fill(0.0);
        panel_.setPlaneTilts(lastPlaneTilts_);
        processor_.requestPreset(preset, index);
    };
    panel_.onRandomize = [this] { processor_.requestRandomize(panel_.chaosSlider().getValue() / 100.0); };
    panel_.onReset = [this] { processor_.requestReset(); };
    panel_.onCameraChanged = [this](const CameraState& camera) {
        presentation_.camera = camera;
        processor_.setPresentationState(presentation_);
        processor_.updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
    };
    panel_.onNotePaneMinimizedChanged = [this](bool minimized) {
        presentation_.notePaneMinimized = minimized;
        processor_.setPresentationState(presentation_);
        processor_.updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
    };
    panel_.onNotePaneStyleChanged = [this](NotePaneStyle style) {
        presentation_.notePaneStyle = style;
        processor_.setPresentationState(presentation_);
        processor_.updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
    };
    panel_.onAdvanced = [this] {
        const auto target = panel_.getLocalArea(&panel_.advancedButton(), panel_.advancedButton().getLocalBounds());
        showAdvancedStateEditor(processor_.currentInitialState(), target, panel_,
            [this](const SimulationState& editedState) {
                processor_.requestExactState(editedState);
                lastPlaneTilts_.fill(0.0);
                panel_.setPlaneTilts(lastPlaneTilts_);
            });
    };
    panel_.setMidiOutputAvailable(false);
    presentation_ = processor_.presentationState();
    panel_.setPresentationState(presentation_);
    lastPlaneTilts_ = processor_.initialPlaneTilts();
    panel_.setPlaneTilts(lastPlaneTilts_);
    startTimerHz(30);
}

ThreeBSEditor::~ThreeBSEditor() {
    stopTimer();
}

void ThreeBSEditor::resized() {
    panel_.setBounds(getLocalBounds());
}

void ThreeBSEditor::timerCallback() {
    presentation_.visual.trailSeconds = static_cast<float>(panel_.trailSlider().getValue());
    presentation_.visual.bloom = static_cast<float>(panel_.bloomSlider().getValue() / 100.0);
    presentation_.camera.minimumDistance = static_cast<float>(panel_.minimumCameraDistanceSlider().getValue());
    presentation_.camera.maximumDistance = static_cast<float>(panel_.maximumCameraDistanceSlider().getValue());
    presentation_.camera.autoFrame = panel_.autoFrameButton().getToggleState();
    presentation_.camera = sanitizedCameraState(presentation_.camera);
    panel_.setPresentationState(presentation_);
    processor_.setPresentationState(presentation_);
    const auto tilts = panel_.planeTilts();
    bool tiltChanged{};
    for (std::size_t body = 0; body < bodyCount; ++body)
        tiltChanged = tiltChanged || std::abs(tilts[body] - lastPlaneTilts_[body]) > 0.05;
    if (tiltChanged) {
        lastPlaneTilts_ = tilts;
        processor_.requestPlaneTilts(lastPlaneTilts_);
        processor_.updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
    }
    if (processor_.escapePromptMask() != 0)
        panel_.setStatus("BODY ESCAPED / RESET OR NEW SYSTEM");
    if (auto* preset = processor_.parameters().getRawParameterValue("preset"))
        panel_.setSelectedPresetIndex(static_cast<int>(std::lround(preset->load())));
}

} // namespace threebs
