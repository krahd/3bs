// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"
#include "core/SnapshotQueue.h"
#include "ui/ArtworkPanel.h"
#include "ui/AdvancedStateEditor.h"
#include "ui/PresetCatalog.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace threebs {

class StandaloneContent final : public juce::Component, private juce::Timer {
public:
    StandaloneContent() : panel_(snapshots_, noteVisualizationEvents_) {
        setWantsKeyboardFocus(true);
        addAndMakeVisible(panel_);
        panel_.setPresetNames(presets_.names());
        panel_.presetSelector().onChange = [this] { applyPreset(); };
        panel_.onRandomize = [this] {
            baseInitial_ = makeInitialState(InitialSystem::ControlledChaos, ++seed_,
                                            panel_.chaosSlider().getValue() / 100.0);
            initial_ = applyInitialPlaneTilts(baseInitial_, planeTilts_);
            engine_.reset(initial_);
            presentation_.visualSeed = initial_.seed;
            panel_.setPresentationState(presentation_);
            ++trajectoryRevision_;
        };
        panel_.onReset = [this] { engine_.reset(initial_); ++trajectoryRevision_; };
        panel_.onCameraChanged = [this](const CameraState& camera) { presentation_.camera = camera; };
        panel_.onNotePaneMinimizedChanged = [this](bool minimized) {
            presentation_.notePaneMinimized = minimized;
        };
        panel_.onNotePaneStyleChanged = [this](NotePaneStyle style) {
            presentation_.notePaneStyle = style;
        };
        panel_.onAdvanced = [this] {
            const auto target = panel_.getLocalArea(&panel_.advancedButton(), panel_.advancedButton().getLocalBounds());
            showAdvancedStateEditor(initial_, target, panel_, [this](const SimulationState& state) {
                initial_ = state;
                baseInitial_ = state;
                planeTilts_.fill(0.0);
                engine_.reset(initial_);
                presentation_.visualSeed = initial_.seed;
                panel_.setPresentationState(presentation_);
                panel_.setPlaneTilts(planeTilts_);
                ++trajectoryRevision_;
                const auto masses = panel_.massSliders();
                for (std::size_t body = 0; body < bodyCount; ++body)
                    masses[body]->setValue(initial_.bodies[body].mass, juce::dontSendNotification);
            });
        };
        panel_.syncButton().setToggleState(false, juce::dontSendNotification);
        panel_.syncButton().setButtonText("FREE RUN");
        panel_.setMidiOutputAvailable(true);
        populateMidiOutputs();
        panel_.midiOutputSelector().onChange = [this] { openSelectedOutput(); };
        panel_.midiOutputSelector().setSelectedId(1, juce::sendNotificationSync);
        initial_ = engine_.simulation().initialState();
        engine_.prepare(sampleRate_);
        applyPreset();
        startTimerHz(timerRate_);
    }

    ~StandaloneContent() override {
        stopTimer();
        if (midiOutput_ != nullptr)
            midiOutput_->stopBackgroundThread();
    }

    void resized() override { panel_.setBounds(getLocalBounds()); }

    bool keyPressed(const juce::KeyPress& key) override {
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
        selector.clear();
        selector.addItem("Virtual MIDI: 3bs", 1);
        devices_ = juce::MidiOutput::getAvailableDevices();
        for (int i = 0; i < devices_.size(); ++i)
            selector.addItem(devices_.getReference(i).name, i + 2);
    }

    void openSelectedOutput() {
        if (midiOutput_ != nullptr)
            midiOutput_->stopBackgroundThread();
        const auto selected = panel_.midiOutputSelector().getSelectedId();
        midiOutput_ = selected == 1
            ? juce::MidiOutput::createNewDevice("The Three Body Solution")
            : juce::MidiOutput::openDevice(devices_.getReference(selected - 2).identifier);
        if (midiOutput_ != nullptr) {
            midiOutput_->startBackgroundThread();
            panel_.setStatus("COREMIDI / " + panel_.midiOutputSelector().getText());
        } else {
            panel_.setStatus("MIDI OUTPUT UNAVAILABLE");
        }
    }

