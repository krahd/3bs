// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/AdvancedConfigurationEditor.h"
#include "ui/AdvancedStateEditor.h"
#include "ui/ArtworkPanel.h"
#include "ui/NumericText.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace threebs {
// Keep page and focus access private to this native integration test.
class ArtworkPanelTestAccess {
public:
    static void selectPage(ArtworkPanel& panel, std::size_t index) { panel.tabs_.at(index).triggerClick(); }
    static std::size_t page(const ArtworkPanel& panel) { return static_cast<std::size_t>(panel.deckPage_); }
    static bool presenting(const ArtworkPanel& panel) { return panel.presentationMode_; }
    static bool factorySelectionUnmodified(const ArtworkPanel& panel) {
        return panel.configurationState_.getText() == "FACTORY CONFIGURATION";
    }
    static juce::TextEditor& presetName(ArtworkPanel& panel) { return panel.userPresetName_; }
    static juce::Component& scene(ArtworkPanel& panel) { return panel.scene_; }
    static std::vector<juce::Component*> requiredControls(ArtworkPanel& panel) {
        std::vector<juce::Component*> controls{&panel.title_, &panel.subtitle_, &panel.status_,
            &panel.notification_, &panel.pageHelp_, &panel.run_, &panel.sync_, &panel.present_,
            &panel.reset_, &panel.panic_};
        const auto add = [&controls](auto& components) {
            for (auto& component : components) controls.push_back(&component);
        };
        add(panel.tabs_);
        switch (panel.deckPage_) {
        case ArtworkPanel::DeckPage::System:
            controls.insert(controls.end(), {&panel.randomize_, &panel.advanced_, &panel.autoReset_,
                &panel.autoResetLabel_, &panel.autoResetBars_, &panel.speed_, &panel.gravity_,
                &panel.softening_, &panel.chaos_, &panel.massOne_, &panel.massTwo_, &panel.massThree_});
            break;
        case ArtworkPanel::DeckPage::Voices:
            controls.insert(controls.end(), {&panel.timeSigLabel_, &panel.timeSigSource_, &panel.timeSigNum_,
                &panel.timeSigDenom_, &panel.density_, &panel.advancedConfigurationButton_,
                &panel.voicingPreset_, &panel.voicingMode_, &panel.voiceModeContext_});
            add(panel.voiceHeaders_); add(panel.voiceEnable_); add(panel.voiceRoot_);
            add(panel.voiceScale_); add(panel.voiceOctave_); add(panel.voicePitch_);
            add(panel.voiceTrigger_); add(panel.voiceDurMap_); add(panel.voiceDurGrid_);
            add(panel.voiceDurMin_); add(panel.voiceDurMax_); add(panel.voiceRowLabels_);
            if (panel.voicingMode_.getSelectedId() >= 2)
                controls.insert(controls.end(), {&panel.chordRoot_, &panel.chordScale_,
                    &panel.chordRootLabel_, &panel.chordScaleLabel_});
            if (panel.voicingMode_.getSelectedId() == 3) {
                controls.push_back(&panel.strumUnit_);
                controls.push_back(panel.strumUnit_.getSelectedId() == 1 ? &panel.chordStrum_ : &panel.strumValue_);
            }
            break;
        case ArtworkPanel::DeckPage::Space:
            controls.insert(controls.end(), {&panel.advanced_, &panel.autoFrame_, &panel.trail_, &panel.bloom_,
                &panel.tiltOne_, &panel.tiltTwo_, &panel.tiltThree_,
                &panel.minimumCameraDistance_, &panel.maximumCameraDistance_});
            break;
        case ArtworkPanel::DeckPage::Presets:
            controls.insert(controls.end(), {&panel.randomize_, &panel.configurationState_});
            add(panel.presetCategories_); add(panel.presetCategoryLabels_);
            break;
        case ArtworkPanel::DeckPage::Settings:
            controls.insert(controls.end(), {&panel.saveConfiguration_, &panel.loadConfiguration_,
                &panel.advancedConfigurationButton_, &panel.configurationState_, &panel.userPresets_,
                &panel.userPresetName_, &panel.saveUserPreset_, &panel.loadUserPreset_,
                &panel.refreshUserPresets_, &panel.userPresetFolder_});
            if (panel.midiOutputAvailable_) controls.push_back(&panel.midiOutput_);
            break;
        }
        return controls;
    }
};
} // namespace threebs

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string componentName(const juce::Component& component) {
    if (const auto* button = dynamic_cast<const juce::Button*>(&component)) return button->getButtonText().toStdString();
    if (const auto* label = dynamic_cast<const juce::Label*>(&component)) return label->getText().toStdString();
    if (const auto* box = dynamic_cast<const juce::ComboBox*>(&component)) return "selector " + box->getText().toStdString();
    if (dynamic_cast<const juce::Slider*>(&component) != nullptr) return "slider";
    if (dynamic_cast<const juce::TextEditor*>(&component) != nullptr) return "text editor";
    return component.getName().isEmpty() ? "component" : component.getName().toStdString();
}

