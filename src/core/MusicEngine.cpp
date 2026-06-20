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
    for (auto& voice : config_.voices) {
        voice.channel = std::clamp<std::uint8_t>(voice.channel, 1, 16);
        voice.minimumNote = std::min<std::uint8_t>(voice.minimumNote, 127);
        voice.maximumNote = std::min<std::uint8_t>(voice.maximumNote, 127);
        voice.probability = clamp01(voice.probability);
        voice.clockDivisionBeats = std::max(1.0 / 128.0, voice.clockDivisionBeats);
        voice.durationBeats = std::max(1.0 / 1024.0, voice.durationBeats);
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
    random_.seedGenerator(initial.seed, 0x4D494449ULL);
    hasTimelineBeat_ = false;
    timelineBeat_ = 0.0;
    processedSamples_ = 0;
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
        for (std::size_t j = 0; j < bodyCount; ++j) {
            if (i != j)
                nearest = std::min(nearest, length(bodies[i].position - bodies[j].position));
        }
        const auto radialDirection = normalized(relativePosition);
        result[i] = {
            clamp01(length(relativePosition) / radiusScale),
            clamp01(nearest / radiusScale),
            clamp01(0.5 + relativePosition.z / (2.0 * radiusScale)),
            clamp01(length(relativeVelocity) / 4.0),
            clamp01((std::atan2(relativePosition.y, relativePosition.x) + pi) / (2.0 * pi)),
            dot(relativeVelocity, radialDirection),
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
    }
    runtime.previous = current;
    if (!trigger || random_.nextUnit() > voice.probability)
        return false;
    runtime.lastTriggerBeat = absoluteBeat;
    return true;
}

void MusicEngine::triggerVoice(std::size_t bodyIndex, std::uint32_t sampleOffset, double beat,
                               const BodyMeasurements& measurement, EventBuffer& output) noexcept {
    const auto& voice = config_.voices[bodyIndex];
    auto& runtime = runtime_[bodyIndex];
    const auto values = measurements();
    auto note = quantizeNormalizedPitch(mappingValue(bodyIndex, voice.pitchMapping, values), voice);
    note = static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(note) + config_.inputTranspose, 0, 127));
    const auto velocityRange = static_cast<double>(voice.maximumVelocity - voice.minimumVelocity);
    const auto velocity = static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(std::lround(voice.minimumVelocity + measurement.speed * velocityRange)), 1, 127));

    if (runtime.noteActive)
        output.push({sampleOffset, MidiEventType::NoteOff, voice.channel, runtime.activeNote, 0,
                     static_cast<std::uint8_t>(bodyIndex)});
    output.push({sampleOffset, MidiEventType::NoteOn, voice.channel, note, velocity,
                 static_cast<std::uint8_t>(bodyIndex)});
    runtime.noteActive = true;
    runtime.activeNote = note;
    runtime.noteOffBeat = beat + voice.durationBeats;
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
    random_.seedGenerator(initial_.seed, 0x4D494449ULL);
}

void MusicEngine::process(const ProcessContext& context, EventBuffer& output) noexcept {
    output.clear();
    prepare(context.sampleRate);
    if ((!context.playing && wasPlaying_)
        || context.transportStarted || context.seeked
        || (context.loopWrapped && context.loopPolicy == LoopPolicy::Restart)) {
        applyTransportReset(0, output);
        timelineBeat_ = context.beatAtStart;
        hasTimelineBeat_ = true;
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
        simulation_.advance(context.beatsPerSample);
        const auto values = measurements();
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
                triggerVoice(body, sample, beat, values[body], output);
            }
            emitContinuousControllers(body, sample, values, output);
        }
        timelineBeat_ += context.beatsPerSample;
        ++processedSamples_;
    }
    wasPlaying_ = true;
}

void MusicEngine::allNotesOff(std::uint32_t sampleOffset, EventBuffer& output) noexcept {
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
