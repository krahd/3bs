# The Three Body Solution - Project Status

Last updated: 2026-06-20 00-33 GMT-3

## Project purpose

The Three Body Solution is a deterministic generative MIDI instrument and
standalone artwork driven by a normalized three-body gravitational simulation.

## Current implementation state

The repository builds a deterministic core library, Logic AU MIDI effect,
Ableton-oriented VST3 instrument, and standalone CoreMIDI application. It has a
native interactive HDR Metal presentation layer, shared control deck, 24
schema-v2 factory presets, host state serialization, automated tests, and macOS
CI.

## Active focus

Complete external Metal, Logic, and Ableton runtime validation for the existing
three-format vertical slice before making the repository public.

## Architecture overview

The framework-independent core owns physics, measurements, mapping, MIDI event
scheduling, deterministic random state, and fixed-capacity queues. Thin JUCE
adapters translate host transport and MIDI. The Metal renderer consumes
immutable snapshots without touching real-time processing.

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
    <text x="795" y="238" font-size="18">Presentation</text><text x="795" y="264" font-size="13" fill="#9cb4c8">JUCE control deck</text><text x="795" y="286" font-size="13" fill="#9cb4c8">NSView + MetalKit renderer</text>
  </g>
  <g stroke="#64748b" stroke-width="2" fill="none" marker-end="url(#arch-arrow)">
    <path d="M230 89 C275 89 285 145 330 145"/><path d="M230 189 L330 189"/><path d="M230 289 C275 289 285 235 330 235"/><path d="M580 155 C625 155 635 93 680 93"/><path d="M580 228 C625 228 635 260 680 260"/>
  </g>
</svg>

### Data-flow diagram

Commands and render snapshots use fixed-capacity queues; MIDI stays on the
processing path.

<svg xmlns="http://www.w3.org/2000/svg" width="960" height="330" viewBox="0 0 960 330" role="img" aria-labelledby="flow-title flow-desc">
  <title id="flow-title">Real-time data flow</title><desc id="flow-desc">Host transport and parameters enter processing, simulation produces mappings and MIDI, while snapshots go independently to Metal.</desc>
  <defs><marker id="flow-arrow" markerWidth="10" markerHeight="10" refX="9" refY="3" orient="auto"><path d="M0,0 L0,6 L9,3 z" fill="#64748b"/></marker></defs>
  <rect width="960" height="330" rx="18" fill="#080d18"/>
  <g fill="#111b2d" stroke="#34506f" stroke-width="2">
    <rect x="35" y="52" width="170" height="66" rx="10"/><rect x="35" y="215" width="170" height="66" rx="10"/><rect x="280" y="105" width="190" height="92" rx="10"/><rect x="545" y="55" width="170" height="70" rx="10"/><rect x="545" y="207" width="170" height="70" rx="10"/><rect x="790" y="55" width="135" height="70" rx="10"/><rect x="790" y="207" width="135" height="70" rx="10"/>
  </g>
  <g fill="#d9e7f2" font-family="system-ui, sans-serif" text-anchor="middle">
    <text x="120" y="80" font-size="16">Transport</text><text x="120" y="101" font-size="12" fill="#8ba3b8">parameters + MIDI in</text><text x="120" y="243" font-size="16">Control Deck</text><text x="120" y="264" font-size="12" fill="#8ba3b8">preset/reset commands</text>
    <text x="375" y="139" font-size="17">processBlock</text><text x="375" y="162" font-size="12" fill="#8ba3b8">consume · advance · schedule</text>
    <text x="630" y="85" font-size="16">MIDI Events</text><text x="630" y="106" font-size="12" fill="#8ba3b8">sample offsets</text><text x="630" y="235" font-size="16">Render Snapshot</text><text x="630" y="256" font-size="12" fill="#8ba3b8">immutable body state</text>
    <text x="857" y="86" font-size="16">Host / MIDI</text><text x="857" y="238" font-size="16">Metal 60 fps</text><text x="857" y="259" font-size="12" fill="#8ba3b8">when available</text>
  </g>
  <g stroke="#64748b" stroke-width="2" fill="none" marker-end="url(#flow-arrow)">
    <path d="M205 85 C245 85 245 135 280 135"/><path d="M205 248 C245 248 245 172 280 172"/><path d="M470 136 C505 136 510 90 545 90"/><path d="M715 90 L790 90"/><path d="M470 170 C505 170 510 242 545 242"/><path d="M715 242 L790 242"/>
  </g>
</svg>

## Setup and run instructions

Requirements are Apple Silicon macOS 13+, Xcode, CMake 3.22+, and recursive Git
submodules. Use `cmake --preset dev` for core tests or
`cmake --preset plugin-debug` for all formats, then the matching build and test
presets. Detailed commands and artifact paths are in `README.md`. In VS Code,
run the `3bs: Run Standalone` task to build the standalone target and execute
the application with its output visible in a dedicated terminal.

## Configuration and environment variables

No environment variables are required. CMake options control plugin/test builds
and local ad-hoc signing. Signing credentials must never be stored in the
repository.

## Important files and directories

- `README.md`: project overview and eventual build/use instructions.
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

## Tests and verification status

- `cmake --preset dev`: passed.
- `cmake --build --preset dev -j 8`: passed.
- `ctest --preset dev --output-on-failure`: 2/2 passed.
- Full plugin configure/build: passed for arm64 AU, VST3, and standalone.
- Full CTest: core, schema-v2 preset, and processor/state tests passed; Metal
  smoke test skipped because the sandbox returned no Metal device.
- Standalone was launched externally on Apple Silicon; runtime Metal shader
  compilation passed and the procedural planets, atmosphere, trails, deep-sky
  background, bloom, and tone mapping rendered successfully.
- `codesign --verify --deep --strict`: passed for all three ad-hoc-signed bundles.
- Bundle metadata identifies AU type `aumi`, subtype `Tbs1`, manufacturer `Krhd`.
- GitHub Actions run `27852219185`: native core tests and the complete arm64
  Release bundle build passed.
- `auval -v aumi Tbs1 Krhd`: attempted and failed discovery because the local
  component was not installed in the Audio Unit search path.
- Standalone launch, installed `auval`, pluginval, Logic, Ableton, and 60 fps
  profiling remain unverified.

## Known issues, risks, and limitations

- GitHub repository `krahd/3bs` exists and was verified private on `main`.
- AU MIDI effects and VST3 MIDI generation still require real host validation.
- Apple signing and notarization are blocked until Developer credentials exist.
- The pinned full HYG v4.1 payload could not be downloaded in this environment;
  the generator, provenance, optional embedded target, real bright-star fallback,
  and deterministic faint field are present.
- User preset file import/export, detailed per-voice mapping UI, and advanced
  graphics controls are not yet exposed.
- Allocation/lock instrumentation and baseline 60 fps profiling have not run.

## Pending tasks

- Populate and verify the pinned compact HYG catalogue.
- Complete hands-on mouse/trackpad interaction and 60 fps profiling.
- Install and validate AU with `auval`, then test it in Logic.
- Run plugin validation and test VST3 MIDI routing in Ableton.
- Complete advanced editing, voice, camera, and user-preset controls.
- Profile processing allocation/locking and renderer frame time.

## Next steps

1. Complete interaction/performance and host validation.
2. Close the remaining control-surface gaps and rerun the full suite.
3. Audit licences/secrets, tag `v0.1.0-alpha.1`, and make the repository public.

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

Last updated: 2026-06-20 00-33 GMT-3
