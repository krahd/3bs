// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"

#include <algorithm>
#include <cmath>

namespace threebs {
namespace {

constexpr double pi = 3.14159265358979323846;

std::array<VoiceConfig, bodyCount> defaultVoices() noexcept {
    std::array<VoiceConfig, bodyCount> voices{};
    voices[0].channel = 1;
    voices[0].root = 0;
    voices[0].pitchMapping = PitchMapping::BarycentricRadius;
    voices[0].triggerMapping = TriggerMapping::Clock;
    voices[0].clockDivisionBeats = 0.25;

    voices[1].channel = 2;
    voices[1].root = 5;
    voices[1].pitchMapping = PitchMapping::NearestBodyDistance;
    voices[1].triggerMapping = TriggerMapping::PlaneCrossing;
    voices[1].minimumNote = 43;
    voices[1].maximumNote = 91;

    voices[2].channel = 3;
    voices[2].root = 7;
    voices[2].pitchMapping = PitchMapping::Speed;
    voices[2].triggerMapping = TriggerMapping::TurningPoint;
    voices[2].minimumNote = 31;
    voices[2].maximumNote = 79;
    return voices;
}

} // namespace

MusicEngine::MusicEngine()
    : MusicEngine(makeInitialState(InitialSystem::FigureEight, 0x334253ULL), [] {
          EngineConfig config;
          config.voices = defaultVoices();
          return config;
      }()) {}

MusicEngine::MusicEngine(SimulationState initial, EngineConfig config)
    : initial_(initial), simulation_(initial, config.simulation), config_(config),
      random_(initial.seed, 0x4D494449ULL) {
    bool anyVoiceConfigured{};
    for (const auto& voice : config_.voices)
        anyVoiceConfigured = anyVoiceConfigured || voice.channel != 1 || voice.root != 0;
    if (!anyVoiceConfigured)
        config_.voices = defaultVoices();
}

void MusicEngine::prepare(double sampleRate) noexcept {
    sampleRate_ = std::clamp(sampleRate, 8000.0, 768000.0);
}

void MusicEngine::setConfig(const EngineConfig& config) noexcept {
    config_ = config;
    simulation_.setConfig(config_.simulation);
    config_.chordStrumMilliseconds = std::clamp(config_.chordStrumMilliseconds, 0.0, 250.0);
    config_.chordStrumValue = std::clamp(config_.chordStrumValue, 0.0, 4.0);
    config_.minimumChordIntervalBeats = std::max(0.0, config_.minimumChordIntervalBeats);
    config_.autoResetBars = std::clamp(config_.autoResetBars, 1.0 / 16.0, 256.0);
    config_.chordSystem.root = static_cast<std::uint8_t>(config_.chordSystem.root % 12U);
    for (auto& voice : config_.voices) {
        voice.channel = std::clamp<std::uint8_t>(voice.channel, 1, 16);
        voice.root = static_cast<std::uint8_t>(voice.root % 12U);
        voice.octave = std::clamp<std::int8_t>(voice.octave, -4, 4);
        voice.minimumNote = std::min<std::uint8_t>(voice.minimumNote, 127);
        voice.maximumNote = std::min<std::uint8_t>(voice.maximumNote, 127);
        voice.probability = clamp01(voice.probability);
        voice.clockDivisionBeats = std::max(1.0 / 128.0, voice.clockDivisionBeats);
        voice.minimumDurationBeats = std::max(1.0 / 1024.0, voice.minimumDurationBeats);
        voice.maximumDurationBeats = std::max(voice.minimumDurationBeats, voice.maximumDurationBeats);
        voice.minimumTriggerIntervalBeats = std::max(0.0, voice.minimumTriggerIntervalBeats);
        voice.closeApproachDistance = std::max(0.0001, voice.closeApproachDistance);
    }
}

void MusicEngine::setBodyMass(std::size_t bodyIndex, double mass) noexcept {
    if (bodyIndex >= bodyCount)
        return;
    initial_.bodies[bodyIndex].mass = std::clamp(mass, 0.05, 1000.0);
    simulation_.setBodyMass(bodyIndex, mass);
}

void MusicEngine::reset(const SimulationState& initial, EventBuffer* noteOffs) noexcept {
    if (noteOffs != nullptr)
        allNotesOff(0, *noteOffs);
    initial_ = initial;
    simulation_.reset(initial);
    runtime_ = {};
    pendingNotes_ = {};
    random_.seedGenerator(initial.seed, 0x4D494449ULL);
    hasTimelineBeat_ = false;
    timelineBeat_ = 0.0;
    processedSamples_ = 0;
    lastChordTriggerBeat_ = -1.0e12;
    chordIndex_ = 0;
    hasAutoResetIndex_ = false;
}

std::array<BodyMeasurements, bodyCount> MusicEngine::measurements() const noexcept {
    std::array<BodyMeasurements, bodyCount> result{};
    const auto& bodies = simulation_.state().bodies;
    const auto center = simulation_.barycenter();
    const auto centerVelocity = simulation_.barycentricVelocity();
    const auto radiusScale = std::max(1.0, simulation_.config().escapeRadius);
    for (std::size_t i = 0; i < bodyCount; ++i) {
        const auto relativePosition = bodies[i].position - center;
        const auto relativeVelocity = bodies[i].velocity - centerVelocity;
        auto nearest = radiusScale;
        std::size_t nearestIndex = i == 0U ? 1U : 0U;
        for (std::size_t j = 0; j < bodyCount; ++j) {
            if (i != j) {
                const auto distance = length(bodies[i].position - bodies[j].position);
                if (distance < nearest) {
                    nearest = distance;
                    nearestIndex = j;
                }
            }
        }
        const auto radialDirection = normalized(relativePosition);
        const auto relativeBodyVelocity = bodies[i].velocity - bodies[nearestIndex].velocity;
        Vec3 acceleration{};
        for (std::size_t j = 0; j < bodyCount; ++j) {
            if (i == j)
                continue;
            const auto displacement = bodies[j].position - bodies[i].position;
            const auto softened = dot(displacement, displacement)
                + simulation_.config().softening * simulation_.config().softening;
            acceleration += displacement * (simulation_.config().gravitationalConstant
                * bodies[j].mass / std::pow(softened, 1.5));
        }
        result[i] = {
            clamp01(length(relativePosition) / radiusScale),
            clamp01(nearest / radiusScale),
            clamp01(0.5 + relativePosition.z / (2.0 * radiusScale)),
            clamp01(length(relativeVelocity) / 4.0),
            clamp01((std::atan2(relativePosition.y, relativePosition.x) + pi) / (2.0 * pi)),
            dot(relativeVelocity, radialDirection),
            clamp01(length(relativeBodyVelocity) / 6.0),
            clamp01(length(acceleration) / 4.0),
            clamp01(length(cross(relativePosition, relativeVelocity)) / 6.0),
        };
    }
    return result;
}

double MusicEngine::mappingValue(std::size_t bodyIndex, PitchMapping mapping,
                                 const std::array<BodyMeasurements, bodyCount>& values) const noexcept {
    const auto& value = values[bodyIndex];
    switch (mapping) {
    case PitchMapping::BarycentricRadius: return value.barycentricRadius;
    case PitchMapping::NearestBodyDistance: return value.nearestBodyDistance;
    case PitchMapping::SignedPlaneDistance: return value.signedPlaneDistance;
    case PitchMapping::Speed: return value.speed;
    case PitchMapping::OrbitalPhase: return value.orbitalPhase;
    case PitchMapping::RadialVelocity: return clamp01(0.5 + value.radialVelocity / 6.0);
    case PitchMapping::RelativeSpeed: return value.relativeSpeed;
    case PitchMapping::Acceleration: return value.acceleration;
    case PitchMapping::AngularMomentum: return value.angularMomentum;
    }
    return 0.0;
}

bool MusicEngine::shouldTrigger(std::size_t bodyIndex, const VoiceConfig& voice,
                                const BodyMeasurements& current, double absoluteBeat) noexcept {
    auto& runtime = runtime_[bodyIndex];
    if (absoluteBeat - runtime.lastTriggerBeat < voice.minimumTriggerIntervalBeats)
        return false;

    bool trigger{};
    switch (voice.triggerMapping) {
    case TriggerMapping::Clock: {
        const auto index = std::floor((absoluteBeat + 1.0e-9) / voice.clockDivisionBeats);
        trigger = index > runtime.lastClockIndex;
        runtime.lastClockIndex = std::max(runtime.lastClockIndex, index);
        break;
    }
    case TriggerMapping::PlaneCrossing:
        trigger = (runtime.previous.signedPlaneDistance < 0.5 && current.signedPlaneDistance >= 0.5)
                  || (runtime.previous.signedPlaneDistance > 0.5 && current.signedPlaneDistance <= 0.5);
        break;
    case TriggerMapping::CloseApproach: {
        const auto threshold = clamp01(voice.closeApproachDistance / simulation_.config().escapeRadius);
        trigger = runtime.previous.nearestBodyDistance > threshold
                  && current.nearestBodyDistance <= threshold;
        break;
    }
    case TriggerMapping::TurningPoint:
        trigger = runtime.previous.radialVelocity > 0.0 && current.radialVelocity <= 0.0;
        break;
    case TriggerMapping::Apsis:
        trigger = (runtime.previous.radialVelocity < 0.0 && current.radialVelocity >= 0.0)
            || (runtime.previous.radialVelocity > 0.0 && current.radialVelocity <= 0.0);
        break;
    case TriggerMapping::RecedingThreshold: {
        const auto threshold = clamp01(voice.closeApproachDistance / simulation_.config().escapeRadius);
        trigger = runtime.previous.nearestBodyDistance < threshold
            && current.nearestBodyDistance >= threshold;
        break;
    }
    case TriggerMapping::SpeedPeak: {
        const auto delta = current.speed - runtime.previous.speed;
        trigger = runtime.previousSpeedDelta > 0.0 && delta <= 0.0;
        runtime.previousSpeedDelta = delta;
        break;
    }
    case TriggerMapping::PhaseStep: {
        const auto step = std::floor(current.orbitalPhase * 16.0);
        trigger = step != runtime.lastPhaseStep;
        runtime.lastPhaseStep = step;
        break;
    }
    }
    runtime.previous = current;
    if (!trigger || random_.nextUnit() > voice.probability)
        return false;
    runtime.lastTriggerBeat = absoluteBeat;
    return true;
}

double MusicEngine::durationBeatsFor(
    std::size_t bodyIndex, const std::array<BodyMeasurements, bodyCount>& values) const noexcept {
    const auto& voice = config_.voices[bodyIndex];
    const auto t = clamp01(mappingValue(bodyIndex, voice.durationMapping, values));
    const auto duration = voice.minimumDurationBeats
        + t * (voice.maximumDurationBeats - voice.minimumDurationBeats);
    return std::max(1.0 / 1024.0, duration);
}

void MusicEngine::triggerVoice(std::size_t bodyIndex, std::uint32_t sampleOffset, double beat,
                               const BodyMeasurements& measurement, EventBuffer& output) noexcept {
    const auto& voice = config_.voices[bodyIndex];
    const auto values = measurements();
    auto note = quantizeNormalizedPitch(mappingValue(bodyIndex, voice.pitchMapping, values), voice);
    note = static_cast<std::uint8_t>(std::clamp<int>(
        static_cast<int>(note) + config_.inputTranspose + voice.octave * 12, 0, 127));
    const auto velocityRange = static_cast<double>(voice.maximumVelocity - voice.minimumVelocity);
    const auto velocity = static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(std::lround(voice.minimumVelocity + measurement.speed * velocityRange)), 1, 127));

    startNote(bodyIndex, note, velocity, sampleOffset, beat, durationBeatsFor(bodyIndex, values), output);
}

