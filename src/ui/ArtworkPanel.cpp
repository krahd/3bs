// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/ArtworkPanel.h"

#include <cmath>
#include <initializer_list>

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
    scene_.onCameraInteractionComplete = [this](const CameraState& camera) {
        presentation_.camera = camera;
        if (onCameraChanged)
            onCameraChanged(camera);
    };

    title_.setText("THE THREE BODY SOLUTION", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(22.0F).withStyle("Light")));
    title_.setColour(juce::Label::textColourId, juce::Colour(0xffecf2f6));
    addAndMakeVisible(title_);
    subtitle_.setText("DRAG ORBIT  /  CLICK FOCUS  /  SCROLL ZOOM", juce::dontSendNotification);
    subtitle_.setFont(juce::Font(juce::FontOptions(10.0F).withStyle("Bold")));
    subtitle_.setColour(juce::Label::textColourId, juce::Colour(0xff70859b));
    addAndMakeVisible(subtitle_);
    status_.setText(scene_.rendererAvailable() ? "METAL / 60 FPS" : "METAL UNAVAILABLE", juce::dontSendNotification);
    status_.setJustificationType(juce::Justification::centredRight);
    status_.setColour(juce::Label::textColourId, juce::Colour(0xff63778d));
    addAndMakeVisible(status_);
    pageTitle_.setFont(juce::Font(juce::FontOptions(13.0F).withStyle("Bold")));
    pageTitle_.setColour(juce::Label::textColourId, juce::Colour(0xffdce7ef));
    addAndMakeVisible(pageTitle_);
    pageHelp_.setFont(juce::Font(juce::FontOptions(10.5F)));
    pageHelp_.setColour(juce::Label::textColourId, juce::Colour(0xff7f93a8));
    pageHelp_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(pageHelp_);

    run_.setToggleState(true, juce::dontSendNotification);
    sync_.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(run_);
    addAndMakeVisible(sync_);

    configureKnob(speed_, "x");
    configureKnob(gravity_);
    configureKnob(softening_);
    configureKnob(chaos_, "%");
    configureKnob(density_, "%");
    configureKnob(trail_, " s");
    configureKnob(bloom_, "%");
    configureKnob(massOne_);
    configureKnob(massTwo_);
    configureKnob(massThree_);
    configureKnob(tiltOne_, " deg");
    configureKnob(tiltTwo_, " deg");
    configureKnob(tiltThree_, " deg");

    speed_.setRange(0.0, 8.0, 0.001); speed_.setValue(1.0);
    gravity_.setRange(0.01, 3.0, 0.001); gravity_.setValue(0.65);
    softening_.setRange(0.001, 0.25, 0.001); softening_.setValue(0.04);
    chaos_.setRange(0.0, 100.0, 0.1); chaos_.setValue(20.0);
    density_.setRange(0.0, 100.0, 0.1); density_.setValue(80.0);
    trail_.setRange(5.0, 60.0, 0.1); trail_.setValue(30.0);
    bloom_.setRange(0.0, 100.0, 0.1); bloom_.setValue(34.0);
    for (auto* mass : massSliders()) { mass->setRange(0.05, 8.0, 0.001); mass->setValue(1.0); }
    for (auto* tilt : planeTiltSliders()) { tilt->setRange(-75.0, 75.0, 0.1); tilt->setValue(0.0); }

    const std::array<juce::String, 13> labels{
        "SPEED", "GRAVITY", "SOFTEN", "CHAOS", "DENSITY", "TRAIL", "BLOOM",
        "MASS I", "MASS II", "MASS III", "TILT I", "TILT II", "TILT III"};
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
        tabs_[i].onClick = [this, i] { selectDeckPage(static_cast<DeckPage>(i)); };
    }
    tabs_[0].setToggleState(true, juce::dontSendNotification);
    updateDeckVisibility();
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
    auto left = content.removeFromLeft(262);
    pageTitle_.setBounds(left.removeFromTop(24));
    pageHelp_.setBounds(left.removeFromTop(50));
    left.removeFromTop(6);

    const auto layoutKnobs = [this](std::initializer_list<std::pair<std::size_t, juce::Slider*>> knobs,
                                    juce::Rectangle<int> area) {
        if (knobs.size() == 0U)
            return;
        const auto cellWidth = std::max(72, area.getWidth() / static_cast<int>(knobs.size()));
        for (const auto& knob : knobs) {
            auto cell = area.removeFromLeft(cellWidth);
            knobLabels_[knob.first].setBounds(cell.removeFromTop(18));
            knob.second->setBounds(cell.reduced(1));
        }
    };

    switch (deckPage_) {
    case DeckPage::System:
        randomize_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        reset_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        left.removeFromTop(6);
        advanced_.setBounds(left.removeFromTop(28));
        layoutKnobs({{0U, &speed_}, {1U, &gravity_}, {2U, &softening_}, {3U, &chaos_},
                     {7U, &massOne_}, {8U, &massTwo_}, {9U, &massThree_}}, content);
        break;
    case DeckPage::Voices:
        layoutKnobs({{4U, &density_}}, left.removeFromTop(96));
        break;
    case DeckPage::Space:
        advanced_.setBounds(left.removeFromTop(28));
        layoutKnobs({{5U, &trail_}, {6U, &bloom_}, {10U, &tiltOne_}, {11U, &tiltTwo_}, {12U, &tiltThree_}},
                    content);
        break;
    case DeckPage::Presets:
        presets_.setBounds(left.removeFromTop(30));
        left.removeFromTop(8);
        randomize_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        reset_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        break;
    case DeckPage::Settings:
        if (midiOutputAvailable_)
            midiOutput_.setBounds(left.removeFromTop(30));
        break;
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
    updateDeckVisibility();
    resized();
    repaint();
    grabKeyboardFocus();
}

