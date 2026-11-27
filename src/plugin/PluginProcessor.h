// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/MusicEngine.h"
#include "core/SnapshotQueue.h"
#include "plugin/LatestCommandMailbox.h"
#include "ui/PresetCatalog.h"
#include "ui/UserConfiguration.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <mutex>

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
    std::uint32_t realtimeFaults() const noexcept { return realtimeFaults_.load(std::memory_order_relaxed); }
    std::uint64_t droppedSnapshots() const noexcept { return droppedSnapshots_.load(std::memory_order_relaxed); }
    std::uint64_t configurationRevision() const noexcept { return publishedRevision_.load(std::memory_order_acquire); }
    std::uint64_t appliedConfigurationRevision() const noexcept { return appliedRevision_.load(std::memory_order_acquire); }
    juce::String lastStateError() const;

    void requestPreset(const ArtworkPreset& preset, int presetIndex);
    void requestVoicingPreset(const VoicingPreset& preset, int presetIndex);
    void setSelectedVoicingPresetIndex(int index);
    void requestRandomize(double chaos);
    void requestReset();
    void requestPanic() noexcept { panicRequested_.store(true, std::memory_order_release); }
    void requestExactState(const SimulationState& state);
    void requestPlaneTilts(const std::array<double, bodyCount>& tiltDegrees);
    SimulationState currentInitialState() const;
    std::array<double, bodyCount> initialPlaneTilts() const;
    bool setPresentationState(const PresentationState& state, std::uint64_t expectedRevision = 0);
    PresentationState presentationState() const;
    UserConfiguration currentUserConfiguration() const;
    bool applyUserConfiguration(const UserConfiguration& configuration);
    bool applyAdvancedConfiguration(const EngineConfig& configuration);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    enum ParameterIndex : std::size_t {
        Run, Sync, Speed, Gravity, Softening, Chaos, Density, Trail, Bloom,
        Mass1, Mass2, Mass3,
        Voice1, Voice2 = Voice1 + 10, Voice3 = Voice2 + 10,
        Voicing = Voice3 + 10, StrumMilliseconds, ChordRoot, ChordScale,
        StrumUnits, StrumAmount, MeterSource, MeterNumerator, MeterDenominator,
        AutoReset, AutoResetBars, InputMode, Preset, ParameterCount
    };
    enum VoiceField : std::size_t {
        Enabled, Scale, Root, Pitch, Trigger, Octave, DurationMap, DurationGridValue,
        DurationMinimum, DurationMaximum
    };
    static_assert(ParameterCount <= 64);
    using ParameterValues = std::array<float, ParameterCount>;
    struct EngineCommand {
        UserConfiguration configuration{};
        ParameterValues parameters{};
        std::uint64_t revision{};
        std::uint64_t resetRevision{};
    };
    struct InputEvent {
        std::uint32_t offset{};
        std::uint8_t status{}, data1{}, data2{};
    };
    static constexpr std::size_t maximumInputEvents = 32768;

    static const std::array<const char*, ParameterCount> parameterIds_;
    ParameterValues readParameters() const noexcept;
    static ParameterValues plainParameterValues(const UserConfiguration&) noexcept;
    ParameterValues legalParameterValues(const UserConfiguration&) const;
    static void applyParameters(UserConfiguration&, const ParameterValues&, std::uint64_t changedMask) noexcept;
    UserConfiguration currentConfigurationLocked() const;
    bool publishConfigurationLocked(const UserConfiguration&, bool reset);
    void installParameterValues(const UserConfiguration&);
    bool consumeCommand(EventSink) noexcept;
    void updateEngineConfigFromParameters() noexcept;
    void clearInputKeys() noexcept;
    void applyInputEvent(const InputEvent&) noexcept;
    void publishSnapshot() noexcept;
    void emitEvent(const MidiEvent&) noexcept;
    static void writeEvent(void*, const MidiEvent&) noexcept;
    void setParameterValue(const juce::String& id, float plainValue);
    void applyVoiceParameters(const std::array<VoiceConfig, bodyCount>& voices);

    juce::AudioProcessorValueTreeState parameters_;
    std::array<std::atomic<float>*, ParameterCount> parameterRefs_{};
    ParameterValues parameterMinimums_{}, parameterMaximums_{}, parameterDefaults_{};

    // Only non-processing entry points take this mutex. The callback consumes
    // immutable mailbox slots and never touches desired_ or this mutex.
    mutable std::recursive_mutex configurationMutex_;
    UserConfiguration desired_{};
    ParameterValues parameterBaseline_{};
    juce::String stateError_;
    std::uint64_t resetRevision_{1};
    std::atomic<std::uint64_t> publishedRevision_{0};
    std::atomic<std::uint64_t> parameterTransaction_{0};
    LatestCommandMailbox<EngineCommand> commands_;

    MusicEngine engine_;
    UserConfiguration activeConfiguration_{};
    ParameterValues activeParameters_{};
    std::uint64_t activeRevision_{}, activeResetRevision_{}, activeChangedMask_{};
    std::atomic<std::uint64_t> appliedRevision_{0}, appliedChangedMask_{0};
    SpscQueue<RenderSnapshot, 64> snapshots_;
    NoteVisualizationQueue noteVisualizationEvents_;
    std::atomic<std::uint64_t> nextSeed_{0x33425310ULL};
    std::atomic<std::uint32_t> escapePromptMask_{0}, realtimeFaults_{0};
    std::atomic<std::uint64_t> droppedSnapshots_{0};
    std::atomic<bool> panicRequested_{false};
    std::array<InputEvent, maximumInputEvents> inputEvents_{};
    std::array<std::array<std::uint16_t, 128>, 16> heldKeys_{};
    std::array<std::array<std::uint64_t, 128>, 16> keyOrder_{};
    std::uint64_t inputOrder_{};
    int heldNotes_{};
    std::int8_t inputTranspose_{};
    double sampleRate_{48000.0};
    double freeBeat_{}, lastPpq_{}, processingSeconds_{};
    double lastBeatsPerSample_{2.0 / 48000.0};
    int lastBlockSize_{};
    bool hostWasPlaying_{}, haveHostPosition_{};
    std::uint64_t snapshotSequence_{}, noteVisualizationSequence_{}, trajectoryRevision_{1};
    std::array<std::uint32_t, bodyCount> lastRespawnCounts_{};
    juce::MidiBuffer* outputMidi_{};
    std::uint32_t outputOffset_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreeBSProcessor)
};

} // namespace threebs
