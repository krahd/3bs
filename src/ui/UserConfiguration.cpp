// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/UserConfiguration.h"
#include "ui/NumericText.h"

#include <algorithm>
#include <cmath>
#include <charconv>
#include <limits>
#include <iomanip>
#include <locale>
#include <sstream>

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

// JUCE's JSON formatter uses fixed decimal places through much of the physical
// range. Its JSON reader counts leading fractional zeros in its digit budget.
// Scientific max_digits10 output retains precision. For subnormal values,
// hold the exponent at -308 and shift the decimal point to avoid JUCE's early
// underflow path, retaining JSON number types and the configuration structure.
void writeExactJson(juce::OutputStream& output, const juce::var& value) {
    if (value.isDouble()) {
        const auto number = static_cast<double>(value);
        if (!std::isfinite(number)) { output << "null"; return; }
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10 - 1) << number;
        auto encoded = text.str();
        const auto exponentMarker = encoded.find("e-");
        if (exponentMarker != std::string::npos) {
            int exponentMagnitude{};
            const auto converted = std::from_chars(encoded.data() + exponentMarker + 2U,
                                                   encoded.data() + encoded.size(), exponentMagnitude);
            constexpr auto minimumExponentMagnitude = 1 - std::numeric_limits<double>::min_exponent10;
            if (converted.ec == std::errc{} && exponentMagnitude > minimumExponentMagnitude) {
                const auto signLength = encoded.front() == '-' ? 1U : 0U;
                auto digits = encoded.substr(signLength, exponentMarker - signLength);
                digits.erase(1U, 1U); // Scientific formatting always has one digit before the decimal.
                encoded = std::string(signLength != 0U ? "-0." : "0.")
                    + std::string(static_cast<std::size_t>(exponentMagnitude - minimumExponentMagnitude - 1), '0')
                    + digits + "e-" + std::to_string(minimumExponentMagnitude);
            }
        }
        output << juce::String(encoded);
    } else if (const auto* object = value.getDynamicObject()) {
        output << '{';
        const auto& properties = object->getProperties();
        for (int index = 0; index < properties.size(); ++index) {
            if (index != 0) output << ',';
            const auto name = properties.getName(index);
            output << juce::JSON::toString(juce::var(name.toString()), true) << ':';
            writeExactJson(output, object->getProperty(name));
        }
        output << '}';
    } else if (const auto* values = value.getArray()) {
        output << '[';
        for (int index = 0; index < values->size(); ++index) {
            if (index != 0) output << ',';
            writeExactJson(output, (*values)[index]);
        }
        output << ']';
    } else {
        output << juce::JSON::toString(value, true);
    }
}

// JUCE parses integer prefixes and exponents with signed accumulation before
// property validation. Bound those tokens and recursive nesting first; leave
// structural JSON grammar to JUCE. Quoted strings and escapes are ignored.
bool safeJsonInput(const juce::String& json) {
    const auto* bytes = json.toRawUTF8();
    const auto length = static_cast<std::size_t>(json.getNumBytesAsUTF8());
    bool quoted{}, escaped{};
    std::size_t nesting{};
    constexpr std::size_t maximumNesting = 64U;
    const auto digit = [](char value) { return value >= '0' && value <= '9'; };
    const auto delimiter = [](char value) {
        return value == ',' || value == ']' || value == '}' || value == ' '
            || value == '\t' || value == '\r' || value == '\n';
    };
    for (std::size_t position = 0; position < length;) {
        const auto character = bytes[position];
        if (quoted) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == '"') quoted = false;
            ++position;
            continue;
        }
        if (character == '"') { quoted = true; ++position; continue; }
        if (character == '[' || character == '{') {
            if (++nesting > maximumNesting) return false;
        } else if (character == ']' || character == '}') {
            if (nesting != 0U) --nesting;
        }
        if (character != '-' && !digit(character)) { ++position; continue; }
        const auto start = position;
        while (position < length && !delimiter(bytes[position])) ++position;
        auto cursor = start + (bytes[start] == '-' ? 1U : 0U);
        const auto integerStart = cursor;
        while (cursor < position && digit(bytes[cursor])) ++cursor;
        if (cursor == integerStart) return false;
        std::uint64_t integer{};
        const auto converted = std::from_chars(bytes + integerStart, bytes + cursor, integer);
        if (converted.ec != std::errc{} || converted.ptr != bytes + cursor
            || integer > static_cast<std::uint64_t>(std::numeric_limits<juce::int64>::max()))
            return false;
        for (auto exponent = cursor; exponent < position; ++exponent) {
            if (bytes[exponent] != 'e' && bytes[exponent] != 'E') continue;
            auto digits = exponent + 1U;
            if (digits < position && (bytes[digits] == '+' || bytes[digits] == '-')) ++digits;
            const auto exponentStart = digits;
            while (digits < position && digit(bytes[digits])) ++digits;
            // digits10 leaves room for JUCE's extra mantissa exponent as well.
            if (digits == exponentStart || digits != position
                || digits - exponentStart > static_cast<std::size_t>(std::numeric_limits<int>::digits10))
                return false;
            break;
        }
        double value{};
        if (!parseFiniteNumber(juce::String::fromUTF8(bytes + start, static_cast<int>(position - start)), value))
            return false;
    }
    return true;
}

