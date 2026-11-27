// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace threebs {
namespace {
constexpr auto stateTreeName = "THREE_BS_STATE";
constexpr std::array<double, 7> autoResetBarsValues{0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0};
constexpr std::array<int, 5> timeSigDenominators{1, 2, 4, 8, 16};

int autoResetBarsToIndex(double bars) noexcept {
    int best{2};
    double bestError{1.0e18};
    for (int i = 0; i < static_cast<int>(autoResetBarsValues.size()); ++i) {
        const auto error = std::abs(autoResetBarsValues[static_cast<std::size_t>(i)] - bars);
        if (error < bestError) { bestError = error; best = i; }
    }
    return best;
}

juce::var stateParameter(const juce::ValueTree& state, const juce::String& id) {
    if (state.hasProperty(id)) return state.getProperty(id);
    for (int index = 0; index < state.getNumChildren(); ++index) {
        const auto child = state.getChild(index);
        if (child.getProperty("id").toString() == id) return child.getProperty("value");
    }
    return {};
}
void setStateParameter(juce::ValueTree& state, const juce::String& id, float value) {
    if (state.hasProperty(id)) state.setProperty(id, value, nullptr);
    for (int index = 0; index < state.getNumChildren(); ++index) {
        auto child = state.getChild(index);
        if (child.getProperty("id").toString() == id) {
            child.setProperty("value", value, nullptr); return;
        }
    }
    juce::ValueTree child{"PARAM"};
    child.setProperty("id", id, nullptr);
    child.setProperty("value", value, nullptr);
    state.addChild(child, -1, nullptr);
}

// ValueTree XML stores numbers as text. Require a complete finite conversion;
// unlike JSON input, numeric strings are part of this published host format.
bool hostNumber(const juce::var& value, double& result) {
    if (value.isInt() || value.isInt64() || value.isDouble() || value.isBool()) {
        result = static_cast<double>(value); return std::isfinite(result);
    }
    if (!value.isString()) return false;
    const auto text = value.toString().toStdString();
    if (text.empty() || text.front() == ' ' || text.back() == ' ') return false;
    char* end{};
    errno = 0;
    result = std::strtod(text.c_str(), &end);
    return errno != ERANGE && end == text.c_str() + text.size() && std::isfinite(result);
}
bool hostSeed(const juce::var& value, std::uint64_t& result) {
    const auto text = value.toString().toStdString();
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), result);
    return !text.empty() && converted.ec == std::errc{}
        && converted.ptr == text.data() + text.size();
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ThreeBSProcessor::createParameterLayout() {
    using Parameter = std::unique_ptr<juce::RangedAudioParameter>;
    std::vector<Parameter> values;
    values.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"run", 1}, "Run", true));
    values.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync", 1}, "Host Sync", true));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"speed", 1}, "Speed",
        juce::NormalisableRange<float>(0.0F, 8.0F, 0.001F, 0.45F), 1.0F));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gravity", 1}, "Gravity",
        juce::NormalisableRange<float>(0.01F, 3.0F, 0.001F, 0.45F), 0.65F));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"softening", 1}, "Softening",
        juce::NormalisableRange<float>(0.001F, 0.25F, 0.001F, 0.6F), 0.04F));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"chaos", 1}, "Chaos", 0.0F, 100.0F, 20.0F));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"density", 1}, "Density", 0.0F, 100.0F, 80.0F));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"trail", 1}, "Trail Seconds", 5.0F, 60.0F, 30.0F));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"bloom", 1}, "Bloom", 0.0F, 100.0F, 34.0F));
    for (int i = 0; i < 3; ++i)
        values.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"mass" + juce::String(i + 1), 1}, "Mass " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.05F, 8.0F, 0.001F, 0.45F), 1.0F));
    for (int i = 0; i < static_cast<int>(bodyCount); ++i) {
        const auto suffix = juce::String(i + 1);
        values.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"voiceEnabled" + suffix, 1}, "Planet " + suffix + " Enabled", true));
        values.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"voiceScale" + suffix, 1}, "Planet " + suffix + " Scale",
            scaleDisplayNames(), static_cast<int>(ScaleId::MinorPentatonic)));
        values.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"voiceRoot" + suffix, 1}, "Planet " + suffix + " Root",
            juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
        values.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"voicePitch" + suffix, 1}, "Planet " + suffix + " Pitch Map",
            pitchMappingDisplayNames(), 0));
        values.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"voiceTrigger" + suffix, 1}, "Planet " + suffix + " Trigger",
            triggerMappingDisplayNames(), 0));
        values.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"voiceOctave" + suffix, 1}, "Planet " + suffix + " Octave", -4, 4, 0));
        values.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"voiceDurMap" + suffix, 1}, "Planet " + suffix + " Length Map",
            pitchMappingDisplayNames(), static_cast<int>(PitchMapping::Speed)));
        values.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"voiceDurGrid" + suffix, 1}, "Planet " + suffix + " Length Grid",
            durationGridDisplayNames(), static_cast<int>(DurationGrid::Straight)));
        values.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"voiceDurMin" + suffix, 1}, "Planet " + suffix + " Length Min",
            juce::NormalisableRange<float>(0.02F, 8.0F, 0.001F, 0.4F), 0.2F));
        values.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"voiceDurMax" + suffix, 1}, "Planet " + suffix + " Length Max",
            juce::NormalisableRange<float>(0.02F, 8.0F, 0.001F, 0.4F), 0.2F));
    }
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"voicingMode", 1}, "Voicing Mode",
        juce::StringArray{"Independent", "Chord", "Strum"}, 0));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chordStrum", 1}, "Chord Strum",
        juce::NormalisableRange<float>(0.0F, 120.0F, 1.0F), 24.0F));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"strumUnit", 1}, "Strum Unit",
        juce::StringArray{"Milliseconds", "Beats", "Bar Fraction"}, 0));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"strumValue", 1}, "Strum Amount",
        juce::NormalisableRange<float>(0.0F, 4.0F, 0.001F, 0.4F), 0.0625F));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"chordRoot", 1}, "Chord Root",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"chordScale", 1}, "Chord Scale",
        scaleDisplayNames(), static_cast<int>(ScaleId::MinorPentatonic)));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"timeSigSource", 1}, "Time Sig Source",
        juce::StringArray{"Host", "Manual"}, 0));
    {
        juce::StringArray numerators;
        for (int n = 1; n <= 16; ++n)
            numerators.add(juce::String(n));
        values.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"timeSigNum", 1}, "Time Sig Numerator", numerators, 3));
    }
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"timeSigDenom", 1}, "Time Sig Denominator",
        juce::StringArray{"1", "2", "4", "8", "16"}, 2));
    values.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"autoReset", 1}, "Auto Reset", false));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"autoResetBars", 1}, "Auto Reset Bars",
        juce::StringArray{"1/4", "1/2", "1", "2", "4", "8", "16"}, 2));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"inputMode", 1}, "MIDI Input",
        juce::StringArray{"Off", "Transpose", "Gate"}, 0));
    juce::StringArray presetNames;
    for (int i = 1; i <= 24; ++i)
        presetNames.add("Preset " + juce::String(i));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"preset", 1}, "Factory Preset", presetNames, 0));
    return {values.begin(), values.end()};
}

