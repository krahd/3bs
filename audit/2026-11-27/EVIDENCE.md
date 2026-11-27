# Audit evidence — 27 November 2026

Audited production revision: `06066967b243e250d97da37ea0195b31f5c9521c`.

These diagnostics exercise unchanged production code. They are deliberately separate from the normal regression suite. Their exit code is not a blanket pass/fail judgement: several programs print a reproduced defect and exit zero; the dormant-test and non-finite probes intentionally exit nonzero on this baseline.

## Environment and boundaries

Execution used Linux, GCC 14 and C++20. CMake was absent. No JUCE native build, Metal frame, CoreMIDI destination, Logic/Ableton session, `auval` or `pluginval` was run. All runtime claims below identify whether they execute the actual core/queue/helper or mirror a specific adapter decision. The source/API findings in the plan require native acceptance where stated.

## Existing core suite

Compiled and executed with:

```bash
g++ -std=c++20 -O0 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Isrc \
  src/core/MusicEngine.cpp src/core/Scale.cpp src/core/ThreeBodySimulation.cpp \
  src/render/CameraController.cpp src/render/PresentationState.cpp \
  tests/CoreTests.cpp -o ../audit-build/core-tests
../audit-build/core-tests
```

Result: exit 0, `All core tests passed`. Compiler warnings identified five defined-but-unused tests: `testOctaveTranspose`, `testDurationMapping`, `testGlobalChordFrame`, `testStrumBeatUnit`, `testBarAutoReset`.

## Targeted reproductions

Commands are run from the repository root. Create the isolated output directory first:

```bash
mkdir -p ../audit-build
```

### core-probes

Uses the actual MusicEngine. Five independent cases reproduce configuration replacement, false SpeedPeak events during monotonic acceleration, pending notes after disable/gate closure, incorrect cleanup channel, and event-buffer loss. Overflow configuration: three enabled clock voices, clock division 1/128 beat, minimum trigger interval zero, ContinuousLegacy duration 1/1024 beat, 240 BPM at 48 kHz, 16,384-sample horizon plus one final sample.

```bash
g++ -std=c++20 -O2 -Isrc audit/2026-11-27/core-probes.cpp src/core/MusicEngine.cpp src/core/Scale.cpp src/core/ThreeBodySimulation.cpp -o ../audit-build/core-probes && ../audit-build/core-probes
```

Observed exit code: 0.

```text
constructor requested octave=2 enabled=[1,0,0], got octave=0 enabled=[1,1,1]
speed_peak monotonic_window: increases=96 decreases=0 note_ons=4
pending_strum after_disable_all_and_close_gate note_ons=2
channel_change note_on_channel=1 note_off_channel=7
buffer_overflow @240bpm 16384_samples: partition_512_events=1050 single_block_events=1024 unmatched_note_ons=2
```

### input-reset

The reset case executes the same reset signature as the standalone callbacks. The input case mirrors PluginProcessor.cpp:481–489 (all input applied before the block), then runs the actual core. It is not a native JUCE processor test. The input timeline is NoteOn at absolute sample 100 and NoteOff at 900 over 1,024 samples.

```bash
g++ -std=c++20 -O2 -Isrc audit/2026-11-27/input-reset-probe.cpp src/core/MusicEngine.cpp src/core/Scale.cpp src/core/ThreeBodySimulation.cpp -o ../audit-build/input-reset-probe && ../audit-build/input-reset-probe
```

Observed exit code: 0.

```text
standalone-reset-path note_ons_before_reset=1 note_offs_recoverable_after_reset=0
adapter-input-batching note_ons_1024_block=0 note_ons_128_blocks=5
```

### queue-current

Uses the actual SpscQueue<RenderSnapshot,64>, two threads and two million writes. The first run applies the producer-pop-on-full policy currently used by the processor. The control uses the same queue but drops a new snapshot on saturation. Exact race counts vary; this is one observed pair of runs. The control establishes the consequence of respecting index ownership in this probe, not universal queue correctness.

```bash
g++ -std=c++20 -O2 -pthread -Isrc audit/2026-11-27/queue-probe.cpp -o ../audit-build/queue-probe && ../audit-build/queue-probe && ../audit-build/queue-probe control
```

Observed exit code: 0.

```text
mode=current-producer-pop reads=68159 full=1926532 corrupt=155 duplicate_or_regressed=280
mode=control-drop-new reads=81599 full=1918401 corrupt=0 duplicate_or_regressed=0
```

### dormant-tests

Includes the original CoreTests.cpp under a renamed main and invokes its five omitted functions. Octave/duration failures stem from the constructor's replacement of an explicit configuration. The bar-fraction strum assertion expects an event at sample 24,000 while its helper renders only samples below 24,000. That failure must be repaired in the fixture before asserting a strum scheduling bug.

```bash
g++ -std=c++20 -O2 -Isrc audit/2026-11-27/dormant-tests.cpp src/core/MusicEngine.cpp src/core/Scale.cpp src/core/ThreeBodySimulation.cpp src/render/CameraController.cpp src/render/PresentationState.cpp -o ../audit-build/dormant-tests && ../audit-build/dormant-tests
```