void checkVisibleBounds(const juce::Component& component, const juce::Component& parent) {
    const auto name = componentName(component);
    check(component.isVisible() && component.isShowing(), "required control is hidden: " + name);
    check(!component.getBounds().isEmpty(), "empty control bounds: " + name);
    check(parent.getLocalBounds().contains(component.getBounds()), "control extends outside its parent: " + name);
    int minimumHeight = 1;
    if (dynamic_cast<const juce::TextButton*>(&component) != nullptr) minimumHeight = 20;
    else if (dynamic_cast<const juce::ToggleButton*>(&component) != nullptr) minimumHeight = 16;
    else if (dynamic_cast<const juce::ComboBox*>(&component) != nullptr) minimumHeight = 18;
    else if (dynamic_cast<const juce::Slider*>(&component) != nullptr) minimumHeight = 16;
    else if (dynamic_cast<const juce::TextEditor*>(&component) != nullptr) minimumHeight = 20;
    check(component.getHeight() >= minimumHeight, "control clipped below usable height: " + name);
    if (minimumHeight > 1) check(component.getWidth() >= 32, "control clipped below usable width: " + name);
    if (const auto* slider = dynamic_cast<const juce::Slider*>(&component);
        slider != nullptr && slider->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
        check(slider->getHeight() >= 40, "rotary control has no usable dial above its value: " + name);
}

void checkPanel(threebs::ArtworkPanel& panel) {
    using Access = threebs::ArtworkPanelTestAccess;
    for (const auto* control : Access::requiredControls(panel)) checkVisibleBounds(*control, panel);
    std::vector<juce::Component*> visible;
    for (auto* child : panel.getChildren()) {
        if (!child->isVisible()) continue;
        checkVisibleBounds(*child, panel);
        for (const auto* other : visible)
            check(!child->getBounds().intersects(other->getBounds()),
                  "overlapping controls: " + componentName(*child) + " / " + componentName(*other));
        visible.push_back(child);
    }
    check(Access::scene(panel).getHeight() >= 100, "control deck consumes the whole artwork area");
}

template <typename T>
std::vector<T*> descendants(juce::Component& component) {
    std::vector<T*> found;
    for (auto* child : component.getChildren()) {
        if (auto* match = dynamic_cast<T*>(child)) found.push_back(match);
        auto nested = descendants<T>(*child);
        found.insert(found.end(), nested.begin(), nested.end());
    }
    return found;
}

juce::TextButton& button(juce::Component& component, const juce::String& name) {
    for (auto* candidate : descendants<juce::TextButton>(component))
        if (candidate->getButtonText() == name) return *candidate;
    throw std::runtime_error("dialog button not found: " + name.toStdString());
}

juce::DialogWindow* dialog(const juce::String& title) {
    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* candidate = dynamic_cast<juce::DialogWindow*>(desktop.getComponent(i));
            candidate != nullptr && candidate->getName() == title && candidate->isVisible()) return candidate;
    return nullptr;
}

juce::Component& dialogContent(const juce::String& title) {
    auto* window = dialog(title);
    check(window != nullptr && window->getContentComponent() != nullptr, "native dialog did not open: " + title.toStdString());
    return *window->getContentComponent();
}

