// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/AdvancedConfigurationEditor.h"
#include "ui/NumericText.h"
#include "ui/PresetCatalog.h"
#include <array>
#include <memory>

namespace threebs {
namespace {
constexpr std::array<const char*, 12> pitchClasses{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
void styleLabel(juce::Label& label, const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9eb4c8));
    label.setFont(juce::Font(juce::FontOptions(12.0F)));
}
void styleField(juce::TextEditor& field, double value) {
    field.setText(formatExactNumber(value), false);
    field.setInputRestrictions(0, "-+.0123456789eE");
    field.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101726));
    field.setColour(juce::TextEditor::textColourId, juce::Colour(0xffdce7ef));
}
class VoiceFields final : public juce::Component {
public:
    explicit VoiceFields(const VoiceConfig& voice) {
        constexpr std::array<const char*, 9> titles{"MIDI channel (1–16)", "Lowest note (0–127)", "Highest note (0–127)",
            "Clock interval (beats)", "Probability (0–1)", "Minimum trigger interval (beats)",
            "Close approach distance", "Minimum velocity (1–127)", "Maximum velocity (1–127)"};
        const std::array<double, 9> values{static_cast<double>(voice.channel), static_cast<double>(voice.minimumNote),
            static_cast<double>(voice.maximumNote), voice.clockDivisionBeats, voice.probability,
            voice.minimumTriggerIntervalBeats, voice.closeApproachDistance,
            static_cast<double>(voice.minimumVelocity), static_cast<double>(voice.maximumVelocity)};
        for (std::size_t i = 0; i < fields_.size(); ++i) {
            styleLabel(labels_[i], titles[i]); styleField(fields_[i], values[i]);
            addAndMakeVisible(labels_[i]); addAndMakeVisible(fields_[i]);
        }
        styleLabel(scaleLabel_, "CUSTOM SCALE / intervals from the voice root; select Custom on the Voices page");
        addAndMakeVisible(scaleLabel_);
        for (std::size_t i = 0; i < custom_.size(); ++i) {
            custom_[i].setButtonText(juce::String(static_cast<int>(i)) + " (" + pitchClasses[i] + ")");
            custom_[i].setToggleState(voice.customScale[i], juce::dontSendNotification);
            addAndMakeVisible(custom_[i]);
        }
        for (std::size_t lane = 0; lane < 2; ++lane) {
            enabled_[lane].setButtonText("CC " + juce::String(static_cast<int>(lane + 1)));
            enabled_[lane].setToggleState(voice.ccLanes[lane].enabled, juce::dontSendNotification);
            source_[lane].addItemList(pitchMappingDisplayNames(), 1);
            source_[lane].setSelectedId(static_cast<int>(voice.ccLanes[lane].source) + 1);
            styleField(controller_[lane], voice.ccLanes[lane].controller);
            styleField(smoothing_[lane], voice.ccLanes[lane].smoothing);
            styleLabel(controllerLabel_[lane], "Controller (0–119)");
            styleLabel(smoothingLabel_[lane], "Smoothing (0–1)");
            addAndMakeVisible(enabled_[lane]); addAndMakeVisible(source_[lane]);
            addAndMakeVisible(controller_[lane]); addAndMakeVisible(smoothing_[lane]);
            addAndMakeVisible(controllerLabel_[lane]); addAndMakeVisible(smoothingLabel_[lane]);
        }
    }
    bool read(VoiceConfig& voice, juce::String& error) const {
        std::array<double, 9> values{};
        for (std::size_t i = 0; i < values.size(); ++i)
            if (!parseFiniteNumber(fields_[i].getText(), values[i])) {
                error = "Every field must contain one finite number."; return false;
            }
        for (auto i : {0U, 1U, 2U, 7U, 8U})
            if (std::floor(values[i]) != values[i] || values[i] < 0 || values[i] > 127) {
                error = "Channel, note and velocity fields must contain whole numbers in range."; return false;
            }
        if (values[0] < 1 || values[0] > 16 || values[1] > values[2]
            || values[7] < 1 || values[7] > values[8]) {
            error = "Check channel (1–16), ascending note range and velocity range (1–127)."; return false;
        }
        voice.channel = static_cast<std::uint8_t>(values[0]);
        voice.minimumNote = static_cast<std::uint8_t>(values[1]); voice.maximumNote = static_cast<std::uint8_t>(values[2]);
        voice.clockDivisionBeats = values[3]; voice.probability = values[4];
        voice.minimumTriggerIntervalBeats = values[5]; voice.closeApproachDistance = values[6];
        voice.minimumVelocity = static_cast<std::uint8_t>(values[7]); voice.maximumVelocity = static_cast<std::uint8_t>(values[8]);
        for (std::size_t i = 0; i < custom_.size(); ++i) voice.customScale[i] = custom_[i].getToggleState();
        for (std::size_t lane = 0; lane < 2; ++lane) {
            double controller{}, smoothing{};
            if (!parseFiniteNumber(controller_[lane].getText(), controller) || std::floor(controller) != controller
                || controller < 0 || controller > 119 || !parseFiniteNumber(smoothing_[lane].getText(), smoothing)
                || smoothing < 0 || smoothing > 1) {
                error = "CC controller must be 0–119 and smoothing must be 0–1."; return false;
            }
            voice.ccLanes[lane] = {enabled_[lane].getToggleState(),
                static_cast<PitchMapping>(source_[lane].getSelectedItemIndex()),
                static_cast<std::uint8_t>(controller), smoothing};
        }
        return true;
    }
    void resized() override {
        auto area = getLocalBounds().reduced(16);
        for (std::size_t row = 0; row < 3; ++row) {
            auto line = area.removeFromTop(62);
            const auto width = line.getWidth() / 3;
            for (std::size_t col = 0; col < 3; ++col) {
                auto cell = line.removeFromLeft(width).reduced(5, 2);
                const auto i = row * 3 + col;
                labels_[i].setBounds(cell.removeFromTop(22)); fields_[i].setBounds(cell.removeFromTop(28));
            }
        }
        scaleLabel_.setBounds(area.removeFromTop(26));
        for (std::size_t row = 0; row < 2; ++row) {
            auto line = area.removeFromTop(28); const auto width = line.getWidth() / 6;
            for (std::size_t col = 0; col < 6; ++col) custom_[row * 6 + col].setBounds(line.removeFromLeft(width));
        }
        area.removeFromTop(10);
        for (std::size_t lane = 0; lane < 2; ++lane) {
            auto line = area.removeFromTop(60);
            enabled_[lane].setBounds(line.removeFromLeft(80).reduced(0, 16));
            source_[lane].setBounds(line.removeFromLeft(230).reduced(6, 16));
            auto controller = line.removeFromLeft(180).reduced(6, 0);
            controllerLabel_[lane].setBounds(controller.removeFromTop(24)); controller_[lane].setBounds(controller.removeFromTop(28));
            auto smoothing = line.reduced(6, 0);
            smoothingLabel_[lane].setBounds(smoothing.removeFromTop(24)); smoothing_[lane].setBounds(smoothing.removeFromTop(28));
        }
    }
private:
    std::array<juce::Label, 9> labels_;
    std::array<juce::TextEditor, 9> fields_;
    juce::Label scaleLabel_;
    std::array<juce::ToggleButton, 12> custom_;
    std::array<juce::ToggleButton, 2> enabled_;
    std::array<juce::ComboBox, 2> source_;
    std::array<juce::TextEditor, 2> controller_, smoothing_;
    std::array<juce::Label, 2> controllerLabel_, smoothingLabel_;
};
class SystemFields final : public juce::Component {
public:
    explicit SystemFields(const EngineConfig& config) {
        constexpr std::array<const char*, 4> titles{"Fixed simulation step (beat units)", "Escape radius", "Maximum steps per advance", "Minimum chord interval (beats)"};
        const std::array<double, 4> values{config.simulation.fixedStep, config.simulation.escapeRadius,
            static_cast<double>(config.simulation.maxStepsPerAdvance), config.minimumChordIntervalBeats};
        for (std::size_t i = 0; i < fields_.size(); ++i) {
            styleLabel(labels_[i], titles[i]); styleField(fields_[i], values[i]);
            addAndMakeVisible(labels_[i]); addAndMakeVisible(fields_[i]);
        }
        styleLabel(escapeLabel_, "ESCAPE POLICY"); addAndMakeVisible(escapeLabel_);
        escape_.addItemList({"Leave", "Respawn body", "Prompt"}, 1);
        escape_.setSelectedId(static_cast<int>(config.simulation.escapePolicy) + 1); addAndMakeVisible(escape_);
        styleLabel(customLabel_, "GLOBAL CHORD CUSTOM SCALE / intervals from global root"); addAndMakeVisible(customLabel_);
        for (std::size_t i = 0; i < custom_.size(); ++i) {
            custom_[i].setButtonText(juce::String(static_cast<int>(i)) + " (" + pitchClasses[i] + ")");
            custom_[i].setToggleState(config.chordSystem.customScale[i], juce::dontSendNotification);
            addAndMakeVisible(custom_[i]);
        }
        styleLabel(note_, "These controls edit the existing simulation and voice mappings. Density on the main page replaces per-voice probability when moved.");
        addAndMakeVisible(note_);
    }
    bool read(EngineConfig& config, juce::String& error) const {
        std::array<double, 4> values{};
        for (std::size_t i = 0; i < values.size(); ++i)
            if (!parseFiniteNumber(fields_[i].getText(), values[i])) {
                error = "Every field must contain one finite number."; return false;
            }
        if (values[2] < 1 || values[2] > 1000000 || std::floor(values[2]) != values[2]) {
            error = "Maximum steps must be an integer from 1 to 1000000."; return false;
        }
        config.simulation.fixedStep = values[0]; config.simulation.escapeRadius = values[1];
        config.simulation.maxStepsPerAdvance = static_cast<std::uint32_t>(values[2]);
        config.minimumChordIntervalBeats = values[3];
        config.simulation.escapePolicy = static_cast<EscapePolicy>(escape_.getSelectedItemIndex());
        for (std::size_t i = 0; i < custom_.size(); ++i) config.chordSystem.customScale[i] = custom_[i].getToggleState();
        return true;
    }
    void resized() override {
        auto area = getLocalBounds().reduced(22);
        for (std::size_t i = 0; i < fields_.size(); ++i) {
            auto row = area.removeFromTop(48);
            labels_[i].setBounds(row.removeFromLeft(330)); fields_[i].setBounds(row.removeFromLeft(200).reduced(0, 9));
        }
        auto policy = area.removeFromTop(45);
        escapeLabel_.setBounds(policy.removeFromLeft(330)); escape_.setBounds(policy.removeFromLeft(200).reduced(0, 8));
        customLabel_.setBounds(area.removeFromTop(28));
        for (std::size_t row = 0; row < 2; ++row) {
            auto line = area.removeFromTop(30); const auto width = line.getWidth() / 6;
            for (std::size_t col = 0; col < 6; ++col) custom_[row * 6 + col].setBounds(line.removeFromLeft(width));
        }
        note_.setBounds(area.reduced(0, 15));
    }
private:
    std::array<juce::Label, 4> labels_;
    std::array<juce::TextEditor, 4> fields_;
    juce::Label escapeLabel_, customLabel_, note_;
    juce::ComboBox escape_;
    std::array<juce::ToggleButton, 12> custom_;
};
class AdvancedConfigurationEditor final : public juce::Component {
public:
    AdvancedConfigurationEditor(EngineConfig config, std::function<void(const EngineConfig&)> apply)
        : config_(config), apply_(std::move(apply)), system_(config) {
        for (std::size_t i = 0; i < voices_.size(); ++i) {
            voices_[i] = std::make_unique<VoiceFields>(config.voices[i]);
            tabs_.addTab("PLANET " + juce::String(static_cast<int>(i + 1)), juce::Colour(0xff101726), voices_[i].get(), false);
        }
        tabs_.addTab("SYSTEM", juce::Colour(0xff101726), &system_, false);
        addAndMakeVisible(tabs_); addAndMakeVisible(error_); addAndMakeVisible(save_); addAndMakeVisible(cancel_);
        error_.setColour(juce::Label::textColourId, juce::Colour(0xffff806e));
        save_.onClick = [this] {
            auto candidate = config_; juce::String error;
            for (std::size_t i = 0; i < voices_.size(); ++i) if (!voices_[i]->read(candidate.voices[i], error)) {
                error_.setText(error, juce::dontSendNotification); tabs_.setCurrentTabIndex(static_cast<int>(i)); return;
            }
            if (!system_.read(candidate, error) || !isValidEngineConfig(candidate)) {
                error_.setText(error.isEmpty() ? "Check numeric ranges and select at least one custom-scale interval." : error,
                               juce::dontSendNotification); return;
            }
            apply_(candidate); dismiss();
        };
        cancel_.onClick = [this] { dismiss(); };
        setSize(800, 560);
    }
    void paint(juce::Graphics& graphics) override { graphics.fillAll(juce::Colour(0xff080d18)); }
    void resized() override {
        auto bounds = getLocalBounds().reduced(12); auto footer = bounds.removeFromBottom(48);
        tabs_.setBounds(bounds); cancel_.setBounds(footer.removeFromRight(90).reduced(4, 8));
        save_.setBounds(footer.removeFromRight(100).reduced(4, 8)); error_.setBounds(footer);
    }
private:
    void dismiss() {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>()) dialog->exitModalState(0);
    }
    EngineConfig config_;
    std::function<void(const EngineConfig&)> apply_;
    std::array<std::unique_ptr<VoiceFields>, bodyCount> voices_;
    SystemFields system_;
    juce::TabbedComponent tabs_{juce::TabbedButtonBar::TabsAtTop};
    juce::Label error_;
    juce::TextButton save_{"APPLY"}, cancel_{"CANCEL"};
};
} // namespace