const std::array<const char*, ThreeBSProcessor::ParameterCount> ThreeBSProcessor::parameterIds_{
    "run", "sync", "speed", "gravity", "softening", "chaos", "density", "trail", "bloom",
    "mass1", "mass2", "mass3",
    "voiceEnabled1", "voiceScale1", "voiceRoot1", "voicePitch1", "voiceTrigger1", "voiceOctave1",
    "voiceDurMap1", "voiceDurGrid1", "voiceDurMin1", "voiceDurMax1",
    "voiceEnabled2", "voiceScale2", "voiceRoot2", "voicePitch2", "voiceTrigger2", "voiceOctave2",
    "voiceDurMap2", "voiceDurGrid2", "voiceDurMin2", "voiceDurMax2",
    "voiceEnabled3", "voiceScale3", "voiceRoot3", "voicePitch3", "voiceTrigger3", "voiceOctave3",
    "voiceDurMap3", "voiceDurGrid3", "voiceDurMin3", "voiceDurMax3",
    "voicingMode", "chordStrum", "chordRoot", "chordScale", "strumUnit", "strumValue",
    "timeSigSource", "timeSigNum", "timeSigDenom", "autoReset", "autoResetBars", "inputMode", "preset"
};

ThreeBSProcessor::ThreeBSProcessor()
#if THREEBS_AU_MIDI_EFFECT
    : AudioProcessor(BusesProperties()),
#else
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      parameters_(*this, nullptr, stateTreeName, createParameterLayout()) {
    for (std::size_t index = 0; index < ParameterCount; ++index) {
        parameterRefs_[index] = parameters_.getRawParameterValue(parameterIds_[index]);
        const auto* parameter = parameters_.getParameter(parameterIds_[index]);
        const auto& range = parameter->getNormalisableRange();
        parameterMinimums_[index] = range.start;
        parameterMaximums_[index] = range.end;
        parameterDefaults_[index] = parameter->convertFrom0to1(parameter->getDefaultValue());
    }
    desired_.initial = desired_.baseInitial = engine_.simulation().initialState();
    desired_.engine = engine_.config();
    PresetCatalog catalog;
    if (catalog.valid()) {
        const auto& preset = catalog[0];
        desired_.initial = desired_.baseInitial = makeInitialState(preset.system, preset.seed, preset.chaos);
        desired_.engine.simulation = preset.simulation;
        desired_.engine.voices = preset.voices;
        desired_.presentation = preset.presentation;
        desired_.loopPolicy = preset.loopPolicy;
        desired_.densityPercent = 0.0;
        for (const auto& voice : preset.voices) desired_.densityPercent += voice.probability * (100.0 / 3.0);
    }
    (void)publishConfigurationLocked(desired_, true);
}

ThreeBSProcessor::ParameterValues ThreeBSProcessor::readParameters() const noexcept {
    ParameterValues values;
    for (std::size_t i = 0; i < ParameterCount; ++i) {
        const auto value = parameterRefs_[i]->load(std::memory_order_relaxed);
        values[i] = std::isfinite(value) ? std::clamp(value, parameterMinimums_[i], parameterMaximums_[i])
                                       : parameterDefaults_[i];
    }
    return values;
}

ThreeBSProcessor::ParameterValues ThreeBSProcessor::plainParameterValues(const UserConfiguration& c) noexcept {
    ParameterValues p{};
    p[Run] = c.run ? 1.0F : 0.0F; p[Sync] = c.sync ? 1.0F : 0.0F;
    p[Speed] = static_cast<float>(c.engine.simulation.speed);
    p[Gravity] = static_cast<float>(c.engine.simulation.gravitationalConstant);
    p[Softening] = static_cast<float>(c.engine.simulation.softening);
    p[Chaos] = static_cast<float>(c.chaosPercent); p[Density] = static_cast<float>(c.densityPercent);
    p[Trail] = c.presentation.visual.trailSeconds; p[Bloom] = c.presentation.visual.bloom * 100.0F;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        p[Mass1 + body] = static_cast<float>(c.initial.bodies[body].mass);
        const auto base = Voice1 + body * 10;
        const auto& v = c.engine.voices[body];
        p[base + Enabled] = v.enabled ? 1.0F : 0.0F;
        p[base + Scale] = static_cast<float>(v.scale); p[base + Root] = static_cast<float>(v.root);
        p[base + Pitch] = static_cast<float>(v.pitchMapping); p[base + Trigger] = static_cast<float>(v.triggerMapping);
        p[base + Octave] = static_cast<float>(v.octave); p[base + DurationMap] = static_cast<float>(v.durationMapping);
        p[base + DurationGridValue] = static_cast<float>(v.durationGrid);
        p[base + DurationMinimum] = static_cast<float>(v.minimumDurationBeats);
        p[base + DurationMaximum] = static_cast<float>(v.maximumDurationBeats);
    }
    p[Voicing] = static_cast<float>(c.engine.voicingMode);
    p[StrumMilliseconds] = static_cast<float>(c.engine.chordStrumMilliseconds);
    p[ChordRoot] = static_cast<float>(c.engine.chordSystem.root);
    p[ChordScale] = static_cast<float>(c.engine.chordSystem.scale);
    p[StrumUnits] = static_cast<float>(c.engine.chordStrumUnit);
    p[StrumAmount] = static_cast<float>(c.engine.chordStrumValue);
    p[MeterSource] = static_cast<float>(c.timeSignatureSource);
    p[MeterNumerator] = static_cast<float>(c.timeSignatureNumerator - 1);
    const auto denominator = std::find(timeSigDenominators.begin(), timeSigDenominators.end(), c.timeSignatureDenominator);
    p[MeterDenominator] = static_cast<float>(std::distance(timeSigDenominators.begin(), denominator));
    p[AutoReset] = c.engine.autoResetEnabled ? 1.0F : 0.0F;
    p[AutoResetBars] = static_cast<float>(autoResetBarsToIndex(c.engine.autoResetBars));
    p[InputMode] = c.engine.inputTransposeEnabled ? 1.0F : c.engine.inputGateEnabled ? 2.0F : 0.0F;
    p[Preset] = static_cast<float>(c.presetIndex);
    return p;
}

ThreeBSProcessor::ParameterValues ThreeBSProcessor::legalParameterValues(const UserConfiguration& configuration) const {
    auto result = plainParameterValues(configuration);
    for (std::size_t i = 0; i < ParameterCount; ++i) {
        const auto* parameter = parameters_.getParameter(parameterIds_[i]);
        const auto& range = parameter->getNormalisableRange();
        const auto value = range.snapToLegalValue(std::clamp(result[i], range.start, range.end));
        result[i] = parameter->convertFrom0to1(parameter->convertTo0to1(value));
    }
    return result;
}