void testNumericParser() {
    const std::array<std::pair<const char*, double>, 9> valid{{
        {"+.2", .2}, {"-1e-3", -.001}, {"1.", 1.0}, {"  +2.5E+2  ", 250.0},
        {"0", 0.0}, {"-0", 0.0}, {".001", .001}, {"1000000", 1000000.0},
        {".00000000000000000001", 1e-20}}};
    for (const auto& [source, expected] : valid) {
        double value{};
        check(threebs::parseFiniteNumber(source, value), std::string("valid number rejected: ") + source);
        check(std::abs(value - expected) <= std::max(std::numeric_limits<double>::denorm_min(),
                                                   1e-12 * std::abs(expected)),
              std::string("number parsed incorrectly: ") + source);
    }
    for (const auto* source : {"", " ", "+", "-", ".", "e", "1e", "1e+", "1..2", "1e309", "1e4294967296",
                               "abc", "nan", "NaN", "inf", "-inf", "0x10", "1,2", "1 2", "--2", "+-2"}) {
        double value{};
        check(!threebs::parseFiniteNumber(source, value), std::string("invalid number accepted: ") + source);
    }
}

class SmokeWindow final : public juce::DocumentWindow {
public:
    explicit SmokeWindow(threebs::ArtworkPanel& panel)
        : DocumentWindow("3bs controls acceptance", juce::Colour(0xff060914), closeButton), panel_(panel) {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        viewport_.setViewedComponent(&panel_, false);
        setContentNonOwned(&viewport_, false);
        setLayoutSize(900, 600);
        setTopLeftPosition(30, 30);
        setVisible(true);
        toFront(true);
    }
    ~SmokeWindow() override {
        clearContentComponent();
        viewport_.setViewedComponent(nullptr, false);
    }
    void setLayoutSize(int width, int height) {
        // CI desktops can be smaller than the supported editor sizes. Keep a
        // real visible window while the viewport retains the exact logical
        // panel geometry used for full-size control snapshots and assertions.
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto desktop = display != nullptr ? display->userArea : juce::Rectangle<int>{0, 0, 1024, 768};
        setContentComponentSize(std::min(width, std::max(320, desktop.getWidth() - 80)),
                                std::min(height, std::max(240, desktop.getHeight() - 120)));
        panel_.setSize(width, height);
        viewport_.setViewPosition(0, 0);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
private:
    threebs::ArtworkPanel& panel_;
    juce::Viewport viewport_;
};
} // namespace

class UiSmokeApplication final : public juce::JUCEApplication, private juce::Timer {
public:
    const juce::String getApplicationName() override { return "3bs UI smoke"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}
    void systemRequestedQuit() override { fail("UI acceptance window closed before test completion"); }

    void initialise(const juce::String&) override {
        setApplicationReturnValue(1);
        try {
            testNumericParser();
            if (const auto* path = std::getenv("THREEBS_UI_CAPTURE_DIR"); path != nullptr && *path != '\0') {
                captures_ = juce::File(juce::String::fromUTF8(path));
                check(captures_.createDirectory().wasOk(), "cannot create requested UI capture directory");
            }
            panel_ = std::make_unique<threebs::ArtworkPanel>(snapshots_, notes_);
            const threebs::PresetCatalog systems;
            const threebs::VoicingPresetCatalog voices;
            check(systems.valid() && voices.valid(), "factory catalogs failed during UI initialisation");
            panel_->setPresetCatalog(systems);
            panel_->setVoicingPresetCatalog(voices);
            panel_->setMidiOutputAvailable(true);
            panel_->midiOutputSelector().addItem("Test destination (no MIDI device opened)", 1);
            panel_->midiOutputSelector().setSelectedId(1, juce::dontSendNotification);
            panel_->setStatus("CONTROL LAYOUT ACCEPTANCE / NO PRESET FILES WRITTEN");
            window_ = std::make_unique<SmokeWindow>(*panel_);
            juce::Process::makeForegroundProcess();
            addSteps();
            startTimer(100);
        } catch (const std::exception& error) { fail(error.what()); }
    }

