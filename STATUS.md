# The Three Body Solution - Project Status

Last updated: 2026-06-21 21-55 GMT-3

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

Continue external Metal, Logic, and Ableton runtime validation after publishing
the `v0.1.0-alpha.2` binary refresh.

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

## Tests and verification status

- Debug core and plugin configurations, full builds, and all automated tests
  passed after the voicing-preset and rhythmic-length changes. The Metal smoke
  test skipped because this process exposed no Metal device.
- Standalone captures at 1280x820 visually confirmed readable time-signature and
  legacy-duration values, straight-grid note labels, and non-overlapping
  Independent and Chord layouts.
- The new local install script built arm64 Release AU/VST3 bundles, verified both
  source and installed signatures, installed them under `~/Library/Audio/Plug-Ins`,
  and refreshed the Audio Unit registrar.
- `auval -v aumi Tbs1 Krhd` passed every validation section for the installed
  Release Audio Unit. `pluginval` is not installed, so VST3 validation remains
  limited to build and strict signature verification.
- The alpha.2 packaging script completed a full arm64 Release build and test
  run, verified all three bundle signatures, generated complete/AU/VST3/
  standalone archives, and passed checksum and ZIP-integrity verification.
- Published the `v0.1.0-alpha.2` GitHub prerelease with complete, AU, VST3,
  standalone, and SHA-256 checksum downloads. Tag and main CI runs
  `27923022840` and `27923013158` passed core tests and arm64 bundle builds.
- A fresh arm64 macOS 13 Release configuration and complete build passed for
  the AU, VST3, standalone, and test targets.
- Fresh Release and rebuilt plugin-debug test runs passed all executable tests
  (core, presets, star catalogue, and plugin integration). The Metal smoke test
  skipped in both because this process exposed no Metal device.
- `node --check docs/script.js` passed. Local HTML served successfully; visual
  browser capture remains pending because installed GUI browsers could not be
  launched from the restricted process environment.
- GitHub Pages built commit `9ed3485` successfully. The live homepage,
  screenshot, public repository, four binary archives, and checksum download
  each returned HTTP 200 after deployment.
- The simplified page and its local screenshot asset returned HTTP 200. A
  1400-pixel macOS Quick Look render confirmed the desktop layout and uncropped
  image aspect; automated mobile capture remains unavailable because the local
  Playwright launcher references a removed interpreter and Safari WebDriver is
  disabled at the OS level.
- `shasum -a 256 -c SHA256SUMS.txt` and `unzip -t` passed for the complete,
  AU, VST3, and standalone archives.
- `codesign --verify --deep --strict` passed on the three packaged ad-hoc-signed
  bundles; their Mach-O executables are arm64.
- `cmake --build --preset dev -j8`: passed after the TODO implementation.
- `ctest --preset dev --output-on-failure`: 3/3 passed, including camera reset,
  trail subdivision, per-voice roots, chord/strum scheduling, and the populated
  HYG catalogue validation.
- `cmake --build --preset plugin-debug -j8`: passed with no warnings for arm64
  AU, VST3, and standalone.
- `ctest --preset plugin-debug --output-on-failure`: all executable tests
  passed (core, presets, and plugin); the Metal smoke test was skipped because
  this execution environment exposed no Metal device. Plugin coverage includes
  `.3bs` JSON round trips, malformed-file rejection, hidden-field re-save, and
  host recall after a configuration load.
- Offline `xcrun metal` validation was attempted but the installed Xcode lacks
  the optional Metal Toolchain component.
- Standalone was launched externally on Apple Silicon; runtime Metal shader
  compilation passed. The tabbed deck was manually verified by switching to the
  Presets page. The final cloud/trail attenuation build was compiled into all
  targets.
- The TODO build was activated in the existing standalone instance and captured
  in the foreground: the HYG star field, smooth subdivided trails, selected tab,
  larger dial labels, equal-width NEW SYSTEM/SET STATE/RESET actions, and the
  three-line note-pane icon were visible. Voices/Presets page interaction,
  double-click reset, and chord audition still require hands-on verification.
- `codesign --verify --deep --strict`: passed for all three ad-hoc-signed bundles.
- Bundle metadata identifies AU type `aumi`, subtype `Tbs1`, manufacturer `Krhd`.
- GitHub Actions run `27852219185`: native core tests and the complete arm64
  Release bundle build passed.
- A fresh standalone launch for the bloom/framing/note-pane build could not be
  approved in that run; Logic, Ableton, VST3 plugin validation, and 60 fps
  profiling remain unverified.

## Known issues, risks, and limitations

- GitHub repository `krahd/3bs` is public on `main`; the alpha prerelease and
  Pages site are live.
- AU MIDI effects and VST3 MIDI generation still require real host validation.
- Apple signing and notarization are blocked until Developer credentials exist.
- Authored user-preset library management and advanced graphics controls are not
  yet exposed. Complete configuration import/export is available as `.3bs`.
  Per-planet enable/root/scale/pitch/trigger controls now exist;
  per-voice range and custom-scale editing remain available only through presets.
- The reversed-Z depth fix, glyph atlas, and vertical FastTracker II note view
  compile and pass the Metal smoke test but still need a clean foreground visual
  check (legibility, scrolling, and the absence of z-fighting at close approach).
- The corrected bloom orientation, automatic framing, note overlay, and pane
  hit testing need one clean foreground visual and interaction check.
- The TODO-3 Metal overlap cross-fade and revised FTII/preset/Voices pages build,
  but a fresh foreground restart was blocked by the execution approval limit;
  their final visual and interaction pass remains unverified.
- Allocation/lock instrumentation and baseline 60 fps profiling have not run.

## Pending tasks

- Complete hands-on mouse/trackpad interaction, final foreground renderer
  inspection, and 60 fps profiling.
- Test the installed AU MIDI effect in Logic.
- Run plugin validation and test VST3 MIDI routing in Ableton.
- Complete remaining voice controls (range and custom scale) and authored
  user-preset library management.
- Profile processing allocation/locking and renderer frame time.

## Next steps

1. Complete interaction/performance and host validation.
2. Close the remaining control-surface gaps and rerun the full suite.
3. Continue alpha.2 release validation in Logic and Ableton.

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

Last updated: 2026-06-21 21-55 GMT-3
