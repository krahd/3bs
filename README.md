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

The repository is at the implementation stage. The original concept and agent
policy templates are retained in [`doc/`](doc/). See [`STATUS.md`](STATUS.md)
for the exact current state and verification record.

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

Build commands will be added when the JUCE target scaffold is committed. The
pinned JUCE source will live in `external/JUCE` as a Git submodule.

## Host Routing

Logic uses `3bs` as an AU MIDI effect before a software instrument. Ableton
uses the VST3 on one MIDI track and receives its MIDI output on another track.
Ableton merges internally routed MIDI channels, so separate instruments require
multiple plugin instances, a multitimbral destination, or standalone routing.

## Licensing

Copyright (c) 2026 Tomas Laurenzo.

This project is free software licensed under the GNU Affero General Public
License version 3 only. See [`LICENSE`](LICENSE). Distributed binaries must be
accompanied by the corresponding source and build information required by that
licence. Third-party notices are recorded in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
