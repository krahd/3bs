# The Three Body Solution - Project Status

Last updated: 2026-10-06 15-14 GMT-3

## Project purpose

The Three Body Solution is a deterministic generative MIDI instrument and
standalone artwork driven by a normalized three-body gravitational simulation.

## Current implementation state

The repository builds a deterministic core library, Logic AU MIDI effect,
Ableton-oriented VST3 instrument, and standalone CoreMIDI application. It has a
native interactive HDR Metal presentation layer, shared control deck, 24
schema-v2 factory presets, 12 schema-v1 voicing presets, schema-v7 host state
serialization, automatic camera framing, live note-stream visualization,
automated tests, and macOS CI.
Portable schema-v3 `.3bs` JSON files save and restore complete musical,
simulation, and presentation configurations across plugin and standalone.

## Active focus

Finish hands-on host and interaction validation for the published
`v0.1.0-alpha.3` Tomas Laurenzo-branded binaries. Automated validation is green
again on the current Mac: dev and plugin-debug suites pass, including the Metal
smoke test, the installed AU passes `auval`, and installed AU/VST3 signatures
and arm64 architectures verify. Ableton Live 12 Suite is installed and has
previously scanned and loaded the VST3, but the current alpha.3 routing/editor
workflow still needs a fresh hands-on pass. Logic is not installed on this Mac.

## Architecture overview

The framework-independent core owns physics, measurements, mapping, MIDI event
scheduling, deterministic random state, and fixed-capacity queues. Thin JUCE
adapters translate host transport and MIDI. The Metal renderer consumes
immutable snapshots and bounded note events without touching real-time
processing.

### Architecture diagram

The same engine is shared by all three runtime surfaces.

<svg xmlns="http://www.w3.org/2000/svg" width="960" height="380" viewBox="0 0 960 380" role="img" aria-labelledby="architecture-title architecture-desc">
  <title id="architecture-title">Three Body Solution architecture</title>
  <desc id="architecture-desc">AU, VST3, and standalone adapters use one deterministic engine, which feeds MIDI outputs and a separate Metal presentation layer.</desc>
  <defs><marker id="arch-arrow" markerWidth="10" markerHeight="10" refX="9" refY="3" orient="auto"><path d="M0,0 L0,6 L9,3 z" fill="#64748b"/></marker></defs>
  <rect width="960" height="380" rx="18" fill="#080d18"/>
  <g fill="#111b2d" stroke="#34506f" stroke-width="2">
    <rect x="40" y="58" width="190" height="62" rx="10"/><rect x="40" y="158" width="190" height="62" rx="10"/><rect x="40" y="258" width="190" height="62" rx="10"/>
    <rect x="330" y="115" width="250" height="150" rx="12"/><rect x="680" y="48" width="230" height="90" rx="12"/><rect x="680" y="205" width="230" height="115" rx="12"/>
  </g>
  <g fill="#d9e7f2" font-family="system-ui, sans-serif" text-anchor="middle">
    <text x="135" y="85" font-size="17">Logic AU MIDI FX</text><text x="135" y="106" font-size="12" fill="#7f9bb4">host adapter</text>
    <text x="135" y="185" font-size="17">VST3 Instrument</text><text x="135" y="206" font-size="12" fill="#7f9bb4">silent audio + MIDI out</text>
    <text x="135" y="285" font-size="17">Standalone</text><text x="135" y="306" font-size="12" fill="#7f9bb4">CoreMIDI adapter</text>
    <text x="455" y="150" font-size="19">Deterministic Engine</text><text x="455" y="182" font-size="13" fill="#9cb4c8">Verlet physics · measurements</text><text x="455" y="204" font-size="13" fill="#9cb4c8">pitch/trigger mapping · state</text><text x="455" y="226" font-size="13" fill="#9cb4c8">fixed MIDI event buffers</text>
    <text x="795" y="82" font-size="18">MIDI Destination</text><text x="795" y="106" font-size="12" fill="#7f9bb4">host events or CoreMIDI</text>
    <text x="795" y="238" font-size="18">Presentation</text><text x="795" y="264" font-size="13" fill="#9cb4c8">JUCE control deck</text><text x="795" y="286" font-size="13" fill="#9cb4c8">Metal scene + note streams</text>
  </g>
  <g stroke="#64748b" stroke-width="2" fill="none" marker-end="url(#arch-arrow)">
    <path d="M230 89 C275 89 285 145 330 145"/><path d="M230 189 L330 189"/><path d="M230 289 C275 289 285 235 330 235"/><path d="M580 155 C625 155 635 93 680 93"/><path d="M580 228 C625 228 635 260 680 260"/>
  </g>