    void shutdown() override {
        stopTimer();
        dialogOwner_.reset();
        // All dialogs created by successful steps have already exited their modal state.
        for (const auto* name : {stateTitle, configurationTitle})
            if (auto* open = dialog(name)) { open->setVisible(false); open->exitModalState(0); }
        window_.reset();
        panel_.reset();
    }

private:
    using Access = threebs::ArtworkPanelTestAccess;
    static constexpr const char* stateTitle = "SET INITIAL BODY STATE";
    static constexpr const char* configurationTitle = "VOICE AND SYSTEM SETTINGS";
    static constexpr std::array<const char*, 5> pageNames{"system", "voices", "space", "presets", "settings"};

    void capture(juce::Component& target, const juce::String& name) {
        if (captures_ == juce::File{}) return;
        auto image = target.createComponentSnapshot(target.getLocalBounds(), true, 1.0F);
        check(image.isValid() && image.getWidth() == target.getWidth() && image.getHeight() == target.getHeight(),
              "JUCE control capture has incorrect dimensions");
        auto output = captures_.getChildFile(name + ".png").createOutputStream();
        check(output != nullptr && output->openedOk(), "cannot open UI capture file");
        output->setPosition(0); output->truncate();
        check(juce::PNGImageFormat{}.writeImageToStream(image, *output), "PNG control capture failed");
        output->flush();
        check(output->getStatus().wasOk(), "PNG control capture write failed");
    }

    void capture(const juce::String& name) { capture(*panel_, name); }