void MusicEngine::startNote(std::size_t bodyIndex, std::uint8_t note, std::uint8_t velocity,
                            std::uint32_t sampleOffset, double beat, double durationBeats,
                            EventBuffer& output) noexcept {
    const auto& voice = config_.voices[bodyIndex];
    auto& runtime = runtime_[bodyIndex];
    if (runtime.noteActive)
        output.push({sampleOffset, MidiEventType::NoteOff, voice.channel, runtime.activeNote, 0,
                     static_cast<std::uint8_t>(bodyIndex)});
    output.push({sampleOffset, MidiEventType::NoteOn, voice.channel, note, velocity,
                 static_cast<std::uint8_t>(bodyIndex)});
    runtime.noteActive = true;
    runtime.activeNote = note;
    runtime.noteOffBeat = beat + durationBeats;
}

void MusicEngine::triggerChord(std::size_t triggerBody, std::uint32_t sampleOffset, double beat,
                               const std::array<BodyMeasurements, bodyCount>& values,
                               EventBuffer& output) noexcept {
    if (beat - lastChordTriggerBeat_ < config_.minimumChordIntervalBeats)
        return;
    lastChordTriggerBeat_ = beat;
    const auto base = mappingValue(triggerBody, config_.voices[triggerBody].pitchMapping, values);
    std::uint64_t strumSamples{};
    switch (config_.chordStrumUnit) {
    case StrumUnit::Milliseconds:
        strumSamples = static_cast<std::uint64_t>(std::llround(
            config_.chordStrumMilliseconds * sampleRate_ / 1000.0));
        break;
    case StrumUnit::Beats:
        strumSamples = static_cast<std::uint64_t>(std::llround(
            config_.chordStrumValue / std::max(1.0e-12, currentBeatsPerSample_)));
        break;
    case StrumUnit::BarFraction:
        strumSamples = static_cast<std::uint64_t>(std::llround(
            config_.chordStrumValue * currentBeatsPerBar_ / std::max(1.0e-12, currentBeatsPerSample_)));
        break;
    }
    const auto rotation = static_cast<std::size_t>(chordIndex_++ % bodyCount);
    for (std::size_t order = 0; order < bodyCount; ++order) {
        const auto body = config_.voicingMode == VoicingMode::Strum
            ? (order + rotation) % bodyCount : order;
        auto voice = config_.voices[body];
        if (!voice.enabled)
            continue;
        // Chord and Strum modes harmonize against one global tonal frame.
        voice.root = config_.chordSystem.root;
        voice.scale = config_.chordSystem.scale;
        voice.customScale = config_.chordSystem.customScale;
        auto note = quantizeScaleDegree(base, static_cast<int>(body * 2U), voice);
        note = static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(note)
            + config_.inputTranspose + voice.octave * 12, 0, 127));
        const auto velocityRange = static_cast<double>(voice.maximumVelocity - voice.minimumVelocity);
        const auto velocity = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(voice.minimumVelocity
                + values[body].speed * velocityRange)), 1, 127));
        const auto duration = durationBeatsFor(body, values);
        const auto delay = config_.voicingMode == VoicingMode::Strum ? order * strumSamples : 0U;
        if (delay == 0U) {
            startNote(body, note, velocity, sampleOffset, beat, duration, output);
        } else {
            pendingNotes_[body] = {true, processedSamples_ + delay, body, note, velocity, duration};
        }
    }
}