    void applyPreset() {
        const auto index = panel_.presetSelector().getSelectedItemIndex();
        if (!presets_.valid() || index < 0 || static_cast<std::size_t>(index) >= presets_.size())
            return;
        const auto& preset = presets_[static_cast<std::size_t>(index)];
        baseInitial_ = makeInitialState(preset.system, preset.seed, preset.chaos);
        planeTilts_.fill(0.0);
        panel_.setPlaneTilts(planeTilts_);
        initial_ = baseInitial_;
        config_.simulation = preset.simulation;
        config_.voices = preset.voices;
        engine_.setConfig(config_);
        engine_.reset(initial_);
        presentation_ = preset.presentation;
        panel_.setPresentationState(presentation_);
        ++trajectoryRevision_;
        panel_.gravitySlider().setValue(preset.simulation.gravitationalConstant, juce::dontSendNotification);
        panel_.softeningSlider().setValue(preset.simulation.softening, juce::dontSendNotification);
        panel_.speedSlider().setValue(preset.simulation.speed, juce::dontSendNotification);
        const auto masses = panel_.massSliders();
        for (std::size_t i = 0; i < bodyCount; ++i)
            masses[i]->setValue(initial_.bodies[i].mass, juce::dontSendNotification);
        const auto enables = panel_.voiceEnableButtons();
        const auto scales = panel_.voiceScaleSelectors();
        const auto pitches = panel_.voicePitchSelectors();
        const auto triggers = panel_.voiceTriggerSelectors();
        for (std::size_t body = 0; body < bodyCount; ++body) {
            enables[body]->setToggleState(preset.voices[body].enabled, juce::dontSendNotification);
            scales[body]->setSelectedId(static_cast<int>(preset.voices[body].scale) + 1,
                                        juce::dontSendNotification);
            pitches[body]->setSelectedId(static_cast<int>(preset.voices[body].pitchMapping) + 1,
                                         juce::dontSendNotification);
            triggers[body]->setSelectedId(static_cast<int>(preset.voices[body].triggerMapping) + 1,
                                          juce::dontSendNotification);
        }
    }

