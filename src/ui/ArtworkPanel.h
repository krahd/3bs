// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "render/MetalSceneComponent.h"
#include "ui/PresetCatalog.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace threebs {

class ArtworkPanel final : public juce::Component {
public:
    explicit ArtworkPanel(SpscQueue<RenderSnapshot, 64>& snapshots);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setVisualSettings(const VisualSettings& settings);
    void setPresetNames(const juce::StringArray& names);
    void setStatus(const juce::String& status);

    juce::ToggleButton& runButton() noexcept { return run_; }
    juce::ToggleButton& syncButton() noexcept { return sync_; }
    juce::Slider& speedSlider() noexcept { return speed_; }
    juce::Slider& gravitySlider() noexcept { return gravity_; }
    juce::Slider& softeningSlider() noexcept { return softening_; }
    juce::Slider& chaosSlider() noexcept { return chaos_; }
    juce::Slider& densitySlider() noexcept { return density_; }
    juce::Slider& trailSlider() noexcept { return trail_; }
    juce::Slider& bloomSlider() noexcept { return bloom_; }
    juce::ComboBox& presetSelector() noexcept { return presets_; }
    juce::ComboBox& midiOutputSelector() noexcept { return midiOutput_; }
    juce::TextButton& randomizeButton() noexcept { return randomize_; }
    juce::TextButton& resetButton() noexcept { return reset_; }
    juce::TextButton& advancedButton() noexcept { return advanced_; }
    std::array<juce::Slider*, bodyCount> massSliders() noexcept {
        return {&massOne_, &massTwo_, &massThree_};
    }

    std::function<void()> onRandomize;
    std::function<void()> onReset;
    std::function<void()> onAdvanced;

private:
    class DeckLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        DeckLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
    } lookAndFeel_;

    void configureKnob(juce::Slider& slider, const juce::String& suffix = {});
    void togglePresentation();

    MetalSceneComponent scene_;
    juce::Label title_;
    juce::Label subtitle_;
    juce::Label status_;
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
    juce::ComboBox presets_;
    juce::ComboBox midiOutput_;
    juce::TextButton randomize_{"NEW SYSTEM"};
    juce::TextButton reset_{"RESET"};
    juce::TextButton advanced_{"ADVANCED STATE"};
    juce::TextButton present_{"PRESENT"};
    std::array<juce::TextButton, 5> tabs_{
        juce::TextButton{"SYSTEM"}, juce::TextButton{"VOICES"}, juce::TextButton{"SPACE"},
        juce::TextButton{"PRESETS"}, juce::TextButton{"SETTINGS"}};
    std::array<juce::Label, 10> knobLabels_;
    bool presentationMode_{};
    VisualSettings visual_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtworkPanel)
};

} // namespace threebs
