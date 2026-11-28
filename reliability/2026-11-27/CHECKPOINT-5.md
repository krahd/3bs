# Checkpoint 5: native reliability evidence

**Review date: 27 November 2026, America/Montevideo (GMT-3).** The runner timestamps are UTC on 28 November. Native CTest ran at approximately **21:28:01–21:28:31 GMT-3**; prior-executable recall and AU validation ran at **21:32:16 GMT-3**. UTC midnight does not change the local review date.

This checkpoint built AU, VST3 and standalone, passed **18/18 native tests**, passed the portable and ThreadSanitizer jobs, and passed recall/immediate resave of newly generated prior-executable fixtures. The workflow then **failed AU component discovery**. VST3 validation, the 30-minute virtual CoreMIDI soak and candidate packaging did not run. This is implementation evidence, not full host acceptance or a qualified public release.

## Source and execution provenance

| Item | Exact reference |
| --- | --- |
| Pull request | [krahd/3bs #2](https://github.com/krahd/3bs/pull/2) |
| Requested branch head | [`449f126e6cdea64214374bb0280fa8d16b6d0a82`](https://github.com/krahd/3bs/commit/449f126e6cdea64214374bb0280fa8d16b6d0a82) |
| Actual checked-out candidate | [`16a16372fba7be2a6e7b81a0de73aea7bd9b5826`](https://github.com/krahd/3bs/commit/16a16372fba7be2a6e7b81a0de73aea7bd9b5826), recorded in the native artifact's `evidence/environment.txt` |
| Workflow | [Run 46086793103](https://github.com/krahd/3bs/actions/runs/46086793103), [pinned workflow source](https://github.com/krahd/3bs/blob/16a16372fba7be2a6e7b81a0de73aea7bd9b5826/.github/workflows/ci.yml) |
| Native job | [140798149313](https://github.com/krahd/3bs/actions/runs/46086793103/job/140798149313) |
| Environment | macOS 15.8.2, arm64; Xcode 16.4, build 16F6; CMake 3.22.6; macOS deployment target 13.0 |
| JUCE | [`7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`](https://github.com/juce-framework/JUCE/commit/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2), reported as JUCE 8.0.13 by the TSan executable |

The branch head and actual tested merge candidate are deliberately distinguished. Results below belong to the tested candidate and cannot automatically certify subsequent changes.

## Native test results

`ci-plugin/native-results.xml` records **18 tests, zero failures, zero disabled tests and zero skips**. The native job reports 29.35 seconds total elapsed CTest time. `ci-plugin/Testing/Temporary/LastTest.log` retains complete successful-test output where the JUnit XML truncates long streams.

| Test | Result | Seconds |
| --- | --- | ---: |
| `threebs_core_tests` | Pass | 0.308803 |
| `threebs_core_regression_tests` | Pass | 0.503150 |
| `threebs_numerical_tests` | Pass | 0.007159 |
| `threebs_numerical_presets` | Pass | 0.617482 |
| `threebs_core_realtime_profile` | Pass | 1.554530 |
| `threebs_input_key_ledger_tests` | Pass | 0.106835 |
| `threebs_command_mailbox_tests` | Pass | 0.015702 |
| `threebs_standalone_tests` | Pass | 0.014320 |
| `threebs_renderer_data_tests` | Pass | 0.023211 |
| `threebs_factory_presets` | Pass | 0.345172 |
| `threebs_voicing_presets` | Pass | 0.243263 |
| `threebs_star_catalog` | Pass | 0.039659 |
| `threebs_ui_smoke` | Pass | 13.031200 |
| `threebs_metal_smoke` | Pass | 1.761580 |
| `threebs_plugin_tests` | Pass | 1.948330 |
| `threebs_processor_concurrency_tests` | Pass | 2.725520 |
| `threebs_realtime_profile` | Pass | 1.754400 |
| `threebs_standalone_runtime_tests` | Pass | 4.313780 |

The plugin reliability regressions, including finite subnormal persistence, passed. The previous checkpoint's representable-underflow failure is therefore closed for this executed regression set.

## Portable and concurrency checks

Both portable suites passed **12/12 tests**: [Ubuntu job 140798149266](https://github.com/krahd/3bs/actions/runs/46086793103/job/140798149266), 3.28 seconds; [macOS job 140798149343](https://github.com/krahd/3bs/actions/runs/46086793103/job/140798149343), 3.91 seconds. These jobs also exercised the documented CMake 3.22.6 minimum.

The [portable ownership ThreadSanitizer job](https://github.com/krahd/3bs/actions/runs/46086793103/job/140798148947) passed four tests: core regressions, command mailbox, standalone scheduling and renderer data. Total time was 25.74 seconds; no TSan warning was reported.

The [actual-processor ThreadSanitizer job](https://github.com/krahd/3bs/actions/runs/46086793103/job/140798149180) passed in **171.674 seconds**, with the following recorded workload:

| Metric | TSan | Native Release |
| --- | ---: | ---: |
| Configuration publications | 384 | 384 |
| Save/restore pairs | 768 | 768 |
| Processing callbacks | 63,919 | 34,132 |
| Notes observed | 4,896 | 3,111 |
| Snapshots observed | 37,521 | 27,201 |
| Errors | 0 | 0 |
| Final revision acknowledged | Yes | Yes |
| Timed out | No | No |
| Configured deadline | 600 s | 20 s |

The instrumented workload completed with no reported data race; the result covers this bounded concurrent exercise.

## UI and Metal evidence

The UI smoke test passed the five main pages at **900×600, 1280×820 and 1920×1400 logical pixels**, all voicing modes and strum units, keyboard focus, text entry, presentation entry/escape, strict numeric parsing, exact untouched values, dialog validation and owner-close races. It also passed the new assertions that the initial numeric caret and character-zero geometry are visible and that each tooltip returns the complete current text, including after an edit.

The artifact contains 15 main-page PNGs under `evidence/ui/<width>x<height>-<page>.png` and five advanced captures: `advanced-body-state.png`, `advanced-planet-1.png`, `advanced-planet-2.png`, `advanced-planet-3.png` and `advanced-system.png`. All five corrected advanced captures were visually inspected. Leading signs/digits are visible, range punctuation is readable, and no further clipping or overlap was found within the preserved layout. A separate tooltip mouse-hover screenshot was not captured.

Metal API validation was enabled. The actual drawable integration completed **27 frames**, uploaded **13,824 glyph bytes**, reported **zero GPU errors** and handled **all 21 injected resource-allocation failures**. The completed-drawable readback is `evidence/metal-frame.png`. Main-page JUCE snapshots cover controls; the separate Metal readback covers the native drawable. This short integration does not establish sustained GPU behavior or multiple display-scale acceptance.

## Allocation and timing profiles

The actual processor profile recorded **zero C++ allocations, zero observed C allocations and zero frees across 2,188 callbacks** at 48 kHz. Its [pinned source](https://github.com/krahd/3bs/blob/16a16372fba7be2a6e7b81a0de73aea7bd9b5826/tests/RealtimeProfile.cpp) makes host-buffer capacity explicit: 4 MiB of JUCE MIDI-buffer capacity, with a separately prepared 32,768-event release workload. Input preparation is outside the measured callback. These conditions do not establish allocation-free behavior for arbitrary host-owned capacity.

| Scenario | Blocks | Frames/block | Mean µs | Maximum µs |
| --- | ---: | ---: | ---: | ---: |
| Cold command consumption | 1 | 512 | 198.041 | 198.041 |
| Short input and long SysEx | 128 | 128 | 26.7698 | 155.791 |
| Dense input releases | 4 | 64 | 241.500 | 262.042 |
| Steady state | 2,048 | 512 | 109.072 | 913.625 |
| Reset with input | 1 | 64 | 17.916 | 17.916 |
| State replacement, large block | 4 | 16,384 | 3,116.49 | 3,135.00 |
| Bypass cleanup | 1 | 512 | 1.625 | 1.625 |
| Panic cleanup | 1 | 512 | 103.584 | 103.584 |

The mixed-size aggregate is 109.945 µs mean and 3,135 µs maximum; it is a diagnostic, not a single callback deadline.

The separate core profile contains **24 scenarios and 2,784 measured blocks**, with zero cold/warm C++ allocations and frees. It records **two deadline overruns** in mode 1, 48 kHz, 512-frame transition profiling: 17,894 µs maximum against a 10,666.667 µs block budget. The test reported pass while retaining these timing diagnostics. Zero-overrun acceptance must not be inferred from the successful test exit.

## Short real CoreMIDI loopback

The native standalone test used a test-created virtual CoreMIDI source and connected only its own loopback. It exercised delivery while the JUCE message thread was stalled; reset, panic, stop/resume, output replacement, reconnection to the replacement route and destruction cleanup passed their delivered-note-ledger assertions.

| Recorded metric | Value |
| --- | ---: |
| Requested / measured stress interval | 2 s / 2.08995 s |
| Voice-1 note-ons during stress | 17 |
| Excluded initial onset / measured periodic intervals | 1 / 16 |
| Initial-to-grid interval | 0.105732 s |
| Total captured note-ons | 66 |
| Period drift | −0.00210979 s |
| Maximum interval error | 0.00573604 s |
| Delivery lateness p50 / p95 / p99 | 3.0 / 7.4 / 7.7 ms |
| Maximum delivery lateness | 7.677 ms |
| Diagnostic delivered-event count | 117 |
| Output failures before destruction / worker overruns / test failures | 0 / 0 / 0 |
| Visual snapshots dropped during the deliberate message-thread stall | 83 |

The displayed percentile estimates and maximum retain their original reported rounding. Four `CoreMIDI error, line 1778 - ffffffce` diagnostics also appear in the log; they are not omitted from the retained evidence. The test nevertheless established its virtual route, delivered events and passed cleanup/identity checks. The diagnostic delivered-event snapshot precedes final destruction cleanup; the post-destruction ledger assertions passed separately.

This was a **two-second runtime stress interval**, not the later 30-minute soak. The portable scheduling test's simulated 30-minute timeline is also distinct from a measured native soak. No physical-device or real-host timing claim follows.

## Newly generated prior-executable compatibility fixtures

These are **newly generated prior-executable fixtures**, not historical user sessions. The producer is the actual prior-version executable built from [`06066967b243e250d97da37ea0195b31f5c9521c`](https://github.com/krahd/3bs/commit/06066967b243e250d97da37ea0195b31f5c9521c), with an added capture harness and build target. It emits **host schema 7** and **portable schema 3**. The current candidate passed immediate recall and resave for both default and edited states.

The [capture script](https://github.com/krahd/3bs/blob/16a16372fba7be2a6e7b81a0de73aea7bd9b5826/scripts/capture_legacy_states.py), [capture harness](https://github.com/krahd/3bs/blob/16a16372fba7be2a6e7b81a0de73aea7bd9b5826/tests/CaptureLegacyStates.cpp) and [recall test](https://github.com/krahd/3bs/blob/16a16372fba7be2a6e7b81a0de73aea7bd9b5826/tests/LegacyStateTests.cpp) define the reproducible procedure. `evidence/legacy-states/PROVENANCE.json` records the producer commit, schemas and scope. All four downloaded fixture byte hashes were independently verified against that manifest:

| Fixture | SHA256 |
| --- | --- |
| `default.bin` | `39f86548713dec14d1a20515f05c0c7e33569d2c61681c76ef0e13877a350e34` |
| `default.3bs` | `1f816a65819dff8faae4590889c350e57f0451e9d6667d06f719d8de73ee5cd3` |
| `edited.bin` | `6eae05d43b6138664c2c7d89348cd0cdb0c9a4625ed8fa8653a4a46053d52a64` |
| `edited.3bs` | `7a328767747533f74386f6efce4112cff19ca8a08b31c80674fd9b19af3d1d63` |

The job printed both `PASS prior-executable host7 and portable3 default -> current immediate recall/resave` and the equivalent edited-state pass. These results establish the generated compatibility cases, without attributing them to any user's saved session.

## AU discovery failure and unexecuted gates

`/usr/bin/auval -v aumi Tbs1 Tmlz` exited with code **2**. `evidence/auval.txt` reports that component name strings and component version could not be retrieved, version error **−50**, followed by failure to find the component. The built bundle therefore did not reach a successful validation in this run. Component-discovery/registration remediation requires fresh execution; its outcome is not assumed here.

The following steps were skipped after that failure: pluginval 1.0.4 validation, the 30-minute virtual CoreMIDI soak and development-candidate packaging. Logic/Ableton routing and teardown, physical MIDI loss/reconnection, display scales and sustained combined MIDI/render sessions remain open under `RELIABILITY-ACCEPTANCE.md`.

## Retained artifacts and digests

| Artifact | GitHub ID and link | ZIP SHA256 |
| --- | --- | --- |
| Native evidence | [15240480488](https://github.com/krahd/3bs/actions/runs/46086793103/artifacts/15240480488) | `2b709b45bf7ec1854873a2b8c611b50e77b452f898c896ac862eb1761db4885a` |
| Actual-processor TSan evidence | [15239398406](https://github.com/krahd/3bs/actions/runs/46086793103/artifacts/15239398406) | `23ff4537c94a2be9af6c1f82426c459cbf38fa7101ef6ac3e4e82d91432da530` |
| Portable macOS evidence | [15239860190](https://github.com/krahd/3bs/actions/runs/46086793103/artifacts/15239860190) | `1f66f513f117e4adc7b18ac0ff87a92e297faa63fe09901cdbb31a92f7aef48c` |
| Portable Ubuntu evidence | [15239038833](https://github.com/krahd/3bs/actions/runs/46086793103/artifacts/15239038833) | `2be3c7d1e8e1e7888d6fe360b0618318a1eba167543795fd516f0ee795cd51ce` |

The native and actual-processor TSan ZIP digests were verified against downloaded bytes. Portable ZIP digests are the values reported by GitHub; their successful test totals were read from the corresponding job logs. Artifact access follows repository permissions and GitHub retention. This record preserves the exact outcomes, provenance and fixture hashes; subsequent candidate results should be recorded separately.
