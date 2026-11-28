# Core implementation verification

Date: 2026-11-27. Local compiler: GCC 14.2.0, Linux x86_64, C++20. This evidence describes the shared source tree after the core repairs; it does not modify or replace the original audit reproductions.

## Results

| Target | Result | Evidence |
|---|---|---|
| Existing and restored core tests | 24 named cases passed | `core-tests.log` |
| Targeted core regressions | 7 groups passed | `core-regressions.log` |
| Portable standalone clock/mailbox/ledger/epoch tests | 32 checks passed | `standalone-tests.log` |
| Address/undefined-behavior/float-cast sanitizers | 7 regression groups passed | `core-regressions-sanitized.log` |
| Numerical diagnostics | Analytic convergence, extreme work bound, all 24 actual factory presets passed | `numerical-presets.log` |
| Core processing profile | 24 cases; zero observed C++ allocations/frees and zero budget overruns | `core-realtime-profile.jsonl` |

All five optimized, warning-enabled direct builds completed with no compiler diagnostics. Their exact commands and the executed verification commands are recorded in `commands.json`; build logs are retained separately. LeakSanitizer was disabled because this execution environment uses ptrace, which prevents LeakSanitizer from running. This is not evidence of leak freedom.

## Core behavior covered

The explicit constructor preserves supplied voice definitions, including all-C/channel-1 configurations. Active notes retain their original channel/key for cleanup. Reconfiguration, disabling, gate closure, voicing changes, reset, and panic invalidate pending strums. A per-voice change preserves unaffected voices. SpeedPeak now tracks changes between actual physics measurements and detects a positive-to-negative speed slope, without treating the intervening unchanged audio samples as peaks.

Production adapters use the synchronous `EventSink` process overload. The original dense 16,384-sample fixture now delivers all 1,050 events. Streams match across Independent/Chord/Strum, 44.1/48/96 kHz, block sizes 1/32/64/127/128/512/1,024/2,048/16,384, and varying partitions. The old fixed 1,024-event convenience collector remains deliberately bounded and reports 1,024 delivered plus 26 rejected events in that fixture. It is not a lossless production adapter.

Tempo-relative pending strums retain their musical spacing through tempo changes. Millisecond strums retain elapsed-time spacing through sample-rate changes. Internal auto reset advances the trajectory revision before the sink receives new-generation events. The aligned quarter-beat reset fixture produces starts at samples 0 and 6,000 without a stale pre-reset retrigger.

Simulation/configuration candidates are validated before replacement. Nonfinite and out-of-domain input is rejected without corrupting the previous valid state. Oversized advance work is bounded before integer conversion, and dropped excess advancement is explicitly counted. Published preset defaults, the force model, and velocity-Verlet integration are unchanged.

The standalone destination ledger now supports failure-aware cleanup. Tests exercise a wholly failed flush, a partial successful flush, repeated failure, successful reconnection, exact original channel/key retention, repeated note identities, and idempotent completion. Only accepted note-offs remove cleanup obligations. The runtime retains failed destination cleanup for retry, and native tests additionally assert zero live-transition output failures; destructor cleanup is verified by the captured receiver ledger after the runtime object is destroyed.

## Numerical interpretation

The softened equilateral analytic orbit gives position errors 4.33231273435e-05, 1.08308947418e-05, and 2.70773073423e-06 at steps 1/120, 1/240, and 1/480. Refinement ratios are 3.99995830227 and 3.99998958717, consistent with second-order convergence.

The preset driver reads `resources/presets/factory-presets.json` directly, rather than a copied list. Every preset is checked for deterministic repeat, finite state, angular-momentum behavior, and bounded work. Published escape/respawn behavior is distinguished from a conservative diagnostic run. The largest measured conservative energy excursion is 4.19048862768% for `red-shift-study`; halving the step reduces that diagnostic to 0.227388452954%. Close encounters and long chaotic trajectory separation make these observations diagnostic; they do not justify silently replacing published initial conditions, defaults, or the model.

## Timing and native limits

The core profile records p50/p95/p99/max block time, cold/warm C++ allocation/free counters, callback budget, and overruns. It covers all three modes, three sample rates, 64/512/16,384-sample blocks, controllers, dense clocks, and configuration/reset transitions. On this run the maximum observed block time was 7,622.937 microseconds and the largest observed budget fraction was 0.583355. These are measurements in a shared Linux execution environment, not macOS/DAW real-time qualification. C allocation calls and host-owned `juce::MidiBuffer` storage are outside this core counter. In particular, a plugin callback may allocate when its host supplies insufficient MIDI output capacity; the core zero-allocation result does not establish arbitrary-host allocation freedom.

`tests/StandaloneRuntimeTests.cpp` adds a native CoreMIDI virtual-source loopback test for worker timing, message-thread stalls, reset/panic/stop/output replacement, and destructor cleanup. The source is present but was not built or executed in this Linux environment. The default real-duration test is short; `THREEBS_STANDALONE_STRESS_SECONDS=1800` requests a 30-minute run. A simulated 30-minute absolute-clock test already passed; it must not be reported as a completed 30-minute native soak. Parent-owned CI records native results separately.

The independent JUCE 8.0.13 source review identified message-thread restrictions on MIDI device enumeration/opening. At the runtime owner's request, `src/standalone/CoreMidiOutput.h` now provides a direct CoreMIDI backend: delivery-thread-owned client/port/virtual-source resources, stable decimal destination identifiers, offline detection at endpoint/entity/device level, explicit return status for each send, and fixed stack event-list storage. It uses protocol-explicit MIDI 1.0 channel-voice UMP events supported by the existing macOS minimum. The virtual source retains the earlier JUCE bundle/name-derived identifier when available. Main/runtime integration and native build execution belong to the parent-owned CI work; this header was not compiled on Linux.

Relevant primary API references are Apple's [source creation](https://developer.apple.com/documentation/coremidi/midisourcecreatewithprotocol(_:_:_:_:)), [event-list construction](https://developer.apple.com/documentation/coremidi/midieventlistadd(_:_:_:_:_:_:)), [output delivery](https://developer.apple.com/documentation/coremidi/midisendeventlist(_:_:_:)), [virtual-source delivery](https://developer.apple.com/documentation/coremidi/midireceivedeventlist(_:_:)), and [offline flag](https://developer.apple.com/documentation/coremidi/kmidipropertyoffline) documentation. JUCE's restrictions appear in [the pinned device-list broadcaster](https://github.com/juce-framework/JUCE/blob/8.0.13/modules/juce_audio_devices/midi_io/juce_MidiDeviceListConnectionBroadcaster.cpp). Native runtime status must be taken from the final integrated source and CI, not this portable evidence alone.

## Historical fixtures

`legacy-fixture-search.json` records every published tag's complete source tree and the release asset listings. No authentic saved user configuration or host/DAW state fixture was found in those trees. Historical migration tests construct current states and modify schema fields. Release archives were not downloaded or executed. This search does not establish that private or untracked user sessions never existed.