void MusicEngine::emitContinuousControllers(
    std::size_t bodyIndex, std::uint32_t sampleOffset,
    const std::array<BodyMeasurements, bodyCount>& values, EventBuffer& output) noexcept {
    const auto& voice = config_.voices[bodyIndex];
    auto& runtime = runtime_[bodyIndex];
    const auto minimumSampleInterval = static_cast<std::uint64_t>(std::max(1.0, sampleRate_ / 100.0));
    for (std::size_t lane = 0; lane < voice.ccLanes.size(); ++lane) {
        const auto& cc = voice.ccLanes[lane];
        if (!cc.enabled || processedSamples_ - runtime.lastCcSample[lane] < minimumSampleInterval)
            continue;
        const auto target = mappingValue(bodyIndex, cc.source, values);
        const auto smoothing = clamp01(cc.smoothing);
        runtime.smoothedCc[lane] = runtime.smoothedCc[lane] * smoothing + target * (1.0 - smoothing);
        const auto value = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(runtime.smoothedCc[lane] * 127.0)), 0, 127));
        if (runtime.lastCc[lane] != value) {
            output.push({sampleOffset, MidiEventType::ControlChange, voice.channel,
                         static_cast<std::uint8_t>(std::clamp<int>(cc.controller, 0, 119)), value,
                         static_cast<std::uint8_t>(bodyIndex)});
            runtime.lastCc[lane] = value;
        }
        runtime.lastCcSample[lane] = processedSamples_;
    }
}

