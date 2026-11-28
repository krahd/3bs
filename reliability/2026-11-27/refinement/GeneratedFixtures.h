
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Generated from the validated published catalogue; do not edit by hand.
#pragma once
#include "core/MusicEngine.h"
#include <array>
#include <span>
#include <string_view>
#include <vector>
namespace diagnostic {
using namespace threebs;
struct SourceText {
    std::string_view value;
    bool isNotEmpty() const noexcept { return !value.empty(); }
    bool operator==(std::string_view other) const noexcept { return value == other; }
};
struct Value {
    enum Kind { Missing, Number, Integer, Text } kind{Missing};
    double floating{};
    std::int64_t integral{};
    std::string_view string;
    static constexpr Value numeric(double value) { return {Number, value, 0, {}}; }
    static constexpr Value integer(std::int64_t value) { return {Integer, 0, value, {}}; }
    static constexpr Value text(std::string_view value) { return {Text, 0, 0, value}; }
    bool isVoid() const noexcept { return kind == Missing; }
    explicit operator double() const noexcept { return kind == Integer ? static_cast<double>(integral) : floating; }
    explicit operator std::int64_t() const noexcept { return kind == Integer ? integral : static_cast<std::int64_t>(floating); }
    SourceText toString() const noexcept { return {string}; }
};
struct Field { std::string_view name; Value value; };
using Object = std::span<const Field>;
Value property(Object object, std::string_view name) {
    for (const auto& field : object) if (field.name == name) return field.value;
    return {};
}
SourceText text(Object object, std::string_view name) { return property(object, name).toString(); }
double number(Object object, std::string_view name, double fallback = 0.0) {
    const auto value = property(object, name);
    return value.isVoid() ? fallback : static_cast<double>(value);
}
struct Fixture {
    std::string_view id;
    InitialSystem system{InitialSystem::FigureEight};
    std::uint64_t seed{};
    double chaos{};
    SimulationConfig simulation{};
    LoopPolicy loopPolicy{LoopPolicy::Restart};
    std::array<VoiceConfig, bodyCount> voices{};
};
InitialSystem initialSystem(const SourceText& value) {
    if (value == "hierarchical") return InitialSystem::Hierarchical;
    if (value == "stable") return InitialSystem::Stable;
    if (value == "controlledChaos") return InitialSystem::ControlledChaos;
    if (value == "unbound") return InitialSystem::Unbound;
    return InitialSystem::FigureEight;
}
EscapePolicy escapePolicy(const SourceText& value) {
    if (value == "respawn") return EscapePolicy::RespawnBody;
    if (value == "prompt") return EscapePolicy::Prompt;
    return EscapePolicy::Leave;
}
ScaleId scale(const SourceText& value) {
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
PitchMapping pitchMapping(const SourceText& value) {
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
TriggerMapping triggerMapping(const SourceText& value) {
    if (value == "plane") return TriggerMapping::PlaneCrossing;
    if (value == "close") return TriggerMapping::CloseApproach;
    if (value == "turning") return TriggerMapping::TurningPoint;
    if (value == "apsis") return TriggerMapping::Apsis;
    if (value == "receding") return TriggerMapping::RecedingThreshold;
    if (value == "speedPeak") return TriggerMapping::SpeedPeak;
    if (value == "phaseStep") return TriggerMapping::PhaseStep;
    return TriggerMapping::Clock;
}
Fixture readFixture(std::string_view id, Object item, Object physics, const std::array<Object, bodyCount>& inputVoices) {
Fixture preset; preset.id = id;
preset.system = initialSystem(text(item, "system"));
        preset.seed = static_cast<std::uint64_t>(static_cast<std::int64_t>(property(item, "seed")));
        preset.chaos = number(item, "chaos", 0.2);

        preset.simulation.gravitationalConstant = number(physics, "gravity", 0.65);
        preset.simulation.softening = number(physics, "softening", 0.04);
        preset.simulation.speed = number(physics, "speed", 1.0);
        preset.simulation.escapePolicy = escapePolicy(text(physics, "escapePolicy"));
        preset.loopPolicy = text(physics, "loopPolicy") == "continue"
                                ? LoopPolicy::Continue : LoopPolicy::Restart;

        for (std::size_t index = 0; index < bodyCount; ++index) {
            const auto& sourceVoice = inputVoices[index];
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
return preset;
}
std::vector<Fixture> makeFixtures() {
std::vector<Fixture> result; result.reserve(24);
{
const std::array<Field, 3> item{{
Field{"system", Value::text("figureEight")},
Field{"seed", Value::integer(33425301LL)},
Field{"chaos", Value::numeric(0x1.47ae147ae147bp-6)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.0000000000000p+0)},
Field{"softening", Value::numeric(0x1.47ae147ae147bp-6)},
Field{"speed", Value::numeric(0x1.0000000000000p+0)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(36LL)},
Field{"maximum", Value::integer(72LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.ccccccccccccdp-1)},
Field{"duration", Value::numeric(0x1.ae147ae147ae1p-2)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(48LL)},
Field{"maximum", Value::integer(84LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.b333333333333p-1)},
Field{"duration", Value::numeric(0x1.47ae147ae147bp-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(31LL)},
Field{"maximum", Value::integer(67LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.8000000000000p-1)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("eightfold-dawn", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("stable")},
Field{"seed", Value::integer(33425302LL)},
Field{"chaos", Value::numeric(0x1.999999999999ap-5)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"softening", Value::numeric(0x1.999999999999ap-5)},
Field{"speed", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("lydian")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(38LL)},
Field{"maximum", Value::integer(74LL)},
Field{"division", Value::numeric(0x1.8000000000000p-1)},
Field{"probability", Value::numeric(0x1.a3d70a3d70a3dp-1)},
Field{"duration", Value::numeric(0x1.5c28f5c28f5c3p-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("lydian")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(45LL)},
Field{"maximum", Value::integer(81LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.5c28f5c28f5c3p-1)},
Field{"duration", Value::numeric(0x1.c28f5c28f5c29p-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("majorPentatonic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(52LL)},
Field{"maximum", Value::integer(88LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.851eb851eb852p-1)},
Field{"duration", Value::numeric(0x1.1eb851eb851ecp-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("lagrange-glass", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("hierarchical")},
Field{"seed", Value::integer(33425303LL)},
Field{"chaos", Value::numeric(0x1.eb851eb851eb8p-4)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.4cccccccccccdp-1)},
Field{"softening", Value::numeric(0x1.47ae147ae147bp-5)},
Field{"speed", Value::numeric(0x1.2666666666666p+0)},
Field{"escapePolicy", Value::text("prompt")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(5LL)},
Field{"scale", Value::text("blues")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(29LL)},
Field{"maximum", Value::integer(53LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.999999999999ap-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(5LL)},
Field{"scale", Value::text("blues")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(41LL)},
Field{"maximum", Value::integer(77LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.8f5c28f5c28f6p-1)},
Field{"duration", Value::numeric(0x1.70a3d70a3d70ap-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(5LL)},
Field{"scale", Value::text("blues")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(53LL)},
Field{"maximum", Value::integer(89LL)},
Field{"division", Value::numeric(0x1.0000000000000p-3)},
Field{"probability", Value::numeric(0x1.51eb851eb851fp-1)},
Field{"duration", Value::numeric(0x1.eb851eb851eb8p-4)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("twin-moon-relay", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("hierarchical")},
Field{"seed", Value::integer(33425304LL)},
Field{"chaos", Value::numeric(0x1.47ae147ae147bp-4)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.0a3d70a3d70a4p-1)},
Field{"softening", Value::numeric(0x1.eb851eb851eb8p-5)},
Field{"speed", Value::numeric(0x1.d70a3d70a3d71p-2)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(1LL)},
Field{"scale", Value::text("phrygian")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(25LL)},
Field{"maximum", Value::integer(61LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.d70a3d70a3d71p-1)},
Field{"duration", Value::numeric(0x1.8000000000000p+0)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(1LL)},
Field{"scale", Value::text("phrygian")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(37LL)},
Field{"maximum", Value::integer(73LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"duration", Value::numeric(0x1.ccccccccccccdp-1)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(1LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("plane")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(49LL)},
Field{"maximum", Value::integer(85LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.28f5c28f5c28fp-1)},
Field{"duration", Value::numeric(0x1.199999999999ap-1)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("quiet-periapsis", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("stable")},
Field{"seed", Value::integer(33425305LL)},
Field{"chaos", Value::numeric(0x1.70a3d70a3d70ap-3)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.ae147ae147ae1p-1)},
Field{"softening", Value::numeric(0x1.1eb851eb851ecp-5)},
Field{"speed", Value::numeric(0x1.9eb851eb851ecp+0)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("wholeTone")},
Field{"pitch", Value::text("plane")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(36LL)},
Field{"maximum", Value::integer(84LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.c28f5c28f5c29p-3)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("wholeTone")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(43LL)},
Field{"maximum", Value::integer(91LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.7ae147ae147aep-1)},
Field{"duration", Value::numeric(0x1.47ae147ae147bp-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("wholeTone")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(31LL)},
Field{"maximum", Value::integer(79LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.47ae147ae147bp-1)},
Field{"duration", Value::numeric(0x1.3d70a3d70a3d7p-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("cyan-transit", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("controlledChaos")},
Field{"seed", Value::integer(33425306LL)},
Field{"chaos", Value::numeric(0x1.5c28f5c28f5c3p-2)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.2e147ae147ae1p+0)},
Field{"softening", Value::numeric(0x1.c28f5c28f5c29p-5)},
Field{"speed", Value::numeric(0x1.147ae147ae148p+0)},
Field{"escapePolicy", Value::text("respawn")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(4LL)},
Field{"scale", Value::text("hungarianMinor")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(28LL)},
Field{"maximum", Value::integer(64LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.851eb851eb852p-2)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(4LL)},
Field{"scale", Value::text("hungarianMinor")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(40LL)},
Field{"maximum", Value::integer(76LL)},
Field{"division", Value::numeric(0x1.8000000000000p-2)},
Field{"probability", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"duration", Value::numeric(0x1.1eb851eb851ecp-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(4LL)},
Field{"scale", Value::text("diminished")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(52LL)},
Field{"maximum", Value::integer(88LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.a8f5c28f5c28fp-1)},
Field{"duration", Value::numeric(0x1.999999999999ap-3)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("ember-conjunction", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("stable")},
Field{"seed", Value::integer(33425307LL)},
Field{"chaos", Value::numeric(0x1.999999999999ap-4)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.d70a3d70a3d71p-2)},
Field{"softening", Value::numeric(0x1.47ae147ae147bp-5)},
Field{"speed", Value::numeric(0x1.147ae147ae148p-1)},
Field{"escapePolicy", Value::text("prompt")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(9LL)},
Field{"scale", Value::text("dorian")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(33LL)},
Field{"maximum", Value::integer(69LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.3333333333333p+0)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(9LL)},
Field{"scale", Value::text("dorian")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(45LL)},
Field{"maximum", Value::integer(81LL)},
Field{"division", Value::numeric(0x1.8000000000000p+0)},
Field{"probability", Value::numeric(0x1.c28f5c28f5c29p-1)},
Field{"duration", Value::numeric(0x1.e666666666666p-1)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(9LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(57LL)},
Field{"maximum", Value::integer(93LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.47ae147ae147bp-1)},
Field{"duration", Value::numeric(0x1.3d70a3d70a3d7p-1)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("violet-aphelion", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("controlledChaos")},
Field{"seed", Value::integer(33425308LL)},
Field{"chaos", Value::numeric(0x1.0a3d70a3d70a4p-1)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.4cccccccccccdp+0)},
Field{"softening", Value::numeric(0x1.1eb851eb851ecp-4)},
Field{"speed", Value::numeric(0x1.599999999999ap+0)},
Field{"escapePolicy", Value::text("respawn")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("chromatic")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(24LL)},
Field{"maximum", Value::integer(72LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.a3d70a3d70a3dp-1)},
Field{"duration", Value::numeric(0x1.70a3d70a3d70ap-3)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("chromatic")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(36LL)},
Field{"maximum", Value::integer(84LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.8f5c28f5c28f6p-1)},
Field{"duration", Value::numeric(0x1.47ae147ae147bp-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("chromatic")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(48LL)},
Field{"maximum", Value::integer(96LL)},
Field{"division", Value::numeric(0x1.0000000000000p-3)},
Field{"probability", Value::numeric(0x1.d70a3d70a3d71p-1)},
Field{"duration", Value::numeric(0x1.eb851eb851eb8p-4)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("red-shift-study", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("figureEight")},
Field{"seed", Value::integer(33425309LL)},
Field{"chaos", Value::numeric(0x1.eb851eb851eb8p-5)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.eb851eb851eb8p-1)},
Field{"softening", Value::numeric(0x1.999999999999ap-6)},
Field{"speed", Value::numeric(0x1.c28f5c28f5c29p-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(7LL)},
Field{"scale", Value::text("majorPentatonic")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(31LL)},
Field{"maximum", Value::integer(67LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.70a3d70a3d70ap-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(7LL)},
Field{"scale", Value::text("majorPentatonic")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(43LL)},
Field{"maximum", Value::integer(79LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.b851eb851eb85p-1)},
Field{"duration", Value::numeric(0x1.851eb851eb852p-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(7LL)},
Field{"scale", Value::text("majorPentatonic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(55LL)},
Field{"maximum", Value::integer(91LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.5c28f5c28f5c3p-1)},
Field{"duration", Value::numeric(0x1.ae147ae147ae1p-3)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("golden-barycenter", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("hierarchical")},
Field{"seed", Value::integer(33425310LL)},
Field{"chaos", Value::numeric(0x1.eb851eb851eb8p-3)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.8f5c28f5c28f6p-1)},
Field{"softening", Value::numeric(0x1.70a3d70a3d70ap-5)},
Field{"speed", Value::numeric(0x1.47ae147ae147bp-1)},
Field{"escapePolicy", Value::text("prompt")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(6LL)},
Field{"scale", Value::text("locrian")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(18LL)},
Field{"maximum", Value::integer(54LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.c28f5c28f5c29p-1)},
Field{"duration", Value::numeric(0x1.199999999999ap+0)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(6LL)},
Field{"scale", Value::text("locrian")},
Field{"pitch", Value::text("plane")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(30LL)},
Field{"maximum", Value::integer(66LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"duration", Value::numeric(0x1.51eb851eb851fp-1)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(6LL)},
Field{"scale", Value::text("diminished")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(42LL)},
Field{"maximum", Value::integer(78LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.3d70a3d70a3d7p-1)},
Field{"duration", Value::numeric(0x1.c28f5c28f5c29p-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("black-sun-threads", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("stable")},
Field{"seed", Value::integer(33425311LL)},
Field{"chaos", Value::numeric(0x1.1eb851eb851ecp-3)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.5c28f5c28f5c3p-1)},
Field{"softening", Value::numeric(0x1.eb851eb851eb8p-6)},
Field{"speed", Value::numeric(0x1.a3d70a3d70a3dp-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(3LL)},
Field{"scale", Value::text("naturalMinor")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(27LL)},
Field{"maximum", Value::integer(63LL)},
Field{"division", Value::numeric(0x1.8000000000000p-1)},
Field{"probability", Value::numeric(0x1.d70a3d70a3d71p-1)},
Field{"duration", Value::numeric(0x1.28f5c28f5c28fp-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(3LL)},
Field{"scale", Value::text("naturalMinor")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(39LL)},
Field{"maximum", Value::integer(75LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.5c28f5c28f5c3p-1)},
Field{"duration", Value::numeric(0x1.ae147ae147ae1p-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(3LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(51LL)},
Field{"maximum", Value::integer(87LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.7ae147ae147aep-1)},
Field{"duration", Value::numeric(0x1.1eb851eb851ecp-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("orbital-ink", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("controlledChaos")},
Field{"seed", Value::integer(33425312LL)},
Field{"chaos", Value::numeric(0x1.5c28f5c28f5c3p-1)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.75c28f5c28f5cp+0)},
Field{"softening", Value::numeric(0x1.47ae147ae147bp-4)},
Field{"speed", Value::numeric(0x1.d1eb851eb851fp+0)},
Field{"escapePolicy", Value::text("respawn")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("diminished")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(24LL)},
Field{"maximum", Value::integer(72LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.eb851eb851eb8p-3)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("diminished")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(36LL)},
Field{"maximum", Value::integer(84LL)},
Field{"division", Value::numeric(0x1.8000000000000p-3)},
Field{"probability", Value::numeric(0x1.8000000000000p-1)},
Field{"duration", Value::numeric(0x1.1eb851eb851ecp-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("diminished")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(48LL)},
Field{"maximum", Value::integer(96LL)},
Field{"division", Value::numeric(0x1.0000000000000p-3)},
Field{"probability", Value::numeric(0x1.6666666666666p-1)},
Field{"duration", Value::numeric(0x1.999999999999ap-4)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("slingshot-cadence", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("figureEight")},
Field{"seed", Value::integer(33425313LL)},
Field{"chaos", Value::numeric(0x1.eb851eb851eb8p-6)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.c28f5c28f5c29p-1)},
Field{"softening", Value::numeric(0x1.eb851eb851eb8p-6)},
Field{"speed", Value::numeric(0x1.851eb851eb852p-2)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(5LL)},
Field{"scale", Value::text("mixolydian")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(29LL)},
Field{"maximum", Value::integer(65LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.ccccccccccccdp+0)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(5LL)},
Field{"scale", Value::text("mixolydian")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(41LL)},
Field{"maximum", Value::integer(77LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.a3d70a3d70a3dp-1)},
Field{"duration", Value::numeric(0x1.3333333333333p+0)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(5LL)},
Field{"scale", Value::text("majorPentatonic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(53LL)},
Field{"maximum", Value::integer(89LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.147ae147ae148p-1)},
Field{"duration", Value::numeric(0x1.ccccccccccccdp-1)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("pale-resonance", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("hierarchical")},
Field{"seed", Value::integer(33425314LL)},
Field{"chaos", Value::numeric(0x1.3333333333333p-2)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.0cccccccccccdp+0)},
Field{"softening", Value::numeric(0x1.999999999999ap-5)},
Field{"speed", Value::numeric(0x1.3d70a3d70a3d7p+0)},
Field{"escapePolicy", Value::text("respawn")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(10LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(22LL)},
Field{"maximum", Value::integer(58LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.ae147ae147ae1p-2)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(10LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(46LL)},
Field{"maximum", Value::integer(82LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.a3d70a3d70a3dp-1)},
Field{"duration", Value::numeric(0x1.999999999999ap-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(10LL)},
Field{"scale", Value::text("blues")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(58LL)},
Field{"maximum", Value::integer(94LL)},
Field{"division", Value::numeric(0x1.0000000000000p-3)},
Field{"probability", Value::numeric(0x1.51eb851eb851fp-1)},
Field{"duration", Value::numeric(0x1.0a3d70a3d70a4p-3)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("magenta-eclipse", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("stable")},
Field{"seed", Value::integer(33425315LL)},
Field{"chaos", Value::numeric(0x1.1eb851eb851ecp-4)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.3333333333333p-1)},
Field{"softening", Value::numeric(0x1.1eb851eb851ecp-5)},
Field{"speed", Value::numeric(0x1.851eb851eb852p-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("lydian")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(36LL)},
Field{"maximum", Value::integer(72LL)},
Field{"division", Value::numeric(0x1.8000000000000p-1)},
Field{"probability", Value::numeric(0x1.b851eb851eb85p-1)},
Field{"duration", Value::numeric(0x1.147ae147ae148p-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("lydian")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(43LL)},
Field{"maximum", Value::integer(79LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.8f5c28f5c28f6p-1)},
Field{"duration", Value::numeric(0x1.851eb851eb852p-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("lydian")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(50LL)},
Field{"maximum", Value::integer(86LL)},
Field{"division", Value::numeric(0x1.8000000000000p-2)},
Field{"probability", Value::numeric(0x1.6666666666666p-1)},
Field{"duration", Value::numeric(0x1.147ae147ae148p-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("three-cold-lights", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("hierarchical")},
Field{"seed", Value::integer(33425316LL)},
Field{"chaos", Value::numeric(0x1.47ae147ae147bp-5)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.d70a3d70a3d71p-1)},
Field{"softening", Value::numeric(0x1.999999999999ap-6)},
Field{"speed", Value::numeric(0x1.eb851eb851eb8p-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(26LL)},
Field{"maximum", Value::integer(50LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.7ae147ae147aep-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(38LL)},
Field{"maximum", Value::integer(74LL)},
Field{"division", Value::numeric(0x1.8000000000000p-2)},
Field{"probability", Value::numeric(0x1.ae147ae147ae1p-1)},
Field{"duration", Value::numeric(0x1.eb851eb851eb8p-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(50LL)},
Field{"maximum", Value::integer(86LL)},
Field{"division", Value::numeric(0x1.4000000000000p-2)},
Field{"probability", Value::numeric(0x1.5c28f5c28f5c3p-1)},
Field{"duration", Value::numeric(0x1.851eb851eb852p-3)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("blue-giant-clock", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("controlledChaos")},
Field{"seed", Value::integer(33425317LL)},
Field{"chaos", Value::numeric(0x1.d70a3d70a3d71p-2)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.1eb851eb851ecp+0)},
Field{"softening", Value::numeric(0x1.70a3d70a3d70ap-4)},
Field{"speed", Value::numeric(0x1.7ae147ae147aep+0)},
Field{"escapePolicy", Value::text("respawn")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("chromatic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(30LL)},
Field{"maximum", Value::integer(66LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.e147ae147ae14p-1)},
Field{"duration", Value::numeric(0x1.eb851eb851eb8p-4)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("chromatic")},
Field{"pitch", Value::text("plane")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(42LL)},
Field{"maximum", Value::integer(78LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"duration", Value::numeric(0x1.70a3d70a3d70ap-4)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("chromatic")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(54LL)},
Field{"maximum", Value::integer(90LL)},
Field{"division", Value::numeric(0x1.0000000000000p-3)},
Field{"probability", Value::numeric(0x1.eb851eb851eb8p-2)},
Field{"duration", Value::numeric(0x1.1eb851eb851ecp-4)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("perihelion-dust", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("figureEight")},
Field{"seed", Value::integer(33425318LL)},
Field{"chaos", Value::numeric(0x1.70a3d70a3d70ap-4)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.7ae147ae147aep-1)},
Field{"softening", Value::numeric(0x1.47ae147ae147bp-5)},
Field{"speed", Value::numeric(0x1.0000000000000p-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("blues")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(24LL)},
Field{"maximum", Value::integer(60LL)},
Field{"division", Value::numeric(0x1.0000000000000p+1)},
Field{"probability", Value::numeric(0x1.3d70a3d70a3d7p-1)},
Field{"duration", Value::numeric(0x1.6666666666666p+0)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(36LL)},
Field{"maximum", Value::integer(72LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.0a3d70a3d70a4p-1)},
Field{"duration", Value::numeric(0x1.0000000000000p+0)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("plane")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(48LL)},
Field{"maximum", Value::integer(84LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.851eb851eb852p-2)},
Field{"duration", Value::numeric(0x1.6666666666666p-1)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("silent-conjunction", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("stable")},
Field{"seed", Value::integer(33425319LL)},
Field{"chaos", Value::numeric(0x1.851eb851eb852p-2)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.0000000000000p+0)},
Field{"softening", Value::numeric(0x1.eb851eb851eb8p-5)},
Field{"speed", Value::numeric(0x1.4cccccccccccdp+0)},
Field{"escapePolicy", Value::text("prompt")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(4LL)},
Field{"scale", Value::text("phrygian")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(28LL)},
Field{"maximum", Value::integer(64LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.47ae147ae147bp-2)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(4LL)},
Field{"scale", Value::text("phrygian")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(40LL)},
Field{"maximum", Value::integer(76LL)},
Field{"division", Value::numeric(0x1.8000000000000p-2)},
Field{"probability", Value::numeric(0x1.999999999999ap-1)},
Field{"duration", Value::numeric(0x1.c28f5c28f5c29p-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(4LL)},
Field{"scale", Value::text("diminished")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(52LL)},
Field{"maximum", Value::integer(88LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.6666666666666p-1)},
Field{"duration", Value::numeric(0x1.47ae147ae147bp-3)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("iron-triangle", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("hierarchical")},
Field{"seed", Value::integer(33425320LL)},
Field{"chaos", Value::numeric(0x1.999999999999ap-3)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.6666666666666p-1)},
Field{"softening", Value::numeric(0x1.70a3d70a3d70ap-5)},
Field{"speed", Value::numeric(0x1.ccccccccccccdp-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("major")},
Field{"pitch", Value::text("plane")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(24LL)},
Field{"maximum", Value::integer(60LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.e147ae147ae14p-1)},
Field{"duration", Value::numeric(0x1.47ae147ae147bp-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("major")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(43LL)},
Field{"maximum", Value::integer(79LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.851eb851eb852p-1)},
Field{"duration", Value::numeric(0x1.70a3d70a3d70ap-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("majorPentatonic")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(55LL)},
Field{"maximum", Value::integer(91LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.6666666666666p-1)},
Field{"duration", Value::numeric(0x1.3333333333333p-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("solar-kite", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("controlledChaos")},
Field{"seed", Value::integer(33425321LL)},
Field{"chaos", Value::numeric(0x1.1eb851eb851ecp-2)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.b851eb851eb85p+0)},
Field{"softening", Value::numeric(0x1.eb851eb851eb8p-4)},
Field{"speed", Value::numeric(0x1.28f5c28f5c28fp-1)},
Field{"escapePolicy", Value::text("respawn")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(1LL)},
Field{"scale", Value::text("naturalMinor")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(16LL)},
Field{"maximum", Value::integer(52LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.3333333333333p+0)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(1LL)},
Field{"scale", Value::text("naturalMinor")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(28LL)},
Field{"maximum", Value::integer(64LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.a3d70a3d70a3dp-1)},
Field{"duration", Value::numeric(0x1.8f5c28f5c28f6p-1)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(1LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(40LL)},
Field{"maximum", Value::integer(76LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.5c28f5c28f5c3p-1)},
Field{"duration", Value::numeric(0x1.eb851eb851eb8p-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("gravity-well", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("figureEight")},
Field{"seed", Value::integer(33425322LL)},
Field{"chaos", Value::numeric(0x1.eb851eb851eb8p-4)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.ccccccccccccdp-1)},
Field{"softening", Value::numeric(0x1.eb851eb851eb8p-6)},
Field{"speed", Value::numeric(0x1.51eb851eb851fp-1)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("dorian")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(26LL)},
Field{"maximum", Value::integer(62LL)},
Field{"division", Value::numeric(0x1.0000000000000p+0)},
Field{"probability", Value::numeric(0x1.ccccccccccccdp-1)},
Field{"duration", Value::numeric(0x1.a3d70a3d70a3dp-1)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("dorian")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(38LL)},
Field{"maximum", Value::integer(74LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"duration", Value::numeric(0x1.1eb851eb851ecp-1)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(2LL)},
Field{"scale", Value::text("minorPentatonic")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("turning")},
Field{"minimum", Value::integer(50LL)},
Field{"maximum", Value::integer(86LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.47ae147ae147bp-1)},
Field{"duration", Value::numeric(0x1.851eb851eb852p-2)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("spiral-archive", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("controlledChaos")},
Field{"seed", Value::integer(33425323LL)},
Field{"chaos", Value::numeric(0x1.a3d70a3d70a3dp-1)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.6147ae147ae14p+0)},
Field{"softening", Value::numeric(0x1.999999999999ap-4)},
Field{"speed", Value::numeric(0x1.8f5c28f5c28f6p+0)},
Field{"escapePolicy", Value::text("respawn")},
Field{"loopPolicy", Value::text("continue")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(6LL)},
Field{"scale", Value::text("hungarianMinor")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("close")},
Field{"minimum", Value::integer(24LL)},
Field{"maximum", Value::integer(72LL)},
Field{"division", Value::numeric(0x1.8000000000000p-2)},
Field{"probability", Value::numeric(0x1.0000000000000p+0)},
Field{"duration", Value::numeric(0x1.3333333333333p-2)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(6LL)},
Field{"scale", Value::text("hungarianMinor")},
Field{"pitch", Value::text("speed")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(36LL)},
Field{"maximum", Value::integer(84LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.999999999999ap-1)},
Field{"duration", Value::numeric(0x1.999999999999ap-3)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(6LL)},
Field{"scale", Value::text("diminished")},
Field{"pitch", Value::text("plane")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(48LL)},
Field{"maximum", Value::integer(96LL)},
Field{"division", Value::numeric(0x1.8000000000000p-3)},
Field{"probability", Value::numeric(0x1.70a3d70a3d70ap-1)},
Field{"duration", Value::numeric(0x1.1eb851eb851ecp-3)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("unstable-crown", Object(item), Object(physics), voices));
}
{
const std::array<Field, 3> item{{
Field{"system", Value::text("stable")},
Field{"seed", Value::integer(33425324LL)},
Field{"chaos", Value::numeric(0x1.47ae147ae147bp-6)}
}};
const std::array<Field, 5> physics{{
Field{"gravity", Value::numeric(0x1.28f5c28f5c28fp-1)},
Field{"softening", Value::numeric(0x1.999999999999ap-6)},
Field{"speed", Value::numeric(0x1.0000000000000p+0)},
Field{"escapePolicy", Value::text("leave")},
Field{"loopPolicy", Value::text("restart")}
}};
const std::array<Field, 10> voice0{{
Field{"channel", Value::integer(1LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("wholeTone")},
Field{"pitch", Value::text("radius")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(48LL)},
Field{"maximum", Value::integer(84LL)},
Field{"division", Value::numeric(0x1.0000000000000p-1)},
Field{"probability", Value::numeric(0x1.d70a3d70a3d71p-1)},
Field{"duration", Value::numeric(0x1.70a3d70a3d70ap-2)}
}};
const std::array<Field, 10> voice1{{
Field{"channel", Value::integer(2LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("wholeTone")},
Field{"pitch", Value::text("phase")},
Field{"trigger", Value::text("clock")},
Field{"minimum", Value::integer(55LL)},
Field{"maximum", Value::integer(91LL)},
Field{"division", Value::numeric(0x1.8000000000000p-2)},
Field{"probability", Value::numeric(0x1.8f5c28f5c28f6p-1)},
Field{"duration", Value::numeric(0x1.0a3d70a3d70a4p-2)}
}};
const std::array<Field, 10> voice2{{
Field{"channel", Value::integer(3LL)},
Field{"root", Value::integer(0LL)},
Field{"scale", Value::text("wholeTone")},
Field{"pitch", Value::text("nearest")},
Field{"trigger", Value::text("plane")},
Field{"minimum", Value::integer(60LL)},
Field{"maximum", Value::integer(96LL)},
Field{"division", Value::numeric(0x1.0000000000000p-2)},
Field{"probability", Value::numeric(0x1.47ae147ae147bp-1)},
Field{"duration", Value::numeric(0x1.70a3d70a3d70ap-3)}
}};
const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};
result.push_back(readFixture("white-noon", Object(item), Object(physics), voices));
}
return result;
}
} // namespace diagnostic
