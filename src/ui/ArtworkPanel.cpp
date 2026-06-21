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
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff276675));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffb9c9d8));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xfff2fbff));
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

ArtworkPanel::ArtworkPanel(SpscQueue<RenderSnapshot, 64>& snapshots,
                           NoteVisualizationQueue& noteVisualizationEvents)
    : scene_(snapshots, noteVisualizationEvents) {
    setLookAndFeel(&lookAndFeel_);
    setWantsKeyboardFocus(true);
    setOpaque(true);
    addAndMakeVisible(scene_);
    scene_.onCameraInteractionComplete = [this](const CameraState& camera) {
        presentation_.camera = camera;
        minimumCameraDistance_.setValue(camera.minimumDistance, juce::dontSendNotification);
        maximumCameraDistance_.setValue(camera.maximumDistance, juce::dontSendNotification);
        autoFrame_.setToggleState(camera.autoFrame, juce::dontSendNotification);
        if (onCameraChanged)
            onCameraChanged(camera);
    };
    scene_.onNotePaneMinimizedChanged = [this](bool minimized) {
        presentation_.notePaneMinimized = minimized;
        if (onNotePaneMinimizedChanged)
            onNotePaneMinimizedChanged(minimized);
    };
    scene_.onNotePaneStyleChanged = [this](NotePaneStyle style) {
        presentation_.notePaneStyle = style;
        if (onNotePaneStyleChanged)
            onNotePaneStyleChanged(style);
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
    pageHelp_.setFont(juce::Font(juce::FontOptions(10.5F)));
    pageHelp_.setColour(juce::Label::textColourId, juce::Colour(0xff7f93a8));
    pageHelp_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(pageHelp_);

    run_.setToggleState(true, juce::dontSendNotification);
    sync_.setToggleState(true, juce::dontSendNotification);
    sync_.setTooltip("When enabled, generation follows host play, seek, and loop state. When disabled, RUN advances independently using the current tempo.");
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
    configureKnob(minimumCameraDistance_);
    configureKnob(maximumCameraDistance_);

    speed_.setRange(0.0, 8.0, 0.001); speed_.setValue(1.0);
    gravity_.setRange(0.01, 3.0, 0.001); gravity_.setValue(0.65);
    softening_.setRange(0.001, 0.25, 0.001); softening_.setValue(0.04);
    chaos_.setRange(0.0, 100.0, 0.1); chaos_.setValue(20.0);
    density_.setRange(0.0, 100.0, 0.1); density_.setValue(80.0);
    trail_.setRange(5.0, 60.0, 0.1); trail_.setValue(30.0);
    bloom_.setRange(0.0, 100.0, 0.1); bloom_.setValue(34.0);
    for (auto* mass : massSliders()) { mass->setRange(0.05, 8.0, 0.001); mass->setValue(1.0); }
    for (auto* tilt : planeTiltSliders()) { tilt->setRange(-75.0, 75.0, 0.1); tilt->setValue(0.0); }
    minimumCameraDistance_.setRange(1.0, 20.0, 0.1); minimumCameraDistance_.setValue(2.5);
    maximumCameraDistance_.setRange(5.0, 80.0, 0.1); maximumCameraDistance_.setValue(40.0);
    autoFrame_.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(autoFrame_);

    const std::array<juce::String, bodyCount> voiceNames{"PLANET I", "PLANET II", "PLANET III"};
    for (std::size_t body = 0; body < bodyCount; ++body) {
        voiceHeaders_[body].setText(voiceNames[body], juce::dontSendNotification);
        voiceHeaders_[body].setJustificationType(juce::Justification::centred);
        voiceHeaders_[body].setFont(juce::Font(juce::FontOptions(10.0F).withStyle("Bold")));
        voiceHeaders_[body].setColour(juce::Label::textColourId, juce::Colour(0xff9fb2c6));
        addAndMakeVisible(voiceHeaders_[body]);

        voiceEnable_[body].setButtonText("ON");
        voiceEnable_[body].setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(voiceEnable_[body]);

        voiceScale_[body].addItemList(scaleDisplayNames(), 1);
        voiceScale_[body].setSelectedId(static_cast<int>(ScaleId::MinorPentatonic) + 1,
                                        juce::dontSendNotification);
        addAndMakeVisible(voiceScale_[body]);

        voiceRoot_[body].addItemList({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
        voiceRoot_[body].setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(voiceRoot_[body]);

        voicePitch_[body].addItemList(pitchMappingDisplayNames(), 1);
        voicePitch_[body].setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(voicePitch_[body]);

        voiceTrigger_[body].addItemList(triggerMappingDisplayNames(), 1);
        voiceTrigger_[body].setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(voiceTrigger_[body]);
    }
    const std::array<juce::String, 4> voiceRowNames{"ROOT", "SCALE", "PITCH MAP", "TRIGGER"};
    for (std::size_t i = 0; i < voiceRowLabels_.size(); ++i) {
        voiceRowLabels_[i].setText(voiceRowNames[i], juce::dontSendNotification);
        voiceRowLabels_[i].setJustificationType(juce::Justification::centredLeft);
        voiceRowLabels_[i].setFont(juce::Font(juce::FontOptions(8.5F).withStyle("Bold")));
        voiceRowLabels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff63778d));
        addAndMakeVisible(voiceRowLabels_[i]);
    }

    voicingMode_.addItemList({"INDEPENDENT", "CHORD", "STRUM"}, 1);
    voicingMode_.setSelectedId(1, juce::dontSendNotification);
    voicingMode_.setTooltip("Independent triggers each planet separately. Chord and Strum generate scale triads from all enabled planets.");
    addAndMakeVisible(voicingMode_);
    voicingMode_.addListener(this);
    voiceModeContext_.setFont(juce::Font(juce::FontOptions(10.0F).withStyle("Bold")));
    voiceModeContext_.setColour(juce::Label::textColourId, juce::Colour(0xff8fa9bd));
    voiceModeContext_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(voiceModeContext_);
    chordStrum_.setSliderStyle(juce::Slider::LinearHorizontal);
    chordStrum_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 20);
    chordStrum_.setRange(0.0, 120.0, 1.0);
    chordStrum_.setValue(24.0);
    chordStrum_.setTextValueSuffix(" ms");
    chordStrum_.setTooltip("Delay between planets in STRUM mode.");
    addAndMakeVisible(chordStrum_);

    const std::array<juce::String, 15> labels{
        "SPEED", "GRAVITY", "SOFTEN", "CHAOS", "DENSITY", "TRAIL", "BLOOM",
        "MASS I", "MASS II", "MASS III", "TILT I", "TILT II", "TILT III",
        "ZOOM IN", "ZOOM OUT"};
    for (std::size_t i = 0; i < knobLabels_.size(); ++i) {
        knobLabels_[i].setText(labels[i], juce::dontSendNotification);
        knobLabels_[i].setJustificationType(juce::Justification::centred);
        knobLabels_[i].setFont(juce::Font(juce::FontOptions(11.5F).withStyle("Bold")));
        knobLabels_[i].setColour(juce::Label::textColourId, juce::Colour(0xff73869b));
        addAndMakeVisible(knobLabels_[i]);
    }

    midiOutput_.setTextWhenNothingSelected("MIDI output");
    for (std::size_t category = 0; category < presetCategories_.size(); ++category) {
        auto& selector = presetCategories_[category];
        selector.setTextWhenNothingSelected({});
        addAndMakeVisible(selector);
        auto* selectedBox = &selector;
        selector.onChange = [this, selectedBox] {
            const auto id = selectedBox->getSelectedId();
            if (id <= 0)
                return;
            for (auto& other : presetCategories_)
                if (&other != selectedBox)
                    other.setSelectedId(0, juce::dontSendNotification);
            if (onPresetSelected)
                onPresetSelected(id - 1);
        };
        presetCategoryLabels_[category].setFont(
            juce::Font(juce::FontOptions(10.0F).withStyle("Bold")));
        presetCategoryLabels_[category].setColour(
            juce::Label::textColourId, juce::Colour(0xff71889d));
        presetCategoryLabels_[category].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(presetCategoryLabels_[category]);
    }
    addAndMakeVisible(midiOutput_);
    midiOutput_.setVisible(false);
    addAndMakeVisible(randomize_);
    addAndMakeVisible(reset_);
    addAndMakeVisible(advanced_);
    addAndMakeVisible(saveConfiguration_);
    addAndMakeVisible(loadConfiguration_);
    addAndMakeVisible(present_);
    randomize_.onClick = [this] { if (onRandomize) onRandomize(); };
    reset_.onClick = [this] { if (onReset) onReset(); };
    advanced_.onClick = [this] { if (onAdvanced) onAdvanced(); };
    saveConfiguration_.onClick = [this] { if (onSaveConfiguration) onSaveConfiguration(); };
    loadConfiguration_.onClick = [this] { if (onLoadConfiguration) onLoadConfiguration(); };
    present_.onClick = [this] { togglePresentation(); };

    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        addAndMakeVisible(tabs_[i]);
        tabs_[i].setClickingTogglesState(true);
        tabs_[i].setRadioGroupId(314);
        tabs_[i].onClick = [this, i] { selectDeckPage(static_cast<DeckPage>(i)); };
    }
    tabs_[0].setToggleState(true, juce::dontSendNotification);
    updateVoiceModeControls();
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
    pageHelp_.setBounds(left.removeFromTop(50));
    left.removeFromTop(6);
    reset_.setBounds(left.removeFromBottom(28).removeFromLeft(124));

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
        left.removeFromTop(6);
        advanced_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        layoutKnobs({{0U, &speed_}, {1U, &gravity_}, {2U, &softening_}, {3U, &chaos_},
                     {7U, &massOne_}, {8U, &massTwo_}, {9U, &massThree_}}, content);
        break;
    case DeckPage::Voices: {
        voicingMode_.setBounds(left.removeFromTop(28));
        left.removeFromTop(4);
        chordStrum_.setBounds(left.removeFromTop(28));
        layoutKnobs({{4U, &density_}}, left.removeFromTop(82));
        auto grid = content;
        auto gutter = grid.removeFromLeft(70);
        const auto columnWidth = grid.getWidth() / static_cast<int>(bodyCount);
        voiceModeContext_.setBounds(grid.removeFromTop(18));
        gutter.removeFromTop(18);
        auto headerRow = grid.removeFromTop(16);
        gutter.removeFromTop(16);
        for (std::size_t body = 0; body < bodyCount; ++body)
            voiceHeaders_[body].setBounds(headerRow.removeFromLeft(columnWidth));
        grid.removeFromTop(2);
        gutter.removeFromTop(2);
        auto enableRow = grid.removeFromTop(20);
        gutter.removeFromTop(20);
        for (std::size_t body = 0; body < bodyCount; ++body)
            voiceEnable_[body].setBounds(enableRow.removeFromLeft(columnWidth).reduced(10, 1));
        grid.removeFromTop(2);
        gutter.removeFromTop(2);
        const auto comboRow = [&](std::array<juce::ComboBox, bodyCount>& combos, std::size_t labelIndex) {
            auto row = grid.removeFromTop(22);
            voiceRowLabels_[labelIndex].setBounds(gutter.removeFromTop(22));
            for (std::size_t body = 0; body < bodyCount; ++body)
                combos[body].setBounds(row.removeFromLeft(columnWidth).reduced(4, 1));
            grid.removeFromTop(2);
            gutter.removeFromTop(2);
        };
        comboRow(voiceRoot_, 0U);
        comboRow(voiceScale_, 1U);
        comboRow(voicePitch_, 2U);
        comboRow(voiceTrigger_, 3U);
        break;
    }
    case DeckPage::Space:
        advanced_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        left.removeFromTop(6);
        autoFrame_.setBounds(left.removeFromTop(28));
        layoutKnobs({{5U, &trail_}, {6U, &bloom_}, {10U, &tiltOne_}, {11U, &tiltTwo_}, {12U, &tiltThree_},
                     {13U, &minimumCameraDistance_}, {14U, &maximumCameraDistance_}},
                    content);
        break;
    case DeckPage::Presets:
        {
        auto categories = content;
        const auto categoryWidth = categories.getWidth() / static_cast<int>(presetCategories_.size());
        for (std::size_t category = 0; category < presetCategories_.size(); ++category) {
            auto cell = categories.removeFromLeft(categoryWidth).reduced(5, 2);
            presetCategoryLabels_[category].setBounds(cell.removeFromTop(22));
            presetCategories_[category].setBounds(cell.removeFromTop(30));
        }
        randomize_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        break;
        }
    case DeckPage::Settings:
        saveConfiguration_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        left.removeFromTop(5);
        loadConfiguration_.setBounds(left.removeFromTop(28).removeFromLeft(124));
        left.removeFromTop(8);
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
    minimumCameraDistance_.setVisible(false);
    maximumCameraDistance_.setVisible(false);
    autoFrame_.setVisible(false);
    for (auto& selector : presetCategories_) selector.setVisible(false);
    for (auto& label : presetCategoryLabels_) label.setVisible(false);
    midiOutput_.setVisible(false);
    randomize_.setVisible(false);
    reset_.setVisible(showBase);
    advanced_.setVisible(false);
    saveConfiguration_.setVisible(false);
    loadConfiguration_.setVisible(false);
    for (auto& label : knobLabels_)
        label.setVisible(false);
    for (std::size_t body = 0; body < bodyCount; ++body) {
        voiceHeaders_[body].setVisible(false);
        voiceEnable_[body].setVisible(false);
        voiceScale_[body].setVisible(false);
        voiceRoot_[body].setVisible(false);
        voicePitch_[body].setVisible(false);
        voiceTrigger_[body].setVisible(false);
    }
    voicingMode_.setVisible(false);
    chordStrum_.setVisible(false);
    voiceModeContext_.setVisible(false);
    for (auto& label : voiceRowLabels_)
        label.setVisible(false);

    if (!showBase)
        return;

    const auto showKnob = [this](std::size_t label, juce::Slider& slider) {
        knobLabels_[label].setVisible(true);
        slider.setVisible(true);
    };
    switch (deckPage_) {
    case DeckPage::System:
        pageHelp_.setText("Physics, masses, reset, and exact body-state editing.", juce::dontSendNotification);
        randomize_.setVisible(true);
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
        pageHelp_.setText("Per-planet enable, scale/mode, and movement-to-note mapping. Density is global.",
                          juce::dontSendNotification);
        showKnob(4U, density_);
        voicingMode_.setVisible(true);
        chordStrum_.setVisible(voicingMode_.getSelectedItemIndex() == 2);
        voiceModeContext_.setVisible(true);
        for (std::size_t body = 0; body < bodyCount; ++body) {
            voiceHeaders_[body].setVisible(true);
            voiceEnable_[body].setVisible(true);
            voiceScale_[body].setVisible(true);
            voiceRoot_[body].setVisible(true);
            voicePitch_[body].setVisible(true);
            voiceTrigger_[body].setVisible(true);
        }
        for (auto& label : voiceRowLabels_)
            label.setVisible(true);
        break;
    case DeckPage::Space:
        pageHelp_.setText("Visuals, orbital-plane tilt, and automatic camera framing limits.",
                          juce::dontSendNotification);
        advanced_.setVisible(true);
        autoFrame_.setVisible(true);
        showKnob(5U, trail_);
        showKnob(6U, bloom_);
        showKnob(10U, tiltOne_);
        showKnob(11U, tiltTwo_);
        showKnob(12U, tiltThree_);
        showKnob(13U, minimumCameraDistance_);
        showKnob(14U, maximumCameraDistance_);
        break;
    case DeckPage::Presets:
        pageHelp_.setText("Choose a deterministic authored system, randomize from the current chaos amount, or reset.",
                          juce::dontSendNotification);
        for (auto& selector : presetCategories_) selector.setVisible(true);
        for (auto& label : presetCategoryLabels_) label.setVisible(true);
        randomize_.setVisible(true);
        break;
    case DeckPage::Settings:
        pageHelp_.setText(midiOutputAvailable_ ? "Save/load complete .3bs JSON configurations or choose the CoreMIDI destination."
                                                : "Save or load complete portable .3bs JSON configurations.",
                          juce::dontSendNotification);
        saveConfiguration_.setVisible(true);
        loadConfiguration_.setVisible(true);
        midiOutput_.setVisible(midiOutputAvailable_);
        break;
    }
}