Observed exit code: 1.

```text
FAIL: per-voice octave shifts notes up by twelve semitones
FAIL: per-voice octave shifts notes down by twelve semitones
testOctaveTranspose: 2 failed assertions
FAIL: legacy continuous duration mapping preserves varied note lengths
testDurationMapping: 1 failed assertions
testGlobalChordFrame: 0 failed assertions
FAIL: bar-fraction strum spacing scales by the time signature
testStrumBeatUnit: 1 failed assertions
testBarAutoReset: 0 failed assertions
```

### nonfinite

Injects NaN into one initial body position, then processes the actual engine under undefined-behaviour/float-cast-overflow sanitisation. This proves the downstream failure from malformed state. The corresponding unvalidated host-restore path was inspected, but not executed in a DAW.

```bash
g++ -std=c++20 -O1 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all -Isrc audit/2026-11-27/nonfinite-probe.cpp src/core/MusicEngine.cpp src/core/Scale.cpp src/core/ThreeBodySimulation.cpp -o ../audit-build/nonfinite-probe && ../audit-build/nonfinite-probe
```

Observed exit code: 1.

```text
src/core/Scale.cpp:73:76: runtime error: nan is outside the range of representable values of type 'long unsigned int'
```

### trails

Exercises the actual TrailHistory<4096> header. A radius-10 circle at one radian/second is sampled at 60 Hz for 60 seconds, requesting a 30-second trail. This proves duration loss for that finite helper input; it does not establish which physical presets currently encounter it.

```bash
g++ -std=c++20 -O2 -Isrc audit/2026-11-27/trail-probe.cpp -o ../audit-build/trail-probe && ../audit-build/trail-probe
```

Observed exit code: 0.

```text
requested_seconds=30 retained_seconds=22.75 samples=4096
```

### numerical

Uses the 24 factory initial-system recipes, seeds, chaos, gravity, softening and speed values captured from the audited resource. Each advances 6,000 times by 0.01 beat-units. Escape policy is deliberately Leave so respawns do not invalidate the energy comparison. The printed max_relative_energy_drift field is actually max |E(t)-E(0)| / max(1, |E(0)|), a normalised excursion; it is not a universal physical-error estimate. All positions and velocities stayed finite. These results motivate convergence testing, not changed defaults.

```bash
g++ -std=c++20 -O2 -Isrc audit/2026-11-27/numerical-sweep.cpp src/core/ThreeBodySimulation.cpp -o ../audit-build/numerical-sweep && ../audit-build/numerical-sweep
```

Observed exit code: 0.

```text
eightfold-dawn max_relative_energy_drift=5.47851e-06 finite=1
lagrange-glass max_relative_energy_drift=0.000914512 finite=1
twin-moon-relay max_relative_energy_drift=5.17686e-05 finite=1
quiet-periapsis max_relative_energy_drift=9.70127e-07 finite=1
cyan-transit max_relative_energy_drift=0.00222217 finite=1
ember-conjunction max_relative_energy_drift=0.0161175 finite=1
violet-aphelion max_relative_energy_drift=6.99248e-08 finite=1
red-shift-study max_relative_energy_drift=0.0419049 finite=1
golden-barycenter max_relative_energy_drift=2.88527e-05 finite=1
black-sun-threads max_relative_energy_drift=0.000241447 finite=1
orbital-ink max_relative_energy_drift=1.32165e-06 finite=1
slingshot-cadence max_relative_energy_drift=0.00171115 finite=1
pale-resonance max_relative_energy_drift=1.92171e-05 finite=1
magenta-eclipse max_relative_energy_drift=0.00055148 finite=1
three-cold-lights max_relative_energy_drift=0.000179305 finite=1
blue-giant-clock max_relative_energy_drift=0.00592787 finite=1
perihelion-dust max_relative_energy_drift=0.00571238 finite=1
silent-conjunction max_relative_energy_drift=0.0141986 finite=1
iron-triangle max_relative_energy_drift=0.0115568 finite=1
solar-kite max_relative_energy_drift=0.000593574 finite=1
gravity-well max_relative_energy_drift=0.00140757 finite=1
spiral-archive max_relative_energy_drift=0.0326065 finite=1
unstable-crown max_relative_energy_drift=0.00578756 finite=1
white-noon max_relative_energy_drift=0.00475376 finite=1
```

## Interpretation and remaining checks

The original test runner is green, while targeted probes demonstrate failures it does not cover. The dormant tests are not four independent proven product defects: three assertions expose constructor behaviour, and one includes a fixture-horizon error. The queue probe deliberately exercises the existing invalid concurrent ownership pattern; a future repair must also pass race detection and native lifecycle checks.

The numerical sweep retained the published algorithm and altered escape policy only in this diagnostic. Its largest normalised energy excursion was 0.0419049 (red-shift-study), followed by 0.0326065 (spiral-archive). No convergence study or long native performance run was completed here.

The audit did not modify production source or factory assets. The implementation packages and native acceptance criteria are defined in [the improvement plan](../../AUDIT-AND-IMPROVEMENT-PLAN.md).

