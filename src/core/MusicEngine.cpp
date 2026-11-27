// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "core/MusicEngine.h"

#include <algorithm>
#include <cmath>

namespace threebs {
namespace {

constexpr double pi = 3.14159265358979323846;

bool inRange(double value, double minimum, double maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

bool validScale(ScaleId scale, const std::array<bool, 12>& custom) noexcept {
    return static_cast<unsigned>(scale) <= static_cast<unsigned>(ScaleId::Custom)
        && (scale != ScaleId::Custom || std::any_of(custom.begin(), custom.end(), [](bool note) { return note; }));
}

std::size_t nearestStraightDurationIndex(double beats) noexcept {
    std::size_t nearest{};
    auto distance = std::abs(beats - straightDurationBeats.front());
    for (std::size_t index = 1; index < straightDurationBeats.size(); ++index) {
        const auto candidate = std::abs(beats - straightDurationBeats[index]);
        if (candidate < distance) {
            nearest = index;
            distance = candidate;
        }
    }
    return nearest;
}

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

EngineConfig defaultConfig() noexcept {
    EngineConfig config;
    config.voices = defaultVoices();
    return config;
}

} // namespace

bool isValidEngineConfig(const EngineConfig& config) noexcept {
    if (!isValidSimulationConfig(config.simulation)
        || static_cast<unsigned>(config.voicingMode) > static_cast<unsigned>(VoicingMode::Strum)
        || static_cast<unsigned>(config.chordStrumUnit) > static_cast<unsigned>(StrumUnit::BarFraction)
        || !inRange(config.chordStrumMilliseconds, 0.0, 250.0)
        || !inRange(config.chordStrumValue, 0.0, 4.0)
        || !inRange(config.minimumChordIntervalBeats, 0.0, 64.0)
        || !inRange(config.autoResetBars, 1.0 / 16.0, 256.0)
        || config.inputTranspose < -127 || config.chordSystem.root > 11
        || !validScale(config.chordSystem.scale, config.chordSystem.customScale))
        return false;
    for (const auto& voice : config.voices) {
        if (voice.channel < 1 || voice.channel > 16 || voice.root > 11
            || voice.octave < -4 || voice.octave > 4
            || voice.maximumNote > 127 || voice.minimumNote > voice.maximumNote
            || !validScale(voice.scale, voice.customScale)
            || static_cast<unsigned>(voice.pitchMapping) > static_cast<unsigned>(PitchMapping::AngularMomentum)
            || static_cast<unsigned>(voice.durationMapping) > static_cast<unsigned>(PitchMapping::AngularMomentum)
            || static_cast<unsigned>(voice.triggerMapping) > static_cast<unsigned>(TriggerMapping::PhaseStep)
            || static_cast<unsigned>(voice.durationGrid) > static_cast<unsigned>(DurationGrid::Straight)
            || !inRange(voice.probability, 0.0, 1.0)
            || !inRange(voice.clockDivisionBeats, 1.0 / 128.0, 16.0)
            || !inRange(voice.minimumDurationBeats, 1.0 / 1024.0, 64.0)
            || !inRange(voice.maximumDurationBeats, voice.minimumDurationBeats, 64.0)
            || !inRange(voice.minimumTriggerIntervalBeats, 0.0, 64.0)
            || !inRange(voice.closeApproachDistance, 0.0001, 1000.0)
            || voice.minimumVelocity < 1 || voice.maximumVelocity > 127
            || voice.minimumVelocity > voice.maximumVelocity)
            return false;
        for (const auto& lane : voice.ccLanes)
            if (lane.controller > 119
                || static_cast<unsigned>(lane.source) > static_cast<unsigned>(PitchMapping::AngularMomentum)
                || !inRange(lane.smoothing, 0.0, 1.0))
                return false;
    }
    return true;
}

double mappedDurationBeats(double normalizedValue, double minimumBeats, double maximumBeats,
                           DurationGrid grid) noexcept {
    const auto minimum = std::isfinite(minimumBeats)
        ? std::clamp(minimumBeats, 1.0 / 1024.0, 64.0) : 0.125;
    const auto maximum = std::isfinite(maximumBeats)
        ? std::clamp(maximumBeats, minimum, 64.0) : minimum;
    const auto t = clamp01(normalizedValue);
    if (grid == DurationGrid::ContinuousLegacy)
        return minimum + t * (maximum - minimum);

    const auto lower = nearestStraightDurationIndex(minimum);
    const auto upper = std::max(lower, nearestStraightDurationIndex(maximum));
    const auto span = static_cast<double>(upper - lower);
    const auto offset = static_cast<std::size_t>(std::llround(t * span));
    return straightDurationBeats[lower + std::min(offset, upper - lower)];
}

MusicEngine::MusicEngine()
    : MusicEngine(makeInitialState(InitialSystem::FigureEight, 0x334253ULL), defaultConfig()) {}

MusicEngine::MusicEngine(SimulationState initial)
    : MusicEngine(initial, defaultConfig()) {}

MusicEngine::MusicEngine(SimulationState initial, EngineConfig config)
    : initial_(isValidSimulationState(initial) ? initial
          : makeInitialState(InitialSystem::FigureEight, 0x334253ULL)),
      simulation_(initial_, isValidEngineConfig(config) ? config.simulation : SimulationConfig{}),
      config_(isValidEngineConfig(config) ? config : defaultConfig()),
      random_(initial_.seed, 0x4D494449ULL) {}

void MusicEngine::prepare(double sampleRate) noexcept {
    if (inRange(sampleRate, 8000.0, 768000.0))
        sampleRate_ = sampleRate;
}

bool MusicEngine::setConfig(const EngineConfig& config) noexcept {
    if (!isValidEngineConfig(config))
        return false;
    const auto globalChange = config.voicingMode != config_.voicingMode
        || config.chordSystem.root != config_.chordSystem.root
        || config.chordSystem.scale != config_.chordSystem.scale
        || config.chordSystem.customScale != config_.chordSystem.customScale
        || config.chordStrumUnit != config_.chordStrumUnit
        || config.chordStrumValue != config_.chordStrumValue
        || config.chordStrumMilliseconds != config_.chordStrumMilliseconds
        || config.inputTranspose != config_.inputTranspose
        || (config.inputGateEnabled && !config.inputGateOpen);
    for (std::size_t body = 0; body < bodyCount; ++body) {
        if (config.voices[body].triggerMapping != config_.voices[body].triggerMapping) {
            runtime_[body].hasSpeedMeasurement = false;
            runtime_[body].previousSpeedDelta = 0.0;
        }
        if (globalChange || config.voices[body] != config_.voices[body]) {
            pendingNotes_[body].active = false;
            runtime_[body].cleanupRequested = runtime_[body].noteActive;
        }
    }
    config_ = config;
    simulation_.setConfig(config_.simulation);
    return true;
}

void MusicEngine::setBodyMass(std::size_t bodyIndex, double mass) noexcept {
    if (bodyIndex >= bodyCount || !inRange(mass, 0.05, 1000.0))
        return;
    initial_.bodies[bodyIndex].mass = std::clamp(mass, 0.05, 1000.0);
    simulation_.setBodyMass(bodyIndex, mass);
}

bool MusicEngine::reset(const SimulationState& initial, EventBuffer* noteOffs) noexcept {
    return reset(initial, noteOffs != nullptr ? noteOffs->sink() : EventSink{});
}

bool MusicEngine::reset(const SimulationState& initial, EventSink noteOffs) noexcept {
    if (!isValidSimulationState(initial))
        return false;
    if (noteOffs.write != nullptr)
        allNotesOff(0, noteOffs);
    const auto sounding = runtime_;
    initial_ = initial;
    simulation_.reset(initial);
    runtime_ = {};
    // A reset without an output route must retain cleanup, not forget notes
    // already sent. The next processing/cleanup call flushes them first.
    for (std::size_t body = 0; body < bodyCount; ++body) {
        if (sounding[body].noteActive) {
            runtime_[body].noteActive = true;
            runtime_[body].activeNote = sounding[body].activeNote;
            runtime_[body].activeChannel = sounding[body].activeChannel;
            runtime_[body].cleanupRequested = true;
        }
    }
    pendingNotes_ = {};
    random_.seedGenerator(initial.seed, 0x4D494449ULL);
    hasTimelineBeat_ = false;
    timelineBeat_ = 0.0;
    processedSamples_ = 0;
    processedSeconds_ = 0.0;
    lastChordTriggerBeat_ = -1.0e12;
    chordIndex_ = 0;
    hasAutoResetIndex_ = false;
    return true;
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
    const auto refractory = absoluteBeat - runtime.lastTriggerBeat < voice.minimumTriggerIntervalBeats;
    if (refractory && voice.triggerMapping != TriggerMapping::SpeedPeak)
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
        if (!runtime.hasSpeedMeasurement) {
            runtime.hasSpeedMeasurement = true;
            break;
        }
        const auto delta = current.speed - runtime.previous.speed;
        trigger = runtime.previousSpeedDelta > 0.0 && delta < 0.0;
        // Audio samples between fixed physics steps have equal measurements;
        // retain the slope across that plateau rather than inventing a peak.
        if (delta != 0.0)
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
    if (refractory || !trigger || random_.nextUnit() >= voice.probability)
        return false;
    runtime.lastTriggerBeat = absoluteBeat;
    return true;
}

double MusicEngine::durationBeatsFor(
    std::size_t bodyIndex, const std::array<BodyMeasurements, bodyCount>& values) const noexcept {
    const auto& voice = config_.voices[bodyIndex];
    const auto t = clamp01(mappingValue(bodyIndex, voice.durationMapping, values));
    return mappedDurationBeats(t, voice.minimumDurationBeats, voice.maximumDurationBeats,
                               voice.durationGrid);
}

void MusicEngine::triggerVoice(std::size_t bodyIndex, std::uint32_t sampleOffset, double beat,
                               const BodyMeasurements& measurement, EventSink output) noexcept {
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
                            EventSink output) noexcept {
    if (!canStartNote(bodyIndex))
        return;
    const auto& voice = config_.voices[bodyIndex];
    auto& runtime = runtime_[bodyIndex];
    if (runtime.noteActive)
        output.push({sampleOffset, MidiEventType::NoteOff, runtime.activeChannel, runtime.activeNote, 0,
                     static_cast<std::uint8_t>(bodyIndex)});
    output.push({sampleOffset, MidiEventType::NoteOn, voice.channel, note, velocity,
                 static_cast<std::uint8_t>(bodyIndex)});
    runtime.noteActive = true;
    runtime.activeNote = note;
    runtime.activeChannel = voice.channel;
    runtime.cleanupRequested = false;
    runtime.noteOffBeat = beat + durationBeats;
}

void MusicEngine::triggerChord(std::size_t triggerBody, std::uint32_t sampleOffset, double beat,
                               const std::array<BodyMeasurements, bodyCount>& values,
                               EventSink output) noexcept {
    if (beat - lastChordTriggerBeat_ < config_.minimumChordIntervalBeats)
        return;
    lastChordTriggerBeat_ = beat;
    const auto base = mappingValue(triggerBody, config_.voices[triggerBody].pitchMapping, values);
    const bool beatRelative = config_.chordStrumUnit != StrumUnit::Milliseconds;
    const auto strumSamples = static_cast<std::uint64_t>(std::llround(
        config_.chordStrumMilliseconds * sampleRate_ / 1000.0));
    const auto strumBeats = config_.chordStrumValue
        * (config_.chordStrumUnit == StrumUnit::BarFraction ? currentBeatsPerBar_ : 1.0);
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
        const auto delayed = config_.voicingMode == VoicingMode::Strum && order != 0U
            && (beatRelative ? strumBeats > 0.0 : strumSamples != 0U);
        if (!delayed) {
            startNote(body, note, velocity, sampleOffset, beat, duration, output);
        } else {
            // Musical units follow subsequent tempo changes; milliseconds use
            // elapsed seconds so a sample-rate change cannot stretch the strum.
            pendingNotes_[body] = {true, beatRelative, beat + static_cast<double>(order) * strumBeats,
                processedSeconds_ + static_cast<double>(order * strumSamples) / sampleRate_,
                body, note, velocity, duration};
        }
    }
}

void MusicEngine::emitContinuousControllers(
    std::size_t bodyIndex, std::uint32_t sampleOffset,
    const std::array<BodyMeasurements, bodyCount>& values, EventSink output) noexcept {
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

void MusicEngine::applyTransportReset(std::uint32_t sampleOffset, EventSink output) noexcept {
    allNotesOff(sampleOffset, output);
    simulation_.reset(initial_);
    runtime_ = {};
    pendingNotes_ = {};
    random_.seedGenerator(initial_.seed, 0x4D494449ULL);
    lastChordTriggerBeat_ = -1.0e12;
    chordIndex_ = 0;
}

EventDelivery MusicEngine::process(const ProcessContext& context, EventBuffer& output) noexcept {
    output.clear();
    process(context, output.sink());
    return {output.size(), output.rejected()};
}

bool MusicEngine::canStartNote(std::size_t body) const noexcept {
    return body < bodyCount && config_.voices[body].enabled
        && (!config_.inputGateEnabled || config_.inputGateOpen);
}

void MusicEngine::flushRequestedCleanup(std::uint32_t sampleOffset, EventSink output) noexcept {
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& voice = runtime_[body];
        if (!voice.cleanupRequested)
            continue;
        if (voice.noteActive)
            output.push({sampleOffset, MidiEventType::NoteOff, voice.activeChannel, voice.activeNote, 0,
                         static_cast<std::uint8_t>(body)});
        voice.noteActive = false;
        voice.cleanupRequested = false;
    }
}

void MusicEngine::process(const ProcessContext& context, EventSink output) noexcept {
    if (output.write == nullptr)
        return;
    if (!inRange(context.sampleRate, 8000.0, 768000.0)
        || !std::isfinite(context.beatAtStart) || std::abs(context.beatAtStart) > 1.0e12
        || !inRange(context.beatsPerSample, 0.0, 1.0)
        || context.timeSigNumerator < 1 || context.timeSigNumerator > 64
        || context.timeSigDenominator < 1 || context.timeSigDenominator > 64
        || static_cast<unsigned>(context.loopPolicy) > static_cast<unsigned>(LoopPolicy::Continue)) {
        allNotesOff(0, output);
        wasPlaying_ = false;
        hasTimelineBeat_ = false;
        return;
    }
    flushRequestedCleanup(0, output);
    prepare(context.sampleRate);
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
        processedSeconds_ += static_cast<double>(context.sampleCount) / sampleRate_;
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
            const auto due = pending.beatRelative ? beat + 1.0e-10 >= pending.dueBeat
                : processedSeconds_ + 1.0e-10 >= pending.dueSeconds;
            if (pending.active && due) {
                if (config_.voicingMode == VoicingMode::Strum && canStartNote(pending.body))
                    startNote(pending.body, pending.note, pending.velocity, sample, beat,
                              pending.durationBeats, output);
                pending.active = false;
            }
        }
        simulation_.advance(context.beatsPerSample);
        if (simulation_.faulted()) {
            allNotesOff(sample, output);
            wasPlaying_ = false;
            return;
        }
        const auto values = measurements();
        std::size_t chordTriggerBody = bodyCount;
        for (std::size_t body = 0; body < bodyCount; ++body) {
            const auto& voice = config_.voices[body];
            auto& runtime = runtime_[body];
            if (runtime.noteActive && beat >= runtime.noteOffBeat) {
                output.push({sample, MidiEventType::NoteOff, runtime.activeChannel, runtime.activeNote, 0,
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
        processedSeconds_ += 1.0 / sampleRate_;
    }
    wasPlaying_ = true;
}

void MusicEngine::allNotesOff(std::uint32_t sampleOffset, EventBuffer& output) noexcept {
    allNotesOff(sampleOffset, output.sink());
}

void MusicEngine::allNotesOff(std::uint32_t sampleOffset, EventSink output) noexcept {
    if (output.write == nullptr)
        return;
    pendingNotes_ = {};
    for (std::size_t body = 0; body < bodyCount; ++body) {
        auto& runtime = runtime_[body];
        if (runtime.noteActive) {
            output.push({sampleOffset, MidiEventType::NoteOff, runtime.activeChannel,
                         runtime.activeNote, 0, static_cast<std::uint8_t>(body)});
            runtime.noteActive = false;
        }
        runtime.cleanupRequested = false;
    }
}

} // namespace threebs
