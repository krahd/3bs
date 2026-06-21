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

The deterministic core, AU, VST3, standalone application, interactive HDR Metal
presentation with automatic framing and live note streams, 24 factory presets,
and automated test targets are implemented. The original concept and policy
templates remain in [`doc/`](doc/). See
[`STATUS.md`](STATUS.md) for exact verification results and remaining host tests.

The repository remains private until a deterministic three-format vertical
slice works in standalone, Logic AU, and Ableton VST3. It will then become
public under AGPL-3.0-only.

## Design

The simulation uses normalized artistic units and fixed-step deterministic
integration. Each body has independent pitch, trigger, root, scale, range, channel,
velocity, duration, and optional continuous-controller mappings. Host-synced
operation follows musical beat time; standalone and free-run operation follow
an internal musical clock. Chord and strum modes turn a physical trigger into
deterministic three-body scale triads.

The visual identity is an eclipse-like gallery scene: seeded procedural planet
surfaces, subtle cloud layers, atmospheres, distant light, a catalogue-aware
celestial sphere, restrained HDR bloom, and fading luminous trajectories.
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

The shared control deck is split into System, Voices, Space, Presets, and
Settings pages. It exposes run/sync, speed, gravity, softening, chaos, density,
trail duration, bloom, body masses, non-automatable initial orbital-plane tilt
macros, automatic framing and zoom limits, deterministic randomization, reset,
and the 24-scene factory catalog grouped horizontally by orbital family. `RESET` remains
available on every deck page.
Drag the scene to orbit around the active target, scroll to zoom, click a
planet to follow it, and click the background to return to the mass-weighted
barycenter. Double-click the background to fit every planet without changing
the current yaw or pitch. Scroll zoom also returns to the barycenter. Manual input
pauses cinematic auto-orbit for three seconds before
it eases back in. Barycenter auto-frame keeps all bodies visible as the system
expands and contracts. Manual scroll zoom disables it until `AUTO FRAME` is
enabled again on the Space page.

A translucent six-second piano-roll pane overlays the lower-right scene with
one body-coloured stream per generated voice. Note position, duration, and
velocity control each bar. Its minimize control collapses it to a small
three-lane icon; this preference is recalled with plugin state.

The Voices page changes context with `INDEPENDENT`, `CHORD`, and `STRUM`.
Independent mode uses each planet's own note trigger. In chord and strum modes,
any enabled planet can launch the shared scale-degree voicing from its selected
clock, crossing, distance, apsis, speed-peak, or phase-step criterion. Strum
adds an adjustable inter-note delay.

`SET STATE` opens a separate nonmodal numerical window for every mass and
initial position/velocity vector. Schema-v5 host state stores parameters, simulation
seed and vectors, non-automatable plane-tilt macro state, mappings, loop policy,
per-planet roots, chord controls, palette, camera target, orbit angle, framing
limits, zoom, and note-pane state. Schemas v1-v4 migrate with compatible defaults.
Rendering consumes revisioned immutable snapshots and a separate fixed-capacity
note-event queue; it never runs on the MIDI processing thread.

The Settings page saves and loads portable `.3bs` files. These are versioned,
human-readable JSON configurations containing exact initial/base body vectors,
physics, all voice and chord settings, camera/presentation state, plane tilts,
run/sync state, and preset selection. Hardware-specific MIDI destinations and
window geometry are intentionally excluded.

The renderer embeds 8,921 stars from the pinned HYG v4.1 magnitude-6.5
catalogue, plus a deterministic faint field. To regenerate it, obtain
`hygdata_v41.csv` from HYG commit
`c7f7f883fe678cc7680169a50ccd7dcc49b060ce` and run:

```bash
python3 scripts/generate_star_catalog.py hygdata_v41.csv resources/stars/hyg-v41-mag65.csv
```

## Licensing

Copyright (c) 2026 Tomas Laurenzo.

This project is free software licensed under the GNU Affero General Public
License version 3 only. See [`LICENSE`](LICENSE). Distributed binaries must be
accompanied by the corresponding source and build information required by that
licence. Third-party notices are recorded in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