</svg>

### Data-flow diagram

Commands and render snapshots use fixed-capacity queues; MIDI stays on the
processing path.

<svg xmlns="http://www.w3.org/2000/svg" width="960" height="330" viewBox="0 0 960 330" role="img" aria-labelledby="flow-title flow-desc">
  <title id="flow-title">Real-time data flow</title><desc id="flow-desc">Host transport and parameters enter processing, simulation produces mappings and MIDI, while snapshots and bounded note events go independently to Metal.</desc>
  <defs><marker id="flow-arrow" markerWidth="10" markerHeight="10" refX="9" refY="3" orient="auto"><path d="M0,0 L0,6 L9,3 z" fill="#64748b"/></marker></defs>
  <rect width="960" height="330" rx="18" fill="#080d18"/>
  <g fill="#111b2d" stroke="#34506f" stroke-width="2">
    <rect x="35" y="52" width="170" height="66" rx="10"/><rect x="35" y="215" width="170" height="66" rx="10"/><rect x="280" y="105" width="190" height="92" rx="10"/><rect x="545" y="55" width="170" height="70" rx="10"/><rect x="545" y="207" width="170" height="70" rx="10"/><rect x="790" y="55" width="135" height="70" rx="10"/><rect x="790" y="207" width="135" height="70" rx="10"/>
  </g>
  <g fill="#d9e7f2" font-family="system-ui, sans-serif" text-anchor="middle">
    <text x="120" y="80" font-size="16">Transport</text><text x="120" y="101" font-size="12" fill="#8ba3b8">parameters + MIDI in</text><text x="120" y="243" font-size="16">Control Deck</text><text x="120" y="264" font-size="12" fill="#8ba3b8">preset/reset commands</text>
    <text x="375" y="139" font-size="17">processBlock</text><text x="375" y="162" font-size="12" fill="#8ba3b8">consume · advance · schedule</text>
    <text x="630" y="85" font-size="16">MIDI Events</text><text x="630" y="106" font-size="12" fill="#8ba3b8">sample offsets</text><text x="630" y="235" font-size="16">Render Snapshot</text><text x="630" y="256" font-size="12" fill="#8ba3b8">immutable body state</text>
    <text x="857" y="86" font-size="16">Host / MIDI</text><text x="857" y="238" font-size="16">Metal + note pane</text><text x="857" y="259" font-size="12" fill="#8ba3b8">60 fps when available</text>
  </g>
  <g stroke="#64748b" stroke-width="2" fill="none" marker-end="url(#flow-arrow)">
    <path d="M205 85 C245 85 245 135 280 135"/><path d="M205 248 C245 248 245 172 280 172"/><path d="M470 136 C505 136 510 90 545 90"/><path d="M715 90 L790 90"/><path d="M690 112 C760 140 745 212 790 226"/><path d="M470 170 C505 170 510 242 545 242"/><path d="M715 242 L790 242"/>
  </g>
</svg>

## Setup and run instructions

Requirements are Apple Silicon macOS 13+, Xcode, CMake 3.22+, and recursive Git
submodules. Use `cmake --preset dev` for core tests or
`cmake --preset plugin-debug` for all formats, then the matching build and test
presets. Detailed commands and artifact paths are in `README.md`. VS Code tasks
can build/run the standalone or build Release AU/VST3 bundles and install them
into the current user's plugin directories.

## Configuration and environment variables

No environment variables are required. CMake options control plugin/test builds
and local ad-hoc signing. Signing credentials must never be stored in the
repository.

## Important files and directories

