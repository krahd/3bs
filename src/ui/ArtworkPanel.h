// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "render/MetalSceneComponent.h"
#include "ui/PresetCatalog.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace threebs {

class ArtworkPanel final : public juce::Component, private juce::ComboBox::Listener {
public:
    ArtworkPanel(SpscQueue<RenderSnapshot, 64>& snapshots,
                 NoteVisualizationQueue& noteVisualizationEvents);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setPresentationState(const PresentationState& state);
    PresentationState presentationState() const noexcept { return presentation_; }
    void setPresetCatalog(const PresetCatalog& catalog);
    void setSelectedPresetIndex(int index);
    void setVoicingPresetCatalog(const VoicingPresetCatalog& catalog);
    void setSelectedVoicingPresetIndex(int index);
    void setStatus(const juce::String& status);
    void setMidiOutputAvailable(bool available);
    void setPlaneTilts(const std::array<double, bodyCount>& tiltDegrees);
    std::array<double, bodyCount> planeTilts() const noexcept;

    juce::ToggleButton& runButton() noexcept { return run_; }
    juce::ToggleButton& syncButton() noexcept { return sync_; }
    juce::Slider& speedSlider() noexcept { return speed_; }
    juce::Slider& gravitySlider() noexcept { return gravity_; }
    juce::Slider& softeningSlider() noexcept { return softening_; }
    juce::Slider& chaosSlider() noexcept { return chaos_; }
    juce::Slider& densitySlider() noexcept { return density_; }
    juce::Slider& trailSlider() noexcept { return trail_; }
    juce::Slider& bloomSlider() noexcept { return bloom_; }
    juce::Slider& minimumCameraDistanceSlider() noexcept { return minimumCameraDistance_; }
    juce::Slider& maximumCameraDistanceSlider() noexcept { return maximumCameraDistance_; }
    juce::ToggleButton& autoFrameButton() noexcept { return autoFrame_; }
    juce::ComboBox& midiOutputSelector() noexcept { return midiOutput_; }
    juce::TextButton& randomizeButton() noexcept { return randomize_; }
    juce::TextButton& resetButton() noexcept { return reset_; }
    juce::TextButton& advancedButton() noexcept { return advanced_; }
    std::array<juce::Slider*, bodyCount> massSliders() noexcept {
        return {&massOne_, &massTwo_, &massThree_};
    }
    std::array<juce::Slider*, bodyCount> planeTiltSliders() noexcept {
        return {&tiltOne_, &tiltTwo_, &tiltThree_};
    }
    std::array<juce::ToggleButton*, bodyCount> voiceEnableButtons() noexcept {
        return {&voiceEnable_[0], &voiceEnable_[1], &voiceEnable_[2]};
    }
    std::array<juce::ComboBox*, bodyCount> voiceScaleSelectors() noexcept {
        return {&voiceScale_[0], &voiceScale_[1], &voiceScale_[2]};
    }
    std::array<juce::ComboBox*, bodyCount> voiceRootSelectors() noexcept {
        return {&voiceRoot_[0], &voiceRoot_[1], &voiceRoot_[2]};
    }
    std::array<juce::ComboBox*, bodyCount> voicePitchSelectors() noexcept {
        return {&voicePitch_[0], &voicePitch_[1], &voicePitch_[2]};
    }
    std::array<juce::ComboBox*, bodyCount> voiceTriggerSelectors() noexcept {
        return {&voiceTrigger_[0], &voiceTrigger_[1], &voiceTrigger_[2]};
    }
    std::array<juce::ComboBox*, bodyCount> voiceOctaveSelectors() noexcept {
        return {&voiceOctave_[0], &voiceOctave_[1], &voiceOctave_[2]};
    }
    std::array<juce::ComboBox*, bodyCount> voiceDurationMapSelectors() noexcept {
        return {&voiceDurMap_[0], &voiceDurMap_[1], &voiceDurMap_[2]};
    }
    std::array<juce::ComboBox*, bodyCount> voiceDurationGridSelectors() noexcept {
        return {&voiceDurGrid_[0], &voiceDurGrid_[1], &voiceDurGrid_[2]};
    }
    std::array<juce::Slider*, bodyCount> voiceDurationMinSliders() noexcept {
        return {&voiceDurMin_[0], &voiceDurMin_[1], &voiceDurMin_[2]};
    }
    std::array<juce::Slider*, bodyCount> voiceDurationMaxSliders() noexcept {
        return {&voiceDurMax_[0], &voiceDurMax_[1], &voiceDurMax_[2]};
    }
    juce::ComboBox& voicingModeSelector() noexcept { return voicingMode_; }
    juce::Slider& chordStrumSlider() noexcept { return chordStrum_; }
    juce::ComboBox& strumUnitSelector() noexcept { return strumUnit_; }
    juce::Slider& strumValueSlider() noexcept { return strumValue_; }
    juce::ComboBox& chordRootSelector() noexcept { return chordRoot_; }
    juce::ComboBox& chordScaleSelector() noexcept { return chordScale_; }
    juce::ComboBox& timeSigSourceSelector() noexcept { return timeSigSource_; }
    juce::ComboBox& timeSigNumeratorSelector() noexcept { return timeSigNum_; }
    juce::ComboBox& timeSigDenominatorSelector() noexcept { return timeSigDenom_; }
    juce::ToggleButton& autoResetButton() noexcept { return autoReset_; }
    juce::ComboBox& autoResetBarsSelector() noexcept { return autoResetBars_; }

