// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/UserConfiguration.h"

#include <algorithm>
#include <cmath>

namespace threebs {
namespace {

juce::var makeObject() { return juce::var(new juce::DynamicObject()); }

void set(juce::var& target, const char* name, juce::var value) {
    target.getDynamicObject()->setProperty(name, std::move(value));
}

juce::var get(const juce::var& target, const char* name) {
    if (auto* object = target.getDynamicObject())
        return object->getProperty(name);
    return {};
}

double number(const juce::var& target, const char* name, double fallback,
              double minimum, double maximum) {
    const auto property = get(target, name);
    if (property.isVoid())
        return fallback;
    const auto value = static_cast<double>(property);
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

int integer(const juce::var& target, const char* name, int fallback, int minimum, int maximum) {
    const auto property = get(target, name);
    return property.isVoid() ? fallback
                             : std::clamp(static_cast<int>(property), minimum, maximum);
}

bool boolean(const juce::var& target, const char* name, bool fallback) {
    const auto property = get(target, name);
    return property.isBool() ? static_cast<bool>(property) : fallback;
}

juce::var writeVec3(Vec3 value) {
    juce::Array<juce::var> values;
    values.add(value.x, value.y, value.z);
    return values;
}

bool readVec3(const juce::var& value, Vec3& result) {
    const auto* values = value.getArray();
    if (values == nullptr || values->size() != 3)
        return false;
    const Vec3 candidate{static_cast<double>((*values)[0]), static_cast<double>((*values)[1]),
                         static_cast<double>((*values)[2])};
    if (!isFinite(candidate) || std::abs(candidate.x) > 1.0e6 || std::abs(candidate.y) > 1.0e6
        || std::abs(candidate.z) > 1.0e6)
        return false;
    result = candidate;
    return true;
}

juce::var writeSimulationState(const SimulationState& state) {
    auto result = makeObject();
    set(result, "seed", juce::String(state.seed));
    juce::Array<juce::var> bodies;
    for (const auto& body : state.bodies) {
        auto item = makeObject();
        set(item, "mass", body.mass);
        set(item, "position", writeVec3(body.position));
        set(item, "velocity", writeVec3(body.velocity));
        bodies.add(item);
    }
    set(result, "bodies", bodies);
    return result;
}

bool readSimulationState(const juce::var& source, SimulationState& state) {
    auto* bodies = get(source, "bodies").getArray();
    if (bodies == nullptr || bodies->size() != static_cast<int>(bodyCount))
        return false;
    SimulationState result;
    result.seed = static_cast<std::uint64_t>(get(source, "seed").toString().getLargeIntValue());
    for (std::size_t body = 0; body < bodyCount; ++body) {
        const auto& item = bodies->getReference(static_cast<int>(body));
        result.bodies[body].mass = number(item, "mass", 1.0, 0.05, 1000.0);
        if (!readVec3(get(item, "position"), result.bodies[body].position)
            || !readVec3(get(item, "velocity"), result.bodies[body].velocity))
            return false;
    }
    state = result;
    return true;
}

juce::var writeEngine(const EngineConfig& config) {
    auto result = makeObject();
    auto simulation = makeObject();
    set(simulation, "gravity", config.simulation.gravitationalConstant);
    set(simulation, "softening", config.simulation.softening);
    set(simulation, "fixedStep", config.simulation.fixedStep);
    set(simulation, "speed", config.simulation.speed);
    set(simulation, "escapeRadius", config.simulation.escapeRadius);
    set(simulation, "maxSteps", static_cast<int>(config.simulation.maxStepsPerAdvance));
    set(simulation, "escapePolicy", static_cast<int>(config.simulation.escapePolicy));
    set(result, "simulation", simulation);
    set(result, "voicingMode", static_cast<int>(config.voicingMode));
    set(result, "strumMilliseconds", config.chordStrumMilliseconds);
    set(result, "strumUnit", static_cast<int>(config.chordStrumUnit));
    set(result, "strumValue", config.chordStrumValue);
    set(result, "minimumChordInterval", config.minimumChordIntervalBeats);
    set(result, "chordRoot", static_cast<int>(config.chordSystem.root));
    set(result, "chordScale", static_cast<int>(config.chordSystem.scale));
    juce::Array<juce::var> chordCustomScale;
    for (const auto enabled : config.chordSystem.customScale) chordCustomScale.add(enabled);
    set(result, "chordCustomScale", chordCustomScale);
    set(result, "autoResetEnabled", config.autoResetEnabled);
    set(result, "autoResetBars", config.autoResetBars);
    set(result, "inputTransposeEnabled", config.inputTransposeEnabled);
    set(result, "inputGateEnabled", config.inputGateEnabled);
    set(result, "inputTranspose", static_cast<int>(config.inputTranspose));

    juce::Array<juce::var> voices;
    for (const auto& voice : config.voices) {
        auto item = makeObject();
        set(item, "enabled", voice.enabled);
        set(item, "channel", static_cast<int>(voice.channel));
        set(item, "root", static_cast<int>(voice.root));
        set(item, "minimumNote", static_cast<int>(voice.minimumNote));
        set(item, "maximumNote", static_cast<int>(voice.maximumNote));
        set(item, "scale", static_cast<int>(voice.scale));
        set(item, "octave", static_cast<int>(voice.octave));
        set(item, "pitchMapping", static_cast<int>(voice.pitchMapping));
        set(item, "triggerMapping", static_cast<int>(voice.triggerMapping));
        set(item, "durationMapping", static_cast<int>(voice.durationMapping));
        set(item, "clockDivision", voice.clockDivisionBeats);
        set(item, "probability", voice.probability);
        set(item, "durationMin", voice.minimumDurationBeats);
        set(item, "durationMax", voice.maximumDurationBeats);
        set(item, "minimumTriggerInterval", voice.minimumTriggerIntervalBeats);
        set(item, "closeApproachDistance", voice.closeApproachDistance);
        set(item, "minimumVelocity", static_cast<int>(voice.minimumVelocity));
        set(item, "maximumVelocity", static_cast<int>(voice.maximumVelocity));
        juce::Array<juce::var> customScale;
        for (const auto enabled : voice.customScale) customScale.add(enabled);
        set(item, "customScale", customScale);
        juce::Array<juce::var> lanes;
        for (const auto& lane : voice.ccLanes) {
            auto cc = makeObject();
            set(cc, "enabled", lane.enabled);
            set(cc, "source", static_cast<int>(lane.source));
            set(cc, "controller", static_cast<int>(lane.controller));
            set(cc, "smoothing", lane.smoothing);
            lanes.add(cc);
        }
        set(item, "ccLanes", lanes);
        voices.add(item);
    }
    set(result, "voices", voices);
    return result;
}

bool readEngine(const juce::var& source, EngineConfig& config) {
    auto* voices = get(source, "voices").getArray();
    if (voices == nullptr || voices->size() != static_cast<int>(bodyCount))
        return false;
    EngineConfig result;
    const auto simulation = get(source, "simulation");
    result.simulation.gravitationalConstant = number(simulation, "gravity", 0.65, 0.01, 3.0);
    result.simulation.softening = number(simulation, "softening", 0.04, 0.001, 0.25);
    result.simulation.fixedStep = number(simulation, "fixedStep", 1.0 / 480.0, 1.0e-6, 0.1);
    result.simulation.speed = number(simulation, "speed", 1.0, 0.0, 8.0);
    result.simulation.escapeRadius = number(simulation, "escapeRadius", 12.0, 1.0, 1000.0);
    result.simulation.maxStepsPerAdvance = static_cast<std::uint32_t>(
        integer(simulation, "maxSteps", 16384, 1, 1000000));
    result.simulation.escapePolicy = static_cast<EscapePolicy>(
        integer(simulation, "escapePolicy", 0, 0, static_cast<int>(EscapePolicy::Prompt)));
    result.voicingMode = static_cast<VoicingMode>(integer(source, "voicingMode", 0, 0, 2));
    result.chordStrumMilliseconds = number(source, "strumMilliseconds", 24.0, 0.0, 250.0);
    result.chordStrumUnit = static_cast<StrumUnit>(integer(source, "strumUnit", 0, 0, 2));
    result.chordStrumValue = number(source, "strumValue", 0.0625, 0.0, 4.0);
    result.minimumChordIntervalBeats = number(source, "minimumChordInterval", 0.125, 0.0, 16.0);
    result.chordSystem.root = static_cast<std::uint8_t>(integer(source, "chordRoot", 0, 0, 11));
    result.chordSystem.scale = static_cast<ScaleId>(integer(source, "chordScale", 8, 0, 14));
    if (auto* custom = get(source, "chordCustomScale").getArray(); custom != nullptr && custom->size() == 12)
        for (int note = 0; note < 12; ++note)
            result.chordSystem.customScale[static_cast<std::size_t>(note)] = static_cast<bool>((*custom)[note]);
    result.autoResetEnabled = boolean(source, "autoResetEnabled", false);
    result.autoResetBars = number(source, "autoResetBars", 1.0, 1.0 / 16.0, 256.0);
    result.inputTransposeEnabled = boolean(source, "inputTransposeEnabled", false);
    result.inputGateEnabled = boolean(source, "inputGateEnabled", false);
    result.inputTranspose = static_cast<std::int8_t>(integer(source, "inputTranspose", 0, -127, 127));
    for (std::size_t index = 0; index < bodyCount; ++index) {
        const auto& item = voices->getReference(static_cast<int>(index));
        auto& voice = result.voices[index];
        voice.enabled = boolean(item, "enabled", true);
        voice.channel = static_cast<std::uint8_t>(integer(item, "channel", static_cast<int>(index + 1U), 1, 16));
        voice.root = static_cast<std::uint8_t>(integer(item, "root", 0, 0, 11));
        voice.minimumNote = static_cast<std::uint8_t>(integer(item, "minimumNote", 36, 0, 127));
        voice.maximumNote = static_cast<std::uint8_t>(integer(item, "maximumNote", 84, 0, 127));
        voice.scale = static_cast<ScaleId>(integer(item, "scale", 8, 0, 14));
        voice.octave = static_cast<std::int8_t>(integer(item, "octave", 0, -4, 4));
        voice.pitchMapping = static_cast<PitchMapping>(integer(item, "pitchMapping", 0, 0, 8));
        voice.triggerMapping = static_cast<TriggerMapping>(integer(item, "triggerMapping", 0, 0, 7));
        voice.durationMapping = static_cast<PitchMapping>(integer(item, "durationMapping", 3, 0, 8));
        voice.clockDivisionBeats = number(item, "clockDivision", 0.25, 1.0 / 128.0, 16.0);
        voice.probability = number(item, "probability", 1.0, 0.0, 1.0);
        // Legacy schema-1 files store a single "duration"; fall back to it for both bounds.
        const auto legacyDuration = number(item, "duration", 0.2, 1.0 / 1024.0, 64.0);
        voice.minimumDurationBeats = number(item, "durationMin", legacyDuration, 1.0 / 1024.0, 64.0);
        voice.maximumDurationBeats = number(item, "durationMax", legacyDuration, 1.0 / 1024.0, 64.0);
        voice.minimumTriggerIntervalBeats = number(item, "minimumTriggerInterval", 0.0625, 0.0, 64.0);
        voice.closeApproachDistance = number(item, "closeApproachDistance", 1.0, 0.0001, 1000.0);
        voice.minimumVelocity = static_cast<std::uint8_t>(integer(item, "minimumVelocity", 44, 1, 127));
        voice.maximumVelocity = static_cast<std::uint8_t>(integer(item, "maximumVelocity", 112, 1, 127));
        if (auto* custom = get(item, "customScale").getArray(); custom != nullptr && custom->size() == 12)
            for (int note = 0; note < 12; ++note)
                voice.customScale[static_cast<std::size_t>(note)] = static_cast<bool>((*custom)[note]);
        if (auto* lanes = get(item, "ccLanes").getArray(); lanes != nullptr && lanes->size() == 2) {
            for (int lane = 0; lane < 2; ++lane) {
                const auto& cc = lanes->getReference(lane);
                auto& destination = voice.ccLanes[static_cast<std::size_t>(lane)];
                destination.enabled = boolean(cc, "enabled", false);
                destination.source = static_cast<PitchMapping>(integer(cc, "source", 3, 0, 8));
                destination.controller = static_cast<std::uint8_t>(integer(cc, "controller", 1, 0, 119));
                destination.smoothing = number(cc, "smoothing", 0.85, 0.0, 1.0);
            }
        }
    }
    config = result;
    return true;
}

juce::var writePresentation(const PresentationState& state) {
    auto result = makeObject();
    auto visual = makeObject();
    set(visual, "trailSeconds", state.visual.trailSeconds);
    set(visual, "trailWidth", state.visual.trailWidth);
    set(visual, "extrusion", state.visual.extrusion);
    set(visual, "bloom", state.visual.bloom);
    set(visual, "starDensity", state.visual.starDensity);
    set(visual, "palette", static_cast<int>(state.visual.palette));
    set(visual, "quality", static_cast<int>(state.visual.quality));
    set(result, "visual", visual);

    const auto cameraState = sanitizedCameraState(state.camera);
    auto camera = makeObject();
    set(camera, "yaw", cameraState.yaw);
    set(camera, "pitch", cameraState.pitch);
    set(camera, "distance", cameraState.distance);
    set(camera, "minimumDistance", cameraState.minimumDistance);
    set(camera, "maximumDistance", cameraState.maximumDistance);
    set(camera, "autoOrbit", cameraState.autoOrbit);
    set(camera, "focusBody", cameraState.focusBody);
    set(camera, "autoFrame", cameraState.autoFrame);
    set(result, "camera", camera);
    set(result, "visualSeed", juce::String(state.visualSeed));
    set(result, "notePaneMinimized", state.notePaneMinimized);
    set(result, "notePaneStyle", static_cast<int>(state.notePaneStyle));
    return result;
}

PresentationState readPresentation(const juce::var& source) {
    PresentationState result;
    const auto visual = get(source, "visual");
    result.visual.trailSeconds = static_cast<float>(number(visual, "trailSeconds", 30.0, 5.0, 60.0));
    result.visual.trailWidth = static_cast<float>(number(visual, "trailWidth", 1.2, 0.1, 10.0));
    result.visual.extrusion = static_cast<float>(number(visual, "extrusion", 0.12, 0.0, 2.0));
    result.visual.bloom = static_cast<float>(number(visual, "bloom", 0.34, 0.0, 1.0));
    result.visual.starDensity = static_cast<float>(number(visual, "starDensity", 0.62, 0.0, 1.0));
    result.visual.palette = static_cast<PaletteId>(integer(visual, "palette", 0, 0, 5));
    result.visual.quality = static_cast<GraphicsQuality>(integer(visual, "quality", 2, 0, 2));
    const auto camera = get(source, "camera");
    result.camera.yaw = static_cast<float>(number(camera, "yaw", 0.0, -1000.0, 1000.0));
    result.camera.pitch = static_cast<float>(number(camera, "pitch", -0.34, -1.49, 1.49));
    result.camera.distance = static_cast<float>(number(camera, "distance", 7.0, 1.0, 80.0));
    result.camera.minimumDistance = static_cast<float>(number(camera, "minimumDistance", 2.5, 1.0, 20.0));
    result.camera.maximumDistance = static_cast<float>(number(camera, "maximumDistance", 40.0, 5.0, 80.0));
    result.camera.autoOrbit = static_cast<float>(number(camera, "autoOrbit", 0.035, -2.0, 2.0));
    result.camera.focusBody = integer(camera, "focusBody", -1, -1, 2);
    result.camera.autoFrame = boolean(camera, "autoFrame", true);
    result.camera = sanitizedCameraState(result.camera);
    result.visualSeed = static_cast<std::uint64_t>(get(source, "visualSeed").toString().getLargeIntValue());
    result.notePaneMinimized = boolean(source, "notePaneMinimized", false);
    result.notePaneStyle = static_cast<NotePaneStyle>(integer(source, "notePaneStyle", 0, 0, 1));
    return result;
}

} // namespace

juce::String serializeUserConfiguration(const UserConfiguration& configuration) {
    auto root = makeObject();
    set(root, "format", "three-body-solution");
    set(root, "schemaVersion", userConfigurationSchemaVersion);
    set(root, "initialState", writeSimulationState(configuration.initial));
    set(root, "baseInitialState", writeSimulationState(configuration.baseInitial));
    juce::Array<juce::var> tilts;
    for (const auto tilt : configuration.planeTilts) tilts.add(tilt);
    set(root, "planeTilts", tilts);
    set(root, "engine", writeEngine(configuration.engine));
    set(root, "presentation", writePresentation(configuration.presentation));
    set(root, "loopPolicy", static_cast<int>(configuration.loopPolicy));
    set(root, "chaosPercent", configuration.chaosPercent);
    set(root, "densityPercent", configuration.densityPercent);
    set(root, "presetIndex", configuration.presetIndex);
    set(root, "run", configuration.run);
    set(root, "sync", configuration.sync);
    return juce::JSON::toString(root, true);
}

bool deserializeUserConfiguration(const juce::String& json, UserConfiguration& configuration,
                                  juce::String& error) {
    if (json.getNumBytesAsUTF8() > 2 * 1024 * 1024) {
        error = "Configuration file is larger than 2 MB.";
        return false;
    }
    const auto root = juce::JSON::parse(json);
    const auto fileSchema = integer(root, "schemaVersion", 0, 0, 1000);
    if (root.getDynamicObject() == nullptr || get(root, "format").toString() != "three-body-solution"
        || fileSchema < 1 || fileSchema > userConfigurationSchemaVersion) {
        error = "This is not a supported .3bs configuration.";
        return false;
    }
    UserConfiguration result;
    if (!readSimulationState(get(root, "initialState"), result.initial)
        || !readSimulationState(get(root, "baseInitialState"), result.baseInitial)
        || !readEngine(get(root, "engine"), result.engine)) {
        error = "The configuration is incomplete or contains invalid body data.";
        return false;
    }
    auto* tilts = get(root, "planeTilts").getArray();
    if (tilts == nullptr || tilts->size() != static_cast<int>(bodyCount)) {
        error = "The configuration has invalid plane tilts.";
        return false;
    }
    for (std::size_t body = 0; body < bodyCount; ++body) {
        const auto tilt = static_cast<double>((*tilts)[static_cast<int>(body)]);
        if (!std::isfinite(tilt)) {
            error = "The configuration has a non-finite plane tilt.";
            return false;
        }
        result.planeTilts[body] = std::clamp(tilt, -75.0, 75.0);
    }
    result.presentation = readPresentation(get(root, "presentation"));
    result.loopPolicy = static_cast<LoopPolicy>(integer(root, "loopPolicy", 0, 0, 1));
    result.chaosPercent = number(root, "chaosPercent", 20.0, 0.0, 100.0);
    result.densityPercent = number(root, "densityPercent", 80.0, 0.0, 100.0);
    result.presetIndex = integer(root, "presetIndex", 0, 0, 23);
    result.run = boolean(root, "run", true);
    result.sync = boolean(root, "sync", true);
    configuration = result;
    error.clear();
    return true;
}

} // namespace threebs