void ThreeBSProcessor::applyParameters(UserConfiguration& c, const ParameterValues& p,
                                      std::uint64_t mask) noexcept {
    const auto changed = [mask](std::size_t i) { return (mask & (std::uint64_t{1} << i)) != 0; };
    const auto integer = [&p](std::size_t i) { return static_cast<int>(std::lround(p[i])); };
    if (changed(Run)) c.run = p[Run] >= 0.5F;
    if (changed(Sync)) c.sync = p[Sync] >= 0.5F;
    if (changed(Speed)) c.engine.simulation.speed = p[Speed];
    if (changed(Gravity)) c.engine.simulation.gravitationalConstant = p[Gravity];
    if (changed(Softening)) c.engine.simulation.softening = p[Softening];
    if (changed(Chaos)) c.chaosPercent = p[Chaos];
    if (changed(Density)) c.densityPercent = p[Density];
    if (changed(Trail)) c.presentation.visual.trailSeconds = p[Trail];
    if (changed(Bloom)) c.presentation.visual.bloom = p[Bloom] / 100.0F;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        if (changed(Mass1 + body)) {
            c.initial.bodies[body].mass = p[Mass1 + body];
            c.baseInitial.bodies[body].mass = p[Mass1 + body];
        }
        const auto b = Voice1 + body * 10;
        auto& v = c.engine.voices[body];
        if (changed(b + Enabled)) v.enabled = p[b + Enabled] >= 0.5F;
        if (changed(b + Scale)) v.scale = static_cast<ScaleId>(integer(b + Scale));
        if (changed(b + Root)) v.root = static_cast<std::uint8_t>(integer(b + Root));
        if (changed(b + Pitch)) v.pitchMapping = static_cast<PitchMapping>(integer(b + Pitch));
        if (changed(b + Trigger)) v.triggerMapping = static_cast<TriggerMapping>(integer(b + Trigger));
        if (changed(b + Octave)) v.octave = static_cast<std::int8_t>(integer(b + Octave));
        if (changed(b + DurationMap)) v.durationMapping = static_cast<PitchMapping>(integer(b + DurationMap));
        if (changed(b + DurationGridValue)) v.durationGrid = static_cast<DurationGrid>(integer(b + DurationGridValue));
        if (changed(b + DurationMinimum)) v.minimumDurationBeats = p[b + DurationMinimum];
        if (changed(b + DurationMaximum)) v.maximumDurationBeats = p[b + DurationMaximum];
        v.maximumDurationBeats = std::max(v.minimumDurationBeats, v.maximumDurationBeats);
        if (changed(Density)) v.probability = c.densityPercent / 100.0;
    }
    if (changed(Voicing)) c.engine.voicingMode = static_cast<VoicingMode>(integer(Voicing));
    if (changed(StrumMilliseconds)) c.engine.chordStrumMilliseconds = p[StrumMilliseconds];
    if (changed(ChordRoot)) c.engine.chordSystem.root = static_cast<std::uint8_t>(integer(ChordRoot));
    if (changed(ChordScale)) c.engine.chordSystem.scale = static_cast<ScaleId>(integer(ChordScale));
    if (changed(StrumUnits)) c.engine.chordStrumUnit = static_cast<StrumUnit>(integer(StrumUnits));
    if (changed(StrumAmount)) c.engine.chordStrumValue = p[StrumAmount];
    if (changed(MeterSource)) c.timeSignatureSource = static_cast<TimeSignatureSource>(integer(MeterSource));
    if (changed(MeterNumerator)) c.timeSignatureNumerator = integer(MeterNumerator) + 1;
    if (changed(MeterDenominator)) c.timeSignatureDenominator = timeSigDenominators[static_cast<std::size_t>(integer(MeterDenominator))];
    if (changed(AutoReset)) c.engine.autoResetEnabled = p[AutoReset] >= 0.5F;
    if (changed(AutoResetBars)) c.engine.autoResetBars = autoResetBarsValues[static_cast<std::size_t>(integer(AutoResetBars))];
    if (changed(InputMode)) {
        c.engine.inputTransposeEnabled = integer(InputMode) == 1;
        c.engine.inputGateEnabled = integer(InputMode) == 2;
    }
    if (changed(Preset)) c.presetIndex = integer(Preset);
}

UserConfiguration ThreeBSProcessor::currentConfigurationLocked() const {
    auto result = desired_;
    if ((parameterTransaction_.load(std::memory_order_acquire) & 1U) != 0U) return result;
    const auto values = readParameters();
    auto changed = appliedRevision_.load(std::memory_order_acquire) == publishedRevision_.load(std::memory_order_relaxed)
        ? appliedChangedMask_.load(std::memory_order_acquire) : std::uint64_t{};
    for (std::size_t i = 0; i < ParameterCount; ++i)
        if (values[i] != parameterBaseline_[i]) changed |= std::uint64_t{1} << i;
    applyParameters(result, values, changed);
    return result;
}

UserConfiguration ThreeBSProcessor::currentUserConfiguration() const {
    std::lock_guard lock(configurationMutex_);
    return currentConfigurationLocked();
}
SimulationState ThreeBSProcessor::currentInitialState() const { return currentUserConfiguration().initial; }
std::array<double, bodyCount> ThreeBSProcessor::initialPlaneTilts() const {
    std::lock_guard lock(configurationMutex_); return desired_.planeTilts;
}
PresentationState ThreeBSProcessor::presentationState() const { return currentUserConfiguration().presentation; }
juce::String ThreeBSProcessor::lastStateError() const {
    std::lock_guard lock(configurationMutex_); return stateError_;
}
bool ThreeBSProcessor::setPresentationState(const PresentationState& presentation, std::uint64_t expectedRevision) {
    std::lock_guard lock(configurationMutex_);
    if (expectedRevision != 0 && expectedRevision != publishedRevision_.load(std::memory_order_relaxed)) return false;
    auto candidate = currentConfigurationLocked();
    candidate.presentation = presentation;
    if (!validateUserConfiguration(candidate, stateError_)) return false;
    desired_.presentation = presentation;
    return true;
}
void ThreeBSProcessor::setSelectedVoicingPresetIndex(int index) {
    std::lock_guard lock(configurationMutex_);
    desired_.voicingPresetIndex = std::clamp(index, -1, 11);
}

