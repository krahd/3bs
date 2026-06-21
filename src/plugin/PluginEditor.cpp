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
    const auto octaves = panel_.voiceOctaveSelectors();
    const auto durMaps = panel_.voiceDurationMapSelectors();
    const auto durGrids = panel_.voiceDurationGridSelectors();
    const auto durMins = panel_.voiceDurationMinSliders();
    const auto durMaxs = panel_.voiceDurationMaxSliders();
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
        voiceOctaveAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state, "voiceOctave" + suffix, *octaves[body]);
        voiceDurMapAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state, "voiceDurMap" + suffix, *durMaps[body]);
        voiceDurGridAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            state, "voiceDurGrid" + suffix, *durGrids[body]);
        voiceDurMinAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "voiceDurMin" + suffix, *durMins[body]);
        voiceDurMaxAttachments_[body] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, "voiceDurMax" + suffix, *durMaxs[body]);
    }

    voicingModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "voicingMode", panel_.voicingModeSelector());
    strumValueAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "strumValue", panel_.strumValueSlider());
    strumUnitAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "strumUnit", panel_.strumUnitSelector());
    chordRootAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "chordRoot", panel_.chordRootSelector());
    chordScaleAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "chordScale", panel_.chordScaleSelector());
    timeSigSourceAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "timeSigSource", panel_.timeSigSourceSelector());
    timeSigNumAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "timeSigNum", panel_.timeSigNumeratorSelector());
    timeSigDenomAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "timeSigDenom", panel_.timeSigDenominatorSelector());
    autoResetAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, "autoReset", panel_.autoResetButton());
    autoResetBarsAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, "autoResetBars", panel_.autoResetBarsSelector());

    panel_.setPresetCatalog(presets_);
    panel_.setVoicingPresetCatalog(voicingPresets_);
    panel_.onPresetSelected = [this](int index) {
        if (!presets_.valid() || index < 0 || static_cast<std::size_t>(index) >= presets_.size())
            return;
        const auto& preset = presets_[static_cast<std::size_t>(index)];
        presentation_ = preset.presentation;
        panel_.setPresentationState(presentation_);
        lastPlaneTilts_.fill(0.0);
        panel_.setPlaneTilts(lastPlaneTilts_);
        panel_.setSelectedVoicingPresetIndex(-1);
        processor_.requestPreset(preset, index);
    };
    panel_.onVoicingPresetSelected = [this](int index) {
        if (!voicingPresets_.valid() || index < 0
            || static_cast<std::size_t>(index) >= voicingPresets_.size())
            return;
        processor_.requestVoicingPreset(voicingPresets_[static_cast<std::size_t>(index)], index);
        panel_.setSelectedVoicingPresetIndex(index);
    };
    panel_.onVoicingEdited = [this] { processor_.setSelectedVoicingPresetIndex(-1); };
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
    panel_.onSaveConfiguration = [this] {
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Save 3bs configuration", juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory).getChildFile("Three Body Solution.3bs"), "*.3bs");
        fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& chooser) {
                auto file = chooser.getResult();
                if (file == juce::File{})
                    return;
                file = file.withFileExtension(".3bs");
                const auto saved = file.replaceWithText(
                    serializeUserConfiguration(processor_.currentUserConfiguration()));
                panel_.setStatus(saved ? "SAVED / " + file.getFileName() : "SAVE FAILED");
            });
    };
    panel_.onLoadConfiguration = [this] {
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Load 3bs configuration", juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory), "*.3bs");
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& chooser) {
                const auto file = chooser.getResult();
                if (file == juce::File{})
                    return;
                UserConfiguration configuration;
                juce::String error;
                if (!deserializeUserConfiguration(file.loadFileAsString(), configuration, error)) {
                    panel_.setStatus("LOAD FAILED / " + error);
                    return;
                }
                processor_.applyUserConfiguration(configuration);
                presentation_ = configuration.presentation;
                lastPlaneTilts_ = configuration.planeTilts;
                panel_.setPresentationState(presentation_);
                panel_.setPlaneTilts(lastPlaneTilts_);
                panel_.setSelectedPresetIndex(configuration.presetIndex);
                panel_.setSelectedVoicingPresetIndex(configuration.voicingPresetIndex);
                panel_.setStatus("LOADED / " + file.getFileName());
            });
    };
    panel_.setMidiOutputAvailable(false);
    presentation_ = processor_.presentationState();
    panel_.setPresentationState(presentation_);
    lastPlaneTilts_ = processor_.initialPlaneTilts();
    panel_.setPlaneTilts(lastPlaneTilts_);
    panel_.setSelectedVoicingPresetIndex(processor_.currentUserConfiguration().voicingPresetIndex);
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
