# Reliability implementation and acceptance

This work implements the 27 November 2026 audit in `AUDIT-AND-IMPROVEMENT-PLAN.md`.
The original gravitational MIDI instrument, three formats, factory systems,
procedural planetary scene and Verlet solver remain the governing design.
The dated audit and alpha.3 release are historical records.

## Candidate status

The integrated source implementation and all required automated gates passed for
candidate `581548314288311da805da7a7c7a3f0cdaedc536`, tested as merge
`da925c1f3d6d043e314f78af7d4a2ba22966750d`. Both identify source tree
`ae1530038ecfb42ced1306655075a04766b8f160`. The integration commit is
`bdc32750b8677c06630295cb1467c29de7646ed5`; source-tree equivalence was verified remotely.
Results and candidate archives belong to
[run 46089189294](https://github.com/krahd/3bs/actions/runs/46089189294).

| Gate | Executed result |
| --- | --- |
| Portable CTest on Linux and macOS | 12/12 on each platform, zero skips, CMake 3.22.6 |
| Queue/ownership ThreadSanitizer | Passed |
| Actual processor ThreadSanitizer | 384 publications, 768 save/restore pairs, 115,639 callbacks; zero errors, final acknowledgement; 177.14 seconds |
| Release AU, VST3 and standalone | Built for arm64 macOS 13+ |
| Native CTest | 18/18, zero failures or skips; real Metal drawable and native UI checks included |
| Exact prior-executable recall/resave | Default and edited host-7/portable-3 configurations passed |
| AU | Installed metadata, arm64 architecture, signature, bounded discovery and authoritative auval passed |
| VST3 | pluginval 1.0.4, strictness 5, passed |
| Virtual CoreMIDI real-duration gate | 1,800.14 measured seconds; 86,421 delivered events; zero output failures, worker overruns or unmatched notes |
| Candidate archives | Complete, AU, VST3 and standalone ZIPs built; all four SHA-256 digests, archive integrity and embedded provenance verified |

The [final native evidence record](reliability/2026-11-27/CHECKPOINT-6.md)
retains environment, individual timing/allocation results, raw measurement
values, artifact digests and provenance. Development candidates are ad-hoc
signed and not notarised. They are distinct from the historical alpha.3 files.

Actual Logic/Ableton, physical-device, display-scale and combined sustained
MIDI/render sessions remain open under the
[host qualification procedure](reliability/2026-11-27/HOST-QUALIFICATION.md).
The complete plan is not release-qualified solely by these automated gates.

The [implementation coverage amendment](reliability/2026-11-27/IMPLEMENTATION-COVERAGE.md)
records the disposition of the audit findings without closing the outstanding
host and device qualification. The optional separate Steinberg SDK validator
was not configured and was explicitly skipped; the required VST3 gate was
pluginval, which passed.

The soak measured delivery lateness of 5 / 11.2 / 12.3 ms at p50 / p95 / p99,
with a 17.422 ms maximum. Its 73,378 discarded visual snapshots were intentional
queue saturation during stalled message dispatch, not lost MIDI. These are
observations from a virtual CoreMIDI CI session; they do not define an accepted
performance jitter tolerance or qualify combined interactive UI/GPU stress.

## Intermediate checkpoints

The following records retain the outcomes and outstanding gates at each earlier
checkpoint. The current-candidate table above supersedes their pending status.

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

Checkpoint `449f126e6cdea64214374bb0280fa8d16b6d0a82` passed both 12-entry
portable suites with CMake 3.22.6, queue/ownership TSan and actual-processor TSan
in [run 46086793103](https://github.com/krahd/3bs/actions/runs/46086793103).
The latter completed all 384 publications and 768 save/restore pairs in
171.67 seconds, with 63,919 callbacks, 4,896 notes, 37,521 snapshots, zero errors
and final revision acknowledgement. Release compilation and the native
integration step also passed, including exact subnormal recall. A final text
check found the same implicit non-ASCII constructor issue in the error-only
plugin timing notification. Its ASCII-only repair is being included before the
remaining validator/soak/package gates. The actual pre-repair executable
produced default and edited host-7/portable-3 files, and both passed current
recall and immediate resave. AU validation then failed before opening the
component because the runner could not find its registration. A bounded
CI-only metadata/signature/discovery check now precedes authoritative auval;
that repair, pluginval, the long soak and candidate packaging require execution.
The separate native core profile recorded two deadline overruns in the Chord
transition case; the actual processor profile recorded zero C/C++ allocations
or frees in all 2,188 callbacks. Timing observations are retained with their
individual sample counts and budgets, not generalised to every host.

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

## Factory musical refinement

The [reproducible diagnostic](reliability/2026-11-27/refinement/REPORT.md)
compares all 24 actual factory systems at the published step and half that
step, each for 60 beats at 120 BPM / 48 kHz. All 48 runs had finite state,
bounded work, valid MIDI and empty final receiving ledgers. Full traces differ
in 21 of 24 systems; per-voice counts differ in 9 and ordinal-paired pitch in
13. Ordinal pairing after changed trigger counts is descriptive, not a causal
note match or jitter measurement. Published-policy respawns remain part of
those music runs. The original fixed step, presets and solver remain intact.

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