class Reader {
public:
    bool valid{true};
    juce::String error;

    void fail(const char* field) {
        valid = false;
        if (error.isEmpty()) error = "Invalid configuration field: " + juce::String(field);
    }
    bool object(const juce::var& value, const char* field) {
        if (value.getDynamicObject() != nullptr) return true;
        fail(field); return false;
    }
    static bool has(const juce::var& target, const char* name) {
        const auto* object = target.getDynamicObject();
        return object != nullptr && object->hasProperty(name);
    }
    static bool numeric(const juce::var& value) {
        return value.isInt() || value.isInt64() || value.isDouble();
    }
    double number(const juce::var& target, const char* name, double fallback,
                  double minimum, double maximum) {
        const auto value = get(target, name);
        if (!has(target, name)) return fallback; // Explicit legacy/default migration only.
        if (!numeric(value)) { fail(name); return fallback; }
        const auto result = static_cast<double>(value);
        if (!std::isfinite(result) || result < minimum || result > maximum) {
            fail(name); return fallback;
        }
        return result;
    }
    int integer(const juce::var& target, const char* name, int fallback, int minimum, int maximum) {
        const auto value = number(target, name, fallback, minimum, maximum);
        if (std::trunc(value) != value) { fail(name); return fallback; }
        return static_cast<int>(value);
    }
    bool boolean(const juce::var& target, const char* name, bool fallback) {
        const auto value = get(target, name);
        if (!has(target, name)) return fallback;
        if (!value.isBool()) { fail(name); return fallback; }
        return static_cast<bool>(value);
    }
    std::uint64_t seed(const juce::var& target, const char* name, std::uint64_t fallback) {
        const auto value = get(target, name);
        if (!has(target, name)) return fallback;
        if (value.isInt() || value.isInt64()) {
            const auto signedValue = static_cast<juce::int64>(value);
            if (signedValue >= 0) return static_cast<std::uint64_t>(signedValue);
        } else if (value.isString()) {
            const auto text = value.toString().toStdString();
            std::uint64_t result{};
            const auto conversion = std::from_chars(text.data(), text.data() + text.size(), result);
            if (!text.empty() && conversion.ec == std::errc{}
                && conversion.ptr == text.data() + text.size()) return result;
        }
        fail(name); return fallback;
    }
    bool booleanArray(const juce::var& objectValue, const char* name, std::array<bool, 12>& out) {
        const auto value = get(objectValue, name);
        if (!has(objectValue, name)) return true;
        const auto* values = value.getArray();
        if (values == nullptr || values->size() != 12) { fail(name); return false; }
        for (int i = 0; i < 12; ++i) {
            if (!(*values)[i].isBool()) { fail(name); return false; }
            out[static_cast<std::size_t>(i)] = static_cast<bool>((*values)[i]);
        }
        return true;
    }
};

juce::var writeVec3(Vec3 value) {
    juce::Array<juce::var> values;
    values.add(value.x, value.y, value.z);
    return values;
}