void ArtworkPanel::setPresentationState(const PresentationState& state) {
    presentation_ = state;
    scene_.setPresentationState(presentation_);
    trail_.setValue(presentation_.visual.trailSeconds, juce::dontSendNotification);
    bloom_.setValue(presentation_.visual.bloom * 100.0, juce::dontSendNotification);
    minimumCameraDistance_.setValue(presentation_.camera.minimumDistance, juce::dontSendNotification);
    maximumCameraDistance_.setValue(presentation_.camera.maximumDistance, juce::dontSendNotification);
    autoFrame_.setToggleState(presentation_.camera.autoFrame, juce::dontSendNotification);
}

void ArtworkPanel::setPresetCatalog(const PresetCatalog& catalog) {
    for (auto& selector : presetCategories_)
        selector.clear();
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const auto system = catalog[index].system;
        std::size_t category{};
        if (system == InitialSystem::Hierarchical) category = 1U;
        else if (system == InitialSystem::Stable) category = 2U;
        else if (system == InitialSystem::ControlledChaos || system == InitialSystem::Unbound) category = 3U;
        presetCategories_[category].addItem(catalog[index].name, static_cast<int>(index + 1U));
    }
    for (std::size_t category = 0; category < presetCategories_.size(); ++category) {
        const std::array<InitialSystem, 4> systems{InitialSystem::FigureEight, InitialSystem::Hierarchical,
            InitialSystem::Stable, InitialSystem::ControlledChaos};
        presetCategoryLabels_[category].setText(presetCategoryName(systems[category]),
                                                juce::dontSendNotification);
        presetCategories_[category].setTextWhenNothingSelected({});
    }
    if (catalog.size() > 0U)
        setSelectedPresetIndex(0);
}

