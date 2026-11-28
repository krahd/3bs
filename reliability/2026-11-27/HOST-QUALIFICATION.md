# Native host and device qualification

This procedure completes the environmental acceptance still open after the
[reliability implementation](../../RELIABILITY-ACCEPTANCE.md). It preserves
the existing three-format instrument. A checked box requires an observed
result from the named build and environment; an automated CI pass does not
fill in these sessions.

## Session record

Record the source SHA, candidate archive SHA-256, format, macOS version, Mac
model/architecture, host version, sample rate, block size, destination and
capture method. Record display resolution and scale for visual checks. Retain
the exact saved host project and `.3bs` files used for recall, and link the
resulting evidence from the acceptance record. Distinguish a newly generated
compatibility fixture from an authentic historical user session.

Use a new temporary host project. Preserve existing work before changing any
installation or host preferences. A receiver-side MIDI capture must identify
actual channel/note ownership; silence alone does not prove that every NoteOn
received its NoteOff. Record limitations when the chosen host or receiver does
not expose the relevant events.

## 1. Logic AU MIDI effect

Use the project's existing AU MIDI-effect routing in the
[README](../../README.md). Record discovery, insertion and destination
before testing the following transitions:

| Check | Action | Required observation |
| --- | --- | --- |
| Basic routing | Run a factory system and record output from all three voices | MIDI reaches the intended instrument; no duplicate routing |
| Input timing | Record a press/release inside one host block, simultaneous input and generated events, repeated keys and multiple input channels | Gate/transpose changes follow the recorded input timing and ownership contract |
| Voice identity | While a note sounds, change its configured channel/key range or disable that voice | Cleanup targets the originally sounded identity; unaffected voices continue |
| Pending strum | Stop, disable or close the input gate while a strum is pending | No cancelled note starts later |
| Transport | Stop/start, seek and loop with each supported loop policy | Receiving notes are cleaned when required; restart/continue behaviour matches the selected policy |
| Bypass and removal | Bypass, re-enable and remove the plug-in while notes are active | No receiver note remains stranded; record whether the host supplies a final processing callback |
| Multiple instances | Use independent settings and routing in multiple instances | Configuration, note ownership and renderer history remain instance-local |
| Offline operation | Exercise the host's supported offline processing path | No invalid state, crash or silently missing MIDI; record any host format limitation |

The processor cannot emit through a host MIDI buffer from its destructor or
`releaseResources()`, because those calls provide no output buffer. Pending
cleanup is delivered on the next valid callback. The removal/teardown result
therefore needs explicit host evidence, including the host's own note-ending
behaviour when no further callback occurs.

## 2. Ableton VST3 instrument

Use the existing two-track MIDI routing described in the README and record the
actual destination configuration. Repeat the routing, identity, pending-strum,
transport, bypass/removal, multiple-instance and offline checks above. Inspect
channel handling at the receiver instead of assuming that internal routing
preserves independent voice channels.

Record automation of existing parameters, play it back and inspect host
readback and saved values. Confirm that parameter identities/normalised ranges
remain compatible and that automating one scalar preserves unrelated exact
configuration fields. Record the host's automation timing and offline behaviour.

Record and reopen a project containing an active instance and its destination.
Repeat recall with the editor open and closed, and with playback stopped.
Verify the current configuration before restarting playback, then immediately
save again and reopen. Retain the project and exported `.3bs` evidence.

## 3. Complete configuration and authored-file workflow

Perform this workflow in AU, VST3 and standalone: choose a factory system,
adjust bodies and voice mappings, record MIDI, save, close, reopen and compare
the restored configuration and output under the same timeline and inputs.

