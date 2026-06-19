// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "plugin/PluginProcessor.h"

#include "plugin/PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace threebs {
namespace {

constexpr auto stateTreeName = "THREE_BS_STATE";

float parameterValue(const juce::AudioProcessorValueTreeState& state, const juce::String& id) noexcept {
    if (const auto* value = state.getRawParameterValue(id))
        return value->load(std::memory_order_relaxed);
    return 0.0F;
}

} // namespace

void ThreeBSProcessor::AtomicState::store(const SimulationState& state) noexcept {
    seed.store(state.seed, std::memory_order_relaxed);
    for (std::size_t i = 0; i < bodyCount; ++i) {
        mass[i].store(state.bodies[i].mass, std::memory_order_relaxed);
        px[i].store(state.bodies[i].position.x, std::memory_order_relaxed);
        py[i].store(state.bodies[i].position.y, std::memory_order_relaxed);
        pz[i].store(state.bodies[i].position.z, std::memory_order_relaxed);
        vx[i].store(state.bodies[i].velocity.x, std::memory_order_relaxed);
        vy[i].store(state.bodies[i].velocity.y, std::memory_order_relaxed);
        vz[i].store(state.bodies[i].velocity.z, std::memory_order_relaxed);
    }
}

SimulationState ThreeBSProcessor::AtomicState::load() const noexcept {
    SimulationState state;
    state.seed = seed.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < bodyCount; ++i) {
        state.bodies[i].mass = mass[i].load(std::memory_order_relaxed);
        state.bodies[i].position = {px[i].load(std::memory_order_relaxed), py[i].load(std::memory_order_relaxed),
                                    pz[i].load(std::memory_order_relaxed)};
        state.bodies[i].velocity = {vx[i].load(std::memory_order_relaxed), vy[i].load(std::memory_order_relaxed),
                                    vz[i].load(std::memory_order_relaxed)};
    }
    return state;
}

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
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"trail", 1}, "Trail", 5.0F, 100.0F, 82.0F));
    values.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"bloom", 1}, "Bloom", 0.0F, 100.0F, 34.0F));
    for (int i = 0; i < 3; ++i)
        values.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"mass" + juce::String(i + 1), 1}, "Mass " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.05F, 8.0F, 0.001F, 0.45F), 1.0F));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"inputMode", 1}, "MIDI Input",
        juce::StringArray{"Off", "Transpose", "Gate"}, 0));
    juce::StringArray presetNames;
    for (int i = 1; i <= 24; ++i)
        presetNames.add("Preset " + juce::String(i));
    values.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"preset", 1}, "Factory Preset", presetNames, 0));
    return {values.begin(), values.end()};
}

ThreeBSProcessor::ThreeBSProcessor()
#if THREEBS_AU_MIDI_EFFECT
    : AudioProcessor(BusesProperties()),
#else
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      parameters_(*this, nullptr, stateTreeName, createParameterLayout()), engine_() {
    parameterRefs_.run = parameters_.getRawParameterValue("run");
    parameterRefs_.sync = parameters_.getRawParameterValue("sync");
    parameterRefs_.speed = parameters_.getRawParameterValue("speed");
    parameterRefs_.gravity = parameters_.getRawParameterValue("gravity");
    parameterRefs_.softening = parameters_.getRawParameterValue("softening");
    parameterRefs_.chaos = parameters_.getRawParameterValue("chaos");
    parameterRefs_.density = parameters_.getRawParameterValue("density");
    parameterRefs_.trail = parameters_.getRawParameterValue("trail");
    parameterRefs_.bloom = parameters_.getRawParameterValue("bloom");
    for (std::size_t body = 0; body < bodyCount; ++body)
        parameterRefs_.masses[body] = parameters_.getRawParameterValue("mass" + juce::String(body + 1));
    parameterRefs_.inputMode = parameters_.getRawParameterValue("inputMode");
    parameterRefs_.preset = parameters_.getRawParameterValue("preset");
    engineConfig_ = engine_.config();
    PresetCatalog catalog;
    if (catalog.valid()) {
        const auto& preset = catalog[0];
        const auto initial = makeInitialState(preset.system, preset.seed, preset.chaos);
        engineConfig_.simulation = preset.simulation;
        engineConfig_.voices = preset.voices;
        engine_.setConfig(engineConfig_);
        engine_.reset(initial);
        loopPolicy_.store(static_cast<int>(preset.loopPolicy), std::memory_order_relaxed);
        storedInitial_.store(initial);
        setParameterValue("gravity", static_cast<float>(preset.simulation.gravitationalConstant));
        setParameterValue("softening", static_cast<float>(preset.simulation.softening));
        setParameterValue("speed", static_cast<float>(preset.simulation.speed));
        for (std::size_t body = 0; body < bodyCount; ++body)
            setParameterValue("mass" + juce::String(body + 1), static_cast<float>(initial.bodies[body].mass));
    } else {
        storedInitial_.store(engine_.simulation().initialState());
    }
}

