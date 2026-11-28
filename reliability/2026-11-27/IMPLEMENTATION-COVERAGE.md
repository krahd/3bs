# Final audit coverage amendment — checkpoint 6

The implementation is integrated and its automated acceptance gates passed. This amendment supplements the [checkpoint-5 coverage matrix](https://github.com/krahd/3bs/blob/581548314288311da805da7a7c7a3f0cdaedc536/reliability/2026-11-27/AUDIT-COVERAGE.md) and preserves its historical observations. It does not mark every work package or release gate closed.

| Identity | Value |
|---|---|
| Audit baseline | `06066967b243e250d97da37ea0195b31f5c9521c` |
| Checkpoint-5 branch / tested merge | `449f126e6cdea64214374bb0280fa8d16b6d0a82` / `16a16372fba7be2a6e7b81a0de73aea7bd9b5826` |
| Current candidate branch | `581548314288311da805da7a7c7a3f0cdaedc536` |
| Tested merge / integrated merge | `da925c1f3d6d043e314f78af7d4a2ba22966750d` / `bdc32750b8677c06630295cb1467c29de7646ed5` |
| Identical tested and integrated tree | `ae1530038ecfb42ced1306655075a04766b8f160` |
| Executed workflow / native job | [46089189294](https://github.com/krahd/3bs/actions/runs/46089189294) / `140805849590` |
| Current overall workflow result | All five jobs completed successfully: native bundles, both portable platforms, queue/ownership TSan and actual-processor TSan |

## Implementation and remote evidence integrity

The audit's first-party reliability repairs are implemented within the existing three-body design. The gravitational model, Verlet solver, factory data, musical vocabulary, parameter identities and three formats remain. Implementation completion is distinct from full release qualification.

A read-only check of checkpoint 6's remote tree verified **all 87 archived evidence files**, totalling **967,158 bytes**, against their local Git blob hashes and sizes. All **13 product-source hashes** in the musical-refinement provenance match that remote tree. The compiled diagnostic binary is absent; its report's relative link to the sibling numerical log resolves. The decoded default/edited legacy binary fixtures match their recorded SHA-256 digests.

Checkpoint 6 adds the archived evidence and a bounded AU registration/discovery check. The only production `src/` change from checkpoint 5 is the plugin error-notification's ASCII punctuation correction. The old `STATUS.md` task list has been replaced with current acceptance gates. No core, solver, preset or default change was introduced by this checkpoint.

## P0 disposition

The bounded source/evidence review found **no additional known semantic omission in the repaired P0 paths**. This statement covers the inspected implementation and exercised regressions; external lifecycle and environment qualifications remain below.

| P0 finding | Implemented contract and retained passing evidence | Remaining boundary |
|---|---|---|
| **A01 — note lifecycle** | Original channel/key cleanup, pending cancellation, per-voice preservation and failure-aware destination retry. Core/processor regressions and actual virtual-CoreMIDI reset, panic, stop, route replacement and destruction cleanup passed. | A plugin destructor/`releaseResources()` has no outgoing host buffer. Final teardown without another callback requires host note termination. Physical disconnect/reconnect is not established by virtual-source replacement. |
| **A02 — event saturation** | Production streaming delivers the complete 1,050-event reproduction across large/small/mixed blocks. Bounded diagnostic collectors report rejection; native output exceeds the old 1,024-event limit. | Host-owned MIDI-buffer capacity remains outside the plugin's admission control; allocation observations retain their preallocation condition. |
| **A03 — input timing** | Sample-offset segmentation, equal-time ordering and bounded channel/key ownership passed equivalent-timeline partition tests, duplicate/unmatched release and CC120/123 cases. | Actual DAW routing/session qualification remains separate. |
| **A06 — visual queue ownership** | Producers drop new visual updates on saturation; only consumers move the read index. Queue stress and TSan passed. Real virtual-MIDI delivery continued during message-thread stalls and visual drops. | Sustained simultaneous renderer/GPU stress and routed-MIDI acceptance remain open. |
| **A07 — coherent commands/state** | Complete mailbox revisions, intentional coalescing, retained reset semantics and applied acknowledgement. Actual processor publication/save/restore completed under TSan with no reported error and final acknowledgement. | Evidence covers the exercised workload. Host scalar automation remains a separate edit stream. |
| **A10 — invalid state** | Complete validation before publication, finite/enum/range checks, bounded file/depth handling and guarded numerical conversion. Portable sanitizer and native strict/exact/subnormal persistence regressions passed. | Targeted validation evidence, not exhaustive fuzzing. |
| **A12 — GPU resource ownership** | Completion-owned frame resources, buffered glyphs and complete allocation readiness. Actual drawable/readback exercised 13,824 glyph bytes, all 21 allocation failures and zero GPU errors. | Fullscreen/recreation, display scales and sustained maximum-trail GPU workload require their own sessions. |

All other A01–A16 mappings and W0–W8 distinctions remain in the checkpoint-5 matrix. The punctuation and stale-task-text items identified there are implemented in checkpoint 6; their earlier state remains an accurate historical record.

## Final automated outcomes

Current measurements are from the executed checkpoint-6 artifacts and job results. The native environment was macOS 15.8.2 arm64, Xcode 16.4 and CMake 3.22.6; Release bundles retain the macOS 13 minimum and pinned JUCE 8.0.13 commit `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`. The [checkpoint-6 record](CHECKPOINT-6.md) retains the final measurements and artifact identities.

| Gate | Retained checkpoint-5 result | Current candidate |
|---|---|---|
| Minimum CMake / portable | CMake 3.22.6; 12/12 entries on Linux and macOS | CMake 3.22.6; 12/12 on Ubuntu 24.04 and 12/12 on macOS 15, zero failures/skips; 3.33566448 s and 2.40077245 s respectively |
| Queue/ownership TSan | Passed | Passed the standalone, core regression, command mailbox and renderer data tests under ThreadSanitizer; job `140805849753` |
| Actual processor TSan | 384 publications, 768 save/restore pairs, 63,919 callbacks; zero errors, acknowledgement 1; 171.67 s | Passed in 177.14 s: 384 publications, 768 save/restore pairs, 115,639 callbacks, 8,070 notes, 48,345 snapshots; errors 0, acknowledgement 1, timeout 0; no TSan report |
| Native Release integration | AU/VST3/standalone built; 18/18 tests, no skips | AU/VST3/standalone built from tested merge `da925c1f`; 18/18 native CTests, zero failures/skips. Metal: 27 completed frames, 13,824 glyph bytes, zero GPU errors, all 21 allocation failures handled |
| Prior-executable recall | Default and edited host-7/portable-3 capture, recall and immediate resave passed | Passed default and edited host-7/portable-3 capture, recall and immediate resave. Producer: actual baseline `06066967b243e250d97da37ea0195b31f5c9521c` executable with capture harness; generated compatibility fixtures, not historical user sessions |
| AU discovery and `auval` | **Failed before open: component not found** | Installed metadata, arm64 binary and strict ad-hoc signature verified; bounded exact `aumi/Tbs1/Tmlz` discovery passed; authoritative arm64 `auval -v` returned exit 0 and `AU VALIDATION SUCCEEDED` |
| VST3 pluginval 1.0.4 | Not reached | Passed pluginval 1.0.4 at strictness 5; retained `evidence/pluginval.txt` ends `SUCCESS`. The optional separate Steinberg SDK validator was unconfigured and skipped |
| Thirty-minute virtual CoreMIDI | Not reached; only the short live test passed | Passed: 1,800.14 s measured stress (1,802.364815208 s wrapper), 14,400 periodic intervals, 43,218 captured note-ons and 86,421 delivered events. Delivery lateness p50/p95/p99/max: 5 / 11.2 / 12.3 / 17.422 ms; output failures 0, worker overruns 0, assertion failures 0, balanced receiver ledgers, no capture drops; 73,378 expendable visual updates dropped during the deliberate stall |
| Candidate packaging | Not reached | Passed: candidate artifact `15242395298`; four ZIP archives verified against SHA-256, CRC and embedded provenance for tested merge `da925c1f`. Complete archive SHA-256: `476cc4b9e9f298bd18c69f33f8504701c5195d08159ba9ded55aa60de167d509` |

The long virtual-source test deliberately stalled message dispatch. Its passing result does not establish the separate combined interactive UI/GPU stress or physical-MIDI qualification. One immediate onset was excluded from periodic analysis; measured periodic drift was 9.8305 ms and maximum interval error 14.2097 ms. Visual drops are intentional expendable-queue losses, not dropped MIDI. The runtime's 100-ms late-failure threshold is not a musical jitter tolerance. The logs retain transient JUCE/CoreMIDI input-endpoint metadata diagnostics during test-side connection/reconnection; measured production output failures remained zero.

The actual processor profile observed zero C/C++ allocations or frees across 2,188 callbacks with 4 MiB of preallocated host MIDI storage. The 16,384-frame replacement case had a 3,595.54-µs maximum; the separate 512-frame steady case had a 4,273.12-µs maximum. These conditions and differing block sizes matter: the profile is not an arbitrary-host storage or universal callback-deadline guarantee. The final native core profile recorded zero overruns across its 24 scenarios and 2,784 measured blocks; the preceding checkpoint's two overruns remain below.

## Historical failures and timing observations retained

- **AU discovery failure:** checkpoint 5's `auval` reported unavailable name/version data and “didn't find the component” for `aumi/Tbs1/Tmlz`. Checkpoint 6's successful registration and validation supersede the blocker for that candidate; the observed checkpoint-5 failure remains historical evidence.
- **Core-profile deadline overruns:** checkpoint 5 measured **two overruns** in the 48-kHz, 512-frame Chord transition case, with **17,894 µs maximum** against a **10,666.667 µs budget**. Its 24 scenarios contained 2,784 measured blocks. Allocation/numerical assertions passed; timing was diagnostic. Do not infer zero overruns from the test's successful exit or replace this observation with a later faster sample.
- **Separate processor profile:** checkpoint 5 observed zero C/C++ allocations/frees across **2,188 actual callbacks with 4 MiB of host MIDI storage**. Its mixed-size 109.945-µs mean and 3,135-µs maximum do not constitute a common block-budget threshold or an arbitrary-host capacity guarantee.
- **Short real CoreMIDI evidence:** the 2.08995-second stall passed continuity and cleanup with zero output failures/worker overruns. Its periodic analysis excluded one immediate initial onset, and the recorded logs retain transient JUCE input-open diagnostics. This short checkpoint-5 result remains separate from the completed checkpoint-6 30-minute result.
- Earlier UI-harness, editor-timer-test and subnormal-parser failures remain in their checkpoint records. The corresponding exercised regressions passed checkpoint 5; they are not silently omitted from the acceptance history.

## Numerical and musical evidence

The archived [musical-refinement diagnostic](https://github.com/krahd/3bs/blob/581548314288311da805da7a7c7a3f0cdaedc536/reliability/2026-11-27/refinement/REPORT.md) closes the previously missing per-preset event-trace measurement: **48 runs**, all **24 actual factory systems**, published and half steps, each **60 beats**. All receiving ledgers ended empty without rejected events or unmatched offs. Complete traces differed in **21/24** systems, per-voice counts in **9/24**, and ordinal-paired pitches in **13/24**.

Those differences are descriptive fixed-step sensitivity. Pairing the nth note after trigger insertions/deletions is not a causal match or jitter estimate. Published-policy respawn differences are retained. The existing conservative energy/momentum/separation and analytic convergence evidence supplies a distinct physical diagnostic. No physical default, musical threshold or release tolerance was changed.

The preserved older profiling branch contains longer sampling at one block/rate; the new profiles broaden exercised paths with shorter samples. They must not be represented as identical stress coverage.

## Acceptance that remains open even after automated success

The repairs and automated gates do not complete W8. The outstanding acceptance requires recorded environment/version/build/destination evidence for:

- Logic AU and Ableton VST3 routing, host save/reopen, editor-open recall, loop/seek/bypass, automation, multiple instances, offline processing and final teardown note termination.
- Physical CoreMIDI destination change/loss/reconnection/quit, and sustained MIDI measurement combined with actual interactive UI/GPU load.
- Representative display scales, matched scene captures, camera/pane interaction, fullscreen/editor recreation and sustained maximum-trail rendering.
- Authored Save/Load/Folder actions and the complete preset→edit→record→save→reopen workflow in each format.
- Public distribution qualification, including any separately evidenced Developer ID signing/notarization. A candidate archive remains distinct from a qualified public release.

## Disposition

The reliability repairs and reproducible diagnostics are integrated without changing the instrument's governing concept. The source tree completed both portable suites, both ThreadSanitizer jobs, all 18 native tests, prior-executable recall/resave, AU validation, pluginval, the 30-minute virtual CoreMIDI soak and candidate packaging. Required automated gates passed. The optional separate Steinberg SDK validator was not run. The recorded real-host, physical-device, display and combined-render sessions remain open under the [host qualification protocol](HOST-QUALIFICATION.md); W8 and public-release qualification remain incomplete. The candidate is ad-hoc signed and not notarised.

