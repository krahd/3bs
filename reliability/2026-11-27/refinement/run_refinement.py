#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo
"""Reproduce descriptive MIDI refinement measurements without editing the product.

Python strictly parses the actual factory JSON. The generated C++ reuses the
canonical catalogue's musical enum functions and assignments, with only its
string/array access adapted to portable, already validated scalar views.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import platform
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

EXPECTED_CATALOG_SHA256 = "887fad8252e1e96cd2872247a50a68c533e7813a0209c660b0b36b0635e1f1a3"
FUNCTIONS = {
    "initialSystem": ("InitialSystem", "figureEight", "FigureEight"),
    "escapePolicy": ("EscapePolicy", "leave", "Leave"),
    "scale": ("ScaleId", "minorPentatonic", "MinorPentatonic"),
    "pitchMapping": ("PitchMapping", "radius", "BarycentricRadius"),
    "triggerMapping": ("TriggerMapping", "clock", "Clock"),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path):
    return json.loads(path.read_text(), object_pairs_hook=no_duplicates,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def keys(value, required, optional=()):
    require(isinstance(value, dict), "Expected JSON object")
    require(set(required) <= value.keys(), f"Missing keys: {set(required) - value.keys()}")
    require(value.keys() <= set(required) | set(optional),
            f"Unmapped keys: {value.keys() - set(required) - set(optional)}")


def number(value, label):
    require(type(value) in (int, float) and math.isfinite(value), f"Invalid number: {label}")


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for end in range(opening, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                return source[start:end + 1]
    raise ValueError(f"Unterminated canonical block: {marker}")


def enum_members(types, name):
    match = re.search(r"enum class " + name + r"\s*:\s*std::uint8_t\s*\{([^}]+)\}", types)
    require(match is not None, f"Missing canonical enum {name}")
    members = [entry.strip() for entry in match[1].split(",") if entry.strip()]
    require(all(re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", member) for member in members),
            f"Explicit/changed enum layout requires review: {name}")
    return {member: index for index, member in enumerate(members)}


def canonical_mapping(catalogue, types):
    snippets, mappings = [], {}
    for function, (enum, fallback_token, fallback_member) in FUNCTIONS.items():
        original = block(catalogue, f"{enum} {function}(const juce::String& value)")
        pairs = re.findall(r'if \(value == "([A-Za-z0-9]+)"\) return ' + enum + r'::([A-Za-z0-9]+);', original)
        require(original.rstrip().endswith(f"return {enum}::{fallback_member};\n}}"),
                f"Canonical fallback changed: {function}")
        members = enum_members(types, enum)
        mapping = {token: members[member] for token, member in pairs}
        mapping[fallback_token] = members[fallback_member]
        mappings[function] = mapping
        snippets.append(original.replace("juce::String", "SourceText"))
    return snippets, mappings


def validate_document(document, mappings):
    keys(document, {"schemaVersion", "presets"})
    require(type(document["schemaVersion"]) is int and document["schemaVersion"] == 2,
            "This diagnostic validates the actual published factory schema 2")
    presets = document["presets"]
    require(isinstance(presets, list) and len(presets) == 24, "Expected exactly 24 factory systems")
    require(len({preset["id"] for preset in presets}) == 24, "Duplicate preset ID")
    for preset in presets:
        keys(preset, {"id", "name", "description", "system", "seed", "chaos", "physics", "voices", "visual", "camera"})
        require(re.fullmatch(r"[a-z0-9-]+", preset["id"]) is not None, "Unsafe preset ID")
        for field in ("name", "description", "system"):
            require(isinstance(preset[field], str), f"Invalid string {field}")
        require(preset["system"] in mappings["initialSystem"], "Unrecognized system token")
        require(type(preset["seed"]) is int and 0 <= preset["seed"] <= 2**63 - 1,
                "Factory seed must match canonical signed-64 extraction")
        number(preset["chaos"], "chaos")
        physics = preset["physics"]
        keys(physics, {"gravity", "softening", "speed", "escapePolicy", "loopPolicy"})
        for field in ("gravity", "softening", "speed"):
            number(physics[field], field)
        require(physics["escapePolicy"] in mappings["escapePolicy"], "Unrecognized escape policy")
        require(physics["loopPolicy"] in {"restart", "continue"}, "Unrecognized loop policy")
        require(isinstance(preset["voices"], list) and len(preset["voices"]) == 3, "Expected three voices")
        for voice in preset["voices"]:
            keys(voice, {"channel", "root", "scale", "pitch", "trigger", "minimum", "maximum", "division", "probability", "duration"},
                 {"octave", "durationMin", "durationMax", "durationMap"})
            for field, function in (("scale", "scale"), ("pitch", "pitchMapping"), ("trigger", "triggerMapping"), ("durationMap", "pitchMapping")):
                if field in voice:
                    require(voice[field] in mappings[function], f"Unrecognized {field} token")
            for field, value in voice.items():
                if field not in {"scale", "pitch", "trigger", "durationMap"}:
                    number(value, field)
            for field in ("channel", "root", "minimum", "maximum", "octave"):
                if field in voice:
                    require(type(voice[field]) is int, f"Expected integral {field}")
            require(1 <= voice["channel"] <= 16 and 0 <= voice["root"] <= 11, "Invalid channel/root")
            require(0 <= voice["minimum"] <= voice["maximum"] <= 127, "Invalid note range")
            require(0 <= voice["probability"] <= 1, "Invalid probability")
        # These blocks are retained/hashed but cannot affect a core-only MIDI run.
        keys(preset["visual"], {"palette", "trailSeconds", "trailWidth", "extrusion", "bloom", "stars"})
        keys(preset["camera"], {"focus", "distance", "yaw", "pitch", "autoOrbit"})
        for name in ("visual", "camera"):
            for field, value in preset[name].items():
                if field not in {"palette", "focus"}:
                    number(value, field)
    return presets


SUPPORT = r'''
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
'''


def generate(repo, output, presets, snippets, catalogue):
    constructor = block(catalogue, "PresetCatalog::PresetCatalog()")
    initial = constructor[constructor.index("preset.system ="):constructor.index("const auto physics =")]
    physical = constructor[constructor.index("preset.simulation.gravitationalConstant ="):constructor.index("auto* voices =")]
    voices = block(constructor, "for (std::size_t index = 0; index < bodyCount; ++index)")
    require(voices.count("voices->getReference(static_cast<int>(index))") == 1, "Changed canonical array access")
    portable_voices = voices.replace("voices->getReference(static_cast<int>(index))", "inputVoices[index]")
    canonical = "\n".join(snippets) + "\nFixture readFixture(std::string_view id, Object item, Object physics, const std::array<Object, bodyCount>& inputVoices) {\nFixture preset; preset.id = id;\n"
    canonical += initial + physical + portable_voices + "\nreturn preset;\n}\n"
    header = SUPPORT + canonical + "std::vector<Fixture> makeFixtures() {\nstd::vector<Fixture> result; result.reserve(24);\n"

    def fields(name, values):
        lines = []
        for key, value in values.items():
            if isinstance(value, str):
                expression = "Value::text(" + json.dumps(value, ensure_ascii=True) + ")"
            elif type(value) is int:
                expression = f"Value::integer({value}LL)"
            else:
                expression = f"Value::numeric({float(value).hex()})"
            lines.append("Field{" + json.dumps(key) + ", " + expression + "}")
        return f"const std::array<Field, {len(lines)}> {name}" + "{{\n" + ",\n".join(lines) + "\n}};\n"

    for preset in presets:
        header += "{\n"
        header += fields("item", {key: preset[key] for key in ("system", "seed", "chaos")})
        header += fields("physics", preset["physics"])
        for index, voice in enumerate(preset["voices"]):
            header += fields(f"voice{index}", voice)
        header += 'const std::array<Object, bodyCount> voices{Object(voice0), Object(voice1), Object(voice2)};\n'
        header += "result.push_back(readFixture(" + json.dumps(preset["id"]) + ", Object(item), Object(physics), voices));\n}\n"
    header += "return result;\n}\n} // namespace diagnostic\n"
    (output / "GeneratedFixtures.h").write_text(header)
    (output / "canonical-musical-mapping.cpp.txt").write_text(canonical)
    return {
        "canonical_source": "src/ui/PresetCatalog.cpp",
        "canonical_sha256": sha256(repo / "src/ui/PresetCatalog.cpp"),
        "transformations": ["juce::String parameter type becomes scalar SourceText", "validated JSON scalar views implement property/text/number", "canonical voice-array access becomes inputVoices[index]"],
        "unchanged_logic": "Five enum functions; initial-system/seed/chaos, physics/loop-policy and all factory voice assignments",
        "defaults": "Real SimulationConfig, VoiceConfig and EngineConfig defaults from the frozen product headers",
        "visual_camera": "Validated, retained in input digest, excluded from the core-only diagnostic",
    }


def validate_effective(presets, effective, mappings):
    require(len(effective) == 24, "Missing effective configuration")
    for preset, actual in zip(presets, effective, strict=True):
        require(actual["id"] == preset["id"] and actual["seed"] == preset["seed"] and actual["chaos"] == preset["chaos"], "Initial data mapping differs")
        require(actual["system"] == mappings["initialSystem"][preset["system"]], "System enum mapping differs")
        require(actual["voicing_mode"] == 0, "Expected fresh Independent mode, with no voicing overlay")
        require(actual["loop_policy"] == (preset["physics"]["loopPolicy"] == "continue"), "Loop mapping differs")
        for field in ("gravity", "softening", "speed"):
            require(actual["simulation"][field] == preset["physics"][field], f"Physics mapping differs: {field}")
        require(actual["simulation"]["escape_policy"] == mappings["escapePolicy"][preset["physics"]["escapePolicy"]], "Escape mapping differs")
        for source, voice in zip(preset["voices"], actual["voices"], strict=True):
            for field in ("channel", "root", "minimum", "maximum", "division", "probability"):
                require(voice[field] == source[field], f"Voice mapping differs: {field}")
            for field, function in (("scale", "scale"), ("pitch", "pitchMapping"), ("trigger", "triggerMapping")):
                require(voice[field] == mappings[function][source[field]], f"Enum mapping differs: {field}")
            require(voice["octave"] == source.get("octave", 0), "Octave mapping differs")
            require(voice["duration_min"] == source.get("durationMin", source["duration"]), "Duration minimum differs")
            require(voice["duration_max"] == source.get("durationMax", source["duration"]), "Duration maximum differs")
            require(voice["duration_grid"] == 0, "Factory schema2 must retain ContinuousLegacy duration")
            if "durationMap" in source:
                require(voice["duration_map"] == mappings["pitchMapping"][source["durationMap"]], "Duration mapping differs")


def read_trace(path):
    rows = []
    with path.open(newline="") as source:
        for row in csv.DictReader(source):
            rows.append({key: value if key == "phase" else int(value) for key, value in row.items()})
    return rows


def pairs(rows, body):
    active = None
    notes = []
    for row in rows:
        if row["source_body"] != body or row["type"] == 2:
            continue
        if row["type"] == 0:
            require(active is None, "Overlapping note ownership within one source body")
            active = row
        else:
            require(active is not None and (active["channel"], active["data1"]) == (row["channel"], row["data1"]), "Mismatched receiving note identity")
            notes.append((active, row))
            active = None
    require(active is None, "Unclosed note after cleanup")
    return notes


def absolute_statistics(values):
    ordered = sorted(abs(value) for value in values)
    if not ordered:
        return {"pairs": 0, "changed": 0, "max_samples": 0, "p50_samples": 0, "p95_samples": 0}
    return {"pairs": len(values), "changed": sum(value != 0 for value in values),
            "max_samples": ordered[-1], "p50_samples": ordered[(len(ordered) - 1) * 50 // 100],
            "p95_samples": ordered[(len(ordered) - 1) * 95 // 100]}


def compare(preset, output, runs):
    files = [output / "traces" / f"{preset['id']}.{step}.csv" for step in ("published", "half-step")]
    traces = [read_trace(path) for path in files]
    common = 0
    for first, second in zip(*traces):
        if first != second:
            break
        common += 1
    same = traces[0] == traces[1]
    voices = []
    for body in range(3):
        notes = [pairs(trace, body) for trace in traces]
        on_deltas, off_deltas = [], []
        pitch_changes = velocity_changes = channel_changes = duration_changes = 0
        for first, second in zip(*notes):
            on_deltas.append(second[0]["absolute_sample"] - first[0]["absolute_sample"])
            off_deltas.append(second[1]["absolute_sample"] - first[1]["absolute_sample"])
            pitch_changes += first[0]["data1"] != second[0]["data1"]
            velocity_changes += first[0]["data2"] != second[0]["data2"]
            channel_changes += first[0]["channel"] != second[0]["channel"]
            duration_changes += first[1]["absolute_sample"] - first[0]["absolute_sample"] != second[1]["absolute_sample"] - second[0]["absolute_sample"]
        voices.append({"body": body, "notes_published": len(notes[0]), "notes_half_step": len(notes[1]),
                       "ordinal_paired_notes": len(on_deltas), "ordinal_pitch_changes": pitch_changes,
                       "ordinal_velocity_changes": velocity_changes, "ordinal_channel_changes": channel_changes,
                       "ordinal_duration_changes": duration_changes,
                       "ordinal_onset_difference": absolute_statistics(on_deltas),
                       "ordinal_offset_difference": absolute_statistics(off_deltas)})
    run_pair = [runs[preset["id"], name] for name in ("published", "half-step")]
    state_difference = max(math.dist(first[field], second[field])
                           for first, second in zip(run_pair[0]["final_bodies"], run_pair[1]["final_bodies"], strict=True)
                           for field in ("position", "velocity"))
    return {"preset": preset["id"], "same_complete_trace": same,
            "events_published": len(traces[0]), "events_half_step": len(traces[1]),
            "matching_prefix_events": common,
            "first_difference": None if same else {"zero_based_ordinal": common,
                "published": traces[0][common] if common < len(traces[0]) else None,
                "half_step": traces[1][common] if common < len(traces[1]) else None},
            "trace_sha256": {"published": sha256(files[0]), "half_step": sha256(files[1])},
            "max_final_position_or_velocity_difference": state_difference,
            "published_policy_respawns": [sum(body["respawns"] for body in run["final_bodies"]) for run in run_pair],
            "voices": voices,
            "cleanup_passed_both": all(run["passed"] and run["active_after_cleanup"] == 0 for run in run_pair)}


def write_report(output, provenance, comparisons):
    changes = sum(not item["same_complete_trace"] for item in comparisons)
    changed_counts = sum(any(voice["notes_published"] != voice["notes_half_step"] for voice in item["voices"]) for item in comparisons)
    pitch_changes = sum(any(voice["ordinal_pitch_changes"] for voice in item["voices"]) for item in comparisons)
    lines = ["# Factory-preset musical refinement diagnostic", "",
             f"Declared source checkpoint: `{provenance['declared_source_revision']}`. Exact file and compiler hashes are in `provenance.json`.", "",
             "## Method", "",
             "The diagnostic uses the unchanged product core and all 24 schema-2 factory presets. A strict Python JSON reader rejects duplicate, missing, unknown and nonfinite data. Generated portable fixture code reuses the canonical catalogue's enum functions and musical assignment blocks; the catalogue source hash is pinned. Real product configuration defaults remain in force. Every factory-mapped musical field is recorded, and present factory fields are checked against the compiled configuration before the result is accepted.", "",
             "Each preset starts from its own published system/seed/chaos with a fresh Independent-mode engine and no prior voicing overlay, transpose, input gate, tilt or auto-reset. Both runs use 120 BPM, 48 kHz, 4/4, 512-sample blocks and exactly 60 beats (30 seconds, 1,440,000 samples). One run uses the published 1/480 step; the diagnostic copy alone uses 1/960. Published escape/respawn policy is retained. This measures fixed-step sensitivity; it is not an old-versus-repaired-build comparison.", "",
             "The synchronous sink writes into bounded per-block diagnostic storage; CSV output occurs after processing. Each row records phase, absolute sample, type (0 NoteOn / 1 NoteOff / 2 CC), channel, note/controller, velocity/value and source body. Cleanup at the exclusive end sample is separately tagged. Integrity checks require finite state, bounded work, ordered/valid events, no sink rejection or unmatched offs, and an empty final receiving ledger.", "",
             "## Results", "",
             f"All 48 runs passed those integrity checks. {changes}/24 presets changed at least one complete trace entry when the step was halved; {changed_counts}/24 changed at least one per-voice note count; {pitch_changes}/24 changed at least one ordinal-paired pitch. These counts describe observed sensitivity, not failure rates or release tolerances.", "",
             "| Preset | Notes h / h÷2 | Events h / h÷2 | First differing sample h / h÷2 | Ordinal pitch changes | Respawns h / h÷2 | Final state difference |",
             "|---|---:|---:|---|---:|---:|---:|"]
    for item in comparisons:
        notes = [sum(voice[key] for voice in item["voices"]) for key in ("notes_published", "notes_half_step")]
        first = item["first_difference"]
        first_text = "identical" if first is None else " / ".join(str(first[key]["absolute_sample"]) if first[key] else "end" for key in ("published", "half_step"))
        lines.append(f"| {item['preset']} | {notes[0]} / {notes[1]} | {item['events_published']} / {item['events_half_step']} | {first_text} | {sum(v['ordinal_pitch_changes'] for v in item['voices'])} | {item['published_policy_respawns'][0]} / {item['published_policy_respawns'][1]} | {item['max_final_position_or_velocity_difference']:.6g} |")
    lines += ["", "Final state difference is the largest Euclidean position or velocity difference across the three bodies; position and velocity have different normalized units, so the value is only a trajectory divergence diagnostic. Full final states, separate per-voice onset/offset distributions, counts, first differing rows and trace hashes are retained in `runs.jsonl` and `comparisons.json`.", "",
              "## Interpretation and limits", "",
              "- Pitch/velocity and onset/offset summaries pair the nth note of each source body with the nth note in the other run. If a trigger is added or removed, later pairs need not identify the same physical event; large ordinal differences are not scheduling-jitter estimates. Full CSV traces permit alternate matching without rerunning the simulation.",
              "- Small numerical trajectory changes can cross discrete musical thresholds or alter trigger/PRNG consumption; later note sequences can diverge. This result neither establishes a convergence order for quantized MIDI nor implies that one sequence is artistically preferable.",
              "- Published-policy respawns can separate trajectories discontinuously. Conservative energy/momentum, sampled separation and half-step position diagnostics already exist for every preset in [numerical-presets.log](../core/numerical-presets.log); their conservative runs must not be conflated with the published-policy music runs here.",
              "- The measurement covers the 24 complete factory systems in their fresh Independent mode, not every combination with the 12 separate voicing presets, every automation/edit, or all accepted extremes. Existing synthetic chord/strum partition and performance tests cover different questions.",
              "- CSV and JSON work are intentionally outside the callback. This is a portable musical/numerical diagnostic, not a processing benchmark, real-time host test, physical MIDI measurement or combined GPU/UI stress session.",
              "- No product source, preset, physical default, musical threshold, release tolerance or CI requirement was changed. The diagnostic reports sensitivity and supplies traceable references for any later compatibility decision.", "",
              "## Reproduce", "", "```sh", "python3 run_refinement.py --repo /absolute/path/to/3bs --source-ref " + provenance["declared_source_revision"] + " --output /absolute/path/to/refinement-results", "```", "",
              "The generator stops if the canonical catalogue source changes; review the extraction before updating its expected hash. `provenance.json` records exact commands, environment and SHA-256/git-blob digests. `build.log` and `run.log` retain execution output. `GeneratedFixtures.h` and the exact adapted canonical mapping are retained for inspection.", ""]
    (output / "REPORT.md").write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--source-ref", required=True)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    repo, output = args.repo.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    catalogue_path = repo / "src/ui/PresetCatalog.cpp"
    require(sha256(catalogue_path) == EXPECTED_CATALOG_SHA256,
            "Canonical catalogue changed: review extraction and mappings before rerunning")
    catalogue = catalogue_path.read_text()
    types = (repo / "src/core/Types.h").read_text()
    snippets, mappings = canonical_mapping(catalogue, types)
    document = load_json(repo / "resources/presets/factory-presets.json")
    presets = validate_document(document, mappings)
    mapping = generate(repo, output, presets, snippets, catalogue)
    mapping["enum_mappings_derived_from_source"] = mappings
    mapping["input_schema"] = document["schemaVersion"]
    mapping["preset_ids"] = [preset["id"] for preset in presets]
    (output / "mapping-validation.json").write_text(json.dumps(mapping, indent=2) + "\n")
    source_files = sorted((repo / "src/core").glob("*"))
    source_files = [path for path in source_files if path.suffix in {".cpp", ".h"}]
    source_files += [catalogue_path, repo / "src/ui/PresetCatalog.h", repo / "resources/presets/factory-presets.json"]
    def source_digest():
        return {str(path.relative_to(repo)): {"sha256": sha256(path), "git_blob_sha1": hashlib.sha1(b"blob " + str(path.stat().st_size).encode() + b"\0" + path.read_bytes()).hexdigest()} for path in source_files}
    before = source_digest()
    harness = Path(__file__).resolve().with_name("Refinement.cpp")
    binary = output / "refinement"
    command = [args.compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow",
               "-I", str(repo / "src"), "-I", str(output), str(harness),
               *(str(repo / name) for name in ("src/core/MusicEngine.cpp", "src/core/ThreeBodySimulation.cpp", "src/core/Scale.cpp")),
               "-o", str(binary)]
    compiler = subprocess.run([args.compiler, "--version"], capture_output=True, text=True, check=True).stdout
    provenance = {"declared_source_revision": args.source_ref, "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                  "platform": platform.platform(), "machine": platform.machine(), "python": sys.version,
                  "compiler": compiler, "build_command": command, "run_command": [str(binary), str(output)],
                  "product_sources": before, "generator_sha256": sha256(Path(__file__).resolve()),
                  "harness_sha256": sha256(harness), "generated_header_sha256": sha256(output / "GeneratedFixtures.h"),
                  "measurement": {"beats": 60, "tempo_bpm": 120, "sample_rate": 48000, "block_samples": 512,
                                  "samples": 1440000, "steps": [1 / 480, 1 / 960], "candidate_runs": 48}}
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    started = time.monotonic()
    build = subprocess.run(command, capture_output=True, text=True)
    (output / "build.log").write_text(build.stdout + build.stderr)
    require(build.returncode == 0, "Build failed; see build.log")
    with (output / "run.log").open("w") as log:
        run = subprocess.Popen([str(binary), str(output)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in run.stdout:
            log.write(line); log.flush()
            print(line, end="", flush=True)
        returncode = run.wait()
    require(returncode == 0, "Diagnostic failed; see run.log")
    effective = [json.loads(line) for line in (output / "effective-configurations.jsonl").read_text().splitlines()]
    validate_effective(presets, effective, mappings)
    rows = [json.loads(line) for line in (output / "runs.jsonl").read_text().splitlines()]
    require(len(rows) == 48 and all(row["passed"] for row in rows), "Expected 48 successful integrity measurements")
    runs = {(row["preset"], row["step"]): row for row in rows}
    comparisons = [compare(preset, output, runs) for preset in presets]
    (output / "comparisons.json").write_text(json.dumps(comparisons, indent=2) + "\n")
    require(source_digest() == before, "Product source changed during measurement")
    provenance.update({"elapsed_seconds_including_build": time.monotonic() - started,
                       "build_exit_code": build.returncode, "run_exit_code": returncode,
                       "compiled_field_mapping_validation": "passed for all 24 presets and 72 voices",
                       "unchanged_product_source_digests": True, "binary_sha256": sha256(binary),
                       "output_sha256": {str(path.relative_to(output)): sha256(path) for path in sorted(output.glob("traces/*.csv"))}})
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    write_report(output, provenance, comparisons)
    print("PASS canonical mappings, 48 runs, receiving ledgers, output references and frozen-source digests")


if __name__ == "__main__":
    main()