void ThreeBSProcessor::prepareToPlay(double sampleRate, int maximumBlockSize) {
    juce::ignoreUnused(maximumBlockSize);
    sampleRate_ = sampleRate;
    engine_.prepare(sampleRate);
    freeBeat_ = 0.0;
    haveHostPosition_ = false;
}

void ThreeBSProcessor::releaseResources() {}

bool ThreeBSProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
#if THREEBS_AU_MIDI_EFFECT
    return layouts.getMainInputChannelSet().isDisabled() && layouts.getMainOutputChannelSet().isDisabled();
#else
    return layouts.getMainInputChannelSet().isDisabled()
           && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
#endif
}

void ThreeBSProcessor::consumeCommands(MusicEngine::EventBuffer& eventBuffer) noexcept {
    EngineCommand command;
    while (commands_.pop(command)) {
        if (command.type == CommandType::Replace) {
            engineConfig_ = command.config;
            engine_.setConfig(engineConfig_);
            loopPolicy_.store(static_cast<int>(command.loopPolicy), std::memory_order_relaxed);
        }
        engine_.reset(command.state, &eventBuffer);
        storedInitial_.store(command.state);
        freeBeat_ = 0.0;
        hostWasPlaying_ = false;
        haveHostPosition_ = false;
    }
}

void ThreeBSProcessor::updateEngineConfigFromParameters() noexcept {
    engineConfig_.simulation.gravitationalConstant = parameterRefs_.gravity->load(std::memory_order_relaxed);
    engineConfig_.simulation.softening = parameterRefs_.softening->load(std::memory_order_relaxed);
    engineConfig_.simulation.speed = parameterRefs_.speed->load(std::memory_order_relaxed);
    const auto density = static_cast<double>(parameterRefs_.density->load(std::memory_order_relaxed) / 100.0F);
    for (auto& voice : engineConfig_.voices)
        voice.probability = density;
    const auto inputMode = static_cast<int>(parameterRefs_.inputMode->load(std::memory_order_relaxed));
    engineConfig_.inputTransposeEnabled = inputMode == 1;
    engineConfig_.inputGateEnabled = inputMode == 2;
    engineConfig_.inputTranspose = engineConfig_.inputTransposeEnabled ? inputTranspose_ : 0;
    engineConfig_.inputGateOpen = !engineConfig_.inputGateEnabled || heldNotes_ > 0;
    engine_.setConfig(engineConfig_);
    for (std::size_t body = 0; body < bodyCount; ++body)
        engine_.setBodyMass(body, parameterRefs_.masses[body]->load(std::memory_order_relaxed));
    storedInitial_.store(engine_.simulation().initialState());
}

void ThreeBSProcessor::addEvents(const MusicEngine::EventBuffer& events, juce::MidiBuffer& midi) {
    for (const auto& event : events) {
        juce::MidiMessage message;
        switch (event.type) {
        case MidiEventType::NoteOn:
            message = juce::MidiMessage::noteOn(event.channel, event.data1, event.data2);
            break;
        case MidiEventType::NoteOff:
            message = juce::MidiMessage::noteOff(event.channel, event.data1);
            break;
        case MidiEventType::ControlChange:
            message = juce::MidiMessage::controllerEvent(event.channel, event.data1, event.data2);
            break;
        }
        midi.addEvent(message, static_cast<int>(event.sampleOffset));
    }
}

void ThreeBSProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    audio.clear();

    for (const auto metadata : midi) {
        const auto message = metadata.getMessage();
        if (message.isNoteOn()) {
            ++heldNotes_;
            inputTranspose_ = static_cast<std::int8_t>(message.getNoteNumber() % 12);
        } else if (message.isNoteOff()) {
            heldNotes_ = std::max(0, heldNotes_ - 1);
        }
    }
    midi.clear();

    MusicEngine::EventBuffer generated;
    consumeCommands(generated);
    addEvents(generated, midi);
    updateEngineConfigFromParameters();

    const auto sampleCount = static_cast<std::uint32_t>(audio.getNumSamples());
    auto bpm = 120.0;
    auto ppq = freeBeat_;
    auto hostPlaying = true;
    bool loopWrapped{};
    bool seeked{};
    if (auto* playHead = getPlayHead()) {
        if (const auto position = playHead->getPosition()) {
            bpm = position->getBpm().orFallback(120.0);
            ppq = position->getPpqPosition().orFallback(freeBeat_);
            hostPlaying = position->getIsPlaying();
            if (haveHostPosition_) {
                loopWrapped = ppq < lastPpq_ - 0.25;
                const auto expected = lastPpq_ + lastBeatsPerSample_ * static_cast<double>(lastBlockSize_);
                seeked = !loopWrapped && std::abs(ppq - expected) > 0.125;
            }
            haveHostPosition_ = true;
        }
    }

    const auto beatsPerSample = bpm / (60.0 * sampleRate_);
    const auto sync = parameterRefs_.sync->load(std::memory_order_relaxed) >= 0.5F;
    const auto run = parameterRefs_.run->load(std::memory_order_relaxed) >= 0.5F;
    ProcessContext context;
    context.sampleCount = sampleCount;
    context.sampleRate = sampleRate_;
    context.beatAtStart = sync ? ppq : freeBeat_;
    context.beatsPerSample = beatsPerSample;
    context.playing = run && (sync ? hostPlaying : true);
    context.transportStarted = context.playing && !hostWasPlaying_;
    context.seeked = sync && seeked;
    context.loopWrapped = sync && loopWrapped;
    context.loopPolicy = static_cast<LoopPolicy>(loopPolicy_.load(std::memory_order_relaxed));

    engine_.process(context, generated);
    addEvents(generated, midi);
    freeBeat_ += beatsPerSample * static_cast<double>(sampleCount);
    hostWasPlaying_ = context.playing;
    lastPpq_ = ppq;
    lastBeatsPerSample_ = beatsPerSample;
    lastBlockSize_ = static_cast<int>(sampleCount);
    publishSnapshot();
}

void ThreeBSProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    audio.clear();
    midi.clear();
    MusicEngine::EventBuffer events;
    engine_.allNotesOff(0, events);
    addEvents(events, midi);
    hostWasPlaying_ = false;
}

void ThreeBSProcessor::publishSnapshot() noexcept {
    RenderSnapshot snapshot;
    snapshot.bodies = engine_.simulation().state().bodies;
    snapshot.escaped = engine_.simulation().state().escaped;
    snapshot.sequence = ++snapshotSequence_;
    snapshot.interpolationAlpha = engine_.simulation().interpolationAlpha();
    std::uint32_t escapeMask{};
    if (engineConfig_.simulation.escapePolicy == EscapePolicy::Prompt) {
        for (std::size_t body = 0; body < bodyCount; ++body) {
            if (snapshot.escaped[body])
                escapeMask |= (1U << body);
        }
    }
    escapePromptMask_.store(escapeMask, std::memory_order_relaxed);
    if (!snapshots_.push(snapshot)) {
        RenderSnapshot discarded;
        snapshots_.pop(discarded);
        snapshots_.push(snapshot);
    }
}

juce::AudioProcessorEditor* ThreeBSProcessor::createEditor() {
    return new ThreeBSEditor(*this);
}

