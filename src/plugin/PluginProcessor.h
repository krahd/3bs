// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/MusicEngine.h"
#include "core/SnapshotQueue.h"
#include "ui/PresetCatalog.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace threebs {

class ThreeBSProcessor final : public juce::AudioProcessor {
public:
    ThreeBSProcessor();
    ~ThreeBSProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "The Three Body Solution"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return THREEBS_AU_MIDI_EFFECT != 0; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& parameters() noexcept { return parameters_; }
    SpscQueue<RenderSnapshot, 64>& snapshots() noexcept { return snapshots_; }
    NoteVisualizationQueue& noteVisualizationEvents() noexcept { return noteVisualizationEvents_; }
    std::uint32_t escapePromptMask() const noexcept { return escapePromptMask_.load(std::memory_order_relaxed); }
    void requestPreset(const ArtworkPreset& preset, int presetIndex);
    void requestRandomize(double chaos);
    void requestReset();
    void requestExactState(const SimulationState& state);
    void requestPlaneTilts(const std::array<double, bodyCount>& tiltDegrees);
    SimulationState currentInitialState() const noexcept { return storedInitial_.load(); }
    std::array<double, bodyCount> initialPlaneTilts() const noexcept;
    void setPresentationState(const PresentationState& state) noexcept { storedPresentation_.store(state); }
    PresentationState presentationState() const noexcept { return storedPresentation_.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    enum class CommandType : std::uint8_t { Reset, Replace };
    struct EngineCommand {
        CommandType type{CommandType::Reset};
        SimulationState state{};
        EngineConfig config{};
        LoopPolicy loopPolicy{LoopPolicy::Restart};
    };

    struct AtomicState {
        std::atomic<std::uint64_t> seed{0x334253ULL};
        std::array<std::atomic<double>, bodyCount> mass{};
        std::array<std::atomic<double>, bodyCount> px{}, py{}, pz{}, vx{}, vy{}, vz{};
        void store(const SimulationState& state) noexcept;
        SimulationState load() const noexcept;
    } storedInitial_;

    AtomicState storedBaseInitial_;

    struct AtomicPlaneTilts {
        std::array<std::atomic<double>, bodyCount> degrees{};
        void store(const std::array<double, bodyCount>& values) noexcept;
        std::array<double, bodyCount> load() const noexcept;
    } storedPlaneTilts_;

    struct AtomicPresentationState {
        std::atomic<float> trailSeconds{30.0F};
        std::atomic<float> trailWidth{1.2F};
        std::atomic<float> extrusion{0.12F};
        std::atomic<float> bloom{0.34F};
        std::atomic<float> starDensity{0.62F};
        std::atomic<int> palette{static_cast<int>(PaletteId::Eclipse)};
        std::atomic<int> quality{static_cast<int>(GraphicsQuality::High)};
        std::atomic<float> yaw{};
        std::atomic<float> pitch{-0.34F};
        std::atomic<float> distance{7.0F};
        std::atomic<float> minimumDistance{2.5F};
        std::atomic<float> maximumDistance{40.0F};
        std::atomic<float> autoOrbit{0.035F};
        std::atomic<int> focusBody{-1};
        std::atomic<bool> autoFrame{true};
        std::atomic<std::uint64_t> visualSeed{0x334253ULL};
        std::atomic<bool> notePaneMinimized{};
        void store(const PresentationState& state) noexcept;
        PresentationState load() const noexcept;
    } storedPresentation_;

    struct ParameterRefs {
        std::atomic<float>* run{};
        std::atomic<float>* sync{};
        std::atomic<float>* speed{};
        std::atomic<float>* gravity{};
        std::atomic<float>* softening{};
        std::atomic<float>* chaos{};
        std::atomic<float>* density{};
        std::atomic<float>* trail{};
        std::atomic<float>* bloom{};
        std::array<std::atomic<float>*, bodyCount> masses{};
        std::atomic<float>* inputMode{};
        std::atomic<float>* preset{};
    } parameterRefs_;

    void consumeCommands(MusicEngine::EventBuffer& eventBuffer) noexcept;
    void updateEngineConfigFromParameters() noexcept;
    void publishSnapshot() noexcept;
    void publishNoteVisualization(const MusicEngine::EventBuffer&) noexcept;
    void setParameterValue(const juce::String& id, float plainValue);
    static void addEvents(const MusicEngine::EventBuffer&, juce::MidiBuffer&);

    juce::AudioProcessorValueTreeState parameters_;
    MusicEngine engine_;
    EngineConfig engineConfig_{};
    SpscQueue<RenderSnapshot, 64> snapshots_;
    NoteVisualizationQueue noteVisualizationEvents_;
    SpscQueue<EngineCommand, 8> commands_;
    std::atomic<std::uint64_t> nextSeed_{0x33425310ULL};
    std::atomic<int> loopPolicy_{static_cast<int>(LoopPolicy::Restart)};
    std::atomic<std::uint32_t> escapePromptMask_{};
    double sampleRate_{48000.0};
    double freeBeat_{};
    double lastPpq_{};
    double lastBeatsPerSample_{2.0 / 48000.0};
    int lastBlockSize_{};
    int heldNotes_{};
    std::int8_t inputTranspose_{};
    bool hostWasPlaying_{};
    bool haveHostPosition_{};
    std::uint64_t snapshotSequence_{};
    std::uint64_t noteVisualizationSequence_{};
    std::uint64_t trajectoryRevision_{1};
    std::array<std::uint32_t, bodyCount> lastRespawnCounts_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreeBSProcessor)
};

} // namespace threebs
