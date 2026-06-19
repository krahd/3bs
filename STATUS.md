# The Three Body Solution - Project Status

Last updated: 2026-06-19 19-15 GMT-3

## Project purpose

The Three Body Solution is a deterministic generative MIDI instrument and
standalone artwork driven by a normalized three-body gravitational simulation.

## Current implementation state

The repository contains the initial concept and repository documentation. No
simulation, MIDI engine, plugin target, standalone application, renderer,
factory preset, automated test, or CI workflow has been implemented yet.

## Active focus

Bootstrap the private repository, then implement and verify a deterministic
standalone/AU/VST3 vertical slice before making the repository public.

## Architecture overview

No runtime architecture currently exists. The approved design separates a
framework-independent deterministic core from JUCE host adapters, standalone
CoreMIDI handling, shared UI, and an isolated Metal renderer. Architecture and
data-flow SVGs will be added after those components exist; placeholder diagrams
would inaccurately describe the current repository.

## Setup and run instructions

There is no buildable target yet. Planned requirements are Apple Silicon macOS
13+, Xcode, CMake 3.22+, and the pinned JUCE submodule.

## Configuration and environment variables

No runtime configuration or environment variables exist. Signing credentials
must never be stored in the repository.

## Important files and directories

- `README.md`: project overview and eventual build/use instructions.
- `AGENTS.md`: authoritative repository working rules.
- `doc/project-initial-description.md`: original concept.
- `doc/AGENTS.md`: source agent-policy template.
- `doc/STATUS.md`: source status template.
- `src/`: currently empty implementation root.
- `ignore/`: deliberately ignored local material.

## Recent changes

- Defined the AGPL-3.0-only project policy and initial documentation.
- Selected JUCE 8.0.13, C++20, CMake, and an isolated native Metal renderer.

## Tests and verification status

No executable code exists, so no build or runtime checks are available. The
documentation has not yet been committed or pushed at this status point.

## Known issues, risks, and limitations

- GitHub authentication must be valid before the private remote can be created.
- AU MIDI effects and VST3 MIDI-generating instruments require different host
  behavior and separate manual validation.
- Apple signing and notarization are blocked until Developer credentials exist.
- The Metal renderer must remain isolated from real-time MIDI processing.

## Pending tasks

- Initialize Git and create the private GitHub repository.
- Add the pinned JUCE submodule and CMake targets.
- Implement deterministic simulation, mapping, MIDI, state, presets, and tests.
- Implement the Metal scene and shared controls.
- Verify the vertical slice in standalone, Logic, and Ableton.

## Next steps

1. Commit and publish the repository bootstrap privately.
2. Implement the framework-independent engine with automated tests.
3. Add host wrappers, standalone routing, and the Metal presentation layer.

## Longer-term steps

1. Author and validate 24 complete artwork presets.
2. Make the repository public after the vertical-slice publication audit.
3. Add signed and notarized release packaging when credentials are available.

## Decisions and rationale

- AGPL-3.0-only matches JUCE's open-source licensing path.
- Normalized units prioritize musical control while retaining gravitational
  behavior.
- Beat-time synchronization makes orbital evolution reproducible in composition.
- Procedural visual assets avoid external asset licensing and establish a
  coherent visual identity.

## Documentation alignment notes

Root documentation is authoritative. Files under `doc/` are retained as source
material and may contain placeholders or superseded spelling.

---

Last updated: 2026-06-19 19-15 GMT-3