    void addSteps() {
        for (const auto size : {juce::Point<int>{900, 600}, {1280, 820}, {1920, 1400}}) {
            for (std::size_t page = 0; page < pageNames.size(); ++page) {
                steps_.push_back([this, size, page] {
                    window_->setLayoutSize(size.x, size.y);
                    Access::selectPage(*panel_, page);
                });
                steps_.push_back([this, size, page] {
                    check(panel_->getWidth() == size.x && panel_->getHeight() == size.y,
                          "viewport did not retain requested panel size " + std::to_string(size.x) + "x"
                              + std::to_string(size.y) + "; actual " + std::to_string(panel_->getWidth())
                              + "x" + std::to_string(panel_->getHeight()));
                    check(Access::page(*panel_) == page, "tab button did not switch pages");
                    check(Access::factorySelectionUnmodified(*panel_),
                          "initial control notifications incorrectly marked the selected factory configuration modified");
                    capture(juce::String(size.x) + "x" + juce::String(size.y) + "-" + pageNames[page]);
                    checkPanel(*panel_);
                    ++checkedPages_;
                });
            }
        }
        steps_.push_back([this] {
            window_->setLayoutSize(900, 600);
            Access::selectPage(*panel_, 1);
        });
        steps_.push_back([this] {
            for (int mode = 1; mode <= 3; ++mode) {
                panel_->voicingModeSelector().setSelectedId(mode, juce::sendNotificationSync);
                for (int unit = 1; unit <= 3; ++unit) {
                    panel_->strumUnitSelector().setSelectedId(unit, juce::sendNotificationSync);
                    checkPanel(*panel_);
                }
            }
            Access::selectPage(*panel_, 4);
        });
        steps_.push_back([this] {
            panel_->setMidiOutputAvailable(false);
            checkPanel(*panel_);
            panel_->setMidiOutputAvailable(true);
            checkPanel(*panel_);
            window_->toFront(true);
            auto& name = Access::presetName(*panel_);
            name.setText({}, false);
            name.grabKeyboardFocus();
        });
        steps_.push_back([this] {
            auto& name = Access::presetName(*panel_);
            check(name.hasKeyboardFocus(true), "native preset-name editor did not acquire keyboard focus");
            const juce::KeyPress p('P', juce::ModifierKeys{}, 'p');
            check(!panel_->keyPressed(p), "presentation shortcut consumed text-editor input");
            check(name.keyPressed(p) && name.getText() == "p", "text editor did not accept the p character");
            check(!Access::presenting(*panel_), "typing a preset name entered presentation mode");
            name.moveKeyboardFocusToSibling(true);
            const auto* next = juce::Component::getCurrentlyFocusedComponent();
            check(next != nullptr && next != &name && (next == panel_.get() || panel_->isParentOf(next)),
                  "Tab focus traversal did not reach another panel control");
            Access::selectPage(*panel_, 0);
        });
        steps_.push_back([this] {
            panel_->speedSlider().showTextBox();
        });
        steps_.push_back([this] {
            auto* focused = juce::Component::getCurrentlyFocusedComponent();
            check(dynamic_cast<juce::TextEditor*>(focused) != nullptr,
                  "slider value editor did not acquire text-entry focus");
            check(!panel_->keyPressed(juce::KeyPress('P', {}, 'p')),
                  "presentation shortcut consumed slider text-entry input");
            panel_->speedSlider().hideTextBox(true);
            panel_->grabKeyboardFocus();
            check(!panel_->keyPressed(juce::KeyPress('X', {}, 'x')), "unhandled key was consumed");
            check(panel_->keyPressed(juce::KeyPress('P', {}, 'p')) && Access::presenting(*panel_),
                  "p did not enter presentation mode");
            for (auto* child : panel_->getChildren())
                check(child->isVisible() == (child == &Access::scene(*panel_)),
                      "presentation mode left a control visible");
            check(Access::scene(*panel_).getBounds() == panel_->getLocalBounds(),
                  "presentation artwork does not fill the content area");
            check(panel_->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !Access::presenting(*panel_),
                  "Escape did not restore the control deck");
            checkPanel(*panel_);
            showStateEditor();
        });
        steps_.push_back([this] {
            auto& content = dialogContent(stateTitle);
            auto fields = descendants<juce::TextEditor>(content);
            check(fields.size() == 21, "body-state dialog does not expose all 21 fields");
            for (auto* field : fields) checkVisibleBounds(*field, content);
            capture(content, "advanced-body-state");
            fields.front()->setText("1..2", false);
            button(content, "APPLY + RESET").triggerClick();
        });
        steps_.push_back([this] {
            check(stateApplies_ == 0, "invalid body-state number reached Apply");
            auto& content = dialogContent(stateTitle);
            descendants<juce::TextEditor>(content).front()->setText("1.25", false);
            button(content, "APPLY + RESET").triggerClick();
        });
        steps_.push_back([this] {
            check(stateApplies_ == 1 && stateMass_ == 1.25, "valid body-state edit was not applied exactly once");
            check(statePosition_ == precisePosition && stateTinyVelocity_ == tinyVelocity,
                  "body-state editor rounded untouched position or velocity fields");
            check(dialog(stateTitle) == nullptr, "body-state dialog did not close after Apply");
            threebs::MusicEngine engine;
            auto config = engine.config();
            config.simulation.fixedStep = preciseFixedStep;
            threebs::showAdvancedConfigurationEditor(config, *panel_, [this](const threebs::EngineConfig& candidate) {
                ++configurationApplies_; configurationChannel_ = candidate.voices[0].channel;
                configurationFixedStep_ = candidate.simulation.fixedStep;
            });
        });
        steps_.push_back([this] {
            auto& content = dialogContent(configurationTitle);
            auto tabs = descendants<juce::TabbedComponent>(content);
            check(tabs.size() == 1 && tabs.front()->getNumTabs() == 4,
                  "advanced settings do not expose the three voice tabs and system tab");
            for (int tab = 0; tab < 4; ++tab) {
                tabs.front()->setCurrentTabIndex(tab);
                for (auto* field : descendants<juce::TextEditor>(content))
                    if (field->isShowing()) checkVisibleBounds(*field, *field->getParentComponent());
                for (auto* selector : descendants<juce::ComboBox>(content))
                    if (selector->isShowing()) checkVisibleBounds(*selector, *selector->getParentComponent());
                capture(content, tab < 3 ? "advanced-planet-" + juce::String(tab + 1)
                                         : juce::String("advanced-system"));
            }
            tabs.front()->setCurrentTabIndex(0);
            auto fields = descendants<juce::TextEditor>(content);
            check(!fields.empty(), "advanced settings have no editable fields");
            fields.front()->setText("e", false);
            button(content, "APPLY").triggerClick();
        });
        steps_.push_back([this] {
            check(configurationApplies_ == 0, "invalid advanced number reached Apply");
            auto& content = dialogContent(configurationTitle);
            descendants<juce::TextEditor>(content).front()->setText("2", false);
            button(content, "APPLY").triggerClick();
        });
        steps_.push_back([this] {
            check(configurationApplies_ == 1 && configurationChannel_ == 2,
                  "valid advanced settings were not applied exactly once");
            check(configurationFixedStep_ == preciseFixedStep,
                  "advanced settings rounded an untouched simulation step");
            check(dialog(configurationTitle) == nullptr, "advanced settings dialog did not close after Apply");
        });
        for (const bool configuration : {false, true}) {
            for (const bool queuedApply : {false, true}) {
                steps_.push_back([this, configuration] {
                    dialogOwner_ = std::make_unique<juce::Component>();
                    panel_->addAndMakeVisible(*dialogOwner_);
                    dialogOwner_->setBounds(20, 20, 100, 50);
                    threebs::MusicEngine engine;
                    if (configuration) {
                        threebs::showAdvancedConfigurationEditor(engine.config(), *dialogOwner_,
                            [this](const threebs::EngineConfig&) { ++closedOwnerApplies_; });
                    } else {
                        threebs::showAdvancedStateEditor(engine.simulation().initialState(),
                            dialogOwner_->getLocalBounds(), *dialogOwner_,
                            [this](const threebs::SimulationState&) { ++closedOwnerApplies_; });
                    }
                });
                steps_.push_back([this, configuration, queuedApply] {
                    auto& content = dialogContent(configuration ? configurationTitle : stateTitle);
                    // Queue a real Button command before destroying its owner. Never retain
                    // a raw content pointer or a copied onClick closure across destruction.
                    if (queuedApply) button(content, configuration ? "APPLY" : "APPLY + RESET").triggerClick();
                    dialogOwner_.reset();
                });
                steps_.push_back([this, configuration] {
                    check(closedOwnerApplies_ == 0, "Apply callback ran after the owning component was destroyed");
                    check(dialog(configuration ? configurationTitle : stateTitle) == nullptr,
                          "modal dialog remained open after its owning component was destroyed");
                    check(juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0,
                          "destroyed owner left a modal component blocking the application");
                });
            }
        }
    }