void ThreeBSProcessor::setParameterValue(const juce::String& id, float plainValue) {
    if (auto* parameter = parameters_.getParameter(id)) {
        const auto& range = parameter->getNormalisableRange();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(
            range.snapToLegalValue(std::clamp(plainValue, range.start, range.end))));
    }
}
void ThreeBSProcessor::applyVoiceParameters(const std::array<VoiceConfig, bodyCount>& voices) {
    auto configuration = desired_;
    configuration.engine.voices = voices;
    const auto values = plainParameterValues(configuration);
    for (std::size_t index = Voice1; index < Voicing; ++index)
        setParameterValue(parameterIds_[index], values[index]);
}
void ThreeBSProcessor::installParameterValues(const UserConfiguration& configuration) {
    const auto values = plainParameterValues(configuration);
    for (std::size_t index = 0; index < ParameterCount; ++index)
        setParameterValue(parameterIds_[index], values[index]);
}
bool ThreeBSProcessor::publishConfigurationLocked(const UserConfiguration& candidate, bool reset) {
    if (!validateUserConfiguration(candidate, stateError_)) return false;
    // Writer serialisation is entirely outside processBlock. The sequence guards
    // parameter snapshots against our multi-parameter transactions.
    parameterTransaction_.fetch_add(1, std::memory_order_acq_rel);
    installParameterValues(candidate);
    desired_ = candidate;
    parameterBaseline_ = readParameters();
    const auto revision = publishedRevision_.load(std::memory_order_relaxed) + 1;
    if (reset) resetRevision_ = revision;
    commands_.publish({candidate, parameterBaseline_, revision, resetRevision_});
    publishedRevision_.store(revision, std::memory_order_release);
    parameterTransaction_.fetch_add(1, std::memory_order_release);
    stateError_.clear();
    return true;
}
bool ThreeBSProcessor::applyUserConfiguration(const UserConfiguration& configuration) {
    std::lock_guard lock(configurationMutex_);
    return publishConfigurationLocked(configuration, true);
}

bool ThreeBSProcessor::applyAdvancedConfiguration(const EngineConfig& configuration) {
    std::lock_guard lock(configurationMutex_);
    auto candidate = currentConfigurationLocked();
    // Advanced controls own non-automated fields. Keep currently automated
    // controls rather than restoring the dialog's potentially old copy.
    candidate.engine.simulation.fixedStep = configuration.simulation.fixedStep;
    candidate.engine.simulation.escapeRadius = configuration.simulation.escapeRadius;
    candidate.engine.simulation.escapePolicy = configuration.simulation.escapePolicy;
    candidate.engine.simulation.maxStepsPerAdvance = configuration.simulation.maxStepsPerAdvance;
    candidate.engine.minimumChordIntervalBeats = configuration.minimumChordIntervalBeats;
    candidate.engine.chordSystem.customScale = configuration.chordSystem.customScale;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        const auto& from = configuration.voices[body];
        auto& to = candidate.engine.voices[body];
        to.channel = from.channel;
        to.minimumNote = from.minimumNote; to.maximumNote = from.maximumNote;
        to.customScale = from.customScale;
        to.clockDivisionBeats = from.clockDivisionBeats; to.probability = from.probability;
        to.minimumTriggerIntervalBeats = from.minimumTriggerIntervalBeats;
        to.closeApproachDistance = from.closeApproachDistance;
        to.minimumVelocity = from.minimumVelocity; to.maximumVelocity = from.maximumVelocity;
        to.ccLanes = from.ccLanes;
    }
    candidate.voicingPresetIndex = -1;
    return publishConfigurationLocked(candidate, false);
}

void ThreeBSProcessor::requestPreset(const ArtworkPreset& preset, int index) {
    std::lock_guard lock(configurationMutex_);
    auto candidate = currentConfigurationLocked();
    candidate.initial = candidate.baseInitial = makeInitialState(preset.system, preset.seed, preset.chaos);
    candidate.planeTilts = {};
    candidate.engine.simulation = preset.simulation;
    candidate.engine.voices = preset.voices;
    candidate.presentation = preset.presentation;
    candidate.loopPolicy = preset.loopPolicy;
    candidate.presetIndex = std::clamp(index, 0, 23);
    candidate.voicingPresetIndex = -1;
    candidate.densityPercent = 0.0;
    for (const auto& voice : preset.voices) candidate.densityPercent += voice.probability * (100.0 / 3.0);
    (void)publishConfigurationLocked(candidate, true);
}
void ThreeBSProcessor::requestVoicingPreset(const VoicingPreset& preset, int index) {
    std::lock_guard lock(configurationMutex_);
    auto candidate = currentConfigurationLocked();
    candidate.engine.voices = preset.voices;
    candidate.engine.voicingMode = preset.mode;
    candidate.engine.chordStrumMilliseconds = preset.chordStrumMilliseconds;
    candidate.engine.chordStrumUnit = preset.chordStrumUnit;
    candidate.engine.chordStrumValue = preset.chordStrumValue;
    candidate.engine.chordSystem = preset.chordSystem;
    candidate.densityPercent = preset.densityPercent;
    candidate.voicingPresetIndex = std::clamp(index, 0, 11);
    (void)publishConfigurationLocked(candidate, false);
}
void ThreeBSProcessor::requestRandomize(double chaos) {
    if (!std::isfinite(chaos)) return;
    std::lock_guard lock(configurationMutex_);
    auto candidate = currentConfigurationLocked();
    candidate.baseInitial = makeInitialState(InitialSystem::ControlledChaos,
        nextSeed_.fetch_add(1, std::memory_order_relaxed), std::clamp(chaos, 0.0, 1.0));
    candidate.initial = applyInitialPlaneTilts(candidate.baseInitial, candidate.planeTilts);
    candidate.presentation.visualSeed = candidate.initial.seed;
    (void)publishConfigurationLocked(candidate, true);
}
void ThreeBSProcessor::requestReset() {
    std::lock_guard lock(configurationMutex_);
    (void)publishConfigurationLocked(currentConfigurationLocked(), true);
}
void ThreeBSProcessor::requestExactState(const SimulationState& state) {
    std::lock_guard lock(configurationMutex_);
    auto candidate = currentConfigurationLocked();
    candidate.initial = candidate.baseInitial = state;
    candidate.planeTilts = {};
    candidate.presentation.visualSeed = state.seed;
    (void)publishConfigurationLocked(candidate, true);
}
void ThreeBSProcessor::requestPlaneTilts(const std::array<double, bodyCount>& tiltDegrees) {
    std::lock_guard lock(configurationMutex_);
    for (const auto tilt : tiltDegrees)
        if (!std::isfinite(tilt) || tilt < -75.0 || tilt > 75.0) {
            stateError_ = "Invalid orbital plane tilt."; return;
        }
    auto candidate = currentConfigurationLocked();
    candidate.planeTilts = tiltDegrees;
    candidate.initial = applyInitialPlaneTilts(candidate.baseInitial, tiltDegrees);
    (void)publishConfigurationLocked(candidate, true);
}

void ThreeBSProcessor::prepareToPlay(double sampleRate, int maximumBlockSize) {
    juce::ignoreUnused(maximumBlockSize);
    sampleRate_ = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 768000.0) : 48000.0;
    engine_.prepare(sampleRate_);
    // Preserve elapsed processing time across sample-rate changes. Outstanding
    // notes are flushed on the next legal processing callback.
    panicRequested_.store(true, std::memory_order_release);
    haveHostPosition_ = false;
    hostWasPlaying_ = false;
    clearInputKeys();
}
void ThreeBSProcessor::releaseResources() {
    // JUCE provides no output buffer here. Defer cleanup to the next callback;
    // host teardown must also honour its own MIDI note-termination contract.
    panicRequested_.store(true, std::memory_order_release);
    clearInputKeys();
    haveHostPosition_ = false;
    hostWasPlaying_ = false;
}

bool ThreeBSProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
#if THREEBS_AU_MIDI_EFFECT
    return layouts.getMainInputChannelSet().isDisabled() && layouts.getMainOutputChannelSet().isDisabled();
