// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/AdvancedStateEditor.h"

#include <array>
#include <cmath>
#include <memory>

namespace threebs {
namespace {

class AdvancedStateEditor final : public juce::Component {
public:
    AdvancedStateEditor(const SimulationState& state,
                        std::function<void(const SimulationState&)> onApply)
        : state_(state), onApply_(std::move(onApply)) {
        title_.setText("INITIAL BODY STATE / NORMALIZED UNITS", juce::dontSendNotification);
        title_.setFont(juce::Font(juce::FontOptions(15.0F).withStyle("Bold")));
        title_.setColour(juce::Label::textColourId, juce::Colour(0xffe5edf5));
        addAndMakeVisible(title_);

        const std::array<juce::String, 7> headings{"MASS", "PX", "PY", "PZ", "VX", "VY", "VZ"};
        for (std::size_t i = 0; i < headings.size(); ++i) {
            headings_[i].setText(headings[i], juce::dontSendNotification);
            headings_[i].setJustificationType(juce::Justification::centred);
            headings_[i].setColour(juce::Label::textColourId, juce::Colour(0xff7890a6));
            addAndMakeVisible(headings_[i]);
        }
        for (std::size_t body = 0; body < bodyCount; ++body) {
            bodyLabels_[body].setText("BODY " + juce::String(static_cast<int>(body + 1)), juce::dontSendNotification);
            bodyLabels_[body].setColour(juce::Label::textColourId,
                std::array<juce::Colour, 3>{juce::Colour(0xfff3a44b), juce::Colour(0xff43cede), juce::Colour(0xff9d6ce7)}[body]);
            addAndMakeVisible(bodyLabels_[body]);
            const auto& source = state.bodies[body];
            const std::array<double, 7> values{source.mass, source.position.x, source.position.y,
                source.position.z, source.velocity.x, source.velocity.y, source.velocity.z};
            for (std::size_t field = 0; field < values.size(); ++field) {
                auto& editor = fields_[body * values.size() + field];
                editor.setText(juce::String(values[field], 8), false);
                editor.setInputRestrictions(0, "-+.0123456789eE");
                editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101726));
                editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2c3a52));
                editor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffdce7ef));
                addAndMakeVisible(editor);
            }
        }
        error_.setColour(juce::Label::textColourId, juce::Colour(0xffff806e));
        addAndMakeVisible(error_);
        apply_.onClick = [this] { apply(); };
        cancel_.onClick = [this] { dismiss(); };
        addAndMakeVisible(apply_);
        addAndMakeVisible(cancel_);
        setSize(790, 286);
    }

    void paint(juce::Graphics& graphics) override {
        graphics.fillAll(juce::Colour(0xff080d18));
        graphics.setColour(juce::Colour(0xff34445d));
        graphics.drawRect(getLocalBounds(), 1);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(16);
        title_.setBounds(bounds.removeFromTop(28));
        auto headings = bounds.removeFromTop(24);
        headings.removeFromLeft(72);
        const auto columnWidth = headings.getWidth() / 7;
        for (auto& heading : headings_)
            heading.setBounds(headings.removeFromLeft(columnWidth));
        for (std::size_t body = 0; body < bodyCount; ++body) {
            auto row = bounds.removeFromTop(44);
            bodyLabels_[body].setBounds(row.removeFromLeft(72));
            const auto width = row.getWidth() / 7;
            for (std::size_t field = 0; field < 7; ++field)
                fields_[body * 7 + field].setBounds(row.removeFromLeft(width).reduced(3, 6));
        }
        auto footer = bounds.removeFromBottom(38);
        cancel_.setBounds(footer.removeFromRight(90).reduced(3));
        apply_.setBounds(footer.removeFromRight(110).reduced(3));
        error_.setBounds(footer);
    }

private:
    void apply() {
        auto result = state_;
        for (std::size_t body = 0; body < bodyCount; ++body) {
            std::array<double, 7> values{};
            for (std::size_t field = 0; field < values.size(); ++field) {
                const auto text = fields_[body * values.size() + field].getText().trim();
                values[field] = text.getDoubleValue();
                if (text.isEmpty() || !std::isfinite(values[field])) {
                    error_.setText("Every field must contain a finite number.", juce::dontSendNotification);
                    return;
                }
            }
            if (values[0] < 0.05 || values[0] > 1000.0) {
                error_.setText("Mass must be between 0.05 and 1000.", juce::dontSendNotification);
                return;
            }
            result.bodies[body].mass = values[0];
            result.bodies[body].position = {values[1], values[2], values[3]};
            result.bodies[body].velocity = {values[4], values[5], values[6]};
        }
        onApply_(result);
        dismiss();
    }

    void dismiss() {
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
            callout->dismiss();
    }

    SimulationState state_;
    std::function<void(const SimulationState&)> onApply_;
    juce::Label title_;
    std::array<juce::Label, 7> headings_;
    std::array<juce::Label, bodyCount> bodyLabels_;
    std::array<juce::TextEditor, bodyCount * 7> fields_;
    juce::Label error_;
    juce::TextButton apply_{"APPLY + RESET"};
    juce::TextButton cancel_{"CANCEL"};
};

} // namespace

void showAdvancedStateEditor(const SimulationState& state, juce::Rectangle<int> targetArea,
                             juce::Component& parent,
                             std::function<void(const SimulationState&)> onApply) {
    auto editor = std::make_unique<AdvancedStateEditor>(state, std::move(onApply));
    juce::CallOutBox::launchAsynchronously(std::move(editor), targetArea, &parent);
}

} // namespace threebs
