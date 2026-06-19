// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/ArtworkPanel.h"

#include <cmath>

namespace threebs {

ArtworkPanel::DeckLookAndFeel::DeckLookAndFeel() {
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff7bdce8));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff20283a));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd8e4ee));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111725));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff29334a));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff151c2c));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffb9c9d8));
}

void ArtworkPanel::DeckLookAndFeel::drawRotarySlider(
    juce::Graphics& graphics, int x, int y, int width, int height, float position,
    float startAngle, float endAngle, juce::Slider&) {
    const auto radius = 0.5F * static_cast<float>(std::min(width, height)) - 5.0F;
    const auto center = juce::Point<float>(static_cast<float>(x + width / 2), static_cast<float>(y + height / 2));
    const auto bounds = juce::Rectangle<float>(center.x - radius, center.y - radius, radius * 2.0F, radius * 2.0F);
    graphics.setColour(juce::Colour(0xff1a2233));
    graphics.fillEllipse(bounds);
    graphics.setColour(juce::Colour(0xff2b3851));
    graphics.drawEllipse(bounds, 1.2F);
    juce::Path arc;
    arc.addCentredArc(center.x, center.y, radius - 2.0F, radius - 2.0F, 0.0F,
                      startAngle, startAngle + position * (endAngle - startAngle), true);
    graphics.setColour(juce::Colour(0xff76d5e5));
    graphics.strokePath(arc, juce::PathStrokeType(2.6F, juce::PathStrokeType::curved));
    const auto angle = startAngle + position * (endAngle - startAngle);
    const auto point = center + juce::Point<float>(std::sin(angle), -std::cos(angle)) * (radius - 8.0F);
    graphics.fillEllipse(point.x - 2.3F, point.y - 2.3F, 4.6F, 4.6F);
}

ArtworkPanel::ArtworkPanel(SpscQueue<RenderSnapshot, 64>& snapshots)
    : scene_(snapshots) {
    setLookAndFeel(&lookAndFeel_);
    setWantsKeyboardFocus(true);
    setOpaque(true);
    addAndMakeVisible(scene_);

    title_.setText("THE THREE BODY SOLUTION", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(22.0F).withStyle("Light")));
    title_.setColour(juce::Label::textColourId, juce::Colour(0xffecf2f6));
    addAndMakeVisible(title_);
    subtitle_.setText("DETERMINISTIC ORBITAL MIDI", juce::dontSendNotification);
    subtitle_.setFont(juce::Font(juce::FontOptions(10.0F).withStyle("Bold")));
    subtitle_.setColour(juce::Label::textColourId, juce::Colour(0xff70859b));
    addAndMakeVisible(subtitle_);
    status_.setText(scene_.rendererAvailable() ? "METAL / 60 FPS" : "METAL UNAVAILABLE", juce::dontSendNotification);
    status_.setJustificationType(juce::Justification::centredRight);
    status_.setColour(juce::Label::textColourId, juce::Colour(0xff63778d));
    addAndMakeVisible(status_);

    run_.setToggleState(true, juce::dontSendNotification);
    sync_.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(run_);
    addAndMakeVisible(sync_);

    configureKnob(speed_, "x");
    configureKnob(gravity_);
    configureKnob(softening_);
    configureKnob(chaos_, "%");
    configureKnob(density_, "%");
    configureKnob(trail_, "%");
    configureKnob(bloom_, "%");
    configureKnob(massOne_);
    configureKnob(massTwo_);
    configureKnob(massThree_);

    speed_.setRange(0.0, 8.0, 0.001); speed_.setValue(1.0);
    gravity_.setRange(0.01, 3.0, 0.001); gravity_.setValue(0.65);
    softening_.setRange(0.001, 0.25, 0.001); softening_.setValue(0.04);
    chaos_.setRange(0.0, 100.0, 0.1); chaos_.setValue(20.0);
    density_.setRange(0.0, 100.0, 0.1); density_.setValue(80.0);
    trail_.setRange(5.0, 100.0, 0.1); trail_.setValue(82.0);
    bloom_.setRange(0.0, 100.0, 0.1); bloom_.setValue(34.0);
    for (auto* mass : massSliders()) { mass->setRange(0.05, 8.0, 0.001); mass->setValue(1.0); }

    const std::array<juce::String, 10> labels{
        "SPEED", "GRAVITY", "SOFTEN", "CHAOS", "DENSITY", "TRAIL", "BLOOM", "MASS I", "MASS II", "MASS III"};
    for (std::size_t i = 0; i < knobLabels_.size(); ++i) {
        knobLabels_[i].setText(labels[i], juce::dontSendNotification);
        knobLabels_[i].setJustificationType(juce::Justification::centred);
        knobLabels_[i].setFont(juce::Font(juce::FontOptions(9.0F).withStyle("Bold")));
        knobLabels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff73869b));
        addAndMakeVisible(knobLabels_[i]);
    }

    presets_.setTextWhenNothingSelected("Select a system");
    midiOutput_.setTextWhenNothingSelected("MIDI output");
    addAndMakeVisible(presets_);
    addAndMakeVisible(midiOutput_);
    midiOutput_.setVisible(false);
    addAndMakeVisible(randomize_);
    addAndMakeVisible(reset_);
    addAndMakeVisible(advanced_);
    addAndMakeVisible(present_);
    randomize_.onClick = [this] { if (onRandomize) onRandomize(); };
    reset_.onClick = [this] { if (onReset) onReset(); };
    advanced_.onClick = [this] { if (onAdvanced) onAdvanced(); };
    present_.onClick = [this] { togglePresentation(); };

    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        addAndMakeVisible(tabs_[i]);
        tabs_[i].setClickingTogglesState(true);
        tabs_[i].setRadioGroupId(314);
    }
    tabs_[0].setToggleState(true, juce::dontSendNotification);
}

