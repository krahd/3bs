# The Three Body Solution

The Three Body Solution (`3bs`) is a generative MIDI artwork. Three bodies move
through a deterministic gravitational simulation; their changing distances,
velocities, phases, and crossings become three interlocking musical voices.
The same system is rendered as a cinematic planetary scene.

The project targets Apple Silicon Macs and will produce:

- an Audio Unit MIDI effect for Logic Pro;
- a VST3 MIDI-generating instrument for Ableton Live and compatible hosts;
- a standalone CoreMIDI application.

## Status

The deterministic core, AU, VST3, standalone application, Metal presentation,
24 factory presets, and automated test targets are implemented. The original
concept and policy templates remain in [`doc/`](doc/). See
[`STATUS.md`](STATUS.md) for exact verification results and remaining host tests.

The repository remains private until a deterministic three-format vertical
slice works in standalone, Logic AU, and Ableton VST3. It will then become
public under AGPL-3.0-only.

## Design

The simulation uses normalized artistic units and fixed-step deterministic
integration. Each body has independent pitch, trigger, scale, range, channel,
velocity, duration, and optional continuous-controller mappings. Host-synced
operation follows musical beat time; standalone and free-run operation follow
elapsed time.

The visual identity is an eclipse-like gallery scene: procedural planets,
atmospheres, distant light, restrained stars, and fading luminous trajectories.
Rendering is isolated from the real-time MIDI engine.

## Build Requirements

- Apple Silicon Mac running macOS 13 or newer
- Xcode with command-line tools
- CMake 3.22 or newer
- Git with submodule support

JUCE 8.0.13 is pinned as a Git submodule. Clone and build the core tests:

```bash
git clone --recurse-submodules https://github.com/krahd/3bs.git
cd 3bs
cmake --preset dev
cmake --build --preset dev -j 8
ctest --preset dev --output-on-failure
```

Build all macOS formats and integration tests:

```bash
cmake --preset plugin-debug
cmake --build --preset plugin-debug -j 8
ctest --preset plugin-debug --output-on-failure
```

Artifacts are written below `build/plugin-debug`:

- `src/plugin/ThreeBSAU_artefacts/Debug/AU/`: Audio Unit MIDI effect;
- `src/plugin/ThreeBSVST3_artefacts/Debug/VST3/`: VST3 instrument;
- `src/standalone/ThreeBSStandalone_artefacts/Debug/`: standalone app.

Local bundles are ad-hoc signed after build. Set `THREEBS_ADHOC_SIGN=OFF` when
performing a Developer ID release-signing workflow.

## Host Routing

Logic uses `3bs` as an AU MIDI effect before a software instrument. Ableton
uses the VST3 on one MIDI track and receives its MIDI output on another track.
Ableton merges internally routed MIDI channels, so separate instruments require
multiple plugin instances, a multitimbral destination, or standalone routing.

For local AU validation, copy the component to
`~/Library/Audio/Plug-Ins/Components`, refresh the Audio Unit cache, and run:

```bash
auval -v aumi Tbs1 Krhd
```

The standalone creates a virtual CoreMIDI output named “The Three Body
Solution” by default and can select an existing output from the control deck.
Press `P` for the clean presentation view and `F` to toggle standalone
fullscreen.

## Controls And Presets

The shared control deck exposes run/sync, speed, gravity, softening, chaos,
density, trail, bloom, body masses, deterministic randomization, reset, and the
24-scene factory catalog. `ADVANCED STATE` opens a nonmodal numerical editor
for every mass and initial position/velocity vector. Host state stores schema version, parameters, seed,
initial mass/position/velocity vectors, preset mapping configuration, and loop
policy. Rendering consumes immutable snapshots and never runs on the MIDI
processing thread.

## Licensing

Copyright (c) 2026 Tomas Laurenzo.

This project is free software licensed under the GNU Affero General Public
License version 3 only. See [`LICENSE`](LICENSE). Distributed binaries must be
accompanied by the corresponding source and build information required by that
licence. Third-party notices are recorded in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