void copyAdvancedConfigurationFields(const EngineConfig& source, EngineConfig& destination) noexcept {
    for (std::size_t i = 0; i < bodyCount; ++i) {
        auto& voice = destination.voices[i]; const auto& from = source.voices[i];
        voice.channel = from.channel; voice.minimumNote = from.minimumNote; voice.maximumNote = from.maximumNote;
        voice.clockDivisionBeats = from.clockDivisionBeats; voice.probability = from.probability;
        voice.minimumTriggerIntervalBeats = from.minimumTriggerIntervalBeats;
        voice.closeApproachDistance = from.closeApproachDistance;
        voice.minimumVelocity = from.minimumVelocity; voice.maximumVelocity = from.maximumVelocity;
        voice.customScale = from.customScale; voice.ccLanes = from.ccLanes;
    }
    destination.simulation.fixedStep = source.simulation.fixedStep;
    destination.simulation.escapeRadius = source.simulation.escapeRadius;
    destination.simulation.maxStepsPerAdvance = source.simulation.maxStepsPerAdvance;
    destination.simulation.escapePolicy = source.simulation.escapePolicy;
    destination.minimumChordIntervalBeats = source.minimumChordIntervalBeats;
    destination.chordSystem.customScale = source.chordSystem.customScale;
}
void showAdvancedConfigurationEditor(const EngineConfig& config, juce::Component& parent,
                                     std::function<void(const EngineConfig&)> onApply) {
    auto guard = juce::Component::SafePointer<juce::Component>(&parent);
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "VOICE AND SYSTEM SETTINGS";
    options.dialogBackgroundColour = juce::Colour(0xff080d18);
    options.content.setOwned(new AdvancedConfigurationEditor(config,
        [guard, callback = std::move(onApply)](const EngineConfig& candidate) { if (guard != nullptr) callback(candidate); }));
    options.componentToCentreAround = parent.getTopLevelComponent();
    options.escapeKeyTriggersCloseButton = true; options.useNativeTitleBar = true; options.resizable = false;
    options.launchAsync();
}
} // namespace threebs