- `README.md`: project overview and eventual build/use instructions.
- `doc/INSTALL.md`: binary verification, installation, routing, and removal.
- `doc/releases/`: versioned release notes and known limitations.
- `docs/`: dependency-free GitHub Pages source and product screenshot.
- `AGENTS.md`: authoritative repository working rules.
- `doc/project-initial-description.md`: original concept.
- `doc/AGENTS.md`: source agent-policy template.
- `doc/STATUS.md`: source status template.
- `src/core/`: deterministic simulation, scales, MIDI mapping, and queues.
- `src/plugin/`: AU/VST3 processors, state, and shared editor adapter.
- `src/standalone/`: CoreMIDI application.
- `src/render/` and `src/ui/`: Metal scene, controls, and preset parsing.
- `resources/presets/`: 24 factory artwork states using schema version 2.
- `resources/metal/` and `resources/stars/`: embedded Metal shaders and star data.
- `tests/`: deterministic, preset, Metal, and processor tests.
- `ignore/`: deliberately ignored local material.

## Recent changes

- Added fixed-step double-precision Verlet physics and deterministic PCG state.
- Added five pitch mappings, four trigger mappings, scale quantization, three
  monophonic voices, CC lanes, note cleanup, and escape policies.
- Added AU, VST3, standalone, Metal scene, shared controls, state recall, 24
  factory presets, ad-hoc signing, tests, and CI.
- Added a nonmodal advanced editor for all masses and initial position/velocity
  vectors, shared by plugin and standalone.
- Added tracked VS Code build and run tasks for the standalone application.
- Added mouse orbit/zoom, click-to-focus body tracking, smooth barycenter return,
  delayed auto-orbit resumption, and full camera state recall.
- Replaced point planets and line trails with procedural icospheres, moving
  clouds, atmospheres, 30-second tapered ribbons, deterministic star layers,
  HDR bloom, dithering, and tone mapping.
- Added trajectory revisions so reset, seek, loop restart, preset changes, and
  respawns clear disconnected visual history.
- Added non-coplanar curated initial systems, non-automatable Space-page
  initial orbital-plane tilt macros, and schema-v3 host recall for exact/base
  vectors plus tilt values.
- Wired the System, Voices, Space, Presets, and Settings deck tabs to switch
  visible controls, with Space exposing trail, bloom, and plane-tilt controls.
- Reduced catalogue-star size, background dust peaks, trail additive energy,
  and cloud-shell opacity so visual effects do not read as extra planets.
- Corrected vertically mirrored post-process sampling that made bloom appear as
  detached duplicate planets.
- Added schema-v4 barycenter auto-framing, editable near/far camera limits, and
  manual-zoom override behavior shared by plugin and standalone.
- Added a real-time-safe body-tagged note queue and a persistent, minimizable
  three-lane Metal piano-roll overlay.
- Switched the Metal scene to reversed-Z depth (near=1/far=0 with a GreaterEqual
  test) to remove the surface flicker seen when two planets nearly overlapped.
- Added a CoreText-generated monospace glyph atlas and textured-quad glyph
  pipeline, plus a vertical FastTracker II-style note view that scrolls actual
  note names per planet and a pane button toggling horizontal/vertical style
  (persisted via `notePaneStyle`, shared by plugin and standalone).
- Exposed per-planet voice controls (enable, scale/mode, pitch mapping, trigger
  mapping) on the VOICES deck page, backed by new plugin parameters with editor
  attachments and standalone polling, and synchronized from factory presets.
- Normalized all 24 factory presets so each preset's three voices share one
  tonal center (common root, third-compatible scales) for coherent harmony.
- Added a core test asserting all three planets generate notes and that
  disabling a planet silences only that voice.
- Added bounded Hermite trail subdivision, barycenter-targeted manual zoom, and
  background double-click camera reset with immediate fitted framing.
- Populated the pinned HYG v4.1 magnitude-6.5 resource with 8,921 stars and
  restored larger additive star billboards before opaque planet rendering.
- Made RESET persistent across deck pages, moved SET STATE into a top-level
  window, categorized factory presets, enlarged dial captions, and highlighted
  the selected deck tab instead of repeating its title.
- Added per-planet root parameters, four pitch mappings, four trigger mappings,
  and allocation-free deterministic chord/strum generation with schema-v5 recall.
- Replaced note-pane H/V glyphs with line icons and raised the third FTII
  column's text luminance.
- Removed FTII header glyph artifacts, raised pane buttons above content, and
  reduced the orientation icon stroke width.
- Distributed labeled preset families horizontally and made the Voices page
  expose contextual independent/chord/strum semantics.