void ArtworkPanel::configureKnob(juce::Slider& slider, const juce::String& suffix) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
    slider.setTextValueSuffix(suffix);
    addAndMakeVisible(slider);
}

void ArtworkPanel::paint(juce::Graphics& graphics) {
    graphics.fillAll(juce::Colour(0xff060914));
    if (!presentationMode_) {
        const auto deck = getLocalBounds().removeFromBottom(224).toFloat();
        juce::ColourGradient gradient(juce::Colour(0xf2171d2b), deck.getX(), deck.getY(),
                                      juce::Colour(0xff090d17), deck.getX(), deck.getBottom(), false);
        graphics.setGradientFill(gradient);
        graphics.fillRect(deck);
        graphics.setColour(juce::Colour(0xff29334a));
        graphics.drawHorizontalLine(static_cast<int>(deck.getY()), 0.0F, static_cast<float>(getWidth()));
    }
}

void ArtworkPanel::resized() {
    auto bounds = getLocalBounds();
    if (presentationMode_) {
        scene_.setBounds(bounds);
        return;
    }
    auto header = bounds.removeFromTop(54).reduced(18, 7);
    title_.setBounds(header.removeFromLeft(340));
    subtitle_.setBounds(header.removeFromLeft(230));
    present_.setBounds(header.removeFromRight(86).reduced(2, 5));
    status_.setBounds(header.removeFromRight(140));
    auto deck = bounds.removeFromBottom(224);
    scene_.setBounds(bounds);

    auto tabs = deck.removeFromTop(34).reduced(16, 3);
    for (auto& tab : tabs_) tab.setBounds(tabs.removeFromLeft(82).reduced(2, 0));
    run_.setBounds(tabs.removeFromRight(62));
    sync_.setBounds(tabs.removeFromRight(104));

    auto content = deck.reduced(18, 8);
    auto left = content.removeFromLeft(240);
    presets_.setBounds(left.removeFromTop(30));
    left.removeFromTop(8);
    randomize_.setBounds(left.removeFromTop(28).removeFromLeft(112));
    reset_.setBounds(left.removeFromTop(28).removeFromLeft(112));
    advanced_.setBounds(left.removeFromTop(28));
    midiOutput_.setBounds(left.removeFromTop(28));

    std::array<juce::Slider*, 10> knobs{
        &speed_, &gravity_, &softening_, &chaos_, &density_, &trail_, &bloom_, &massOne_, &massTwo_, &massThree_};
    const auto cellWidth = std::max(58, content.getWidth() / static_cast<int>(knobs.size()));
    for (std::size_t i = 0; i < knobs.size(); ++i) {
        auto cell = content.removeFromLeft(cellWidth);
        knobLabels_[i].setBounds(cell.removeFromTop(18));
        knobs[i]->setBounds(cell.reduced(1));
    }
}

bool ArtworkPanel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && presentationMode_) {
        togglePresentation();
        return true;
    }
    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P') {
        togglePresentation();
        return true;
    }
    return false;
}

void ArtworkPanel::togglePresentation() {
    presentationMode_ = !presentationMode_;
    for (auto* child : getChildren())
        child->setVisible(presentationMode_ ? child == &scene_ : true);
    resized();
    repaint();
    grabKeyboardFocus();
}

void ArtworkPanel::setVisualSettings(const VisualSettings& settings) {
    visual_ = settings;
    scene_.setVisualSettings(visual_);
    trail_.setValue(visual_.trailLength * 100.0, juce::dontSendNotification);
    bloom_.setValue(visual_.bloom * 100.0, juce::dontSendNotification);
}

void ArtworkPanel::setPresetNames(const juce::StringArray& names) {
    presets_.clear();
    presets_.addItemList(names, 1);
    if (!names.isEmpty()) presets_.setSelectedId(1, juce::dontSendNotification);
}

void ArtworkPanel::setStatus(const juce::String& status) {
    status_.setText(status, juce::dontSendNotification);
}

} // namespace threebs
