// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"
#include "core/SnapshotQueue.h"
#include "standalone/StandaloneRuntime.h"
#include "ui/ArtworkPanel.h"
#include "ui/AdvancedStateEditor.h"
#include "ui/PresetCatalog.h"
#include "ui/UserConfiguration.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace threebs {

class StandaloneContent final : public juce::Component, private juce::Timer {
public:
    StandaloneContent() : panel_(snapshots_, noteVisualizationEvents_), runtime_(snapshots_, noteVisualizationEvents_) {
        MusicEngine defaults;
        config_ = defaults.config();
        initial_ = defaults.simulation().initialState();
        baseInitial_ = initial_;
        setWantsKeyboardFocus(true);
        addAndMakeVisible(panel_);
        panel_.setPresetCatalog(presets_);
        panel_.setVoicingPresetCatalog(voicingPresets_);
        panel_.onPresetSelected = [this](int index) { applyPreset(index); };
        panel_.onVoicingPresetSelected = [this](int index) { applyVoicingPreset(index); };
        panel_.onVoicingEdited = [this] { selectedVoicingPresetIndex_ = -1; };
        panel_.onRandomize = [this] {
            baseInitial_ = makeInitialState(InitialSystem::ControlledChaos, ++seed_,
                                            panel_.chaosSlider().getValue() / 100.0);
            initial_ = applyInitialPlaneTilts(baseInitial_, planeTilts_);
            requestReset();
            presentation_.visualSeed = initial_.seed;
            panel_.setPresentationState(presentation_);
            
        };
        panel_.onReset = [this] { requestReset();  };
        panel_.onCameraChanged = [this](const CameraState& camera) { presentation_.camera = camera; };
        panel_.onNotePaneMinimizedChanged = [this](bool minimized) {
            presentation_.notePaneMinimized = minimized;
        };
        panel_.onNotePaneStyleChanged = [this](NotePaneStyle style) {
            presentation_.notePaneStyle = style;
        };
        panel_.onAdvanced = [this] {
            const auto target = panel_.getLocalArea(&panel_.advancedButton(), panel_.advancedButton().getLocalBounds());
            showAdvancedStateEditor(initial_, target, panel_, [safe = juce::Component::SafePointer<StandaloneContent>(this)](const SimulationState& state) {
                if (safe == nullptr) return;
                safe->applyExactState(state);
            });
        };
        panel_.onPanic = [this] { ++panicRevision_; publishControls(); };
        panel_.onAdvancedConfiguration = [this](const EngineConfig& configuration) {
            config_ = configuration;
            panel_.setAdvancedConfiguration(config_);
            publishControls();
            return true;
        };
        panel_.onSaveConfiguration = [this] { saveConfiguration(); };
        panel_.onLoadConfiguration = [this] { loadConfiguration(); };
        panel_.onCaptureConfiguration = [this] { return currentConfiguration(); };
        panel_.onApplyUserConfiguration = [this](const UserConfiguration& configuration) {
            juce::String error;
            if (!validateUserConfiguration(configuration, error)) { panel_.setStatus("LOAD FAILED / " + error); return false; }
            applyConfiguration(configuration); return true;
        };
        panel_.syncButton().setToggleState(false, juce::dontSendNotification);
        panel_.syncButton().setButtonText("INTERNAL 120 BPM");
        panel_.syncButton().setTooltip("Standalone generation always runs from its internal 120 BPM clock.");
        panel_.syncButton().setEnabled(false);
        panel_.setMidiOutputAvailable(true);
        populateMidiOutputs();
        panel_.midiOutputSelector().onChange = [this] { openSelectedOutput(); };
        openSelectedOutput();
        applyPreset(0);
        panel_.timeSigSourceSelector().setSelectedId(2, juce::dontSendNotification);
        panel_.timeSigSourceSelector().setEnabled(false);
        panel_.timeSigSourceSelector().setTooltip("Standalone uses the saved manual meter.");
        publishControls();
        runtime_.start();
        startTimerHz(30);
    }

    ~StandaloneContent() override {
        stopTimer();
        runtime_.stop();
    }

