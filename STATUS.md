# The Three Body Solution - Project Status

Last updated: 2026-11-27 22-45 GMT-3

## Project purpose

The Three Body Solution is a deterministic generative MIDI instrument and
standalone artwork driven by a normalized three-body gravitational simulation.

## Current implementation state

The repository builds a deterministic core library, Logic AU MIDI effect,
Ableton-oriented VST3 instrument, and standalone CoreMIDI application. It has a
native interactive HDR Metal presentation layer, shared control deck, 24
schema-v2 factory presets, 12 schema-v1 voicing presets, schema-v8 host state
serialization, automatic camera framing, live note-stream visualization,
automated tests, and macOS CI.
Portable schema-v4 `.3bs` JSON files save and restore complete musical,
simulation, and presentation configurations across plugin and standalone.

## Active focus

The [27 November audit plan](AUDIT-AND-IMPROVEMENT-PLAN.md) is implemented and
integrated through [PR #2](https://github.com/krahd/3bs/pull/2), merge
`bdc32750b8677c06630295cb1467c29de7646ed5`. Its source tree exactly matches the
candidate that passed all five jobs in
[run 46089189294](https://github.com/krahd/3bs/actions/runs/46089189294).
The gravitational model, Verlet solver, factory systems, mapping vocabulary,
three formats and existing parameter identities/ranges are retained.

Both 12-entry portable suites, both ThreadSanitizer jobs, all 18 native tests,
actual prior-executable recall/resave, authoritative AU validation, pluginval
and the real-duration 30-minute virtual CoreMIDI soak passed. Complete and
individual-format candidate archives have verified checksums and provenance.
The source audit and historical alpha.3 release remain unchanged.

Actual Logic/Ableton, physical-device, display-scale and combined sustained
UI/render/MIDI qualification remains open. The exact evidence and boundaries
are in [RELIABILITY-ACCEPTANCE.md](RELIABILITY-ACCEPTANCE.md), with an executable
[host qualification procedure](reliability/2026-11-27/HOST-QUALIFICATION.md).

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
    <text x="455" y="150" font-size="19">Deterministic Engine</text><text x="455" y="182" font-size="13" fill="#9cb4c8">Verlet physics · measurements</text><text x="455" y="204" font-size="13" fill="#9cb4c8">pitch/trigger mapping · state</text><text x="455" y="226" font-size="13" fill="#9cb4c8">streaming MIDI event delivery</text>
    <text x="795" y="82" font-size="18">MIDI Destination</text><text x="795" y="106" font-size="12" fill="#7f9bb4">host events or CoreMIDI</text>
    <text x="795" y="238" font-size="18">Presentation</text><text x="795" y="264" font-size="13" fill="#9cb4c8">JUCE control deck</text><text x="795" y="286" font-size="13" fill="#9cb4c8">Metal scene + note streams</text>
  </g>
  <g stroke="#64748b" stroke-width="2" fill="none" marker-end="url(#arch-arrow)">
    <path d="M230 89 C275 89 285 145 330 145"/><path d="M230 189 L330 189"/><path d="M230 289 C275 289 285 235 330 235"/><path d="M580 155 C625 155 635 93 680 93"/><path d="M580 228 C625 228 635 260 680 260"/>
  </g>
</svg>

### Data-flow diagram

Complete control revisions use bounded mailboxes; expendable visual snapshots
use SPSC queues. MIDI stays on the processing path, with a monotonic worker and
a destination-owned delivery ledger in standalone.

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

Requirements are Apple Silicon macOS 13+, Xcode, CMake 3.22+, Python 3, and recursive Git
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
- `AUDIT-AND-IMPROVEMENT-PLAN.md`: dated audit, prioritised repairs and acceptance criteria.
- `audit/2026-11-27/`: isolated diagnostic sources, commands and observed results.
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

- 2026-11-27: integrated PR #2 after all automated acceptance gates passed;
  verified development archives and retained the exact native evidence,
  historical failures and remaining host/device qualification procedure.
- 2026-11-27: implemented reliability repairs across core MIDI, coherent state
  publication, standalone timing/destination ownership, GPU frame ownership,
  complete controls and exact-state/preset workflows. Added numerical,
  allocation, concurrency, native UI/Metal/CoreMIDI, prior-executable recall and
  distribution gates; candidate acceptance is recorded separately below.
- 2026-11-27: completed a source audit and targeted portable diagnostics; added
  the detailed improvement plan and reproducible evidence without modifying
  production source, presets, dependencies or released binaries.
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

### Integrated reliability implementation

Source candidate `581548314288311da805da7a7c7a3f0cdaedc536`, tested merge
`da925c1f3d6d043e314f78af7d4a2ba22966750d` and integration merge `bdc32750`
share tree `ae1530038ecfb42ced1306655075a04766b8f160`. The completed run above
passed all 12 portable entries on both Linux and macOS using CMake 3.22.6,
queue/ownership TSan and all 18 native entries without skips. States captured
from the exact audited executable passed current recall and immediate resave.

Actual-processor TSan completed 384 publications, 768 save/restore pairs,
115,639 callbacks, 8,070 notes and 48,345 snapshots in 177.14 seconds, with zero
errors, final acknowledgement and no timeout. Real Metal rendering completed
27 frames with 13,824 glyph bytes, 21 handled allocation failures and zero GPU
errors. All 20 UI captures were produced; the visual review is recorded in the
[final checkpoint](reliability/2026-11-27/CHECKPOINT-6.md).

The actual processor profile observed zero C/C++ allocations or frees across
2,188 callbacks with 4 MiB of preallocated host MIDI storage. Final core
profiling observed zero deadline overruns. The earlier checkpoint-5 core
profile's two overruns in the 48 kHz / 512-sample Chord transition scenario
remain recorded; no universal timing or arbitrary-host capacity guarantee is
claimed.

AU installed metadata, signature and bounded discovery checks passed, followed
by authoritative auval. VST3 pluginval 1.0.4 passed at strictness 5; the optional
separate Steinberg SDK validator was not configured. The virtual CoreMIDI soak
measured 1,800.14 seconds, 86,421 delivered events, balanced receiving ledgers,
zero output failures and zero worker overruns. Delivery lateness was
5 / 11.2 / 12.3 ms at p50 / p95 / p99 and 17.422 ms maximum. The 73,378 visual
queue drops during intentionally stalled message dispatch were not MIDI loss.
Physical routing and actual simultaneous UI/GPU stress remain separate gates.

All four development-candidate ZIPs passed SHA-256, integrity and embedded
provenance verification. They target arm64 macOS 13+, are ad-hoc signed and
are not notarised. They do not replace the historical alpha.3 release.

The [factory musical-refinement diagnostic](reliability/2026-11-27/refinement/REPORT.md)
ran all 24 actual systems at the published and halved steps for 60 beats each.
All 48 runs had finite state, bounded work, valid events and empty final ledgers.
21 of 24 full traces differed. This is descriptive sensitivity evidence, not a
new tolerance or a change to the solver/defaults. The JUCE submodule remains
pinned to 8.0.13.

### Original audit evidence — 27 November 2026

The immutable [audit evidence](audit/2026-11-27/EVIDENCE.md) records the original
portable reproduction of lost/wrong-channel note cleanup, delayed strums after
disable/gate closure, block-dependent input, fixed-buffer event loss, false
SpeedPeak triggers, torn snapshots and non-finite-state conversion. It also
records five omitted tests and the trail-duration loss. Those are baseline
observations; current regression and native outcomes are recorded separately.

Historical native evidence:

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

## Pending acceptance

- Record Logic AU and Ableton VST3 sessions for actual routing, save/reopen,
  editor-open recall, loop/seek/bypass, multiple instances, offline processing
  and final teardown note termination.
- Record physical CoreMIDI destination loss/reconnection, destination changes
  and quit cleanup. Combine sustained MIDI measurement with actual UI/render
  load, rather than treating a message-thread-stall soak as that session.
- Inspect native interaction at representative display scales, matched scene
  captures, camera controls, fullscreen and editor recreation; perform the
  complete authored-file and preset-to-record-to-reopen workflow in each format.
- Qualify a public release separately, including any Developer ID signing and
  notarisation. Candidate archives and alpha.3 are distinct manifestations.
- Preserve the optional Max for Live question as a separate future scope
  decision, outside this audit's existing three-format plan.

## Next steps

Use the exact candidate and versioned acceptance checklist for the remaining
host/device/display sessions. Record observed versions, destination, build SHA,
results and any failure before making a release-readiness claim. Project
administration is derived from the canonical owner and reconciled in
`krahd/tom-work-admin`; technical evidence remains here.

## Longer-term steps

1. Maintain and qualify the existing 24 complete artwork presets and 12 voicing
   presets, preserving published behaviour or explicitly versioning corrections.
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

Last updated: 2026-11-27 22-45 GMT-3