| Case | Required observation |
| --- | --- |
| Exact values | An accepted mass above the coarse slider range and nontrivial position/velocity values survive save/load and immediate resave |
| Coarse editing | Moving one corresponding scalar control updates that scalar without replacing unrelated exact fields |
| Meter | Manual 3/4, 5/8 and 7/8 survive `.3bs` transfer; host-source files use the documented standalone fallback |
| Presentation | Camera, tilts, palette, pane and trail settings recall with the editor already open |
| Authored copies | Save Copy creates a new file; Load, Refresh and Folder correspond to the intended authored location |
| Edited state | Factory/modified indication follows the authoritative configuration; constructor refresh does not create a false edit |
| Invalid imports | Malformed, oversized or unsupported files report a stable error and preserve the previous valid configuration |
| Instance isolation | A configuration action affects only the intended instance |

Preserve the existing parameter IDs and normalisation ranges. State migration
preserves authored values; it does not reproduce previously documented timing
or note-cleanup defects.

## 4. Standalone physical and virtual destinations

Record the exact physical device, driver/firmware where available, interface
and receiver capture method. Repeat reset, panic, stop, output replacement and
quit with active notes. Check the old destination's receiving ledger before
accepting an output change as complete.

Disconnect the selected physical destination, observe its unavailable state,
then reconnect it. Confirm that the intended stable destination is restored,
outstanding cleanup is retried and cancelled notes do not replay. Do not infer
successful cleanup from a failed send or from an unavailable device. Verify
that selecting another destination does not dispose of an externally owned
endpoint.

For a timing fault, record the visible status, verify cleanup and paused
generation, then resume through Reset or a fresh Run transition. Runtime
counters require a documented diagnostic capture or explicitly identified
instrumented build; mark unavailable metrics as such. The
100 ms late-deadline bound is a failure mechanism, not a musical jitter
tolerance.

## 5. Thirty-minute combined MIDI and UI/render session

Use a constant, recorded clock-trigger configuration for the duration of the
timing measurement. Preserve tempo, voice divisions and reset state so UI
actions do not change the expected musical timeline. Use the same full-duration
note-count and steady-period measurements as the virtual CoreMIDI test; exclude
only the documented immediate initial onset from steady-period comparisons.

During the full measured interval, exercise the actual application interface:
move the camera, change focus/zoom, inspect the five pages and note panes,
resize, enter/leave presentation/fullscreen and use maximum supported trails.
Keep those changes independent of musical parameters. Separately exercise
renderer unavailability/recreation and record whether MIDI continues. Record
the UI/render actions and their approximate times alongside receiver events.

Report p50/p95/p99 delivery lateness, maximum lateness, interval errors,
accumulated period drift, full-duration note count, output failures, worker
overruns, queue drops and final receiving ledger. Report CPU submission and GPU
completion/frame timing separately. These internal measurements require a
documented diagnostic capture or identified instrumented build; the release
interface does not expose every counter. Record instrumentation and any
unavailable metrics rather than inferring values from generic UI status.
Preserve any failed interval; restarting
the measurement must create another named result.

A passing CI soak establishes virtual delivery while message-thread dispatch is
stalled. It does not substitute for this concurrent interaction/render session
or for physical MIDI measurements. A release tolerance requires an explicit
decision based on the measured environment and intended performance use.

## 6. Native interaction and scene comparison

Inspect minimum, default and maximum supported logical sizes at representative
display scales. Compare matched preset/camera snapshots or videos against the
preserved scene: planets, atmosphere, lighting, bloom, camera framing and trail
time coverage. Chaotic trajectory differences must not be mistaken for a
rendering regression.

Check text entry and keyboard focus, full numeric-value access, readable range
labels, presentation escape, all note-pane orientations and parent-window
closure while an advanced dialog is open or Apply is pending. Close and reopen
editors and repeat resize/fullscreen transitions. Record any clipping, stale
state, orphaned dialog, GPU error or interruption of routed MIDI.

## Result classification

For each case record **passed**, **failed**, **unavailable** or **not run**, with
the build, evidence and reason. A hardware/host limitation remains open until
it has its own result. Integrate bounded repairs only for observed failures,
retain the failed evidence and rerun the affected checks.

Release qualification remains separate from integration. Development
candidates are ad-hoc signed and not notarised; Developer ID signing and
notarisation require independently recorded distribution evidence. Do not
replace the historical alpha.3 files.