bool readVec3(const juce::var& value, Vec3& result, Reader& reader) {
    const auto* values = value.getArray();
    if (values == nullptr || values->size() != 3) return false;
    for (const auto& component : *values)
        if (!Reader::numeric(component)) return false;
    const Vec3 candidate{static_cast<double>((*values)[0]), static_cast<double>((*values)[1]),
                         static_cast<double>((*values)[2])};
    if (!isFinite(candidate) || std::abs(candidate.x) > 1.0e6 || std::abs(candidate.y) > 1.0e6
        || std::abs(candidate.z) > 1.0e6) {
        reader.fail("body vector"); return false;
    }
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

bool readSimulationState(const juce::var& source, SimulationState& state, Reader& reader) {
    if (!reader.object(source, "simulation state")) return false;
    const auto bodiesValue = get(source, "bodies");
    const auto* bodies = bodiesValue.getArray();
    if (bodies == nullptr || bodies->size() != static_cast<int>(bodyCount)) return false;
    SimulationState result;
    result.seed = reader.seed(source, "seed", 0x334253ULL);
    for (std::size_t body = 0; body < bodyCount; ++body) {
        const auto& item = bodies->getReference(static_cast<int>(body));
        if (!reader.object(item, "body")) return false;
        result.bodies[body].mass = reader.number(item, "mass", 1.0, 0.05, 1000.0);
        if (!readVec3(get(item, "position"), result.bodies[body].position, reader)
            || !readVec3(get(item, "velocity"), result.bodies[body].velocity, reader)) return false;
    }
    if (!reader.valid) return false;
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
        set(item, "durationGrid", static_cast<int>(voice.durationGrid));
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

bool readEngine(const juce::var& source, EngineConfig& config, Reader& reader) {
    if (!reader.object(source, "engine")) return false;
    auto* voices = get(source, "voices").getArray();
    if (voices == nullptr || voices->size() != static_cast<int>(bodyCount))
        return false;
    EngineConfig result;
    const auto simulation = get(source, "simulation");
    if (!reader.object(simulation, "engine simulation")) return false;
    result.simulation.gravitationalConstant = reader.number(simulation, "gravity", 0.65, 0.000001, 1000.0);
    result.simulation.softening = reader.number(simulation, "softening", 0.04, 0.000001, 4.0);
    result.simulation.fixedStep = reader.number(simulation, "fixedStep", 1.0 / 480.0, 1.0 / 96000.0, 0.25);
    result.simulation.speed = reader.number(simulation, "speed", 1.0, 0.0, 64.0);
    result.simulation.escapeRadius = reader.number(simulation, "escapeRadius", 12.0, 1.0, 100000.0);
    result.simulation.maxStepsPerAdvance = static_cast<std::uint32_t>(
        reader.integer(simulation, "maxSteps", 16384, 1, 1000000));
    result.simulation.escapePolicy = static_cast<EscapePolicy>(
        reader.integer(simulation, "escapePolicy", 0, 0, static_cast<int>(EscapePolicy::Prompt)));
    result.voicingMode = static_cast<VoicingMode>(reader.integer(source, "voicingMode", 0, 0, 2));
    result.chordStrumMilliseconds = reader.number(source, "strumMilliseconds", 24.0, 0.0, 250.0);
    result.chordStrumUnit = static_cast<StrumUnit>(reader.integer(source, "strumUnit", 0, 0, 2));
    result.chordStrumValue = reader.number(source, "strumValue", 0.0625, 0.0, 4.0);
    result.minimumChordIntervalBeats = reader.number(source, "minimumChordInterval", 0.125, 0.0, 64.0);
    result.chordSystem.root = static_cast<std::uint8_t>(reader.integer(source, "chordRoot", 0, 0, 11));
    result.chordSystem.scale = static_cast<ScaleId>(reader.integer(source, "chordScale", 8, 0, 14));
    reader.booleanArray(source, "chordCustomScale", result.chordSystem.customScale);
    result.autoResetEnabled = reader.boolean(source, "autoResetEnabled", false);
    result.autoResetBars = reader.number(source, "autoResetBars", 1.0, 1.0 / 16.0, 256.0);
    result.inputTransposeEnabled = reader.boolean(source, "inputTransposeEnabled", false);
    result.inputGateEnabled = reader.boolean(source, "inputGateEnabled", false);
    result.inputTranspose = static_cast<std::int8_t>(reader.integer(source, "inputTranspose", 0, -127, 127));
    for (std::size_t index = 0; index < bodyCount; ++index) {
        const auto& item = voices->getReference(static_cast<int>(index));
        if (!reader.object(item, "voice")) return false;
        auto& voice = result.voices[index];
        voice.enabled = reader.boolean(item, "enabled", true);
        voice.channel = static_cast<std::uint8_t>(reader.integer(item, "channel", static_cast<int>(index + 1U), 1, 16));
        voice.root = static_cast<std::uint8_t>(reader.integer(item, "root", 0, 0, 11));
        voice.minimumNote = static_cast<std::uint8_t>(reader.integer(item, "minimumNote", 36, 0, 127));
        voice.maximumNote = static_cast<std::uint8_t>(reader.integer(item, "maximumNote", 84, 0, 127));
        voice.scale = static_cast<ScaleId>(reader.integer(item, "scale", 8, 0, 14));
        voice.octave = static_cast<std::int8_t>(reader.integer(item, "octave", 0, -4, 4));
        voice.pitchMapping = static_cast<PitchMapping>(reader.integer(item, "pitchMapping", 0, 0, 8));
        voice.triggerMapping = static_cast<TriggerMapping>(reader.integer(item, "triggerMapping", 0, 0, 7));
        voice.durationMapping = static_cast<PitchMapping>(reader.integer(item, "durationMapping", 3, 0, 8));
        voice.durationGrid = static_cast<DurationGrid>(reader.integer(
            item, "durationGrid", static_cast<int>(DurationGrid::ContinuousLegacy), 0, 1));
        voice.clockDivisionBeats = reader.number(item, "clockDivision", 0.25, 1.0 / 128.0, 16.0);
        voice.probability = reader.number(item, "probability", 1.0, 0.0, 1.0);
        // Legacy schema-1 files store a single "duration"; fall back to it for both bounds.
        const auto legacyDuration = reader.number(item, "duration", 0.2, 1.0 / 1024.0, 64.0);
        voice.minimumDurationBeats = reader.number(item, "durationMin", legacyDuration, 1.0 / 1024.0, 64.0);
        voice.maximumDurationBeats = reader.number(item, "durationMax", legacyDuration, 1.0 / 1024.0, 64.0);
        voice.minimumTriggerIntervalBeats = reader.number(item, "minimumTriggerInterval", 0.0625, 0.0, 64.0);
        voice.closeApproachDistance = reader.number(item, "closeApproachDistance", 1.0, 0.0001, 1000.0);
        voice.minimumVelocity = static_cast<std::uint8_t>(reader.integer(item, "minimumVelocity", 44, 1, 127));
        voice.maximumVelocity = static_cast<std::uint8_t>(reader.integer(item, "maximumVelocity", 112, 1, 127));
        reader.booleanArray(item, "customScale", voice.customScale);
        const auto laneValue = get(item, "ccLanes");
        const auto* lanes = laneValue.getArray();
        if (Reader::has(item, "ccLanes") && (lanes == nullptr || lanes->size() != 2)) {
            reader.fail("ccLanes"); return false;
        }
        if (lanes != nullptr) {
            for (int lane = 0; lane < 2; ++lane) {
                const auto& cc = lanes->getReference(lane);
                if (!reader.object(cc, "CC lane")) return false;
                auto& destination = voice.ccLanes[static_cast<std::size_t>(lane)];
                destination.enabled = reader.boolean(cc, "enabled", false);
                destination.source = static_cast<PitchMapping>(reader.integer(cc, "source", 3, 0, 8));
                destination.controller = static_cast<std::uint8_t>(reader.integer(cc, "controller", 1, 0, 119));
                destination.smoothing = reader.number(cc, "smoothing", 0.85, 0.0, 1.0);
            }
        }
    }
    if (!reader.valid || !isValidEngineConfig(result)) return false;
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

    const auto& cameraState = state.camera;
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

PresentationState readPresentation(const juce::var& source, Reader& reader) {
    reader.object(source, "presentation");
    PresentationState result;
    const auto visual = get(source, "visual");
    reader.object(visual, "visual");
    result.visual.trailSeconds = static_cast<float>(reader.number(visual, "trailSeconds", 30.0, 5.0, 60.0));
    result.visual.trailWidth = static_cast<float>(reader.number(visual, "trailWidth", 1.2, 0.1, 10.0));
    result.visual.extrusion = static_cast<float>(reader.number(visual, "extrusion", 0.12, 0.0, 2.0));
    result.visual.bloom = static_cast<float>(reader.number(visual, "bloom", 0.34, 0.0, 1.0));
    result.visual.starDensity = static_cast<float>(reader.number(visual, "starDensity", 0.62, 0.0, 1.0));
    result.visual.palette = static_cast<PaletteId>(reader.integer(visual, "palette", 0, 0, 5));
    result.visual.quality = static_cast<GraphicsQuality>(reader.integer(visual, "quality", 2, 0, 2));
    const auto camera = get(source, "camera");
    reader.object(camera, "camera");
    result.camera.yaw = static_cast<float>(reader.number(camera, "yaw", 0.0, -1000.0, 1000.0));
    result.camera.pitch = static_cast<float>(reader.number(camera, "pitch", -0.34, static_cast<double>(-1.49F), static_cast<double>(1.49F)));
    result.camera.distance = static_cast<float>(reader.number(camera, "distance", 7.0, 1.0, 80.0));
    result.camera.minimumDistance = static_cast<float>(reader.number(camera, "minimumDistance", 2.5, 1.0, 20.0));
    result.camera.maximumDistance = static_cast<float>(reader.number(camera, "maximumDistance", 40.0, 5.0, 80.0));
    result.camera.autoOrbit = static_cast<float>(reader.number(camera, "autoOrbit", 0.035, -2.0, 2.0));
    result.camera.focusBody = reader.integer(camera, "focusBody", -1, -1, 2);
    result.camera.autoFrame = reader.boolean(camera, "autoFrame", true);
    result.visualSeed = reader.seed(source, "visualSeed", 0);
    result.notePaneMinimized = reader.boolean(source, "notePaneMinimized", false);
    result.notePaneStyle = static_cast<NotePaneStyle>(reader.integer(source, "notePaneStyle", 0, 0, 1));
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
    set(root, "voicingPresetIndex", configuration.voicingPresetIndex);
    set(root, "run", configuration.run);
    set(root, "sync", configuration.sync);
    set(root, "timeSignatureSource", static_cast<int>(configuration.timeSignatureSource));
    set(root, "timeSignatureNumerator", configuration.timeSignatureNumerator);
    set(root, "timeSignatureDenominator", configuration.timeSignatureDenominator);
    juce::MemoryOutputStream output;
    writeExactJson(output, root);
    return output.toUTF8();
}

bool loadUserConfiguration(const juce::File& file, UserConfiguration& configuration,
                           juce::String& error) {
    juce::FileInputStream input(file);
    if (!input.openedOk()) {
        error = "Cannot read configuration file: " + input.getStatus().getErrorMessage();
        return false;
    }
    if (input.getTotalLength() > static_cast<juce::int64>(maximumUserConfigurationBytes)) {
        error = "Configuration file is larger than 2 MiB.";
        return false;
    }
    constexpr auto readLimit = maximumUserConfigurationBytes + 1U;
    juce::MemoryBlock data(readLimit);
    std::size_t used{};
    while (used < readLimit) {
        const auto count = input.read(static_cast<char*>(data.getData()) + used,
                                      static_cast<int>(readLimit - used));
        if (count < 0 || input.getStatus().failed()) {
            error = "Cannot read configuration file: " + input.getStatus().getErrorMessage();
            return false;
        }
        if (count == 0) break;
        used += static_cast<std::size_t>(count);
        if (used > maximumUserConfigurationBytes) {
            error = "Configuration file is larger than 2 MiB.";
            return false;
        }
    }
    // Retain JUCE's established BOM/encoding handling, with a bounded byte span.
    const auto text = juce::String::createStringFromData(data.getData(), static_cast<int>(used));
    return deserializeUserConfiguration(text, configuration, error);
}

bool deserializeUserConfiguration(const juce::String& json, UserConfiguration& configuration,
                                  juce::String& error) {
    if (json.getNumBytesAsUTF8() > maximumUserConfigurationBytes) {
        error = "Configuration file is larger than 2 MiB."; return false;
    }
    if (!safeJsonInput(json)) {
        error = "The configuration contains unsafe JSON nesting or an invalid numeric literal.";
        return false;
    }
    Reader reader;
    const auto root = juce::JSON::parse(json);
    const auto fileSchema = reader.integer(root, "schemaVersion", 0, 1, userConfigurationSchemaVersion);
    const auto format = get(root, "format");
    if (root.getDynamicObject() == nullptr || !format.isString()
        || format.toString() != "three-body-solution" || !reader.valid || fileSchema < 1) {
        error = "This is not a supported .3bs configuration."; return false;
    }
    UserConfiguration result;
    if (!readSimulationState(get(root, "initialState"), result.initial, reader)
        || !readSimulationState(get(root, "baseInitialState"), result.baseInitial, reader)
        || !readEngine(get(root, "engine"), result.engine, reader)) {
        error = reader.error.isNotEmpty() ? reader.error
            : "The configuration is incomplete or contains invalid body or voice data.";
        return false;
    }
    const auto tiltsValue = get(root, "planeTilts");
    const auto* tilts = tiltsValue.getArray();
    if (tilts == nullptr || tilts->size() != static_cast<int>(bodyCount)) {
        error = "The configuration has invalid plane tilts."; return false;
    }
    for (std::size_t body = 0; body < bodyCount; ++body) {
        const auto& value = (*tilts)[static_cast<int>(body)];
        if (!Reader::numeric(value)) { error = "Plane tilts must be numbers."; return false; }
        const auto tilt = static_cast<double>(value);
        if (!std::isfinite(tilt) || tilt < -75.0 || tilt > 75.0) {
            error = "Plane tilts must be finite and between -75 and 75 degrees."; return false;
        }
        result.planeTilts[body] = tilt;
    }
    result.presentation = readPresentation(get(root, "presentation"), reader);
    result.loopPolicy = static_cast<LoopPolicy>(reader.integer(root, "loopPolicy", 0, 0, 1));
    result.chaosPercent = reader.number(root, "chaosPercent", 20.0, 0.0, 100.0);
    result.densityPercent = reader.number(root, "densityPercent", 80.0, 0.0, 100.0);
    result.presetIndex = reader.integer(root, "presetIndex", 0, 0, 23);
    result.voicingPresetIndex = reader.integer(root, "voicingPresetIndex", -1, -1, 11);
    result.run = reader.boolean(root, "run", true);
    result.sync = reader.boolean(root, "sync", true);
    result.timeSignatureSource = static_cast<TimeSignatureSource>(
        reader.integer(root, "timeSignatureSource", 0, 0, 1));
    result.timeSignatureNumerator = reader.integer(root, "timeSignatureNumerator", 4, 1, 16);
    result.timeSignatureDenominator = reader.integer(root, "timeSignatureDenominator", 4, 1, 16);
    if (!reader.valid) { error = reader.error; return false; }
    if (!validateUserConfiguration(result, error)) return false;
    configuration = result;
    error.clear();
    return true;
}

bool validateUserConfiguration(const UserConfiguration& configuration, juce::String& error) {
    const auto within = [](double value, double minimum, double maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    };
    if (!isValidSimulationState(configuration.initial)
        || !isValidSimulationState(configuration.baseInitial)
        || !isValidEngineConfig(configuration.engine)) {
        error = "The configuration contains invalid simulation or musical data."; return false;
    }
    for (const auto tilt : configuration.planeTilts)
        if (!within(tilt, -75.0, 75.0)) { error = "Invalid orbital plane tilt."; return false; }
    const auto& visual = configuration.presentation.visual;
    const auto& camera = configuration.presentation.camera;
    if (!within(visual.trailSeconds, 5.0, 60.0) || !within(visual.trailWidth, 0.1, 10.0)
        || !within(visual.extrusion, 0.0, 2.0) || !within(visual.bloom, 0.0, 1.0)
        || !within(visual.starDensity, 0.0, 1.0)
        || static_cast<int>(visual.palette) > 5 || static_cast<int>(visual.quality) > 2
        || static_cast<int>(configuration.presentation.notePaneStyle) > 1
        || !within(camera.yaw, -1000.0, 1000.0) || !within(camera.pitch, static_cast<double>(-1.49F), static_cast<double>(1.49F))
        || !within(camera.distance, 1.0, 80.0) || !within(camera.minimumDistance, 1.0, 20.0)
        || !within(camera.maximumDistance, 5.0, 80.0)
        || camera.minimumDistance > camera.maximumDistance
        || camera.distance < camera.minimumDistance || camera.distance > camera.maximumDistance
        || !within(camera.autoOrbit, -2.0, 2.0) || camera.focusBody < -1 || camera.focusBody > 2) {
        error = "The configuration contains invalid presentation data."; return false;
    }
    const auto denominator = configuration.timeSignatureDenominator;
    if (static_cast<int>(configuration.loopPolicy) > 1
        || static_cast<int>(configuration.timeSignatureSource) > 1
        || configuration.timeSignatureNumerator < 1 || configuration.timeSignatureNumerator > 16
        || !(denominator == 1 || denominator == 2 || denominator == 4 || denominator == 8 || denominator == 16)
        || !within(configuration.chaosPercent, 0.0, 100.0)
        || !within(configuration.densityPercent, 0.0, 100.0)
        || configuration.presetIndex < 0 || configuration.presetIndex > 23
        || configuration.voicingPresetIndex < -1 || configuration.voicingPresetIndex > 11) {
        error = "The configuration contains invalid musical controls."; return false;
    }
    error.clear();
    return true;
}

} // namespace threebs