#else
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
#endif
}

bool ThreeBSProcessor::consumeCommand(EventSink sink) noexcept {
    EngineCommand command;
    if (!commands_.consume(command)) return false;
    engine_.allNotesOff(0, sink);
    activeConfiguration_ = command.configuration;
    activeParameters_ = command.parameters;
    activeChangedMask_ = 0;
    activeRevision_ = command.revision;
    (void)engine_.setConfig(activeConfiguration_.engine);
    if (command.resetRevision > activeResetRevision_) {
        (void)engine_.reset(activeConfiguration_.initial, sink);
        activeResetRevision_ = command.resetRevision;
        freeBeat_ = 0.0;
        hostWasPlaying_ = false;
        haveHostPosition_ = false;
        clearInputKeys();
        ++trajectoryRevision_;
    }
    appliedChangedMask_.store(0, std::memory_order_release);
    appliedRevision_.store(activeRevision_, std::memory_order_release);
    return true;
}

void ThreeBSProcessor::updateEngineConfigFromParameters() noexcept {
    auto current = activeConfiguration_;
    applyParameters(current, activeParameters_, activeChangedMask_);
    current.engine.inputTranspose = current.engine.inputTransposeEnabled ? inputTranspose_ : 0;
    current.engine.inputGateOpen = !current.engine.inputGateEnabled || heldNotes_ > 0;
    if (!engine_.setConfig(current.engine)) {
        realtimeFaults_.fetch_or(1U, std::memory_order_relaxed);
        return;
    }
    for (std::size_t body = 0; body < bodyCount; ++body)
        engine_.setBodyMass(body, current.initial.bodies[body].mass);
}

void ThreeBSProcessor::clearInputKeys() noexcept {
    heldKeys_ = {};
    keyOrder_ = {};
    heldNotes_ = 0;
    inputOrder_ = 0;
    inputTranspose_ = 0;
}

void ThreeBSProcessor::applyInputEvent(const InputEvent& event) noexcept {
    const auto channel = static_cast<std::size_t>(event.status & 0x0FU);
    const auto kind = event.status & 0xF0U;
    const auto note = static_cast<std::size_t>(event.data1);
    if (kind == 0x90U && event.data2 != 0U) {
        auto& count = heldKeys_[channel][note];
        if (count < std::numeric_limits<std::uint16_t>::max()) { ++count; ++heldNotes_; }
        keyOrder_[channel][note] = ++inputOrder_;
        inputTranspose_ = static_cast<std::int8_t>(note % 12U);
        return;
    }
    if (kind == 0x80U || (kind == 0x90U && event.data2 == 0U)) {
        auto& count = heldKeys_[channel][note];
        if (count > 0) { --count; --heldNotes_; }
    } else if (kind == 0xB0U && (note == 120U || note == 123U)) {
        for (auto& count : heldKeys_[channel]) { heldNotes_ -= count; count = 0; }
    } else return;
    std::uint64_t latest{};
    for (std::size_t c = 0; c < heldKeys_.size(); ++c)
        for (std::size_t key = 0; key < heldKeys_[c].size(); ++key)
            if (heldKeys_[c][key] != 0 && keyOrder_[c][key] > latest) {
                latest = keyOrder_[c][key];
                inputTranspose_ = static_cast<std::int8_t>(key % 12U);
            }
    // Transpose holds its last pitch after the final release, as before. Gate
    // depends on the key ledger and closes immediately at the release offset.
}

void ThreeBSProcessor::writeEvent(void* context, const MidiEvent& event) noexcept {
    static_cast<ThreeBSProcessor*>(context)->emitEvent(event);
}
void ThreeBSProcessor::emitEvent(const MidiEvent& event) noexcept {
    const auto absoluteOffset = outputOffset_ + event.sampleOffset;
    std::array<juce::uint8, 3> bytes{};
    const auto kind = event.type == MidiEventType::NoteOn ? 0x90U
        : event.type == MidiEventType::NoteOff ? 0x80U : 0xB0U;
    bytes[0] = static_cast<juce::uint8>(kind | ((event.channel - 1U) & 0x0FU));
    bytes[1] = event.data1; bytes[2] = event.data2;
    // The MIDI buffer is host-owned. Its capacity must be provided/reused by the
    // host; the engine and adapter own no growing storage on this path.
    if (outputMidi_ != nullptr)
        (void)outputMidi_->addEvent(bytes.data(), static_cast<int>(bytes.size()), static_cast<int>(absoluteOffset));
    if (event.type == MidiEventType::ControlChange) return;
    NoteVisualizationEvent visual;
    visual.type = event.type == MidiEventType::NoteOn ? NoteVisualizationType::On : NoteVisualizationType::Off;
    visual.body = event.sourceBody; visual.note = event.data1; visual.velocity = event.data2;
    visual.sequence = ++noteVisualizationSequence_;
    visual.timelineSeconds = processingSeconds_ + static_cast<double>(absoluteOffset) / sampleRate_;
    visual.trajectoryRevision = trajectoryRevision_;
    (void)noteVisualizationEvents_.push(visual);
}

void ThreeBSProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
    const auto sampleCount = static_cast<std::uint32_t>(audio.getNumSamples());
    std::size_t inputCount{};
    bool inputOverflow{};
    for (const auto metadata : midi) {
        // Inspect the non-owning view. Long SysEx/meta messages are intentionally
        // ignored without allocating an owning MidiMessage.
        if (metadata.numBytes != 3 || metadata.data == nullptr) continue;
        const auto kind = metadata.data[0] & 0xF0U;
        if (!(kind == 0x80U || kind == 0x90U
            || (kind == 0xB0U && (metadata.data[1] == 120U || metadata.data[1] == 123U)))) continue;
        if (metadata.data[1] >= 128U || metadata.data[2] >= 128U
            || metadata.samplePosition < 0
            || static_cast<std::uint32_t>(metadata.samplePosition) >= sampleCount) {
            realtimeFaults_.fetch_or(2U, std::memory_order_relaxed); continue;
        }
        if (inputCount == inputEvents_.size()) { inputOverflow = true; break; }
        inputEvents_[inputCount++] = {static_cast<std::uint32_t>(metadata.samplePosition),
            metadata.data[0], metadata.data[1], metadata.data[2]};
    }
    midi.clear();
    outputMidi_ = &midi;
    outputOffset_ = 0;
    const EventSink sink{this, &ThreeBSProcessor::writeEvent};
    const bool replaced = consumeCommand(sink);
    if (!replaced) {
        const auto before = parameterTransaction_.load(std::memory_order_acquire);
        if ((before & 1U) == 0 && publishedRevision_.load(std::memory_order_acquire) == activeRevision_) {
            const auto values = readParameters();
            if (parameterTransaction_.load(std::memory_order_acquire) == before
                && publishedRevision_.load(std::memory_order_acquire) == activeRevision_) {
                for (std::size_t i = 0; i < ParameterCount; ++i)
                    if (values[i] != activeParameters_[i]) activeChangedMask_ |= std::uint64_t{1} << i;
                activeParameters_ = values;
                appliedChangedMask_.store(activeChangedMask_, std::memory_order_release);
            }
        }
    }
    auto current = activeConfiguration_;
    applyParameters(current, activeParameters_, activeChangedMask_);
    if (panicRequested_.exchange(false, std::memory_order_acq_rel) || inputOverflow) {
        engine_.allNotesOff(0, sink);
        clearInputKeys();
    }
    if (inputOverflow) {
        realtimeFaults_.fetch_or(4U, std::memory_order_relaxed);
        processingSeconds_ += static_cast<double>(sampleCount) / sampleRate_;
        freeBeat_ += 2.0 * static_cast<double>(sampleCount) / sampleRate_;
        hostWasPlaying_ = false;
        haveHostPosition_ = false;
        publishSnapshot();
        outputMidi_ = nullptr;
        return;
    }

    double bpm = 120.0;
    double ppq = freeBeat_;
    bool hostPlaying = true, loopWrapped = false, seeked = false;
    int numerator = current.timeSignatureNumerator;
    int denominator = current.timeSignatureDenominator;
    bool timingValid = true;
    if (auto* playHead = getPlayHead()) {
        if (const auto position = playHead->getPosition()) {
            bpm = position->getBpm().orFallback(120.0);
            ppq = position->getPpqPosition().orFallback(freeBeat_);
            hostPlaying = position->getIsPlaying();
            timingValid = std::isfinite(bpm) && bpm > 0.0 && bpm <= 1000.0 && std::isfinite(ppq);
            if (current.timeSignatureSource == TimeSignatureSource::Host)
                if (const auto signature = position->getTimeSignature()) {
                    timingValid = timingValid && signature->numerator >= 1 && signature->numerator <= 64
                        && signature->denominator >= 1 && signature->denominator <= 64;
                    numerator = signature->numerator; denominator = signature->denominator;
                }
            if (haveHostPosition_ && timingValid && hostWasPlaying_ && hostPlaying) {
                const auto expected = lastPpq_ + lastBeatsPerSample_ * static_cast<double>(lastBlockSize_);
                const auto tolerance = std::max(1.0e-7, 2.0 * std::abs(lastBeatsPerSample_));
                if (position->getIsLooping())
                    if (const auto loop = position->getLoopPoints()) {
                        const auto length = loop->ppqEnd - loop->ppqStart;
                        if (std::isfinite(length) && length > 0.0 && expected >= loop->ppqEnd - tolerance) {
                            const auto wrapped = loop->ppqStart + std::fmod(expected - loop->ppqStart, length);
                            loopWrapped = std::abs(ppq - wrapped) <= tolerance;
                        }
                    }
                seeked = !loopWrapped && std::abs(ppq - expected) > tolerance;
            }
            haveHostPosition_ = timingValid;
        }
    }
    if (!timingValid) {
        realtimeFaults_.fetch_or(8U, std::memory_order_relaxed);
        engine_.allNotesOff(0, sink);
        clearInputKeys();
        processingSeconds_ += static_cast<double>(sampleCount) / sampleRate_;
        hostWasPlaying_ = false;
        haveHostPosition_ = false;
        publishSnapshot();
        outputMidi_ = nullptr;
        return;
    }
    const auto beatsPerSample = bpm / (60.0 * sampleRate_);
    ProcessContext context;
    context.sampleRate = sampleRate_;
    context.beatAtStart = current.sync ? ppq : freeBeat_;
    context.beatsPerSample = beatsPerSample;
    context.timeSigNumerator = numerator; context.timeSigDenominator = denominator;
    context.playing = current.run && (!current.sync || hostPlaying);
    context.transportStarted = context.playing && !hostWasPlaying_;
    context.seeked = current.sync && seeked;
    context.loopWrapped = current.sync && loopWrapped;
    context.loopPolicy = current.loopPolicy;
    const bool resetTimeline = context.transportStarted || context.seeked
        || (context.loopWrapped && context.loopPolicy == LoopPolicy::Restart);
    if (resetTimeline || (!context.playing && hostWasPlaying_)) {
        clearInputKeys();
        ++trajectoryRevision_;
    }
    updateEngineConfigFromParameters();
    std::uint32_t offset{};
    std::size_t index{};
    bool first = true;
    const auto processSpan = [&](std::uint32_t end) {
        if (end == offset && !first) return;
        auto segment = context;
        segment.sampleCount = end - offset;
        segment.beatAtStart += beatsPerSample * static_cast<double>(offset);
        segment.transportStarted = first && context.transportStarted;
        segment.seeked = first && context.seeked;
        segment.loopWrapped = first && context.loopWrapped;
        outputOffset_ = offset;
        engine_.process(segment, sink);
        first = false;
        offset = end;
    };
    while (index < inputCount) {
        const auto eventOffset = inputEvents_[index].offset;
        if (eventOffset > offset) processSpan(eventOffset);
        do { applyInputEvent(inputEvents_[index++]); }
        while (index < inputCount && inputEvents_[index].offset == eventOffset);
        updateEngineConfigFromParameters();
    }
    processSpan(sampleCount);
    freeBeat_ += beatsPerSample * static_cast<double>(sampleCount);
    processingSeconds_ += static_cast<double>(sampleCount) / sampleRate_;
    hostWasPlaying_ = context.playing;
    lastPpq_ = ppq;
    lastBeatsPerSample_ = beatsPerSample;
    lastBlockSize_ = static_cast<int>(sampleCount);
    const auto& respawns = engine_.simulation().state().respawnCount;
    if (respawns != lastRespawnCounts_) { lastRespawnCounts_ = respawns; ++trajectoryRevision_; }
    publishSnapshot();
    outputMidi_ = nullptr;
}

void ThreeBSProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    audio.clear(); midi.clear();
    outputMidi_ = &midi; outputOffset_ = 0;
    engine_.allNotesOff(0, EventSink{this, &ThreeBSProcessor::writeEvent});
    clearInputKeys();
    hostWasPlaying_ = false;
    haveHostPosition_ = false;
    processingSeconds_ += static_cast<double>(audio.getNumSamples()) / sampleRate_;
    publishSnapshot();
    outputMidi_ = nullptr;
}