void ArtworkPanel::comboBoxChanged(juce::ComboBox* comboBox) {
    if (comboBox == &voicingMode_)
        updateVoiceModeControls();
}

void ArtworkPanel::updateVoiceModeControls() {
    const auto mode = std::max(0, voicingMode_.getSelectedItemIndex());
    if (mode == 0) {
        voiceModeContext_.setText("INDEPENDENT NOTES / EACH PLANET USES ITS OWN TRIGGER",
                                  juce::dontSendNotification);
        voiceRowLabels_[2].setText("PITCH MAP", juce::dontSendNotification);
        voiceRowLabels_[3].setText("TRIGGER", juce::dontSendNotification);
    } else if (mode == 1) {
        voiceModeContext_.setText("CHORD / ANY PLANET TRIGGER LAUNCHES SCALE DEGREES 0 / 2 / 4",
                                  juce::dontSendNotification);
        voiceRowLabels_[2].setText("DEGREE SRC", juce::dontSendNotification);
        voiceRowLabels_[3].setText("CHORD TRIG", juce::dontSendNotification);
    } else {
        voiceModeContext_.setText("STRUM / PHYSICAL TRIGGER LAUNCHES A ROTATING SHORT SEQUENCE",
                                  juce::dontSendNotification);
        voiceRowLabels_[2].setText("DEGREE SRC", juce::dontSendNotification);
        voiceRowLabels_[3].setText("STRUM TRIG", juce::dontSendNotification);
    }
    chordStrum_.setVisible(!presentationMode_ && deckPage_ == DeckPage::Voices && mode == 2);
    resized();
    repaint();
}

void ArtworkPanel::setSelectedPresetIndex(int index) {
    const auto id = index + 1;
    for (auto& selector : presetCategories_) {
        bool contains{};
        for (int item = 0; item < selector.getNumItems(); ++item)
            contains = contains || selector.getItemId(item) == id;
        selector.setSelectedId(contains ? id : 0, juce::dontSendNotification);
    }
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
