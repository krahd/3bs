# The Three Body Solution: code audit and improvement plan

**Audit date:** 27 November 2026  
**Audited source:** [`06066967b243e250d97da37ea0195b31f5c9521c`](https://github.com/krahd/3bs/tree/06066967b243e250d97da37ea0195b31f5c9521c)  
**Scope:** audit and implementation plan. Production source, presets, dependencies and released binaries were not changed. Diagnostic programs and their results accompany this document in [`audit/2026-11-27/`](audit/2026-11-27/).

## 1. Direction and boundaries

The existing instrument remains the project: three bodies evolving through a deterministic gravitational simulation, with orbital relationships generating three musical voices and a cinematic planetary scene. The original masses, gravity, speed, initial-state editing, scales, mappings, independent/chord/strum modes, camera interaction and fading trails remain its vocabulary. AU MIDI effect, VST3 MIDI-generating instrument and standalone CoreMIDI remain the supported surfaces. The current C++20/JUCE/Metal architecture and arm64 macOS 13+ platform are retained. These boundaries follow the [original description][concept], [README][readme] and [repository requirements][agents].

The audit supports repairing implementation reliability within that design. It does not establish a reason to replace the gravitational model, introduce additional bodies, or redesign the work around another generative mechanism. Chaotic motion, escapes and unusual musical results must remain distinguishable from numerical or scheduling errors. Published presets and old saved configurations are compatibility evidence; corrections that change their musical output require an explicit compatibility decision and recorded before/after results.

The main priorities are **sounding-note integrity, deterministic timing, coherent state, and safe rendering**. Host qualification follows those repairs. Polishing the interface before resolving them would leave apparently successful performances vulnerable to lost notes, changed timing or unreliable recall.

## 2. What was inspected and executed

The audit inspected the complete first-party source, core/plugin tests, preset validators, build configuration, CI, packaging scripts, current status, original brief and remote branches. There were no open pull requests at inspection. The older `ai/workspace/3bs-performance-profile-20261006-1906` branch contains a separate `tests/PerformanceProfile.cpp`; it is one commit ahead and one behind current main. Preserve and evaluate that work when extending profiling; do not assume it is merged or discard it.

The portable core and presentation model were compiled directly with GCC 14/C++20 in a Linux environment. CMake was unavailable, so this audit did **not** execute the repository CMake/CTest presets or their CMake JSON/catalogue validators. Native JUCE, Metal, CoreMIDI, Logic, Ableton, `auval` and `pluginval` were not run. The successful [existing main CI run][baseline-ci] and October measurements in `STATUS.md` are historical evidence, not new host acceptance.

| Check | Result and interpretation |
| --- | --- |
| Existing `CoreTests.cpp` entry point | Passed. Compiler warnings exposed five test functions omitted from `main()`. |
| Five dormant tests, invoked separately | Octave: two failed assertions; duration: one; global chord frame: passed; beat/bar strum: one; bar auto-reset: passed. The first two failures arise from constructor configuration replacement. The strum failure includes an exclusive-end test-horizon error, not a demonstrated engine failure. |
| Note lifecycle and input probes | Reproduced lost cleanup after reset, wrong-channel cleanup, pending notes after disabling, and input-dependent output changing with block partition. The input probe mirrors adapter batching around the real core; it is not a native processor test. |
| Dense event generation | Same 16,384-sample interval produced 1,050 events in 512-sample partitions but 1,024 in one block, leaving two unmatched NoteOns. |
| Snapshot queue stress | Reproduced torn payloads and non-monotonic sequences with the current producer-pop-on-full policy. The same SPSC queue using a drop-new control produced neither in the observed runs. Counts are scheduler-dependent. |
| Malformed initial state under UBSan | A NaN position reached an undefined floating-point-to-integer conversion in `Scale.cpp`. This is a core reproduction plus an inspected unvalidated host-restore path, not a DAW reproduction. |
| Trail-history helper | A 30-second trail request retained 22.75 seconds on the specified fast circular trajectory because subdivisions consumed its 4,096 slots. |
| Numerical diagnostic | All 24 factory initial systems remained finite for 60 beat-units. Escape handling was temporarily set to Leave to avoid respawn discontinuities in the energy diagnostic. Normalised energy excursions varied materially; this is evidence for convergence testing, not permission to alter the artwork's defaults. |

Exact diagnostic sources, commands, outputs and limitations are in [audit evidence](audit/2026-11-27/EVIDENCE.md).

## 3. Findings

**P0** means a correctness or memory-ownership defect to close before qualification. **P1** means required reliability or reproducibility work. **P2** means control-surface or presentation refinement. “Executed” refers to the portable probes above; “source” identifies an inspected code path whose native manifestation still requires testing.

### A01 — P0: sounding notes can lose their matching cleanup

Standalone reset, new-system, exact-state, preset, configuration-load and tilt paths call `reset(initial)` without a cleanup buffer. `MusicEngine::reset()` then discards its active-note ledger. Output replacement and application shutdown also stop the MIDI output without an explicit note flush. Separately, the core stores an active note number but uses the **current** configured channel for its NoteOff; changing channel 1 to 7 sends cleanup on 7 for a note sounded on 1. Pending strums can still start notes after every voice is disabled and the input gate closes. All three core behaviours were reproduced.

**Locations:** [standalone][standalone] lines 35–84, 107–113, 136, 260, 429; [music engine][engine] 119–125, 278–290, 416–420, 451–458. **Consequence:** hanging notes, notes after a stop-like action, or incorrect behaviour during performance transitions. Fix through one defined note-transition contract, preserving the original channel/note identity and cancelling pending work before replacing state or destination.

### A02 — P0: event-buffer saturation silently loses musical events

The core event buffer has 1,024 entries. Emission sites ignore failed `push()` operations while advancing internal note state. A valid dense configuration at 240 BPM/48 kHz loses 26 events in a 16,384-sample block relative to smaller partitions and leaves two unmatched NoteOns. `prepareToPlay()` ignores the declared maximum block size. This disproves unconditional block-size independence at the audited revision.

**Locations:** [engine header][engine-h] 29–38, 96; [engine][engine] 283–290, 429–432, 451–458; [processor][processor] 332–337. Use bounded internal processing segments or an equivalently justified capacity strategy, preserving absolute offsets and guaranteed cleanup. Merely increasing the constant leaves the failure contract undefined.

### A03 — P0: incoming MIDI is applied at the wrong time

The processor consumes all input messages before generating the block and ignores `metadata.samplePosition`. A press at sample 100 and release at 900 yield zero generated notes in one 1,024-sample block, versus five in 128-sample partitions using the mirrored adapter logic. Gate and transposition can take effect before the input event. The scalar held-note counter also lacks channel/note ownership and all-notes-off handling.

**Location:** [processor][processor] 481–489. Process segments between input-event offsets, maintain a bounded key ledger, and specify deterministic ordering for simultaneous input and generated events.

### A04 — P1: standalone tempo follows GUI callback delivery

Standalone generation uses a 100 Hz `juce::Timer`, advances exactly 480 samples per callback, and schedules from the current high-resolution clock time plus 2 ms. A delayed callback loses musical time; elapsed time is not recovered. JUCE documents that this timer runs on the message thread and can be delayed by its workload. This is a source-backed timing defect; live jitter was not measured here.

**Location:** [standalone][standalone] 21, 433–450, 515–521; [JUCE Timer documentation][juce-timer]. Move generation onto an independently scheduled monotonic timeline with bounded look-ahead and explicit late-event handling. Retain the existing internal 120 BPM semantics while making them accurate.

### A05 — P1: the real-time allocation claim has an uncovered failure path

`metadata.getMessage()` creates an owning JUCE message for every incoming event. In the pinned JUCE source, long messages allocate through `std::malloc`, even when the instrument will ignore them. The current profiler intercepts C++ `new/delete`, uses empty input and preallocates 64 KiB of host MIDI storage; it cannot establish allocation freedom for incoming SysEx or general host-buffer behaviour.

**Locations:** [processor][processor] 482; [profile][profile] 24–58, 68–86; [pinned JUCE message implementation][juce-message] 144–152, 349–357. Inspect relevant short messages through the non-owning metadata view. Measure C and C++ allocation paths, including frees and transitions, and document the remaining host-owned buffer boundary.

### A06 — P0: snapshot saturation violates SPSC ownership

On a full snapshot queue, the audio producer calls `pop()` while the renderer also consumes. The queue's design requires a single consumer. A concurrent probe reproduced torn snapshots and sequence regressions. Standalone repeats the same fallback; its native callback affinity should also be checked.

**Locations:** [processor][processor] 594–597; [standalone][standalone] 479–482; [queue][queue] 40–56. Keep each queue index owned by one thread. A dropped visual snapshot is acceptable; corrupted shared state is not. Start with a simple drop-new policy or a proven bounded latest-state handoff, and measure freshness.

### A07 — P0: commands and published state are not one coherent transaction

The command queue has seven usable entries, but producers ignore enqueue failure while updating associated parameters/UI state. An accepted-looking preset or restore can therefore fail to reach the engine. `requestReset()` additionally copies non-atomic `engineConfig_` from the UI thread while processing writes it. `AtomicState` makes individual fields atomic without making the complete body state coherent.

**Locations:** [processor header][processor-h] 159–165; [processor][processor] 78–101, 390–438, 610–720, 1040. Define ownership, accepted/pending revisions and saturation behaviour. Remove the unnecessary racing copy. Coalesce replaceable requests only where semantics permit; reset/cleanup operations must not silently disappear. Keep processing free of locks and unbounded retries.

### A08 — P1: host recall can be overwritten or immediately resaved incorrectly

Restored initial vectors are not published to `storedInitial_` until processing consumes the command. Saving again before the next block can write the previous vectors. An open editor retains its old presentation copy and writes it back at 30 Hz without observing a restore revision, so camera, palette, pane state and tilts can diverge from the recalled configuration.

**Locations:** [processor][processor] 860–883, 944–978, 1039–1040; [editor][editor] 197–213. Save from one coherent configuration revision, and refresh the editor from the authoritative model after restore. Validate both editor-open and editor-closed cases, including restore without starting playback.

### A09 — P1: accepted configuration values do not survive the adapters

The exact-state editor and portable reader accept masses up to 1,000, but sliders and plugin parameters stop at 8. Their values are reapplied during processing, silently changing an accepted exact state. Portable `.3bs` configurations also omit the existing time-signature source, numerator and denominator, so bar-fraction strums and bar resets are not fully portable.

**Locations:** [advanced editor][advanced] 97–103; [panel][panel] 152–154; [processor][processor] 177–180, 439–441, 709–710, 827–829; [configuration header][configuration-h] 15–28. Reconcile ranges without silently remapping existing host automation. Version the portable schema to include musical meter, with explicit legacy defaults and an explicit standalone interpretation of host-derived settings.

### A10 — P0: malformed state can reach undefined numerical behaviour

Host-restored vectors and several presentation fields are read without complete finite/range checks. The portable JSON reader also coerces several types and substitutes/clamps values instead of consistently rejecting malformed data. A NaN position passed to the actual engine reaches an undefined conversion in pitch quantisation under UBSan.

**Locations:** [processor][processor] 952–1035; [configuration reader][configuration] 24–60, 314–357; [scale][scale] 60–90. Validate complete candidate states before publishing them; retain the previous valid state on failure. Test types, finite values, enums, cross-field ranges, schemas, seeds and host timing. Preserve documented legacy defaults rather than confusing them with malformed present values.

### A11 — P1: two core semantics need narrow corrections

The constructor treats “all roots C, all channels 1” as an absent configuration and replaces legitimate supplied voices. A supplied single-voice, octave +2 configuration becomes three enabled voices at octave 0. Separately, SpeedPeak treats a positive speed change followed by an unchanged audio sample as a maximum. The probe recorded four NoteOns during monotonic acceleration with zero decreasing measurements.

**Locations:** [engine][engine] 75–82, 238–242. Make default construction explicit; respect explicitly supplied configurations. Detect speed extrema between meaningful physics measurements and retain derivative state across sample plateaux. Neither repair requires new mappings or a new integrator.

### A12 — P0: the renderer violates two GPU data contracts

Dynamic planet/cloud/atmosphere/trail buffers are allocated once and overwritten on each CPU frame, while asynchronous GPU commands continue to reference them. There is no completion-based reuse control. Vertical note glyphs are also passed through `setVertexBytes`: 29 visible three-character notes require 87 × 48 = **4,176 bytes**, exceeding its 4,096-byte limit; the existing history can require 13,824 bytes. These are source/API-contract findings, not observed Metal crashes in this audit.

**Locations:** [Metal renderer][metal] 100–104, 642–654, 894–941, 1067–1070, 1226–1235; [Apple buffer limit][metal-bytes], [Apple synchronisation guidance][metal-sync]. Give mutable GPU data bounded in-flight ownership and use an appropriately sized buffer or valid batches for glyphs. Include all required instance buffers in readiness checks; current readiness omits resources later dereferenced. Rendering failure must remain non-fatal to MIDI.

### A13 — P1: current green checks leave important code untested

Five core tests are omitted from the runner. CI's native bundle job sets `THREEBS_BUILD_TESTS=OFF`, so it builds but does not execute plugin, Metal or real-time test targets. The Metal smoke test only checks component readiness; it never draws a frame or fills note history. Existing migration tests cover synthetic v3/v6 cases, and one round-trip check verifies nonempty output rather than equal restored content.

**Locations:** [core tests][core-tests] 515–750, 763–782; [plugin tests][plugin-tests] 258–308; [CI][ci] 22–23, 42–47; [Metal smoke][metal-test] 14–28. Correct registration and fixture boundaries, then add tests at actual failure boundaries. Keep visual/DAW acceptance distinct from a successful compile.

### A14 — P2: complete the existing control surface and error feedback

Range and custom-scale controls remain acknowledged gaps in `STATUS.md`; other existing engine settings are available only through host parameters or configuration data. The current status label also multiplexes renderer telemetry and transient load/save messages, so a message can suppress subsequent telemetry updates. An asynchronous advanced-state dialog retains callbacks to its editor/content owner; destruction while that dialog remains open needs an explicit lifetime test.

**Locations:** [panel API][panel-h], [panel][panel] 857–870; [advanced editor][advanced] 129–143; [editor][editor] 128–136. Keep the five-page deck and planetary presentation, expose existing settings in a compact advanced section, retain visible error information, and validate callback lifetimes. The dialog issue is a lifecycle risk to test, not a reproduced native crash.

### A15 — P2: refine visual correctness within the current aesthetic

Trail subdivision consumes history capacity, making the duration control depend on speed. A 30-second request retained 22.75 seconds in the helper probe. The shader also perturbs a world-space normal with a local-space bump gradient. Visual note events carry no musical timestamp and are dated on receipt, so note-history timing should be checked against MIDI under large blocks or delayed drawing.

**Locations:** [trail history][trail] 38–64; [Metal renderer][metal] 873–879, 933, 1067–1070; [shader][shader] 206–212, 254–260; [queue][queue] 26–31. Preserve time coverage separately from render tessellation, transform normals consistently, and carry a suitable timeline through note visualisation. The existing trail-width aesthetic cap needs review before alteration; it is not automatically a defect.

### A16 — P1: align build and qualification claims with what is supported

`CMakePresets.json` uses schema version 6 while the README and declared minimum say 3.22. Schema 6 requires CMake 3.25. Host acceptance remains incomplete in existing project records; an alpha bundle that builds is not evidence that its routing, recall and editor workflows pass.

**Locations:** [presets][cmake-presets], [README][readme], [CMake version reference][cmake-reference]. Prefer the smallest compatible preset schema if no version-6 feature is needed; otherwise align the stated minimum. Retain existing release history and identify tested commits, hosts and binary checksums precisely.

## 4. Implementation sequence

The packages below are recommendations for execution. Completion is defined by their acceptance evidence, not by the number of changed files.

### W0 — Establish an honest regression baseline

**Addresses:** A13, A16. **Dependencies:** none.

Register every intended test, repair the exclusive-end strum fixture and add named reporting so a skipped function is visible. Preserve a small reference corpus: all 24 factory systems, all 12 voicing presets, representative published host/portable states, and fixed input timelines. Record event type, channel, note/value and absolute sample position. Separate deliberately failing audit reproducers from the normal green regression suite; convert each into an expectation of corrected behaviour as its repair lands. Inspect the existing profiling branch before reusing its material.

**Acceptance:** every defined test is accounted for; known failures are individually explained; native CI executes the applicable test binaries; documented configure commands work at the claimed minimum CMake version. Numerical and output references retain their source revision.

### W1 — Repair note ownership and bounded event delivery

**Addresses:** A01, A02, A11 constructor. **Dependencies:** W0.

Represent active notes by their actual channel/note identity. Define cleanup ordering for run/stop, reset, seek, loop restart, bypass, preset/state change, voice disable, gate closure, output replacement and teardown. Cancel pending strums and queued outbound events where required. Clean the old destination before replacing it. Make explicit configuration construction reliable. Introduce bounded internal event chunks or another proven delivery bound, retaining offsets and ensuring failed admission never silently corrupts the note ledger.

**Acceptance:** the receiving ledger is empty after every termination transition; no note starts after disable/gate closure; unaffected voices continue. The dense reproduction delivers the same complete event stream in small, large and mixed blocks. Exercise cleanup at exact pending-note deadlines and during configuration changes. Native teardown behaviour must be verified against each host's available callback lifecycle rather than assumed.

### W2 — Give queues, commands and state explicit ownership

**Addresses:** A06, A07, foundations of A08. **Dependencies:** W0; coordinate note transitions with W1.

Repair snapshot overflow first. Define the producer/consumer for each queue, including callbacks invoked by hosts. Separate expendable visual updates from non-expendable engine commands. For configuration replacement, publish one validated revision and an acknowledgement or equivalent coherent acceptance state; allow intentional coalescing of superseded UI edits without losing required cleanup. Remove the racing reset copy and replace field-by-field pseudo-snapshots where coherent state is required.

**Acceptance:** saturated concurrent tests show no torn payloads, sequence regression or silently lost final configuration. A stalled/closed renderer cannot delay MIDI. Race detection passes for command, save and restore paths. No allocation, mutex acquisition or unbounded retry is introduced into processing. Report queue drops or rejected requests outside the callback.

### W3 — Make the musical timeline independent of UI and block partition

**Addresses:** A03, A04, A05, A11 SpeedPeak. **Dependencies:** W1 and W2.

Apply MIDI input at its sample offset and track held keys by identity. Define equal-time ordering, transpose/gate release behaviour and CC120/123 handling. Move standalone generation to a monotonic worker or equivalent suitable scheduling source with preallocated messages and bounded look-ahead, leaving UI polling on the message thread. Define late-message handling without replaying cancelled notes. Avoid owning copies of ignored long MIDI messages. Detect extrema on distinct physical measurements.

**Acceptance:** identical absolute MIDI output for equivalent timelines partitioned into 1, 32, 64, 127, 128, 512, 1,024, 2,048 and 16,384 samples, plus varying partitions; sample-rate coverage at 44.1, 48 and 96 kHz. Include same-block press/release, tempo changes, seeks, both loop policies and musical meter changes. Monotonic acceleration emits no SpeedPeak events. Timestamped standalone output shows no accumulated clock drift during a 30-minute UI/render stress session; record jitter percentiles, maximum lateness and any dropped events before setting a release tolerance. Allocation measurement includes C/C++ paths, cold and warmed processing, transitions and long incoming MIDI.

### W4 — Make saves and recalls complete, validated and reversible

**Addresses:** A08, A09, A10. **Dependencies:** W2; preserve W1 cleanup.

Build a common validated configuration contract for host state, portable files and exact-state editing. Validate candidates before touching the running model. Make save operations observe the correct revision even before playback resumes. Refresh open editors on model changes and avoid timer-driven writes of stale data. Reconcile mass/control ranges while retaining existing parameter IDs and automation semantics. Add musical meter to a versioned portable schema and preserve legacy imports through explicit migrations.

**Acceptance:** edit/load → process → save → reload preserves every supported musical, physical and presentation field, including masses above 8 if accepted. Restore/resave works without an intervening callback. Open/closed-editor recall is equivalent. Transfer 3/4, 5/8 and 7/8 settings between formats with documented host-source fallback. Authentic old fixtures and malformed/oversized/future-version input produce defined results; failed imports leave the previous configuration intact. Numeric assertions respect declared parameter precision.

### W5 — Repair GPU data ownership and preserve the scene

**Addresses:** A12, visual portions of A15. **Dependencies:** W2 snapshot contract; can proceed alongside W3/W4.

Use bounded frame resources whose reuse is released by GPU completion. Replace oversized immediate glyph uploads with correctly sized storage. Check every required resource and display a stable renderer-failure state while MIDI continues. Keep the existing procedural planets, lighting, palette families, bloom and camera language. Correct the normal-space mismatch; preserve trail time coverage through time-aware history and separately bounded tessellation. Carry timing/revision information into note history where necessary.

**Acceptance:** Metal validation reports no API/data hazards through dense vertical-note histories, maximum trails, repeated resize/fullscreen, close/reopen, unavailable renderer and resource-failure injection. A real render test creates drawable frames and observes command completion; readiness alone does not pass. Compare matched screenshots/videos at minimum, default and maximum editor sizes and representative display scales. Check fast/slow trail coverage by timestamps, camera focus/orbit/zoom, note panes and reset discontinuities. Record GPU completion/frame timing separately from CPU submission time.

### W6 — Complete the present instrument's controls and workflows

**Addresses:** A14 and exposed A09 inconsistencies. **Dependencies:** W4 and relevant W5 fixes.

Complete per-voice note range and custom-scale editing, then expose already implemented channel/trigger/velocity/CC settings where needed for the existing instrument. Use the current five pages, contextual controls and an advanced section; retain the cinematic scene as the primary view. Make edited-preset state legible, give save/load feedback a stable location, and provide a direct note-cleanup action. Keep device/window preferences separate from musical presets. Add simple management of authored `.3bs` configurations without replacing their portable format.

**Acceptance:** in each format, perform preset → adjust bodies/mapping → record MIDI → save → reopen → reproduce the result. Verify keyboard focus, text entry, labels, all supported sizes, presentation-mode escape and exact-state dialog lifetime when the parent closes. Every exposed field has one authoritative value and round-trips correctly. Visual inspection is required before claiming usability; this source audit did not render the native application.

### W7 — Qualify numerical and processing bounds

**Addresses:** numerical evidence and performance coverage. **Dependencies:** W1–W5.

Run each factory system and representative extremes with finite-state, minimum-separation, energy/angular-momentum and escape diagnostics. Compare the existing fixed step against smaller steps to separate discretisation error from genuine chaos. Evaluate conservative trajectories separately from deliberate respawn policies. Use musical event traces as well as body positions. Characterise dense mappings, CCs, strums, multiple instances and supported block sizes; extend profiling beyond one warmed configuration.

**Acceptance:** every preset has a finite-state and bounded-work report plus an explanation of intentional escape/respawn behaviour. Any tolerance or changed physical default is supported by measured convergence and a compatibility decision. Report CPU distributions, overrun counts and machine/build details against the relevant callback budget. Keep the current Verlet solver unless this evidence establishes a concrete defect requiring a bounded repair; do not force chaotic trajectories to look regular.

### W8 — Complete three-format release acceptance

**Addresses:** A13/A16 and the existing project qualification gate. **Dependencies:** W0–W7.

Build Release AU/VST3/standalone from the candidate commit, run the complete available suites, `auval` and `pluginval`, and test actual routing in Logic and Ableton. Validate standalone virtual and physical MIDI destinations, destination loss/reconnection and quit. Exercise host save/reopen, editor-open restore, loop/seek/bypass, multiple instances, offline processing and renderer absence. Package with commit provenance and checksums; keep alpha.3 historical files intact. Signing/notarisation remains a separately evidenced distribution step.

**Acceptance:** all P0/P1 findings are closed with reproductions that now pass; no unmatched notes or known silent state loss; deterministic comparisons pass; native visual and host sessions are recorded with exact versions and device/build information. Update `STATUS.md` and the public known-limitations text from those results. A build-only CI result cannot close this package.

## 5. Execution and completion rules

Start with **W0, W1 and W2**. W3 and W4 then share a stable state/ownership model; W5 can run in parallel after the snapshot contract is settled. W6 builds on corrected recall and rendering. W7 characterises the repaired engine; W8 qualifies the exact candidate across the existing three formats.

Keep changes focused enough to review independently. Each repair records the original failure, the changed behaviour, its test and any compatibility effect. Preserve published state/preset manifestations, parameter identities and the older profiling branch. The audit and plan are complete as documentation; implementation and native acceptance remain open. Subsequent progress belongs in `STATUS.md`, with this document retained as the dated audit and execution reference.

[concept]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/doc/project-initial-description.md
[agents]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/AGENTS.md
[readme]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/README.md
[engine]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/core/MusicEngine.cpp
[engine-h]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/core/MusicEngine.h
[scale]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/core/Scale.cpp
[queue]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/core/SnapshotQueue.h
[processor]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/plugin/PluginProcessor.cpp
[processor-h]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/plugin/PluginProcessor.h
[editor]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/plugin/PluginEditor.cpp
[standalone]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/standalone/Main.cpp
[configuration]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/ui/UserConfiguration.cpp
[configuration-h]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/ui/UserConfiguration.h
[advanced]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/ui/AdvancedStateEditor.cpp
[panel]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/ui/ArtworkPanel.cpp
[panel-h]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/ui/ArtworkPanel.h
[metal]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/render/MetalSceneComponent.mm
[shader]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/resources/metal/SceneShaders.metal
[trail]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/src/render/TrailHistory.h
[core-tests]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/tests/CoreTests.cpp
[plugin-tests]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/tests/PluginTests.cpp
[metal-test]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/tests/MetalSmoke.mm
[profile]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/tests/RealtimeProfile.cpp
[ci]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/.github/workflows/ci.yml
[cmake-presets]: https://github.com/krahd/3bs/blob/06066967b243e250d97da37ea0195b31f5c9521c/CMakePresets.json
[baseline-ci]: https://github.com/krahd/3bs/actions/runs/37563585891
[juce-timer]: https://docs.juce.com/develop/classjuce_1_1Timer.html
[juce-message]: https://github.com/juce-framework/JUCE/blob/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2/modules/juce_audio_basics/midi/juce_MidiMessage.cpp
[metal-bytes]: https://developer.apple.com/documentation/metal/mtlrendercommandencoder/setvertexbytes(_:length:index:)
[metal-sync]: https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/TripleBuffering.html
[cmake-reference]: https://cmake.org/cmake/help/v3.25/manual/cmake-presets.7.html