void ArtworkPanel::selectDeckPage(DeckPage page) {
    deckPage_ = page;
    for (std::size_t i = 0; i < tabs_.size(); ++i)
        tabs_[i].setToggleState(i == static_cast<std::size_t>(page), juce::dontSendNotification);
    updateDeckVisibility();
    resized();
    repaint();
}

void ArtworkPanel::updateDeckVisibility() {
    const auto showBase = !presentationMode_;
    scene_.setVisible(true);
    title_.setVisible(showBase);
    subtitle_.setVisible(showBase);
    status_.setVisible(showBase);
    pageTitle_.setVisible(showBase);
    pageHelp_.setVisible(showBase);
    run_.setVisible(showBase);
    sync_.setVisible(showBase);
    present_.setVisible(showBase);
    for (auto& tab : tabs_)
        tab.setVisible(showBase);

    speed_.setVisible(false);
    gravity_.setVisible(false);
    softening_.setVisible(false);
    chaos_.setVisible(false);
    density_.setVisible(false);
    trail_.setVisible(false);
    bloom_.setVisible(false);
    massOne_.setVisible(false);
    massTwo_.setVisible(false);
    massThree_.setVisible(false);
    tiltOne_.setVisible(false);
    tiltTwo_.setVisible(false);
    tiltThree_.setVisible(false);
    presets_.setVisible(false);
    midiOutput_.setVisible(false);
    randomize_.setVisible(false);
    reset_.setVisible(false);
    advanced_.setVisible(false);
    for (auto& label : knobLabels_)
        label.setVisible(false);

    if (!showBase)
        return;

    const auto showKnob = [this](std::size_t label, juce::Slider& slider) {
        knobLabels_[label].setVisible(true);
        slider.setVisible(true);
    };
    switch (deckPage_) {
    case DeckPage::System:
        pageTitle_.setText("SYSTEM", juce::dontSendNotification);
        pageHelp_.setText("Physics, masses, reset, and exact body-state editing.", juce::dontSendNotification);
        randomize_.setVisible(true);
        reset_.setVisible(true);
        advanced_.setVisible(true);
        showKnob(0U, speed_);
        showKnob(1U, gravity_);
        showKnob(2U, softening_);
        showKnob(3U, chaos_);
        showKnob(7U, massOne_);
        showKnob(8U, massTwo_);
        showKnob(9U, massThree_);
        break;
    case DeckPage::Voices:
        pageTitle_.setText("VOICES", juce::dontSendNotification);
        pageHelp_.setText("Global note density is live. Detailed voice mapping follows the selected preset for now.",
                          juce::dontSendNotification);
        showKnob(4U, density_);
        break;
    case DeckPage::Space:
        pageTitle_.setText("SPACE", juce::dontSendNotification);
        pageHelp_.setText("Visual trails, bloom, and extra initial orbital-plane tilt per body.",
                          juce::dontSendNotification);
        advanced_.setVisible(true);
        showKnob(5U, trail_);
        showKnob(6U, bloom_);
        showKnob(10U, tiltOne_);
        showKnob(11U, tiltTwo_);
        showKnob(12U, tiltThree_);
        break;
    case DeckPage::Presets:
        pageTitle_.setText("PRESETS", juce::dontSendNotification);
        pageHelp_.setText("Choose a deterministic authored system, randomize from the current chaos amount, or reset.",
                          juce::dontSendNotification);
        presets_.setVisible(true);
        randomize_.setVisible(true);
        reset_.setVisible(true);
        break;
    case DeckPage::Settings:
        pageTitle_.setText("SETTINGS", juce::dontSendNotification);
        pageHelp_.setText(midiOutputAvailable_ ? "Standalone CoreMIDI destination. Host plugins route MIDI through the host."
                                                : "Host plugins route MIDI through the host. Standalone exposes CoreMIDI here.",
                          juce::dontSendNotification);
        midiOutput_.setVisible(midiOutputAvailable_);
        break;
    }
}

void ArtworkPanel::setPresentationState(const PresentationState& state) {
    presentation_ = state;
    scene_.setPresentationState(presentation_);
    trail_.setValue(presentation_.visual.trailSeconds, juce::dontSendNotification);
    bloom_.setValue(presentation_.visual.bloom * 100.0, juce::dontSendNotification);
}

void ArtworkPanel::setPresetNames(const juce::StringArray& names) {
    presets_.clear();
    presets_.addItemList(names, 1);
    if (!names.isEmpty()) presets_.setSelectedId(1, juce::dontSendNotification);
}

void ArtworkPanel::setStatus(const juce::String& status) {
    status_.setText(status, juce::dontSendNotification);
}

void ArtworkPanel::setMidiOutputAvailable(bool available) {
    midiOutputAvailable_ = available;
    updateDeckVisibility();
}

void ArtworkPanel::setPlaneTilts(const std::array<double, bodyCount>& tiltDegrees) {
    const auto sliders = planeTiltSliders();
    for (std::size_t body = 0; body < bodyCount; ++body)
        sliders[body]->setValue(tiltDegrees[body], juce::dontSendNotification);
}

std::array<double, bodyCount> ArtworkPanel::planeTilts() const noexcept {
    return {tiltOne_.getValue(), tiltTwo_.getValue(), tiltThree_.getValue()};
}

} // namespace threebs