    void timerCallback() override {
        config_.simulation.gravitationalConstant = panel_.gravitySlider().getValue();
        config_.simulation.softening = panel_.softeningSlider().getValue();
        config_.simulation.speed = panel_.speedSlider().getValue();
        const auto density = panel_.densitySlider().getValue() / 100.0;
        const auto enables = panel_.voiceEnableButtons();
        const auto scales = panel_.voiceScaleSelectors();
        const auto pitches = panel_.voicePitchSelectors();
        const auto triggers = panel_.voiceTriggerSelectors();
        for (std::size_t body = 0; body < bodyCount; ++body) {
            auto& voice = config_.voices[body];
            voice.probability = density;
            voice.enabled = enables[body]->getToggleState();
            voice.scale = static_cast<ScaleId>(std::max(0, scales[body]->getSelectedItemIndex()));
            voice.pitchMapping = static_cast<PitchMapping>(std::max(0, pitches[body]->getSelectedItemIndex()));
            voice.triggerMapping = static_cast<TriggerMapping>(std::max(0, triggers[body]->getSelectedItemIndex()));
        }
        engine_.setConfig(config_);
        const auto masses = panel_.massSliders();
        for (std::size_t i = 0; i < bodyCount; ++i) {
            const auto mass = masses[i]->getValue();
            baseInitial_.bodies[i].mass = mass;
            initial_.bodies[i].mass = mass;
            engine_.setBodyMass(i, mass);
        }
        const auto currentTilts = panel_.planeTilts();
        bool tiltChanged{};
        for (std::size_t body = 0; body < bodyCount; ++body)
            tiltChanged = tiltChanged || std::abs(currentTilts[body] - planeTilts_[body]) > 0.05;
        if (tiltChanged) {
            planeTilts_ = currentTilts;
            initial_ = applyInitialPlaneTilts(baseInitial_, planeTilts_);
            engine_.reset(initial_);
            ++trajectoryRevision_;
        }

        constexpr auto blockSize = static_cast<std::uint32_t>(sampleRate_ / timerRate_);
        ProcessContext context;
        context.sampleCount = blockSize;
        context.sampleRate = sampleRate_;
        context.beatAtStart = beat_;
        context.beatsPerSample = tempo_ / (60.0 * sampleRate_);
        context.playing = panel_.runButton().getToggleState();
        context.transportStarted = context.playing && !wasRunning_;
        if (context.transportStarted)
            ++trajectoryRevision_;
        MusicEngine::EventBuffer events;
        engine_.process(context, events);
        beat_ += context.beatsPerSample * static_cast<double>(blockSize);
        wasRunning_ = context.playing;

        for (const auto& event : events) {
            if (event.type == MidiEventType::ControlChange)
                continue;
            NoteVisualizationEvent visual;
            visual.type = event.type == MidiEventType::NoteOn
                ? NoteVisualizationType::On : NoteVisualizationType::Off;
            visual.body = event.sourceBody;
            visual.note = event.data1;
            visual.velocity = event.data2;
            visual.sequence = ++noteVisualizationSequence_;
            (void)noteVisualizationEvents_.push(visual);
        }

        if (midiOutput_ != nullptr && events.size() > 0) {
            juce::MidiBuffer buffer;
            for (const auto& event : events) {
                juce::MidiMessage message;
                if (event.type == MidiEventType::NoteOn)
                    message = juce::MidiMessage::noteOn(event.channel, event.data1, event.data2);
                else if (event.type == MidiEventType::NoteOff)
                    message = juce::MidiMessage::noteOff(event.channel, event.data1);
                else
                    message = juce::MidiMessage::controllerEvent(event.channel, event.data1, event.data2);
                buffer.addEvent(message, static_cast<int>(event.sampleOffset));
            }
            midiOutput_->sendBlockOfMessages(buffer, juce::Time::getMillisecondCounterHiRes() + 2.0, sampleRate_);
        }

        RenderSnapshot snapshot;
        snapshot.bodies = engine_.simulation().state().bodies;
        snapshot.escaped = engine_.simulation().state().escaped;
        snapshot.sequence = ++sequence_;
        const auto& respawns = engine_.simulation().state().respawnCount;
        if (respawns != lastRespawnCounts_) {
            lastRespawnCounts_ = respawns;
            ++trajectoryRevision_;
        }
        snapshot.trajectoryRevision = trajectoryRevision_;
        snapshot.interpolationAlpha = engine_.simulation().interpolationAlpha();
        if (!snapshots_.push(snapshot)) {
            RenderSnapshot discarded;
            snapshots_.pop(discarded);
            snapshots_.push(snapshot);
        }

        presentation_.visual.trailSeconds = static_cast<float>(panel_.trailSlider().getValue());
        presentation_.visual.bloom = static_cast<float>(panel_.bloomSlider().getValue() / 100.0);
        presentation_.camera.minimumDistance = static_cast<float>(panel_.minimumCameraDistanceSlider().getValue());
        presentation_.camera.maximumDistance = static_cast<float>(panel_.maximumCameraDistanceSlider().getValue());
        presentation_.camera.autoFrame = panel_.autoFrameButton().getToggleState();
        presentation_.camera = sanitizedCameraState(presentation_.camera);
        panel_.setPresentationState(presentation_);
        if (config_.simulation.escapePolicy == EscapePolicy::Prompt) {
            for (const auto escaped : engine_.simulation().state().escaped) {
                if (escaped) {
                    panel_.setStatus("BODY ESCAPED / RESET OR NEW SYSTEM");
                    break;
                }
            }
        }
    }

    static constexpr double sampleRate_ = 48000.0;
    static constexpr int timerRate_ = 100;
    static constexpr double tempo_ = 120.0;
    SpscQueue<RenderSnapshot, 64> snapshots_;
    NoteVisualizationQueue noteVisualizationEvents_;
    ArtworkPanel panel_;
    PresetCatalog presets_;
    MusicEngine engine_;
    EngineConfig config_{engine_.config()};
    SimulationState baseInitial_{};
    SimulationState initial_{};
    juce::Array<juce::MidiDeviceInfo> devices_;
    std::unique_ptr<juce::MidiOutput> midiOutput_;
    PresentationState presentation_{};
    std::uint64_t seed_{0x33425320ULL};
    std::uint64_t sequence_{};
    std::uint64_t noteVisualizationSequence_{};
    std::uint64_t trajectoryRevision_{1};
    std::array<std::uint32_t, bodyCount> lastRespawnCounts_{};
    std::array<double, bodyCount> planeTilts_{};
    double beat_{};
    bool wasRunning_{};
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