    std::function<void()> onRandomize;
    std::function<void()> onReset;
    std::function<void()> onAdvanced;
    std::function<void()> onSaveConfiguration;
    std::function<void()> onLoadConfiguration;
    std::function<void(int)> onPresetSelected;
    std::function<void(int)> onVoicingPresetSelected;
    std::function<void()> onVoicingEdited;
    std::function<void(const CameraState&)> onCameraChanged;
    std::function<void(bool)> onNotePaneMinimizedChanged;
    std::function<void(NotePaneStyle)> onNotePaneStyleChanged;

private:
    class DeckLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        DeckLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
    } lookAndFeel_;

    class RhythmicLengthSlider final : public juce::Slider {
    public:
        void setStraightGrid(bool straight);
        double snapValue(double attemptedValue, DragMode dragMode) override;

    private:
        bool straight_{};
    };

    enum class DeckPage : std::size_t { System, Voices, Space, Presets, Settings };

    void configureKnob(juce::Slider& slider, const juce::String& suffix = {});
    int deckHeight() const noexcept;
    void togglePresentation();
    void selectDeckPage(DeckPage page);
    void updateDeckVisibility();
    void updateVoiceModeControls();
    void markVoicingCustom();
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    MetalSceneComponent scene_;
    juce::Label title_;
    juce::Label subtitle_;
    juce::Label status_;
    juce::Label pageHelp_;
    juce::ToggleButton run_{"RUN"};
    juce::ToggleButton sync_{"HOST SYNC"};
    juce::Slider speed_;
    juce::Slider gravity_;
    juce::Slider softening_;
    juce::Slider chaos_;
    juce::Slider density_;
    juce::Slider trail_;
    juce::Slider bloom_;
    juce::Slider massOne_;
    juce::Slider massTwo_;
    juce::Slider massThree_;
    juce::Slider tiltOne_;
    juce::Slider tiltTwo_;
    juce::Slider tiltThree_;
    juce::Slider minimumCameraDistance_;
    juce::Slider maximumCameraDistance_;
    std::array<juce::ToggleButton, bodyCount> voiceEnable_;
    std::array<juce::ComboBox, bodyCount> voiceScale_;
    std::array<juce::ComboBox, bodyCount> voiceRoot_;
    std::array<juce::ComboBox, bodyCount> voicePitch_;
    std::array<juce::ComboBox, bodyCount> voiceTrigger_;
    std::array<juce::ComboBox, bodyCount> voiceOctave_;
    std::array<juce::ComboBox, bodyCount> voiceDurMap_;
    std::array<juce::ComboBox, bodyCount> voiceDurGrid_;
    std::array<RhythmicLengthSlider, bodyCount> voiceDurMin_;
    std::array<RhythmicLengthSlider, bodyCount> voiceDurMax_;
    std::array<juce::Label, bodyCount> voiceHeaders_;
    std::array<juce::Label, 9> voiceRowLabels_;
    juce::Label voiceModeContext_;
    juce::ComboBox voicingPreset_;
    juce::ComboBox voicingMode_;
    juce::Slider chordStrum_;
    juce::ComboBox strumUnit_;
    juce::Slider strumValue_;
    juce::ComboBox chordRoot_;
    juce::ComboBox chordScale_;
    juce::Label chordRootLabel_;
    juce::Label chordScaleLabel_;
    juce::ComboBox timeSigSource_;
    juce::ComboBox timeSigNum_;
    juce::ComboBox timeSigDenom_;
    juce::Label timeSigLabel_;
    juce::ToggleButton autoReset_{"AUTO RESET"};
    juce::ComboBox autoResetBars_;
    juce::Label autoResetLabel_;
    juce::ToggleButton autoFrame_{"AUTO FRAME"};
    std::array<juce::ComboBox, 4> presetCategories_;
    std::array<juce::Label, 4> presetCategoryLabels_;
    juce::ComboBox midiOutput_;
    juce::TextButton randomize_{"NEW SYSTEM"};
    juce::TextButton reset_{"RESET"};
    juce::TextButton advanced_{"SET STATE"};
    juce::TextButton saveConfiguration_{"SAVE .3BS"};
    juce::TextButton loadConfiguration_{"LOAD .3BS"};
    juce::TextButton present_{"PRESENT"};
    std::array<juce::TextButton, 5> tabs_{
        juce::TextButton{"SYSTEM"}, juce::TextButton{"VOICES"}, juce::TextButton{"SPACE"},
        juce::TextButton{"PRESETS"}, juce::TextButton{"SETTINGS"}};
    std::array<juce::Label, 15> knobLabels_;
    DeckPage deckPage_{DeckPage::System};
    bool midiOutputAvailable_{};
    bool presentationMode_{};
    bool suppressVoicingEdit_{};
    PresentationState presentation_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtworkPanel)
};

} // namespace threebs