void ThreeBSProcessor::publishSnapshot() noexcept {
    RenderSnapshot snapshot;
    snapshot.bodies = engine_.simulation().state().bodies;
    snapshot.escaped = engine_.simulation().state().escaped;
    snapshot.sequence = ++snapshotSequence_;
    snapshot.trajectoryRevision = trajectoryRevision_;
    snapshot.interpolationAlpha = engine_.simulation().interpolationAlpha();
    snapshot.timelineSeconds = processingSeconds_;
    std::uint32_t escaped{};
    if (engine_.config().simulation.escapePolicy == EscapePolicy::Prompt)
        for (std::size_t body = 0; body < bodyCount; ++body)
            if (snapshot.escaped[body]) escaped |= 1U << body;
    escapePromptMask_.store(escaped, std::memory_order_relaxed);
    // The renderer exclusively owns the read index. Dropping a visual frame
    // never modifies its slot while the renderer is reading it.
    if (!snapshots_.push(snapshot)) droppedSnapshots_.fetch_add(1, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* ThreeBSProcessor::createEditor() { return new ThreeBSEditor(*this); }

void ThreeBSProcessor::getStateInformation(juce::MemoryBlock& destination) {
    std::lock_guard lock(configurationMutex_);
    const auto configuration = currentConfigurationLocked();
    auto state = parameters_.copyState();
    const auto values = legalParameterValues(configuration);
    for (std::size_t i = 0; i < ParameterCount; ++i) setStateParameter(state, parameterIds_[i], values[i]);
    state.setProperty("schemaVersion", static_cast<int>(stateSchemaVersion), nullptr);
    state.setProperty("configurationTemplateJson", serializeUserConfiguration(configuration), nullptr);
    const auto& presentation = configuration.presentation;
    state.setProperty("visualTrailSeconds", presentation.visual.trailSeconds, nullptr);
    state.setProperty("visualTrailWidth", presentation.visual.trailWidth, nullptr);
    state.setProperty("visualExtrusion", presentation.visual.extrusion, nullptr);
    state.setProperty("visualBloom", presentation.visual.bloom, nullptr);
    state.setProperty("visualStarDensity", presentation.visual.starDensity, nullptr);
    state.setProperty("visualPalette", static_cast<int>(presentation.visual.palette), nullptr);
    state.setProperty("visualQuality", static_cast<int>(presentation.visual.quality), nullptr);
    state.setProperty("cameraYaw", presentation.camera.yaw, nullptr);
    state.setProperty("cameraPitch", presentation.camera.pitch, nullptr);
    state.setProperty("cameraDistance", presentation.camera.distance, nullptr);
    state.setProperty("cameraMinimumDistance", presentation.camera.minimumDistance, nullptr);
    state.setProperty("cameraMaximumDistance", presentation.camera.maximumDistance, nullptr);
    state.setProperty("cameraAutoOrbit", presentation.camera.autoOrbit, nullptr);
    state.setProperty("cameraFocusBody", presentation.camera.focusBody, nullptr);
    state.setProperty("cameraAutoFrame", presentation.camera.autoFrame, nullptr);
    state.setProperty("notePaneMinimized", presentation.notePaneMinimized, nullptr);
    state.setProperty("notePaneStyle", static_cast<int>(presentation.notePaneStyle), nullptr);
    state.setProperty("visualSeed", juce::String(presentation.visualSeed), nullptr);
    state.setProperty("seed", juce::String(configuration.initial.seed), nullptr);
    state.setProperty("loopPolicy", static_cast<int>(configuration.loopPolicy), nullptr);
    for (std::size_t body = 0; body < bodyCount; ++body) {
        state.setProperty("initialTiltBody" + juce::String(body), configuration.planeTilts[body], nullptr);
        for (int base = 0; base < 2; ++base) {
            const auto prefix = juce::String(base == 0 ? "body" : "baseBody") + juce::String(body) + "_";
            const auto& value = (base == 0 ? configuration.initial : configuration.baseInitial).bodies[body];
            state.setProperty(prefix + "mass", value.mass, nullptr);
            state.setProperty(prefix + "px", value.position.x, nullptr);
            state.setProperty(prefix + "py", value.position.y, nullptr);
            state.setProperty(prefix + "pz", value.position.z, nullptr);
            state.setProperty(prefix + "vx", value.velocity.x, nullptr);
            state.setProperty(prefix + "vy", value.velocity.y, nullptr);
            state.setProperty(prefix + "vz", value.velocity.z, nullptr);
        }
    }
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void ThreeBSProcessor::setStateInformation(const void* data, int size) {
    std::lock_guard lock(configurationMutex_);
    const auto reject = [this](const juce::String& reason) { stateError_ = reason; };
    if (data == nullptr || size <= 0 || size > 2 * 1024 * 1024) {
        reject("Invalid or oversized host state."); return;
    }
    const auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(stateTreeName)) { reject("Invalid host-state document."); return; }
    const auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid()) { reject("Invalid host-state tree."); return; }
    bool valid = true;
    const auto numeric = [&](const juce::var& value, double minimum, double maximum,
                             bool integral, double fallback) {
        double result{};
        if (!hostNumber(value, result) || result < minimum || result > maximum
            || (integral && std::trunc(result) != result)) {
            valid = false; return fallback;
        }
        return result;
    };
    const auto property = [&](const juce::String& name, double fallback, double minimum,
                              double maximum, bool integral = false) {
        return state.hasProperty(name) ? numeric(state.getProperty(name), minimum, maximum, integral, fallback)
                                       : fallback;
    };
    const auto schema = static_cast<int>(property("schemaVersion", 1, 1, stateSchemaVersion, true));
    if (!valid) { reject("Unsupported or malformed host-state schema."); return; }
    int selected{};
    if (const auto value = stateParameter(state, "preset"); !value.isVoid())
        selected = static_cast<int>(numeric(value, 0, 23, true, 0));
    if (!valid) { reject("Invalid host preset index."); return; }
    UserConfiguration candidate;
    MusicEngine defaultEngine;
    candidate.engine = defaultEngine.config();
    candidate.initial = candidate.baseInitial = defaultEngine.simulation().initialState();
    PresetCatalog catalog;
    if (catalog.valid() && static_cast<std::size_t>(selected) < catalog.size()) {
        const auto& preset = catalog[static_cast<std::size_t>(selected)];
        candidate.initial = candidate.baseInitial = makeInitialState(preset.system, preset.seed, preset.chaos);
        candidate.engine.simulation = preset.simulation;
        candidate.engine.voices = preset.voices;
        candidate.presentation = preset.presentation;
        candidate.loopPolicy = preset.loopPolicy;
        candidate.presetIndex = selected;
    }
    if (state.hasProperty("configurationTemplateJson")) {
        const auto json = state.getProperty("configurationTemplateJson");
        if (!json.isString() || !deserializeUserConfiguration(json.toString(), candidate, stateError_)) return;
    } else if (schema >= 8) { reject("Host state is missing its complete configuration."); return; }

    const auto initialSeed = state.getProperty("seed", juce::String(candidate.initial.seed));
    if (!hostSeed(initialSeed, candidate.initial.seed)) { reject("Invalid host-state seed."); return; }
    candidate.baseInitial.seed = candidate.initial.seed;
    for (std::size_t body = 0; body < bodyCount; ++body) {
        for (int base = 0; base < 2; ++base) {
            auto& value = (base == 0 ? candidate.initial : candidate.baseInitial).bodies[body];
            if (base == 1 && !state.hasProperty("baseBody" + juce::String(body) + "_px"))
                value = candidate.initial.bodies[body];
            const auto prefix = juce::String(base == 0 ? "body" : "baseBody") + juce::String(body) + "_";
            value.mass = property(prefix + "mass", value.mass, 0.05, 1000.0);
            value.position.x = property(prefix + "px", value.position.x, -1.0e6, 1.0e6);
            value.position.y = property(prefix + "py", value.position.y, -1.0e6, 1.0e6);
            value.position.z = property(prefix + "pz", value.position.z, -1.0e6, 1.0e6);
            value.velocity.x = property(prefix + "vx", value.velocity.x, -1.0e6, 1.0e6);
            value.velocity.y = property(prefix + "vy", value.velocity.y, -1.0e6, 1.0e6);
            value.velocity.z = property(prefix + "vz", value.velocity.z, -1.0e6, 1.0e6);
        }
        candidate.planeTilts[body] = property("initialTiltBody" + juce::String(body), 0, -75.0, 75.0);
    }
    candidate.loopPolicy = static_cast<LoopPolicy>(static_cast<int>(property(
        "loopPolicy", static_cast<int>(candidate.loopPolicy), 0, 1, true)));
    auto& visual = candidate.presentation.visual;
    auto& camera = candidate.presentation.camera;
    visual.trailSeconds = static_cast<float>(property("visualTrailSeconds", visual.trailSeconds, 5, 60));
    visual.trailWidth = static_cast<float>(property("visualTrailWidth", visual.trailWidth, 0.1, 10));
    visual.extrusion = static_cast<float>(property("visualExtrusion", visual.extrusion, 0, 2));
    visual.bloom = static_cast<float>(property("visualBloom", visual.bloom, 0, 1));
    visual.starDensity = static_cast<float>(property("visualStarDensity", visual.starDensity, 0, 1));
    visual.palette = static_cast<PaletteId>(static_cast<int>(property("visualPalette", static_cast<int>(visual.palette), 0, 5, true)));
    visual.quality = static_cast<GraphicsQuality>(static_cast<int>(property("visualQuality", static_cast<int>(visual.quality), 0, 2, true)));
    camera.yaw = static_cast<float>(property("cameraYaw", camera.yaw, -1000, 1000));
    camera.pitch = static_cast<float>(property("cameraPitch", camera.pitch, -1.49, 1.49));
    camera.distance = static_cast<float>(property("cameraDistance", camera.distance, 1, 80));
    camera.minimumDistance = static_cast<float>(property("cameraMinimumDistance", 2.5, 1, 20));
    camera.maximumDistance = static_cast<float>(property("cameraMaximumDistance", 40, 5, 80));
    camera.autoOrbit = static_cast<float>(property("cameraAutoOrbit", camera.autoOrbit, -2, 2));
    camera.focusBody = static_cast<int>(property("cameraFocusBody", camera.focusBody, -1, 2, true));
    camera.autoFrame = property("cameraAutoFrame", 1, 0, 1, true) != 0;
    candidate.presentation.notePaneMinimized = property("notePaneMinimized", 0, 0, 1, true) != 0;
    candidate.presentation.notePaneStyle = static_cast<NotePaneStyle>(static_cast<int>(property("notePaneStyle", 0, 0, 1, true)));
    if (!hostSeed(state.getProperty("visualSeed", juce::String(candidate.presentation.visualSeed)), candidate.presentation.visualSeed))
        valid = false;
    if (!valid) { reject("Host state contains an invalid numeric value."); return; }

    auto expected = legalParameterValues(candidate);
    auto incoming = expected;
    for (std::size_t i = 0; i < ParameterCount; ++i) {
        int occurrences = state.hasProperty(parameterIds_[i]) ? 1 : 0;
        for (int child = 0; child < state.getNumChildren(); ++child)
            if (state.getChild(child).getProperty("id").toString() == parameterIds_[i]) ++occurrences;
        if (occurrences > 1) { reject("Host state contains duplicate parameter values."); return; }
        const auto value = stateParameter(state, parameterIds_[i]);
        if (occurrences == 0) continue;
        if (schema < 2 && i == Trail) {
            const auto old = numeric(value, 0.0, 100.0, false, 82.0);
            incoming[i] = migrateV1TrailLength(static_cast<float>(old <= 1.0 ? old : old / 100.0));
        } else {
            const bool integral = i == Run || i == Sync || i == Voicing || i == ChordRoot
                || i == ChordScale || i == StrumUnits || i >= MeterSource
                || (i >= Voice1 && i < Voicing && (i - Voice1) % 10 < 8);
            incoming[i] = static_cast<float>(numeric(value, parameterMinimums_[i], parameterMaximums_[i], integral, expected[i]));
        }
    }
    if (schema < 5) {
        incoming[Voicing] = 0.0F; incoming[StrumMilliseconds] = 24.0F;
        for (std::size_t body = 0; body < bodyCount; ++body)
            if (stateParameter(state, parameterIds_[Voice1 + body * 10 + Root]).isVoid())
                incoming[Voice1 + body * 10 + Root] = catalog.valid()
                    ? static_cast<float>(catalog[static_cast<std::size_t>(selected)].voices[body].root) : 0.0F;
    }
    if (schema < 6) {
        for (std::size_t body = 0; body < bodyCount; ++body) {
            const auto base = Voice1 + body * 10;
            const auto value = stateParameter(state, "voiceDuration" + juce::String(body + 1));
            const auto duration = value.isVoid() ? 0.2 : numeric(value, 1.0 / 1024.0, 64.0, false, 0.2);
            candidate.engine.voices[body].minimumDurationBeats = duration;
            candidate.engine.voices[body].maximumDurationBeats = duration;
            incoming[base + Octave] = 0.0F;
            incoming[base + DurationMap] = static_cast<float>(PitchMapping::Speed);
            // A pre-v6 duration is an exact musical value, not an instruction to
            // truncate it to the later coarse host slider's range.
            const auto updated = legalParameterValues(candidate);
            incoming[base + DurationMinimum] = updated[base + DurationMinimum];
            incoming[base + DurationMaximum] = updated[base + DurationMaximum];
        }
        incoming[ChordRoot] = incoming[Voice1 + Root]; incoming[ChordScale] = incoming[Voice1 + Scale];
        incoming[StrumUnits] = 0; incoming[StrumAmount] = 0.0625F;
        incoming[MeterSource] = 0; incoming[MeterNumerator] = 3; incoming[MeterDenominator] = 2;
        incoming[AutoReset] = 0; incoming[AutoResetBars] = 2;
    }
    if (schema < 7)
        for (std::size_t body = 0; body < bodyCount; ++body)
            incoming[Voice1 + body * 10 + DurationGridValue] = static_cast<float>(DurationGrid::ContinuousLegacy);
    if (!valid) { reject("Host state contains an invalid parameter value."); return; }
    expected = legalParameterValues(candidate);
    std::uint64_t changed{};
    for (std::size_t i = 0; i < ParameterCount; ++i)
        if (std::abs(incoming[i] - expected[i]) > 1.0e-6F)
            changed |= std::uint64_t{1} << i;
    applyParameters(candidate, incoming, changed);
    // Published schemas <=3 did not contain camera limits; their established
    // defaults are applied above. Reject contradictory present limits in newer states.
    if (schema < 4) candidate.presentation.camera = sanitizedCameraState(candidate.presentation.camera);
    if (!validateUserConfiguration(candidate, stateError_)) return;
    (void)publishConfigurationLocked(candidate, true);
}

} // namespace threebs

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new threebs::ThreeBSProcessor(); }
