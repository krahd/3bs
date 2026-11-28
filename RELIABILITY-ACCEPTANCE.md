# Reliability implementation and acceptance

This work implements the 27 November 2026 audit in `AUDIT-AND-IMPROVEMENT-PLAN.md`.
The original gravitational MIDI instrument, three formats, factory systems,
procedural planetary scene and Verlet solver remain the governing design.
The dated audit and alpha.3 release are historical records.

## Candidate status

Implementation is in progress. Portable core, numerical, renderer-data and
standalone scheduling checks have executed locally. Native compilation,
integration, rendering and format validation are being run against the
candidate branch. A candidate archive is not a qualified public release.

Checkpoint `988b5fd625bd192c3b34dd8b6bb35c98d7e14340` built all Release formats
and passed 14 of 15 native CTest entries in
[run 46079678297](https://github.com/krahd/3bs/actions/runs/46079678297).
Metal completed 27 frames, exercised 13,824 glyph bytes and all 21 injected
resource failures, and reported zero GPU errors. The processor profile recorded
zero C/C++ allocations or frees across 2,184 callbacks. The UI test stopped when
a requested native window exceeded the runner display; the corrected harness
preserves the logical layout inside a visible viewport. Newer changes and the
remaining native gates require their own execution evidence.

Checkpoint `268a77fcced238ccdafd5767db44f665f76a64ec` passed all 12 portable
CTest entries on both Linux and macOS and the separate queue/ownership TSan
job in [run 46082083483](https://github.com/krahd/3bs/actions/runs/46082083483).
The actual-processor TSan test reached its original 20-second deadline after
127 publications, 53 save/restore pairs and 4,176 callbacks; no race warning or
assertion error was reported during that partial run. Its instrumented limit
is now 600 seconds, retaining the full 384 publications, 768 save/restore pairs,
final acknowledgement and receiver-ledger checks. Ordinary builds retain the
20-second limit. The Release build passed 17 of 18 native entries. Its only
failure was the editor-recall test's single 80 ms timer-dispatch assumption;
JUCE can wait 300 ms for an outstanding timer message. The regression now
dispatches until the unchanged complete-state assertions pass, bounded to one
second. Product editor behaviour is unchanged. All 15 main-page size/layout
captures passed inspection. The two-second real CoreMIDI loopback and actual
processor concurrency tests passed; all 2,188 profiled callbacks reported zero
C/C++ allocations/frees. Final validators, compatibility capture and the long
CoreMIDI soak have not yet run.

Checkpoint `9c00dbfb70d54aeef70b16c95acaa39f1c0ce1d8` built all formats with
CMake 3.22.6 and passed both 12-entry portable suites. The actual-processor
ThreadSanitizer gate passed in 139.28 seconds: 384 publications, 768 save/restore
pairs, 100,415 callbacks, zero errors and final revision acknowledgement
([run 46084843522](https://github.com/krahd/3bs/actions/runs/46084843522)).
Native CTest again passed 17/18 entries; the only failure was the new subnormal
recall regression. Apple's C++ stream conversion marks representable underflow
as failure, so the shared strict decimal parser now uses explicit C-locale
conversion, retaining finite subnormal results. Normal coordinate precision,
bounded file loading, numeric/depth rejection and editor recall passed.
The five advanced-dialog captures also exposed implicit non-ASCII punctuation
and initially scrolled numeric fields; those presentation repairs are awaiting
native re-execution. No tolerance or configuration bound was weakened.

## Implemented contracts

- Core MIDI uses an allocation-free event sink; bounded diagnostic collectors
  report rejected events. Actual channel/key identities own cleanup. Disabled
  voices, closed gates and changed voicings cancel incompatible pending notes.
- Incoming plugin MIDI is applied at sample offsets with held-note identities.
  Ignored long messages are inspected without constructing an owning message.
- Complete validated state revisions cross a bounded mailbox. Visual queues
  discard a new snapshot when full; only their consumer advances the read index.
- Host state is version 8; portable `.3bs` state is version 4, including meter.
  Legacy parameter IDs and normalisation ranges remain intact. Exact values
  survive coarse controls until the corresponding control actually changes.
  File loading reads at most 2 MiB plus one detection byte; numeric preflight
  rejects unsafe integer/exponent accumulation and nesting beyond 64 levels
  before the dependency's parser runs. Scientific round-trip formatting and
  exact duplicate host attributes preserve physical coordinates and velocities.
- Standalone generation uses a monotonic sample timeline, 20 ms look-ahead,
  fixed event storage and a separate destination-owning MIDI worker. A 100 ms
  late deadline or saturated queue cancels the epoch, cleans delivered notes
  and pauses generation until Reset or a new Run transition. This is an
  explicit failure threshold, not a measured jitter acceptance tolerance.
  Direct CoreMIDI client/port lifetime belongs to that worker. Failed note-offs
  remain in the destination's ledger for retry; unavailable destinations retain
  their outstanding notes until reconnection. Per-voice changes cancel only
  affected queued and delivered voices.
- Dynamic Metal buffers belong to completed frames; glyph instances use proper
  buffer storage. Timestamped trail history is independent of tessellation.
  Rendering failures remain independent of MIDI processing.
- The existing five pages expose additional controls through Voice / System
  Settings. Settings also manages authored `.3bs` copies. Device and window
  choices remain outside musical files. Notifications have their own location,
  separate from renderer telemetry.

## Automated acceptance

The CI workflow builds Release AU, VST3 and standalone bundles on arm64 macOS,
executes applicable native tests, runs real drawable Metal checks with API
validation, invokes `auval` and pluginval 1.0.4, and retains logs and candidate
archives. A Metal-less runner reports a skip rather than a render pass.
Portable and native Release builds install the official, checksum-verified
CMake 3.22.6 release and configure through the documented presets, exercising
the stated minimum minor version rather than relying only on newer runners.
Portable builds also execute Linux/macOS tests and a separate ThreadSanitizer
ownership check. Another ThreadSanitizer job exercises the actual processor
under concurrent audio, configuration publication and save/restore. The native
workflow captures state from the exact pre-repair executable, verifies recall
and immediate resave, then runs a 30-minute virtual CoreMIDI timing/cleanup
soak. Unavailable CoreMIDI reports a skip, never a delivery pass. Numerical
reports record finite state, energy, angular
momentum, work bounds and fixed-step refinement without imposing regularity on
chaotic or escaping systems.

Core allocation reports measure C++ allocation/free calls. The native processor
profile additionally observes supported C allocator paths. Host-owned JUCE MIDI
buffer capacity is a separate contract: a plugin cannot promise allocation-free
host storage of arbitrary capacity. Profiles record their preallocation and
input conditions instead of generalising one empty-input result.

JUCE processor destruction and `releaseResources()` provide no outgoing host
MIDI buffer. Pending cleanup is delivered on the next valid callback; final
teardown without a further callback depends on the host terminating notes.
Logic/Ableton teardown sessions therefore remain necessary. Standalone owns
its output and performs its cleanup before closing that destination.

## Required native sessions

The following evidence requires the actual host/device environment and remains
open until recorded with versions, build SHA, destination and results:

| Session | Required observations |
| --- | --- |
| Logic AU MIDI FX | Routing, input gate/transpose, stop/reset/seek/loop/bypass, editor-open recall, save/reopen and note ledger at teardown |
| Ableton VST3 | MIDI routing, multiple instances, automation, offline processing, lifecycle and exact-state recall |
| Standalone | Virtual and physical destination change/loss/reconnection, quit cleanup and 30-minute UI/render stress with jitter percentiles, maximum lateness and drops |
| Native interaction | Minimum/default/maximum sizes and display scales; focus/text entry; presentation escape; advanced-dialog parent closure; authored-file round trip |
| Visual comparison | Matched planets, bloom, camera and trails at representative presets; dense note history; resize and editor close/reopen |
| Distribution | Signing identity and notarisation, independently evidenced if a public release is prepared |

Automated native evidence must not be substituted for Logic/Ableton routing or
physical-device measurements. No user Mac access is assumed by these checks.