    void resized() override { panel_.setBounds(getLocalBounds()); }

    bool keyPressed(const juce::KeyPress& key) override {
        const auto* focused = juce::Component::getCurrentlyFocusedComponent();
        if (focused != nullptr && (dynamic_cast<const juce::TextEditor*>(focused) != nullptr
            || focused->findParentComponentOfClass<juce::TextEditor>() != nullptr)) return false;
        if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F') {
            if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
                window->setFullScreen(!window->isFullScreen());
            return true;
        }
        return panel_.keyPressed(key);
    }

private:
    void populateMidiOutputs() {
        auto& selector = panel_.midiOutputSelector();
        const auto oldName = selector.getText();
        selector.clear(juce::dontSendNotification);
        selector.addItem("Virtual MIDI: 3bs", 1);
        devices_ = CoreMidiOutput::availableDevices();
        int selection = 1;
        bool found = selectedOutputIdentifier_.isEmpty();
        for (int i = 0; i < devices_.size(); ++i) {
            selector.addItem(devices_.getReference(i).name, i + 2);
            if (devices_.getReference(i).identifier == selectedOutputIdentifier_) {
                selection = i + 2; found = true;
            }
        }
        if (!found) {
            devices_.add({oldName.upToFirstOccurrenceOf(" (unavailable)", false, false) + " (unavailable)", selectedOutputIdentifier_});
            selection = devices_.size() + 1;
            selector.addItem(devices_.getLast().name, selection);
        }
        selector.setSelectedId(selection, juce::dontSendNotification);
    }

    void openSelectedOutput() {
        const auto selected = panel_.midiOutputSelector().getSelectedId();
        if (selected < 1 || selected > devices_.size() + 1) return;
        const auto identifier = selected == 1 ? juce::String{} : devices_.getReference(selected - 2).identifier;
        selectedOutputIdentifier_ = identifier;
        if (!runtime_.selectOutput(selected == 1, identifier)) {
            panel_.setStatus("MIDI OUTPUT IDENTIFIER TOO LONG");
            return;
        }
        panel_.setStatus("MIDI OUTPUT / " + panel_.midiOutputSelector().getText());
    }

    void requestReset() { ++resetRevision_; publishControls(); }

    void applyExactState(const SimulationState& state) {
        initial_ = state;
        baseInitial_ = state;
        planeTilts_.fill(0.0);
        presentation_.visualSeed = state.seed;
        panel_.setPresentationState(presentation_);
        panel_.setPlaneTilts(planeTilts_);
        const auto masses = panel_.massSliders();
        for (std::size_t body = 0; body < bodyCount; ++body) {
            masses[body]->setValue(state.bodies[body].mass, juce::dontSendNotification);
            lastMassControls_[body] = masses[body]->getValue();
        }
        requestReset();
    }

    void publishControls() {
        StandaloneControls controls;
        controls.engine = config_;
        controls.initial = initial_;
        controls.resetRevision = resetRevision_;
        controls.panicRevision = panicRevision_;
        controls.playing = panel_.runButton().getToggleState();
        controls.numerator = std::max(1, panel_.timeSigNumeratorSelector().getSelectedItemIndex() + 1);
        constexpr std::array<int, 5> denominators{1, 2, 4, 8, 16};
        controls.denominator = denominators[static_cast<std::size_t>(std::clamp(
            panel_.timeSigDenominatorSelector().getSelectedItemIndex(), 0, 4))];
        runtime_.publish(controls);
    }

    void applyPreset(int index) {
        if (!presets_.valid() || index < 0 || static_cast<std::size_t>(index) >= presets_.size())
            return;
        const auto& preset = presets_[static_cast<std::size_t>(index)];
        selectedPresetIndex_ = index;
        selectedVoicingPresetIndex_ = -1;
        panel_.setSelectedVoicingPresetIndex(-1);
        baseInitial_ = makeInitialState(preset.system, preset.seed, preset.chaos);
        planeTilts_.fill(0.0);
        panel_.setPlaneTilts(planeTilts_);
        initial_ = baseInitial_;
        config_.simulation = preset.simulation;
        config_.voices = preset.voices;
        presentation_ = preset.presentation;
        panel_.setPresentationState(presentation_);
        
        panel_.gravitySlider().setValue(preset.simulation.gravitationalConstant, juce::dontSendNotification);
        panel_.softeningSlider().setValue(preset.simulation.softening, juce::dontSendNotification);
        panel_.speedSlider().setValue(preset.simulation.speed, juce::dontSendNotification);
        const auto masses = panel_.massSliders();
        for (std::size_t i = 0; i < bodyCount; ++i)
            masses[i]->setValue(initial_.bodies[i].mass, juce::dontSendNotification);
        const auto enables = panel_.voiceEnableButtons();
        const auto scales = panel_.voiceScaleSelectors();
        const auto roots = panel_.voiceRootSelectors();
        const auto pitches = panel_.voicePitchSelectors();
        const auto triggers = panel_.voiceTriggerSelectors();
        const auto octaves = panel_.voiceOctaveSelectors();
        const auto durationMaps = panel_.voiceDurationMapSelectors();
        const auto durationGrids = panel_.voiceDurationGridSelectors();
        const auto durationMins = panel_.voiceDurationMinSliders();
        const auto durationMaxs = panel_.voiceDurationMaxSliders();
        for (std::size_t body = 0; body < bodyCount; ++body) {
            enables[body]->setToggleState(preset.voices[body].enabled, juce::dontSendNotification);
            scales[body]->setSelectedId(static_cast<int>(preset.voices[body].scale) + 1,
                                        juce::dontSendNotification);
            roots[body]->setSelectedId(static_cast<int>(preset.voices[body].root) + 1,
                                       juce::dontSendNotification);
            pitches[body]->setSelectedId(static_cast<int>(preset.voices[body].pitchMapping) + 1,
                                         juce::dontSendNotification);
            triggers[body]->setSelectedId(static_cast<int>(preset.voices[body].triggerMapping) + 1,
                                          juce::dontSendNotification);
            octaves[body]->setSelectedId(preset.voices[body].octave + 5, juce::dontSendNotification);
            durationMaps[body]->setSelectedId(static_cast<int>(preset.voices[body].durationMapping) + 1,
                                              juce::dontSendNotification);
            durationGrids[body]->setSelectedId(static_cast<int>(preset.voices[body].durationGrid) + 1,
                                               juce::sendNotificationSync);
            durationMins[body]->setValue(preset.voices[body].minimumDurationBeats,
                                         juce::dontSendNotification);
            durationMaxs[body]->setValue(preset.voices[body].maximumDurationBeats,
                                         juce::dontSendNotification);
        }
        lastDensityControl_ = panel_.densitySlider().getValue() / 100.0;
        for (std::size_t body = 0; body < bodyCount; ++body) lastMassControls_[body] = masses[body]->getValue();
        panel_.setAdvancedConfiguration(config_);
        captureVisibleControls();
        requestReset();
    }

    void applyVoicingPreset(int index) {
        if (!voicingPresets_.valid() || index < 0
            || static_cast<std::size_t>(index) >= voicingPresets_.size())
            return;
        ++panicRevision_;

        const auto& preset = voicingPresets_[static_cast<std::size_t>(index)];
        selectedVoicingPresetIndex_ = index;
        config_.voices = preset.voices;
        config_.voicingMode = preset.mode;
        config_.chordStrumMilliseconds = preset.chordStrumMilliseconds;
        config_.chordStrumUnit = preset.chordStrumUnit;
        config_.chordStrumValue = preset.chordStrumValue;
        config_.chordSystem = preset.chordSystem;

        panel_.densitySlider().setValue(preset.densityPercent, juce::dontSendNotification);
        panel_.voicingModeSelector().setSelectedId(static_cast<int>(preset.mode) + 1,
                                                   juce::sendNotificationSync);
        panel_.chordStrumSlider().setValue(preset.chordStrumMilliseconds, juce::dontSendNotification);
        panel_.strumUnitSelector().setSelectedId(static_cast<int>(preset.chordStrumUnit) + 1,
                                                juce::dontSendNotification);
        panel_.strumValueSlider().setValue(preset.chordStrumValue, juce::dontSendNotification);
        panel_.chordRootSelector().setSelectedId(static_cast<int>(preset.chordSystem.root) + 1,
                                                juce::dontSendNotification);
        panel_.chordScaleSelector().setSelectedId(static_cast<int>(preset.chordSystem.scale) + 1,
                                                 juce::dontSendNotification);
        const auto enables = panel_.voiceEnableButtons();
        const auto roots = panel_.voiceRootSelectors();
        const auto scales = panel_.voiceScaleSelectors();
        const auto pitches = panel_.voicePitchSelectors();
        const auto triggers = panel_.voiceTriggerSelectors();
        const auto octaves = panel_.voiceOctaveSelectors();
        const auto durationMaps = panel_.voiceDurationMapSelectors();
        const auto durationGrids = panel_.voiceDurationGridSelectors();
        const auto durationMins = panel_.voiceDurationMinSliders();
        const auto durationMaxs = panel_.voiceDurationMaxSliders();
        for (std::size_t body = 0; body < bodyCount; ++body) {
            const auto& voice = preset.voices[body];
            enables[body]->setToggleState(voice.enabled, juce::dontSendNotification);
            roots[body]->setSelectedId(static_cast<int>(voice.root) + 1, juce::dontSendNotification);
            scales[body]->setSelectedId(static_cast<int>(voice.scale) + 1, juce::dontSendNotification);
            pitches[body]->setSelectedId(static_cast<int>(voice.pitchMapping) + 1, juce::dontSendNotification);
            triggers[body]->setSelectedId(static_cast<int>(voice.triggerMapping) + 1, juce::dontSendNotification);
            octaves[body]->setSelectedId(voice.octave + 5, juce::dontSendNotification);
            durationMaps[body]->setSelectedId(static_cast<int>(voice.durationMapping) + 1,
                                              juce::dontSendNotification);
            durationGrids[body]->setSelectedId(static_cast<int>(voice.durationGrid) + 1,
                                               juce::sendNotificationSync);
            durationMins[body]->setValue(voice.minimumDurationBeats, juce::dontSendNotification);
            durationMaxs[body]->setValue(voice.maximumDurationBeats, juce::dontSendNotification);
        }
        panel_.setSelectedVoicingPresetIndex(index);
        lastDensityControl_ = panel_.densitySlider().getValue() / 100.0;
        panel_.setAdvancedConfiguration(config_);
        captureVisibleControls();
        publishControls();
    }

    UserConfiguration currentConfiguration() {
        timerCallback();
        UserConfiguration result;
        result.initial = initial_;
        result.baseInitial = baseInitial_;
        result.planeTilts = planeTilts_;
        result.engine = config_;
        result.presentation = presentation_;
        result.chaosPercent = panel_.chaosSlider().getValue();
        result.densityPercent = panel_.densitySlider().getValue();
        result.presetIndex = selectedPresetIndex_;
        result.voicingPresetIndex = selectedVoicingPresetIndex_;
        result.run = panel_.runButton().getToggleState();
        result.sync = false;
        result.loopPolicy = loopPolicy_;
        result.timeSignatureSource = TimeSignatureSource::Manual;
        result.timeSignatureNumerator = std::max(1, panel_.timeSigNumeratorSelector().getSelectedItemIndex() + 1);
        constexpr std::array<int, 5> denominators{1, 2, 4, 8, 16};
        result.timeSignatureDenominator = denominators[static_cast<std::size_t>(std::clamp(
            panel_.timeSigDenominatorSelector().getSelectedItemIndex(), 0, 4))];
        return result;
    }

    void applyConfiguration(const UserConfiguration& configuration) {
        initial_ = configuration.initial;
        baseInitial_ = configuration.baseInitial;
        planeTilts_ = configuration.planeTilts;
        config_ = configuration.engine;
        loopPolicy_ = configuration.loopPolicy;
        presentation_ = configuration.presentation;
        selectedPresetIndex_ = configuration.presetIndex;
        selectedVoicingPresetIndex_ = configuration.voicingPresetIndex;
        panel_.setSelectedPresetIndex(selectedPresetIndex_);
        panel_.setPresentationState(presentation_);
        panel_.setPlaneTilts(planeTilts_);
        panel_.runButton().setToggleState(configuration.run, juce::dontSendNotification);
        panel_.timeSigSourceSelector().setSelectedId(2, juce::dontSendNotification);
        panel_.timeSigNumeratorSelector().setSelectedId(configuration.timeSignatureNumerator, juce::dontSendNotification);
        constexpr std::array<int, 5> denominators{1, 2, 4, 8, 16};
        const auto denominator = std::find(denominators.begin(), denominators.end(), configuration.timeSignatureDenominator);
        panel_.timeSigDenominatorSelector().setSelectedId(static_cast<int>(denominator - denominators.begin()) + 1, juce::dontSendNotification);
        panel_.chaosSlider().setValue(configuration.chaosPercent, juce::dontSendNotification);
        panel_.densitySlider().setValue(configuration.densityPercent, juce::dontSendNotification);
        panel_.gravitySlider().setValue(config_.simulation.gravitationalConstant, juce::dontSendNotification);
        panel_.softeningSlider().setValue(config_.simulation.softening, juce::dontSendNotification);
        panel_.speedSlider().setValue(config_.simulation.speed, juce::dontSendNotification);
        panel_.voicingModeSelector().setSelectedId(static_cast<int>(config_.voicingMode) + 1,
                                                   juce::sendNotificationSync);
        panel_.chordStrumSlider().setValue(config_.chordStrumMilliseconds, juce::dontSendNotification);
        panel_.strumUnitSelector().setSelectedId(static_cast<int>(config_.chordStrumUnit) + 1,
                                                 juce::dontSendNotification);
        panel_.strumValueSlider().setValue(config_.chordStrumValue, juce::dontSendNotification);
        panel_.chordRootSelector().setSelectedId(static_cast<int>(config_.chordSystem.root) + 1,
                                                 juce::dontSendNotification);
        panel_.chordScaleSelector().setSelectedId(static_cast<int>(config_.chordSystem.scale) + 1,
                                                  juce::dontSendNotification);
        panel_.autoResetButton().setToggleState(config_.autoResetEnabled, juce::dontSendNotification);
        {
            constexpr std::array<double, 7> barValues{0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0};
            int barIndex{2};
            double bestError{1.0e18};
            for (int i = 0; i < static_cast<int>(barValues.size()); ++i) {
                const auto error = std::abs(barValues[static_cast<std::size_t>(i)] - config_.autoResetBars);
                if (error < bestError) {
                    bestError = error;
                    barIndex = i;
                }
            }
            panel_.autoResetBarsSelector().setSelectedId(barIndex + 1, juce::dontSendNotification);
        }
        const auto masses = panel_.massSliders();
        const auto enables = panel_.voiceEnableButtons();
        const auto roots = panel_.voiceRootSelectors();
        const auto scales = panel_.voiceScaleSelectors();
        const auto pitches = panel_.voicePitchSelectors();
        const auto triggers = panel_.voiceTriggerSelectors();
        const auto octaves = panel_.voiceOctaveSelectors();
        const auto durationMaps = panel_.voiceDurationMapSelectors();
        const auto durationGrids = panel_.voiceDurationGridSelectors();
        const auto durationMins = panel_.voiceDurationMinSliders();
        const auto durationMaxs = panel_.voiceDurationMaxSliders();
        for (std::size_t body = 0; body < bodyCount; ++body) {
            masses[body]->setValue(initial_.bodies[body].mass, juce::dontSendNotification);
            enables[body]->setToggleState(config_.voices[body].enabled, juce::dontSendNotification);
            roots[body]->setSelectedId(static_cast<int>(config_.voices[body].root) + 1,
                                       juce::dontSendNotification);
            scales[body]->setSelectedId(static_cast<int>(config_.voices[body].scale) + 1,
                                        juce::dontSendNotification);
            pitches[body]->setSelectedId(static_cast<int>(config_.voices[body].pitchMapping) + 1,
                                         juce::dontSendNotification);
            triggers[body]->setSelectedId(static_cast<int>(config_.voices[body].triggerMapping) + 1,
                                          juce::dontSendNotification);
            octaves[body]->setSelectedId(config_.voices[body].octave + 5, juce::dontSendNotification);
            durationMaps[body]->setSelectedId(static_cast<int>(config_.voices[body].durationMapping) + 1,
                                              juce::dontSendNotification);
            durationGrids[body]->setSelectedId(static_cast<int>(config_.voices[body].durationGrid) + 1,
                                               juce::sendNotificationSync);
            durationMins[body]->setValue(config_.voices[body].minimumDurationBeats, juce::dontSendNotification);
            durationMaxs[body]->setValue(config_.voices[body].maximumDurationBeats, juce::dontSendNotification);
        }
        panel_.setSelectedVoicingPresetIndex(selectedVoicingPresetIndex_);
        lastDensityControl_ = panel_.densitySlider().getValue() / 100.0;
        for (std::size_t body = 0; body < bodyCount; ++body) lastMassControls_[body] = masses[body]->getValue();
        panel_.setAdvancedConfiguration(config_);
        captureVisibleControls();
        requestReset();
    }

    void saveConfiguration() {
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Save 3bs configuration", juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory).getChildFile("Three Body Solution.3bs"), "*.3bs");
        fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe = juce::Component::SafePointer<StandaloneContent>(this)](const juce::FileChooser& chooser) {
                if (safe == nullptr) return;
                auto file = chooser.getResult();
                if (file == juce::File{})
                    return;
                file = file.withFileExtension(".3bs");
                const auto saved = file.replaceWithText(serializeUserConfiguration(safe->currentConfiguration()));
                safe->panel_.setStatus(saved ? "SAVED / " + file.getFileName() : "SAVE FAILED");
            });
    }

    void loadConfiguration() {
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Load 3bs configuration", juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory), "*.3bs");
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<StandaloneContent>(this)](const juce::FileChooser& chooser) {
                if (safe == nullptr) return;
                const auto file = chooser.getResult();
                if (file == juce::File{})
                    return;
                UserConfiguration configuration;
                juce::String error;
                if (!deserializeUserConfiguration(file.loadFileAsString(), configuration, error)) {
                    safe->panel_.setStatus("LOAD FAILED / " + error);
                    return;
                }
                safe->applyConfiguration(configuration);
                safe->panel_.setStatus("LOADED / " + file.getFileName());
            });
    }

    VoiceConfig visibleVoice(std::size_t body) {
        VoiceConfig result;
        result.enabled = panel_.voiceEnableButtons()[body]->getToggleState();
        result.scale = static_cast<ScaleId>(std::max(0, panel_.voiceScaleSelectors()[body]->getSelectedItemIndex()));
        result.root = static_cast<std::uint8_t>(std::max(0, panel_.voiceRootSelectors()[body]->getSelectedItemIndex()));
        result.pitchMapping = static_cast<PitchMapping>(std::max(0, panel_.voicePitchSelectors()[body]->getSelectedItemIndex()));
        result.triggerMapping = static_cast<TriggerMapping>(std::max(0, panel_.voiceTriggerSelectors()[body]->getSelectedItemIndex()));
        result.octave = static_cast<std::int8_t>(panel_.voiceOctaveSelectors()[body]->getSelectedItemIndex() - 4);
        result.durationMapping = static_cast<PitchMapping>(std::max(0, panel_.voiceDurationMapSelectors()[body]->getSelectedItemIndex()));
        result.durationGrid = static_cast<DurationGrid>(std::clamp(panel_.voiceDurationGridSelectors()[body]->getSelectedItemIndex(), 0, 1));
        result.minimumDurationBeats = panel_.voiceDurationMinSliders()[body]->getValue();
        result.maximumDurationBeats = panel_.voiceDurationMaxSliders()[body]->getValue();
        return result;
    }
    std::array<double, 6> visibleNumbers() {
        constexpr std::array<double, 7> bars{0.25, 0.5, 1, 2, 4, 8, 16};
        return {panel_.gravitySlider().getValue(), panel_.softeningSlider().getValue(),
                panel_.speedSlider().getValue(), panel_.chordStrumSlider().getValue(),
                panel_.strumValueSlider().getValue(), bars[static_cast<std::size_t>(std::clamp(
                    panel_.autoResetBarsSelector().getSelectedItemIndex(), 0, 6))]};
    }
    void captureVisibleControls() {
        for (std::size_t body = 0; body < bodyCount; ++body) lastVoiceControls_[body] = visibleVoice(body);
        lastNumberControls_ = visibleNumbers();
    }
    void timerCallback() override {
        if (++deviceRefreshTicks_ >= 60) { deviceRefreshTicks_ = 0; populateMidiOutputs(); }
        const auto numbers = visibleNumbers();
        std::array<double*, 6> destinations{&config_.simulation.gravitationalConstant, &config_.simulation.softening,
            &config_.simulation.speed, &config_.chordStrumMilliseconds, &config_.chordStrumValue, &config_.autoResetBars};
        for (std::size_t i = 0; i < numbers.size(); ++i)
            if (numbers[i] != lastNumberControls_[i]) *destinations[i] = numbers[i];
        lastNumberControls_ = numbers;
        const auto density = panel_.densitySlider().getValue() / 100.0;
        for (std::size_t body = 0; body < bodyCount; ++body) {
            auto& voice = config_.voices[body];
            if (density != lastDensityControl_) voice.probability = density;
            const auto visible = visibleVoice(body);
            const auto& previous = lastVoiceControls_[body];
            if (visible.enabled != previous.enabled) voice.enabled = visible.enabled;
            if (visible.scale != previous.scale) voice.scale = visible.scale;
            if (visible.root != previous.root) voice.root = visible.root;
            if (visible.pitchMapping != previous.pitchMapping) voice.pitchMapping = visible.pitchMapping;
            if (visible.triggerMapping != previous.triggerMapping) voice.triggerMapping = visible.triggerMapping;
            if (visible.octave != previous.octave) voice.octave = visible.octave;
            if (visible.durationMapping != previous.durationMapping) voice.durationMapping = visible.durationMapping;
            if (visible.durationGrid != previous.durationGrid) voice.durationGrid = visible.durationGrid;
            if (visible.minimumDurationBeats != previous.minimumDurationBeats) voice.minimumDurationBeats = visible.minimumDurationBeats;
            if (visible.maximumDurationBeats != previous.maximumDurationBeats) voice.maximumDurationBeats = visible.maximumDurationBeats;
            if (voice.minimumDurationBeats > voice.maximumDurationBeats) {
                if (visible.minimumDurationBeats != previous.minimumDurationBeats)
                    voice.maximumDurationBeats = voice.minimumDurationBeats;
                else voice.minimumDurationBeats = voice.maximumDurationBeats;
            }
            lastVoiceControls_[body] = visible;
        }
        lastDensityControl_ = density;
        config_.voicingMode = static_cast<VoicingMode>(
            std::max(0, panel_.voicingModeSelector().getSelectedItemIndex()));
        config_.chordStrumUnit = static_cast<StrumUnit>(
            std::max(0, panel_.strumUnitSelector().getSelectedItemIndex()));
        config_.chordSystem.root = static_cast<std::uint8_t>(
            std::max(0, panel_.chordRootSelector().getSelectedItemIndex()));
        config_.chordSystem.scale = static_cast<ScaleId>(
            std::max(0, panel_.chordScaleSelector().getSelectedItemIndex()));
        config_.autoResetEnabled = panel_.autoResetButton().getToggleState();
        const auto masses = panel_.massSliders();
        for (std::size_t i = 0; i < bodyCount; ++i) {
            const auto mass = masses[i]->getValue();
            if (mass != lastMassControls_[i]) {
                baseInitial_.bodies[i].mass = mass;
                initial_.bodies[i].mass = mass;
                lastMassControls_[i] = mass;
            }
        }
        const auto currentTilts = panel_.planeTilts();
        bool tiltChanged{};
        for (std::size_t body = 0; body < bodyCount; ++body)
            tiltChanged = tiltChanged || std::abs(currentTilts[body] - planeTilts_[body]) > 0.05;
        if (tiltChanged) {
            planeTilts_ = currentTilts;
            initial_ = applyInitialPlaneTilts(baseInitial_, planeTilts_);
            requestReset();
            
        }

        publishControls();
        panel_.setAdvancedConfiguration(config_);
        if (runtime_.timingFault() && !reportedTimingFault_) {
            panel_.setStatus("MIDI TIMING FAULT / RESET TO RESUME");
            reportedTimingFault_ = true;
        } else if (!runtime_.timingFault()) {
            reportedTimingFault_ = false;
        }
        if (runtime_.outputReady() != lastOutputReady_) {
            lastOutputReady_ = runtime_.outputReady();
            panel_.setStatus(lastOutputReady_ ? "MIDI OUTPUT READY" : "MIDI OUTPUT UNAVAILABLE");
        }
        if (runtime_.outputFailures() != reportedOutputFailures_) {
            reportedOutputFailures_ = runtime_.outputFailures();
            panel_.setStatus("MIDI OUTPUT INTERRUPTED / RECONNECTING");
        }
        presentation_.visual.trailSeconds = static_cast<float>(panel_.trailSlider().getValue());
        presentation_.visual.bloom = static_cast<float>(panel_.bloomSlider().getValue() / 100.0);
        presentation_.camera.minimumDistance = static_cast<float>(panel_.minimumCameraDistanceSlider().getValue());
        presentation_.camera.maximumDistance = static_cast<float>(panel_.maximumCameraDistanceSlider().getValue());
        presentation_.camera.autoFrame = panel_.autoFrameButton().getToggleState();
        presentation_.camera = sanitizedCameraState(presentation_.camera);
        panel_.setPresentationState(presentation_);
    }

    SpscQueue<RenderSnapshot, 64> snapshots_;
    NoteVisualizationQueue noteVisualizationEvents_;
    ArtworkPanel panel_;
    PresetCatalog presets_;
    VoicingPresetCatalog voicingPresets_;
    StandaloneRuntime runtime_;
    EngineConfig config_{};
    LoopPolicy loopPolicy_{LoopPolicy::Restart};
    SimulationState baseInitial_{};
    SimulationState initial_{};
    juce::Array<juce::MidiDeviceInfo> devices_;
    juce::String selectedOutputIdentifier_;
    int deviceRefreshTicks_{};
    std::unique_ptr<juce::FileChooser> fileChooser_;
    PresentationState presentation_{};
    std::uint64_t seed_{0x33425320ULL};
    int selectedPresetIndex_{};
    int selectedVoicingPresetIndex_{-1};
    std::uint64_t resetRevision_{1};
    std::uint64_t panicRevision_{};
    std::array<double, bodyCount> lastMassControls_{};
    std::array<VoiceConfig, bodyCount> lastVoiceControls_{};
    std::array<double, 6> lastNumberControls_{};
    double lastDensityControl_{-1.0};
    bool reportedTimingFault_{};
    bool lastOutputReady_{};
    std::uint64_t reportedOutputFailures_{};
    std::array<double, bodyCount> planeTilts_{};
};

class MainWindow final : public juce::DocumentWindow {
public:
    MainWindow()
        : DocumentWindow("The Three Body Solution", juce::Colour(0xff060914),
                         DocumentWindow::allButtons, true) {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(900, 600, 1920, 1400);
        setContentOwned(new StandaloneContent(), true);
        centreWithSize(1280, 820);
        setVisible(true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class ThreeBSApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "The Three Body Solution"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override { window_ = std::make_unique<MainWindow>(); }
    void shutdown() override { window_.reset(); }
    void anotherInstanceStarted(const juce::String&) override {}

private:
    std::unique_ptr<MainWindow> window_;
};

} // namespace threebs

START_JUCE_APPLICATION(threebs::ThreeBSApplication)
