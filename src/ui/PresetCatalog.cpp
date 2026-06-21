// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "ui/PresetCatalog.h"

#include <BinaryData.h>

#include <stdexcept>

namespace threebs {
namespace {

juce::var property(const juce::var& object, const char* name) {
    if (auto* dynamic = object.getDynamicObject())
        return dynamic->getProperty(name);
    return {};
}

juce::String text(const juce::var& object, const char* name) {
    return property(object, name).toString();
}

double number(const juce::var& object, const char* name, double fallback = 0.0) {
    const auto value = property(object, name);
    return value.isVoid() ? fallback : static_cast<double>(value);
}

InitialSystem initialSystem(const juce::String& value) {
    if (value == "hierarchical") return InitialSystem::Hierarchical;
    if (value == "stable") return InitialSystem::Stable;
    if (value == "controlledChaos") return InitialSystem::ControlledChaos;
    if (value == "unbound") return InitialSystem::Unbound;
    return InitialSystem::FigureEight;
}

EscapePolicy escapePolicy(const juce::String& value) {
    if (value == "respawn") return EscapePolicy::RespawnBody;
    if (value == "prompt") return EscapePolicy::Prompt;
    return EscapePolicy::Leave;
}

ScaleId scale(const juce::String& value) {
    if (value == "major") return ScaleId::Major;
    if (value == "naturalMinor") return ScaleId::NaturalMinor;
    if (value == "dorian") return ScaleId::Dorian;
    if (value == "phrygian") return ScaleId::Phrygian;
    if (value == "lydian") return ScaleId::Lydian;
    if (value == "mixolydian") return ScaleId::Mixolydian;
    if (value == "locrian") return ScaleId::Locrian;
    if (value == "majorPentatonic") return ScaleId::MajorPentatonic;
    if (value == "blues") return ScaleId::Blues;
    if (value == "chromatic") return ScaleId::Chromatic;
    if (value == "hungarianMinor") return ScaleId::HungarianMinor;
    if (value == "wholeTone") return ScaleId::WholeTone;
    if (value == "diminished") return ScaleId::Diminished;
    return ScaleId::MinorPentatonic;
}

PitchMapping pitchMapping(const juce::String& value) {
    if (value == "nearest") return PitchMapping::NearestBodyDistance;
    if (value == "plane") return PitchMapping::SignedPlaneDistance;
    if (value == "speed") return PitchMapping::Speed;
    if (value == "phase") return PitchMapping::OrbitalPhase;
    if (value == "radialVelocity") return PitchMapping::RadialVelocity;
    if (value == "relativeSpeed") return PitchMapping::RelativeSpeed;
    if (value == "acceleration") return PitchMapping::Acceleration;
    if (value == "angularMomentum") return PitchMapping::AngularMomentum;
    return PitchMapping::BarycentricRadius;
}

TriggerMapping triggerMapping(const juce::String& value) {
    if (value == "plane") return TriggerMapping::PlaneCrossing;
    if (value == "close") return TriggerMapping::CloseApproach;
    if (value == "turning") return TriggerMapping::TurningPoint;
    if (value == "apsis") return TriggerMapping::Apsis;
    if (value == "receding") return TriggerMapping::RecedingThreshold;
    if (value == "speedPeak") return TriggerMapping::SpeedPeak;
    if (value == "phaseStep") return TriggerMapping::PhaseStep;
    return TriggerMapping::Clock;
}

PaletteId palette(const juce::String& value) {
    if (value == "glass" || value == "cold" || value == "pale" || value == "white")
        return PaletteId::Glass;
    if (value == "ember" || value == "blackSun" || value == "dust" || value == "impact"
        || value == "iron" || value == "redshift")
        return PaletteId::Ember;
    if (value == "nocturne" || value == "archive" || value == "gravity" || value == "ink")
        return PaletteId::Nocturne;
    if (value == "cyan" || value == "blueGiant")
        return PaletteId::Cyan;
    if (value == "violet" || value == "crown" || value == "magenta")
        return PaletteId::Violet;
    return PaletteId::Eclipse;
}

int focusBody(const juce::String& value) {
    if (value == "body0") return 0;
    if (value == "body1") return 1;
    if (value == "body2") return 2;
    return -1;
}

VoicingMode voicingMode(const juce::String& value) {
    if (value == "chord") return VoicingMode::Chord;
    if (value == "strum") return VoicingMode::Strum;
    return VoicingMode::Independent;
}

StrumUnit strumUnit(const juce::String& value) {
    if (value == "beats") return StrumUnit::Beats;
    if (value == "barFraction") return StrumUnit::BarFraction;
    return StrumUnit::Milliseconds;
}

void readVoicingVoice(const juce::var& source, std::size_t index, double density,
                      VoiceConfig& voice) {
    voice.enabled = property(source, "enabled").isVoid()
        || static_cast<bool>(property(source, "enabled"));
    voice.channel = static_cast<std::uint8_t>(number(source, "channel", index + 1.0));
    voice.root = static_cast<std::uint8_t>(number(source, "root", 0.0));
    voice.octave = static_cast<std::int8_t>(number(source, "octave", 0.0));
    voice.scale = scale(text(source, "scale"));
    voice.pitchMapping = pitchMapping(text(source, "pitch"));
    voice.triggerMapping = triggerMapping(text(source, "trigger"));
    voice.minimumNote = static_cast<std::uint8_t>(number(source, "minimum", 36.0));
    voice.maximumNote = static_cast<std::uint8_t>(number(source, "maximum", 84.0));
    voice.clockDivisionBeats = number(source, "division", 0.25);
    voice.probability = density / 100.0;
    voice.durationMapping = pitchMapping(text(source, "durationMap"));
    voice.durationGrid = DurationGrid::Straight;
    voice.minimumDurationBeats = number(source, "durationMin", 0.25);
    voice.maximumDurationBeats = number(source, "durationMax", 1.0);
    voice.minimumTriggerIntervalBeats = number(source, "minimumTriggerInterval", 0.0625);
    voice.closeApproachDistance = number(source, "closeApproachDistance", 1.0);
    voice.minimumVelocity = static_cast<std::uint8_t>(number(source, "minimumVelocity", 44.0));
    voice.maximumVelocity = static_cast<std::uint8_t>(number(source, "maximumVelocity", 112.0));
}

} // namespace

PresetCatalog::PresetCatalog() {
    const auto source = juce::String::fromUTF8(
        ThreeBSAssets::factorypresets_json, ThreeBSAssets::factorypresets_jsonSize);
    const auto root = juce::JSON::parse(source);
    const auto schema = static_cast<int>(property(root, "schemaVersion"));
    if (schema < 1 || schema > 3)
        return;
    auto* array = property(root, "presets").getArray();
    if (array == nullptr || array->size() != 24)
        return;

    presets_.reserve(static_cast<std::size_t>(array->size()));
    for (const auto& item : *array) {
        ArtworkPreset preset;
        preset.id = text(item, "id");
        preset.name = text(item, "name");
        preset.description = text(item, "description");
        preset.system = initialSystem(text(item, "system"));
        preset.seed = static_cast<std::uint64_t>(static_cast<std::int64_t>(property(item, "seed")));
        preset.chaos = number(item, "chaos", 0.2);

        const auto physics = property(item, "physics");
        preset.simulation.gravitationalConstant = number(physics, "gravity", 0.65);
        preset.simulation.softening = number(physics, "softening", 0.04);
        preset.simulation.speed = number(physics, "speed", 1.0);
        preset.simulation.escapePolicy = escapePolicy(text(physics, "escapePolicy"));
        preset.loopPolicy = text(physics, "loopPolicy") == "continue"
                                ? LoopPolicy::Continue : LoopPolicy::Restart;

        auto* voices = property(item, "voices").getArray();
        if (voices == nullptr || voices->size() != static_cast<int>(bodyCount))
            return;
        for (std::size_t index = 0; index < bodyCount; ++index) {
            const auto& sourceVoice = voices->getReference(static_cast<int>(index));
            auto& voice = preset.voices[index];
            voice.channel = static_cast<std::uint8_t>(number(sourceVoice, "channel", index + 1.0));
            voice.root = static_cast<std::uint8_t>(number(sourceVoice, "root", 0.0));
            voice.octave = static_cast<std::int8_t>(number(sourceVoice, "octave", 0.0));
            voice.scale = scale(text(sourceVoice, "scale"));
            voice.pitchMapping = pitchMapping(text(sourceVoice, "pitch"));
            voice.triggerMapping = triggerMapping(text(sourceVoice, "trigger"));
            voice.minimumNote = static_cast<std::uint8_t>(number(sourceVoice, "minimum", 36.0));
            voice.maximumNote = static_cast<std::uint8_t>(number(sourceVoice, "maximum", 84.0));
            voice.clockDivisionBeats = number(sourceVoice, "division", 0.25);
            voice.probability = number(sourceVoice, "probability", 1.0);
            // Schema 1/2 stored a single "duration"; schema 3 adds a mapped range.
            const auto legacyDuration = number(sourceVoice, "duration", 0.2);
            voice.minimumDurationBeats = number(sourceVoice, "durationMin", legacyDuration);
            voice.maximumDurationBeats = number(sourceVoice, "durationMax", legacyDuration);
            const auto durationMapText = text(sourceVoice, "durationMap");
            if (durationMapText.isNotEmpty())
                voice.durationMapping = pitchMapping(durationMapText);
            voice.durationGrid = DurationGrid::ContinuousLegacy;
        }

        const auto visual = property(item, "visual");
        auto& presentation = preset.presentation;
        presentation.visual.trailSeconds = schema == 1
            ? migrateV1TrailLength(static_cast<float>(number(visual, "trailLength", 0.82)))
            : static_cast<float>(number(visual, "trailSeconds", 30.0));
        presentation.visual.trailWidth = static_cast<float>(number(visual, "trailWidth", 1.2));
        presentation.visual.extrusion = static_cast<float>(number(visual, "extrusion", 0.12));
        presentation.visual.bloom = static_cast<float>(number(visual, "bloom", 0.34));
        presentation.visual.starDensity = static_cast<float>(number(visual, "stars", 0.62));
        presentation.visual.palette = palette(text(visual, "palette"));
        const auto camera = property(item, "camera");
        presentation.camera.focusBody = focusBody(text(camera, "focus"));
        presentation.camera.distance = static_cast<float>(number(camera, "distance", 7.0));
        presentation.camera.minimumDistance = static_cast<float>(number(camera, "minimumDistance", 2.5));
        presentation.camera.maximumDistance = static_cast<float>(number(camera, "maximumDistance", 40.0));
        presentation.camera.yaw = static_cast<float>(number(camera, "yaw", 0.0));
        presentation.camera.pitch = static_cast<float>(number(camera, "pitch", -0.34));
        presentation.camera.autoOrbit = static_cast<float>(number(camera, "autoOrbit", 0.035));
        const auto autoFrameValue = property(camera, "autoFrame");
        presentation.camera.autoFrame = autoFrameValue.isVoid() || static_cast<bool>(autoFrameValue);
        presentation.camera = sanitizedCameraState(presentation.camera);
        presentation.visualSeed = preset.seed;
        presets_.push_back(std::move(preset));
    }
    valid_ = presets_.size() == 24;
}

VoicingPresetCatalog::VoicingPresetCatalog() {
    const auto source = juce::String::fromUTF8(
        ThreeBSAssets::voicingpresets_json, ThreeBSAssets::voicingpresets_jsonSize);
    const auto root = juce::JSON::parse(source);
    if (static_cast<int>(property(root, "schemaVersion")) != 1)
        return;
    auto* array = property(root, "presets").getArray();
    if (array == nullptr || array->size() != 12)
        return;

    presets_.reserve(static_cast<std::size_t>(array->size()));
    for (const auto& item : *array) {
        VoicingPreset preset;
        preset.id = text(item, "id");
        preset.name = text(item, "name");
        preset.description = text(item, "description");
        preset.mode = voicingMode(text(item, "mode"));
        preset.densityPercent = number(item, "density", 80.0);
        const auto chord = property(item, "chord");
        preset.chordSystem.root = static_cast<std::uint8_t>(number(chord, "root", 0.0));
        preset.chordSystem.scale = scale(text(chord, "scale"));
        const auto strum = property(item, "strum");
        preset.chordStrumUnit = strumUnit(text(strum, "unit"));
        preset.chordStrumMilliseconds = number(strum, "milliseconds", 24.0);
        preset.chordStrumValue = number(strum, "value", 0.0625);

        auto* voices = property(item, "voices").getArray();
        if (preset.id.isEmpty() || preset.name.isEmpty() || voices == nullptr
            || voices->size() != static_cast<int>(bodyCount))
            return;
        for (std::size_t index = 0; index < bodyCount; ++index)
            readVoicingVoice(voices->getReference(static_cast<int>(index)), index,
                             preset.densityPercent, preset.voices[index]);
        presets_.push_back(std::move(preset));
    }
    valid_ = presets_.size() == 12;
}

const VoicingPreset& VoicingPresetCatalog::operator[](std::size_t index) const {
    if (presets_.empty())
        throw std::out_of_range("No voicing presets are available");
    return presets_[std::min(index, presets_.size() - 1U)];
}

juce::StringArray VoicingPresetCatalog::names() const {
    juce::StringArray result;
    for (const auto& preset : presets_)
        result.add(preset.name);
    return result;
}

const ArtworkPreset& PresetCatalog::operator[](std::size_t index) const {
    if (presets_.empty())
        throw std::out_of_range("No factory presets are available");
    return presets_[std::min(index, presets_.size() - 1U)];
}

juce::StringArray PresetCatalog::names() const {
    juce::StringArray result;
    for (const auto& preset : presets_)
        result.add(preset.name);
    return result;
}

juce::StringArray scaleDisplayNames() {
    return {"Major", "Natural Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
            "Major Pentatonic", "Minor Pentatonic", "Blues", "Chromatic", "Hungarian Minor",
            "Whole Tone", "Diminished", "Custom"};
}

juce::StringArray pitchMappingDisplayNames() {
    return {"Barycentric Radius", "Nearest Body", "Plane Distance", "Speed", "Orbital Phase",
            "Radial Velocity", "Relative Speed", "Acceleration", "Angular Momentum"};
}

juce::StringArray triggerMappingDisplayNames() {
    return {"Clock", "Plane Crossing", "Close Approach", "Turning Point", "Apsis",
            "Receding Threshold", "Speed Peak", "Phase Steps"};
}

juce::StringArray durationGridDisplayNames() {
    return {"Continuous (Legacy)", "Straight Notes"};
}

juce::String presetCategoryName(InitialSystem system) {
    switch (system) {
    case InitialSystem::FigureEight: return "FIGURE EIGHT";
    case InitialSystem::Hierarchical: return "HIERARCHICAL";
    case InitialSystem::Stable: return "STABLE";
    case InitialSystem::ControlledChaos: return "CONTROLLED CHAOS";
    case InitialSystem::Unbound: return "UNBOUND";
    }
    return "SYSTEM";
}

} // namespace threebs