    void showStateEditor() {
        threebs::MusicEngine engine;
        auto source = engine.simulation().initialState();
        source.bodies[0].position.x = precisePosition;
        source.bodies[1].velocity.y = tinyVelocity;
        threebs::showAdvancedStateEditor(source, panel_->getLocalBounds(), *panel_,
            [this](const threebs::SimulationState& state) {
                ++stateApplies_; stateMass_ = state.bodies[0].mass;
                statePosition_ = state.bodies[0].position.x;
                stateTinyVelocity_ = state.bodies[1].velocity.y;
            });
    }

    void timerCallback() override {
        try {
            if (step_ < steps_.size()) { steps_[step_++](); return; }
            stopTimer();
            check(checkedPages_ == 15, "not all requested page/size combinations ran");
            std::cout << "UI acceptance passed: 15 page/size layouts, all voicing modes/units, keyboard focus, "
                         "presentation, strict numeric parsing, dialog validation and owner-close races" << std::endl;
            setApplicationReturnValue(0);
            quit();
        } catch (const std::exception& error) { fail(error.what()); }
    }

    void fail(const std::string& reason) {
        stopTimer();
        std::cerr << "UI acceptance failed at step " << step_ << ": " << reason << std::endl;
        setApplicationReturnValue(1);
        quit();
    }

    threebs::SpscQueue<threebs::RenderSnapshot, 64> snapshots_;
    threebs::NoteVisualizationQueue notes_;
    std::unique_ptr<threebs::ArtworkPanel> panel_;
    std::unique_ptr<SmokeWindow> window_;
    std::unique_ptr<juce::Component> dialogOwner_;
    juce::File captures_;
    std::vector<std::function<void()>> steps_;
    std::size_t step_{}, checkedPages_{};
    int stateApplies_{}, configurationApplies_{}, configurationChannel_{}, closedOwnerApplies_{};
    static constexpr double precisePosition = .12345678901234567;
    static constexpr double tinyVelocity = 1e-20;
    static constexpr double preciseFixedStep = 1.0 / 960.0;
    double stateMass_{}, statePosition_{}, stateTinyVelocity_{}, configurationFixedStep_{};
};

START_JUCE_APPLICATION(UiSmokeApplication)
