# Checkpoint 6: integrated reliability evidence

**Review date: 27 November 2026, America/Montevideo (GMT-3).** Runner timestamps use UTC on 28 November. Native CTest started at **21:52:03 GMT-3**; the long soak started at approximately **21:57:18 GMT-3**. All five workflow jobs had completed successfully when checked at **22:27:38 GMT-3**. UTC midnight does not change the local review date.

This checkpoint built AU, VST3 and standalone, passed **18/18 native tests**, passed both portable suites and both ThreadSanitizer jobs, recalled newly generated prior-executable fixtures, passed AU and VST3 validation, completed the **30-minute virtual CoreMIDI soak**, and produced verified development packages. PR #2 was then merged with an integration tree identical to the tested tree. These results establish the recorded automated scope; real DAW sessions, physical MIDI routing and display qualification remain separate acceptance work.

## Source and execution provenance

| Item | Exact reference |
| --- | --- |
| Pull request | [krahd/3bs #2](https://github.com/krahd/3bs/pull/2) |
| Branch head | [`581548314288311da805da7a7c7a3f0cdaedc536`](https://github.com/krahd/3bs/commit/581548314288311da805da7a7c7a3f0cdaedc536) |
| Actual tested PR merge | [`da925c1f3d6d043e314f78af7d4a2ba22966750d`](https://github.com/krahd/3bs/commit/da925c1f3d6d043e314f78af7d4a2ba22966750d), recorded in `evidence/environment.txt` |
| Integration merge | [`bdc32750b8677c06630295cb1467c29de7646ed5`](https://github.com/krahd/3bs/commit/bdc32750b8677c06630295cb1467c29de7646ed5) |
| Tested and integrated tree | `ae1530038ecfb42ced1306655075a04766b8f160`; equality verified against the remote integration commit |
| Workflow | [Run 46089189294](https://github.com/krahd/3bs/actions/runs/46089189294), [pinned workflow](https://github.com/krahd/3bs/blob/da925c1f3d6d043e314f78af7d4a2ba22966750d/.github/workflows/ci.yml) |
| Native job | [140805849590](https://github.com/krahd/3bs/actions/runs/46089189294/job/140805849590) |
| Actual-processor TSan job | [140805849721](https://github.com/krahd/3bs/actions/runs/46089189294/job/140805849721) |
| Environment | macOS 15.8.2, build 24H232, arm64; Xcode 16.4, build 16F6; CMake 3.22.6; deployment target macOS 13.0 |
| JUCE | [`7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`](https://github.com/juce-framework/JUCE/commit/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2), reported as JUCE 8.0.13 by the TSan executable |

The complete checkpoint-5-to-6 comparison contains one production source change: the plugin timing-fault message uses an ASCII hyphen in place of an implicitly encoded em dash. No UI layout, renderer or shader source changed between these two checkpoints. CI registration support and evidence/documentation changes account for the remaining differences. The gravitational MIDI/artwork concept and visual language remain unchanged.

## Native, portable and concurrency tests

`ci-plugin/native-results.xml` records **18 tests, zero failures, zero disabled tests and zero skips**. The suite duration field is 32 seconds at integer precision; individual durations below retain the XML precision.

| Native test | Seconds |
| --- | ---: |
| `threebs_core_tests` | 0.331763 |
| `threebs_core_regression_tests` | 0.555282 |
| `threebs_numerical_tests` | 0.0129241 |
| `threebs_numerical_presets` | 0.549418 |
| `threebs_core_realtime_profile` | 1.25969 |
| `threebs_input_key_ledger_tests` | 0.0960725 |
| `threebs_command_mailbox_tests` | 0.0258605 |
| `threebs_standalone_tests` | 0.016002 |
| `threebs_renderer_data_tests` | 0.019652 |
| `threebs_factory_presets` | 0.254039 |
| `threebs_voicing_presets` | 0.0983166 |
| `threebs_star_catalog` | 0.0193614 |
| `threebs_ui_smoke` | 11.7867 |
| `threebs_metal_smoke` | 1.95638 |
| `threebs_plugin_tests` | 2.11825 |
| `threebs_processor_concurrency_tests` | 7.2817 |
| `threebs_realtime_profile` | 2.00267 |
| `threebs_standalone_runtime_tests` | 4.39157 |

Both portable jobs passed **12/12 tests without skips**: the macOS test durations sum to **2.40077245 seconds**, and Ubuntu durations sum to **3.33566448 seconds**. These are sums of individual testcase times, not measured wall-clock totals. The separate portable ThreadSanitizer job also passed.

The actual processor concurrency test completed in Release with **384 publications, 768 save/restore pairs, 22,291 callbacks, 2,298 notes and 14,946 snapshots**; `errors=0`, `final_ack=1`, `timed_out=0`, with a 20-second deadline. Under ThreadSanitizer it completed in **177.14 seconds**, with **384 publications, 768 save/restore pairs, 115,639 callbacks, 8,070 notes and 48,345 snapshots**; `errors=0`, `final_ack=1`, `timed_out=0`, with a 600-second deadline. This exercises the actual processor's publication, state replacement and callback ownership, rather than only a queue surrogate.

Sources: native `ci-plugin/Testing/Temporary/LastTest.log`, native JUnit XML, portable artifact JUnit files, and the actual-processor TSan artifact's `LastTest.log` in the linked run.

## UI and Metal verification

UI acceptance passed **15 page/size layouts**: System, Voices, Space, Presets and Settings at **900×600, 1280×820 and 1920×1400 logical pixels**. It also passed the existing mode/unit cases, keyboard focus, presentation mode, strict numeric parsing, dialog validation, unchanged high-precision values, initial leading-text presentation, current-value tooltip assertions and owner-close/queued-Apply lifetime cases. The fresh factory selection remained unmodified after initial messages were dispatched. The test does not write the user's preset files.

Visual review inspected all **15 main-page PNGs**, the **five advanced-dialog PNGs** (exact body state, three planet pages and system settings), and the separate Metal drawable PNG from this final artifact. No new control clipping, overlap, encoding damage or design drift was observed. Exact-state fields initially expose the leading sign/digits; long values retain their complete editable text and tooltip content. Tooltip content is asserted programmatically; these captures do not demonstrate a hovered tooltip on every display configuration.

The main-page images are JUCE component captures: their blank scene region is not evidence of missing Metal rendering. The separate `evidence/metal-frame.png` contains the completed drawable with the three textured bodies, star field and dense note-history panel. The Metal integration test reported **27 completed frames**, **13,824 glyph bytes**, **zero GPU errors** and successful handling of **all 21 injected resource-allocation failures** with Metal API validation enabled. The corresponding resource-failure log messages are expected injection results. The test observes drawable completion and errors rather than treating renderer construction as successful rendering.

This establishes component layout and the tested drawable path on this CI machine. It does not establish Retina/backing-scale behavior, sustained interactive GPU performance, visual behavior across GPU families, or simultaneous long MIDI stress with continuous rendering and user interaction. FPS text within layout snapshots is incidental, not a benchmark.

## Callback profiling

The actual-processor profile measured **2,188 callbacks at 48 kHz** with **zero counted C allocations, C++ allocations or frees** inside each measured callback. The harness preallocates the host MIDI buffer and performs preparation outside the callback; the dense-release scenario includes 32,768 input release events.

| Scenario | Blocks × frames | Mean µs | Maximum µs |
| --- | ---: | ---: | ---: |
| Cold command consumption | 1 × 512 | 190.208 | 190.208 |
| Short input and long SysEx | 128 × 128 | 26.3845 | 77.25 |
| Dense input releases | 4 × 64 | 240.469 | 268.167 |
| Steady state | 2,048 × 512 | 124.202 | 4,273.12 |
| Reset with input | 1 × 64 | 90.625 | 90.625 |
| State replacement, large block | 4 × 16,384 | 3,417.97 | 3,595.54 |
| Bypass cleanup | 1 × 512 | 2.25 | 2.25 |
| Panic cleanup | 1 × 512 | 112.208 | 112.208 |

The mixed-size aggregate is **124.667 µs mean / 4,273.12 µs maximum** and is diagnostic only. These observations are not a universal callback deadline guarantee.

The independent core profile recorded **24 cases and 2,784 measured blocks**, covering all three modes, 44.1/48/96 kHz, 64/512-sample blocks, large blocks and transitions. It counted zero cold/warm C++ allocations or frees and **zero deadline overruns in this run**. Its maximum was **7,737.5 µs** in mode 2 at 48 kHz with 16,384-sample blocks, against that case's **341,333.33 µs** budget. Checkpoint 5's **two timing overruns remain historical evidence** in [CHECKPOINT-5.md](CHECKPOINT-5.md); the current result does not erase them or prove their cause resolved.

## CoreMIDI delivery and 30-minute soak

The short native runtime test completed its requested 2-second stress in **2.0626 seconds**, captured **66 note-ons**, and measured **16 period intervals** with one initial onset excluded. It reported **2.40025 ms period drift**, **5.09654 ms maximum interval error**, delivery lateness p50/p95/p99/max of **5 / 8.3 / 8.4 / 8.323 ms**, **117 delivered events**, **zero output failures before destruction, worker overruns or test failures**, and **80 visual drops during the intentional UI stall**. Percentiles use the diagnostic histogram, so a rounded percentile bucket may exceed the exact observed maximum.

The final long-soak JSON records:

| Measurement | Result |
| --- | ---: |
| Requested stress | 1,800 seconds |
| Measured stress | 1,800.14 seconds |
| Wrapper elapsed time | 1,802.3648152080002 seconds |
| Captured note-ons, all voices | 43,218 |
| Voice-1 stress note-ons | 14,401 |
| Excluded initial onsets | 1 |
| Measured period intervals | 14,400 |
| Initial-to-grid interval | 0.125467 seconds |
| Period drift | 9.8305 ms |
| Maximum interval error | 14.2097 ms |
| Delivery lateness p50 / p95 / p99 / maximum | 5 / 11.2 / 12.3 / 17.422 ms |
| Delivered events | 86,421 |
| Output failures before destruction | 0 |
| Worker overruns / test failures | 0 / 0 |
| Visual drops during intentional message-thread stall | 73,378 |
| Process exit / recorded status | 0 / passed |

The delivered-event and output-failure counters are explicitly sampled before destruction. The test separately checks transition and cleanup delivery. The scope is **actual virtual CoreMIDI delivery while message-thread dispatch is stalled**; visual drops are expected under that deliberate stall. This is not a physical-device or DAW session, and the stalled-UI soak does not establish sustained combined MIDI/render performance. Sources are `evidence/standalone-soak.json`, its full log and the native test log.

## Compatibility and validators

The workflow captured **newly generated prior-executable fixtures** using the actual executable source at [`06066967b243e250d97da37ea0195b31f5c9521c`](https://github.com/krahd/3bs/commit/06066967b243e250d97da37ea0195b31f5c9521c), with an added capture harness. The producer schemas are **host state 7 / portable configuration 3**. Both default and edited configurations passed current recall and immediate resave. The provenance explicitly makes **no claim that these are historical user sessions**. All four downloaded file hashes were recomputed and matched the manifest:

| Fixture | SHA-256 |
| --- | --- |
| `default.bin` | `39f86548713dec14d1a20515f05c0c7e33569d2c61681c76ef0e13877a350e34` |
| `default.3bs` | `1f816a65819dff8faae4590889c350e57f0451e9d6667d06f719d8de73ee5cd3` |
| `edited.bin` | `6eae05d43b6138664c2c7d89348cd0cdb0c9a4625ed8fa8653a4a46053d52a64` |
| `edited.3bs` | `7a328767747533f74386f6efce4112cff19ca8a08b31c80674fd9b19af3d1d63` |

The reproducible capture and recall sources are [capture_legacy_states.py](https://github.com/krahd/3bs/blob/da925c1f3d6d043e314f78af7d4a2ba22966750d/scripts/capture_legacy_states.py), [CaptureLegacyStates.cpp](https://github.com/krahd/3bs/blob/da925c1f3d6d043e314f78af7d4a2ba22966750d/tests/CaptureLegacyStates.cpp) and [LegacyStateTests.cpp](https://github.com/krahd/3bs/blob/da925c1f3d6d043e314f78af7d4a2ba22966750d/tests/LegacyStateTests.cpp). Fixture bytes and their manifest are in the native artifact's `evidence/legacy-states/`.

The installed arm64 AU passed signature verification, appeared in component discovery as **`aumi Tbs1 Tmlz`**, and `/usr/bin/arch -arm64 /usr/bin/auval -v aumi Tbs1 Tmlz` ended **`AU VALIDATION SUCCEEDED`**. It identified Tomas Laurenzo, The Three Body Solution, version 0.1.0. This closes the current CI discovery/validation gate; checkpoint 5's discovery failure remains recorded in its own evidence.

VST3 **pluginval passed at strictness 5**, seed **`0x8f0aa1`**, ending **`SUCCESS`**. Its optional external Steinberg validator was skipped because no validator path was supplied; that skip is not a pluginval failure and is not a claim that Steinberg's separate validator ran. Full outputs are `evidence/auval.txt`, `au-discovery.txt`, `au-installation.txt` and `pluginval.txt`.

## Artifact integrity and development packages

The downloaded native evidence, portable evidence, actual-processor TSan evidence and candidate artifact ZIPs were checked against their reported SHA-256 digests. These links identify the exact artifacts of the tested run; GitHub artifact retention may eventually expire.

| Artifact | ID and download page | ZIP SHA-256 |
| --- | --- | --- |
| Native evidence | [15241948542](https://github.com/krahd/3bs/actions/runs/46089189294/artifacts/15241948542) | `ad7320c97d87e9f0a8f616e409ca776c566382da74eb8ddc915120832531805f` |
| Portable macOS | [15239758681](https://github.com/krahd/3bs/actions/runs/46089189294/artifacts/15239758681) | `4255c9581aa25bbb2ac23e60c791046da66c64438e077b94c3da3cb720136d81` |
| Portable Ubuntu | [15240303339](https://github.com/krahd/3bs/actions/runs/46089189294/artifacts/15240303339) | `c55f6d1fead955da36b0aab81291734bbf442cc573e2495e414a7064f96f68a9` |
| Actual-processor TSan | [15240458126](https://github.com/krahd/3bs/actions/runs/46089189294/artifacts/15240458126) | `2e42f1e62a92f91d5b52d812cb6af75baaa6839b409c745f01d21979fbc04545` |
| Development candidate | [15242395298](https://github.com/krahd/3bs/actions/runs/46089189294/artifacts/15242395298) | `cf064ba63195c94df0561ceeaf4169c6df838b078e7b4bd8d544ed6e18cbad76` |

Candidate provenance identifies the tested merge, **Release / arm64 macOS 13+**, JUCE revision above, and **ad hoc signing, not notarised**. The four inner ZIPs passed byte-hash and archive-integrity checks; their bundle executables and recorded provenance were inspected.

| Inner archive | Bytes | SHA-256 |
| --- | ---: | --- |
| `ThreeBS-AU-da925c1f3d6d-macOS-arm64.zip` | 3,489,933 | `4f55b5311f1e253369545bf51c323873e7d728fbabf30bdc0c9a2701b542459a` |
| `ThreeBS-VST3-da925c1f3d6d-macOS-arm64.zip` | 3,598,777 | `dc0090629b313048a4358a343c0f28af8382588bd8e576c8394ac2b59b0a9165` |
| `ThreeBS-Standalone-da925c1f3d6d-macOS-arm64.zip` | 3,439,217 | `2720ccbefba24d4c8c1c3f84d551465412e92870bec0825cc5bf8a547acddd1c` |
| `ThreeBS-Complete-da925c1f3d6d-macOS-arm64.zip` | 10,480,751 | `476cc4b9e9f298bd18c69f33f8504701c5195d08159ba9ded55aa60de167d509` |

## Remaining acceptance boundary

The integrated implementation has passed the recorded automated gates. The remaining work is the documented host-qualification protocol: Logic AU MIDI-FX and Ableton VST3 workflows, automation readback, transport/bypass/reset and session recall in those hosts; physical MIDI destinations and reconnect/cleanup behavior; actual historical user-session files if available; Retina/backing scales and sustained GPU/combined MIDI-render interaction. Distribution signing/notarisation is also outside these ad hoc development packages. No user Mac, real DAW session or physical MIDI device was accessed for this checkpoint.

The existing acceptance document and host-qualification protocol govern those checks. This record does not turn CI success into a completed host-acceptance or public-release claim.