- Preserved camera yaw/pitch during background double-click fitting, brightened
  stars, and added soft planet-boundary blending with overlap suppression for
  cloud and atmosphere shells.
- Added validated, versioned `.3bs` JSON save/load to plugin and standalone,
  including exact states, hidden voice fields, physics, camera/presentation,
  tilts, mode controls, and preset selection.
- Reworked the README around the working three-format product, added an actual
  standalone screenshot and installation/release documentation, and created a
  responsive GitHub Pages showcase with versioned binary links.
- Published the showcase from `main:/docs` at `https://krahd.github.io/3bs/`
  and the `v0.1.0-alpha.1` prerelease with complete, AU, VST3, standalone, and
  SHA-256 checksum downloads.
- Simplified the Pages site into a sober, single-column project overview with
  one correctly proportioned screenshot and direct source, documentation,
  release, and binary links.
- Added a reproducible arm64 Release packaging flow for the complete bundle and
  individual AU, VST3, and standalone archives, including build provenance,
  licence files, third-party notices, and SHA-256 checksums.
- Corrected the plugin configuration test tolerance to match the duration host
  parameter's declared 0.001-beat precision instead of requiring impossible
  double precision after an atomic-float round trip.
- Added 12 voice-only factory presets split evenly across Independent, Chord,
  and Strum modes; applying one preserves simulation, presentation, transport,
  and time signature while flushing active and pending notes at a block boundary.
- Added per-voice Straight length grids mapping simulation measurements onto
  1/32 through two-whole-note durations, with schema-v7 host and schema-v3
  `.3bs` migration preserving old sessions as Continuous Legacy.
- Moved time signature to Voices, moved the exclusive voicing-mode selector over
  the planet grid, and added contextual voicing-preset and length-grid controls.
- Added tracked scripts for Release build/test/package output and local AU/VST3
  installation, plus the `3bs: Build and Install Plugins` VS Code task.
- Corrected AU, VST3, and standalone company metadata from `krahd` to
  `Tomas Laurenzo`, moved bundle identifiers to `com.tomaslaurenzo`, and changed
  the plugin manufacturer code from `Krhd` to `Tmlz` for alpha.3.
- Made release packaging and local plugin installation clean their Release
  targets first so regenerated bundle metadata is always relinked and resigned.

## Tests and verification status

- 2026-10-06 plugin-debug validation passes 7/7 tests, including Metal smoke,
  plugin behavior, and the steady-state real-time profile.
- The steady-state `processBlock` profile runs 2,048 warmed 512-sample blocks at
  48 kHz with a preallocated host MIDI buffer and observes **0 heap
  allocations**. On the current Mac it measured about **491 us average** and
  **2.12 ms maximum** CPU time against a **10.67 ms** block budget. Timing is
  diagnostic, not a CI performance threshold.
- The audio processing path uses fixed-capacity engine/snapshot queues and has
  no explicit mutex/lock primitives in the audited core/process path.
- The Metal renderer now publishes thread-safe live callback-rate and CPU
  submission-time telemetry; the UI reports measured FPS and smoothed CPU frame
  time instead of the previous hard-coded `60 FPS` label. Metal smoke passes.
- Fresh hands-on host/interaction validation is still required for Ableton and
  renderer behavior; Logic is not installed on this Mac.

## Pending tasks

- Complete hands-on mouse/trackpad interaction, final foreground renderer
  inspection, and 60 fps profiling.
- Test the installed alpha.3 AU MIDI effect on a Mac with Logic installed.
- Run a fresh alpha.3 VST3 routing/editor pass in Ableton Live and run
  `pluginval` when it is available.
- Decide whether to add and support a Max for Live MIDI Effect for same-chain
  Ableton operation.
- Complete remaining voice controls (range and custom scale) and authored
  user-preset library management.
- Use the new live renderer telemetry during hands-on interaction to confirm
  sustained frame rate and inspect worst-case rendering under representative
  presets/window sizes.

## Next steps

1. Complete interaction/performance and host validation.
2. Close the remaining control-surface gaps and rerun the full suite.
3. Continue alpha.3 release validation in Logic and Ableton.

## Longer-term steps

1. Author and validate 24 complete artwork presets.
2. Maintain the public website, release notes, and binary checksums for each
   published version.
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

Last updated: 2026-10-06 15-14 GMT-3
