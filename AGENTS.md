# AGENTS.md

Repository instructions for AI coding agents working on The Three Body Solution.
Read this file and `STATUS.md` before changing the project.

## Non-Negotiable Rules

- Keep root `STATUS.md` accurate. Review and update it after every meaningful
  change to behavior, architecture, dependencies, build steps, tests, or risks.
- Preserve both matching `Last updated` lines in `STATUS.md` using the format
  `YYYY-MM-DD HH-MM GMT+-X` and the user's America/Montevideo timezone.
- Do not invent project facts or claim checks passed unless they were run.
- Do not overwrite unrelated user work or expose credentials and signing data.
- Keep `ignore/` ignored.
- Do not commit generated build products, plugins, applications, Metal binaries,
  local presets, or IDE state.
- Use concise, factual technical communication.

## Project Map

- Purpose: deterministic three-body simulation mapped to generative MIDI and a
  cinematic planetary visualization.
- Runtime surfaces: Logic AU MIDI effect, VST3 MIDI-generating instrument, and
  standalone CoreMIDI application.
- Stack: C++20, CMake, JUCE 8.0.13, Objective-C++, and Metal Shading Language.
- Supported platform: arm64 macOS 13 and newer.
- `src/core/`: framework-independent simulation, mapping, state, and queues.
- `src/plugin/`: JUCE plugin processors and shared editor.
- `src/standalone/`: standalone application and CoreMIDI device handling.
- `src/render/`: renderer interface and macOS Metal implementation.
- `src/ui/`: shared controls and presentation UI.
- `resources/presets/`: versioned factory preset JSON.
- `tests/`: deterministic core and state tests.
- `doc/`: original concept and historical templates.

## Real-Time Safety

- Never allocate, free, block, lock, log, access files, call Objective-C UI
  code, or perform GPU work from an audio/MIDI processing callback.
- Preallocate event, note, command, and snapshot storage.
- Communicate across threads only through atomics or fixed-capacity lock-free
  queues with explicit ownership.
- Always emit required note-offs on stop, reset, seek, bypass, state restore,
  processor destruction, and voice reconfiguration.
- Keep simulation results independent of audio block size when inputs and the
  transport timeline are equivalent.
- Treat renderer failure or absence as non-fatal to MIDI generation.
- Validate every deserialized numeric value before it reaches the engine.

## Determinism And State

- Use the project PRNG; do not use implementation-defined random distributions.
- Use fixed-step double-precision simulation and explicit state versioning.
- Preserve old published state and preset schemas through tested migrations.
- Keep device selection and window geometry out of musical presets.
- Factory presets must be deterministic and pass schema validation.

## Licensing

- Original source files use `SPDX-License-Identifier: AGPL-3.0-only`.
- The copyright holder is Tomas Laurenzo.
- Do not add code or assets whose terms are incompatible with AGPL-3.0-only.
- Retain third-party notices and update `THIRD_PARTY_NOTICES.md` when dependencies
  or bundled assets change.
- Do not copy external textures, fonts, shaders, or examples without recording
  their provenance and licence. Prefer original procedural assets.

## Standard Work Loop

1. Inspect the repository and `STATUS.md`.
2. Search all relevant call sites and state consumers.
3. Make the smallest coherent change.
4. Run the narrowest reliable test, then broaden based on risk.
5. Update public documentation and `STATUS.md` when project state changes.
6. Report changed files, commands run, results, and remaining limitations.

## Build And Validation

The authoritative commands are maintained in `README.md` and `CMakePresets.json`.
Once the scaffold exists, expected checks are:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

For release candidates also build Release targets, run `auval`, run plugin
validation, and manually verify Logic, Ableton, and standalone routing. Record
external checks as unverified until they are actually performed.

## Documentation And Git

- `AGENTS.md` is authoritative; keep `CLAUDE.md` as a short pointer.
- Keep `README.md` user-facing and `STATUS.md` operational.
- Add accurate inline SVG architecture and data-flow diagrams to `STATUS.md`
  when implemented structure makes them meaningful; visually inspect them.
- Prefer focused commits. Never force-push or rewrite published history without
  explicit instruction.
- Keep the GitHub repository private until the three-format vertical slice has
  passed its publication checklist.