void MusicEngine::applyTransportReset(std::uint32_t sampleOffset, EventBuffer& output) noexcept {
    allNotesOff(sampleOffset, output);
    simulation_.reset(initial_);
    runtime_ = {};
    pendingNotes_ = {};
    random_.seedGenerator(initial_.seed, 0x4D494449ULL);
    lastChordTriggerBeat_ = -1.0e12;
    chordIndex_ = 0;
}

void MusicEngine::process(const ProcessContext& context, EventBuffer& output) noexcept {
    output.clear();
    prepare(context.sampleRate);
    currentBeatsPerSample_ = context.beatsPerSample;
    const auto numerator = std::max(1, context.timeSigNumerator);
    const auto denominator = std::max(1, context.timeSigDenominator);
    currentBeatsPerBar_ = static_cast<double>(numerator) * 4.0 / static_cast<double>(denominator);
    if ((!context.playing && wasPlaying_)
        || context.transportStarted || context.seeked
        || (context.loopWrapped && context.loopPolicy == LoopPolicy::Restart)) {
        applyTransportReset(0, output);
        timelineBeat_ = context.beatAtStart;
        hasTimelineBeat_ = true;
        hasAutoResetIndex_ = false;
    }

    if (!context.playing) {
        wasPlaying_ = false;
        processedSamples_ += context.sampleCount;
        return;
    }

    if (!hasTimelineBeat_) {
        timelineBeat_ = context.beatAtStart;
        hasTimelineBeat_ = true;
    }

    for (std::uint32_t sample = 0; sample < context.sampleCount; ++sample) {
        const auto beat = timelineBeat_;
        const auto resetPeriodBeats = config_.autoResetBars * currentBeatsPerBar_;
        const auto resetIndex = std::floor(beat / std::max(1.0e-9, resetPeriodBeats));
        if (!hasAutoResetIndex_) {
            lastAutoResetIndex_ = resetIndex;
            hasAutoResetIndex_ = true;
        } else if (config_.autoResetEnabled && resetIndex != lastAutoResetIndex_) {
            applyTransportReset(sample, output);
            lastAutoResetIndex_ = resetIndex;
        }
        for (auto& pending : pendingNotes_) {
            if (pending.active && pending.dueSample <= processedSamples_) {
                startNote(pending.body, pending.note, pending.velocity, sample, beat,
                          pending.durationBeats, output);
                pending.active = false;
            }
        }
        simulation_.advance(context.beatsPerSample);
        const auto values = measurements();
        std::size_t chordTriggerBody = bodyCount;
        for (std::size_t body = 0; body < bodyCount; ++body) {
            const auto& voice = config_.voices[body];
            auto& runtime = runtime_[body];
            if (runtime.noteActive && beat >= runtime.noteOffBeat) {
                output.push({sample, MidiEventType::NoteOff, voice.channel, runtime.activeNote, 0,
                             static_cast<std::uint8_t>(body)});
                runtime.noteActive = false;
            }
            if (voice.enabled && (!config_.inputGateEnabled || config_.inputGateOpen)
                && shouldTrigger(body, voice, values[body], beat)) {
                if (config_.voicingMode == VoicingMode::Independent)
                    triggerVoice(body, sample, beat, values[body], output);
                else if (chordTriggerBody == bodyCount)
                    chordTriggerBody = body;
            }
            emitContinuousControllers(body, sample, values, output);
        }
        if (chordTriggerBody < bodyCount)
            triggerChord(chordTriggerBody, sample, beat, values, output);
        timelineBeat_ += context.beatsPerSample;
        ++processedSamples_;
    }
    wasPlaying_ = true;
}

void MusicEngine::allNotesOff(std::uint32_t sampleOffset, EventBuffer& output) noexcept {
    pendingNotes_ = {};
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& runtime = runtime_[body];
        if (runtime.noteActive) {
            output.push({sampleOffset, MidiEventType::NoteOff, config_.voices[body].channel,
                         runtime.activeNote, 0, static_cast<std::uint8_t>(body)});
            runtime.noteActive = false;
        }
    }
}

} // namespace threebs