void ThreeBSProcessor::setParameterValue(const juce::String& id, float plainValue) {
    if (auto* parameter = parameters_.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void ThreeBSProcessor::requestPreset(const ArtworkPreset& preset, int presetIndex) {
    EngineCommand command;
    command.type = CommandType::Replace;
    command.state = makeInitialState(preset.system, preset.seed, preset.chaos);
    command.config.simulation = preset.simulation;
    command.config.voices = preset.voices;
    command.loopPolicy = preset.loopPolicy;
    commands_.push(command);
    setParameterValue("preset", static_cast<float>(presetIndex));
    setParameterValue("gravity", static_cast<float>(preset.simulation.gravitationalConstant));
    setParameterValue("softening", static_cast<float>(preset.simulation.softening));
    setParameterValue("speed", static_cast<float>(preset.simulation.speed));
    float averageProbability{};
    for (std::size_t i = 0; i < bodyCount; ++i) {
        setParameterValue("mass" + juce::String(i + 1), static_cast<float>(command.state.bodies[i].mass));
        averageProbability += static_cast<float>(preset.voices[i].probability);
    }
    setParameterValue("density", averageProbability * 100.0F / static_cast<float>(bodyCount));
}

void ThreeBSProcessor::requestRandomize(double chaos) {
    EngineCommand command;
    command.type = CommandType::Replace;
    const auto seed = nextSeed_.fetch_add(1, std::memory_order_relaxed);
    command.state = makeInitialState(InitialSystem::ControlledChaos, seed, chaos);
    PresetCatalog catalog;
    const auto selected = static_cast<int>(parameterValue(parameters_, "preset"));
    if (catalog.valid() && selected >= 0 && static_cast<std::size_t>(selected) < catalog.size()) {
        command.config.simulation = catalog[static_cast<std::size_t>(selected)].simulation;
        command.config.voices = catalog[static_cast<std::size_t>(selected)].voices;
        command.loopPolicy = catalog[static_cast<std::size_t>(selected)].loopPolicy;
    }
    commands_.push(command);
}

void ThreeBSProcessor::requestReset() {
    EngineCommand command;
    command.type = CommandType::Reset;
    command.state = storedInitial_.load();
    command.config = engineConfig_;
    commands_.push(command);
}

void ThreeBSProcessor::requestExactState(const SimulationState& state) {
    EngineCommand command;
    command.type = CommandType::Replace;
    command.state = state;
    PresetCatalog catalog;
    const auto selected = static_cast<int>(parameterValue(parameters_, "preset"));
    if (catalog.valid() && selected >= 0 && static_cast<std::size_t>(selected) < catalog.size()) {
        command.config.simulation = catalog[static_cast<std::size_t>(selected)].simulation;
        command.config.voices = catalog[static_cast<std::size_t>(selected)].voices;
        command.loopPolicy = catalog[static_cast<std::size_t>(selected)].loopPolicy;
    }
    commands_.push(command);
    for (std::size_t body = 0; body < bodyCount; ++body)
        setParameterValue("mass" + juce::String(body + 1), static_cast<float>(state.bodies[body].mass));
}

void ThreeBSProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state = parameters_.copyState();
    state.setProperty("schemaVersion", static_cast<int>(stateSchemaVersion), nullptr);
    const auto initial = storedInitial_.load();
    state.setProperty("seed", juce::String(initial.seed), nullptr);
    state.setProperty("loopPolicy", loopPolicy_.load(std::memory_order_relaxed), nullptr);
    for (std::size_t i = 0; i < bodyCount; ++i) {
        const auto prefix = "body" + juce::String(i) + "_";
        state.setProperty(prefix + "mass", initial.bodies[i].mass, nullptr);
        state.setProperty(prefix + "px", initial.bodies[i].position.x, nullptr);
        state.setProperty(prefix + "py", initial.bodies[i].position.y, nullptr);
        state.setProperty(prefix + "pz", initial.bodies[i].position.z, nullptr);
        state.setProperty(prefix + "vx", initial.bodies[i].velocity.x, nullptr);
        state.setProperty(prefix + "vy", initial.bodies[i].velocity.y, nullptr);
        state.setProperty(prefix + "vz", initial.bodies[i].velocity.z, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}

void ThreeBSProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(parameters_.state.getType().toString()))
        return;
    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return;
    parameters_.replaceState(state);
    auto initial = storedInitial_.load();
    initial.seed = static_cast<std::uint64_t>(
        state.getProperty("seed", juce::String(initial.seed)).toString().getLargeIntValue());
    for (std::size_t i = 0; i < bodyCount; ++i) {
        const auto prefix = "body" + juce::String(i) + "_";
        initial.bodies[i].mass = static_cast<double>(state.getProperty(prefix + "mass", initial.bodies[i].mass));
        initial.bodies[i].position = {
            static_cast<double>(state.getProperty(prefix + "px", initial.bodies[i].position.x)),
            static_cast<double>(state.getProperty(prefix + "py", initial.bodies[i].position.y)),
            static_cast<double>(state.getProperty(prefix + "pz", initial.bodies[i].position.z))};
        initial.bodies[i].velocity = {
            static_cast<double>(state.getProperty(prefix + "vx", initial.bodies[i].velocity.x)),
            static_cast<double>(state.getProperty(prefix + "vy", initial.bodies[i].velocity.y)),
            static_cast<double>(state.getProperty(prefix + "vz", initial.bodies[i].velocity.z))};
    }
    EngineCommand command;
    command.type = CommandType::Replace;
    command.state = initial;
    PresetCatalog catalog;
    const auto selected = static_cast<int>(parameterValue(parameters_, "preset"));
    if (catalog.valid() && selected >= 0 && static_cast<std::size_t>(selected) < catalog.size()) {
        command.config.simulation = catalog[static_cast<std::size_t>(selected)].simulation;
        command.config.voices = catalog[static_cast<std::size_t>(selected)].voices;
        command.loopPolicy = catalog[static_cast<std::size_t>(selected)].loopPolicy;
    }
    command.loopPolicy = static_cast<LoopPolicy>(static_cast<int>(
        state.getProperty("loopPolicy", static_cast<int>(command.loopPolicy))));
    commands_.push(command);
}

} // namespace threebs

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new threebs::ThreeBSProcessor();
}
