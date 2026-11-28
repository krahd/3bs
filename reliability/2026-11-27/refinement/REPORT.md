# Factory-preset musical refinement diagnostic

Declared source checkpoint: `449f126e6cdea64214374bb0280fa8d16b6d0a82`. Exact file and compiler hashes are in `provenance.json`.

## Method

The diagnostic uses the unchanged product core and all 24 schema-2 factory presets. A strict Python JSON reader rejects duplicate, missing, unknown and nonfinite data. Generated portable fixture code reuses the canonical catalogue's enum functions and musical assignment blocks; the catalogue source hash is pinned. Real product configuration defaults remain in force. Every factory-mapped musical field is recorded, and present factory fields are checked against the compiled configuration before the result is accepted.

Each preset starts from its own published system/seed/chaos with a fresh Independent-mode engine and no prior voicing overlay, transpose, input gate, tilt or auto-reset. Both runs use 120 BPM, 48 kHz, 4/4, 512-sample blocks and exactly 60 beats (30 seconds, 1,440,000 samples). One run uses the published 1/480 step; the diagnostic copy alone uses 1/960. Published escape/respawn policy is retained. This measures fixed-step sensitivity; it is not an old-versus-repaired-build comparison.

The synchronous sink writes into bounded per-block diagnostic storage; CSV output occurs after processing. Each row records phase, absolute sample, type (0 NoteOn / 1 NoteOff / 2 CC), channel, note/controller, velocity/value and source body. Cleanup at the exclusive end sample is separately tagged. Integrity checks require finite state, bounded work, ordered/valid events, no sink rejection or unmatched offs, and an empty final receiving ledger.

## Results

All 48 runs passed those integrity checks. 21/24 presets changed at least one complete trace entry when the step was halved; 9/24 changed at least one per-voice note count; 13/24 changed at least one ordinal-paired pitch. These counts describe observed sensitivity, not failure rates or release tolerances.

| Preset | Notes h / h÷2 | Events h / h÷2 | First differing sample h / h÷2 | Ordinal pitch changes | Respawns h / h÷2 | Final state difference |
|---|---:|---:|---|---:|---:|---:|
| eightfold-dawn | 152 / 152 | 304 / 304 | 45949 / 45924 | 0 | 0 / 0 | 0.000166992 |
| lagrange-glass | 152 / 152 | 304 / 304 | 144000 / 144000 | 0 | 0 / 0 | 0.000441842 |
| twin-moon-relay | 269 / 269 | 538 / 538 | 25391 / 25369 | 54 | 0 / 0 | 2.08835 |
| quiet-periapsis | 12 / 12 | 24 / 24 | 699239 / 699184 | 0 | 0 / 0 | 0.000384871 |
| cyan-transit | 257 / 257 | 514 / 514 | 55370 / 55354 | 1 | 0 / 0 | 0.0196311 |
| ember-conjunction | 127 / 144 | 254 / 288 | 14907 / 14884 | 58 | 0 / 0 | 6.37757 |
| violet-aphelion | 36 / 36 | 72 / 72 | identical | 0 | 0 / 0 | 2.34781e-06 |
| red-shift-study | 239 / 232 | 478 / 464 | 11740 / 11722 | 136 | 0 / 0 | 1.84013 |
| golden-barycenter | 329 / 329 | 658 / 658 | 678000 / 678000 | 0 | 0 / 0 | 0.000409488 |
| black-sun-threads | 17 / 17 | 34 / 34 | 23203 / 23164 | 0 | 0 / 0 | 0.052923 |
| orbital-ink | 83 / 83 | 166 / 166 | 291158 / 291128 | 0 | 0 / 0 | 9.78167e-05 |
| slingshot-cadence | 276 / 257 | 552 / 514 | 7637 / 7623 | 145 | 0 / 1 | 6.86736 |
| pale-resonance | 41 / 41 | 82 / 82 | 389078 / 389013 | 0 | 0 / 0 | 4.0778e-05 |
| magenta-eclipse | 359 / 360 | 718 / 720 | 14072 / 14052 | 48 | 1 / 1 | 2.44437 |
| three-cold-lights | 262 / 262 | 524 / 524 | identical | 0 | 0 / 0 | 0.000169843 |
| blue-giant-clock | 323 / 323 | 646 / 646 | 180000 / 180000 | 123 | 0 / 0 | 4.92494 |
| perihelion-dust | 299 / 294 | 598 / 588 | 18209 / 18192 | 115 | 1 / 0 | 11.9489 |
| silent-conjunction | 12 / 12 | 24 / 24 | 99 / 49 | 0 | 0 / 0 | 0.00732347 |
| iron-triangle | 259 / 259 | 518 / 518 | 1081230 / 1081173 | 8 | 0 / 0 | 1.74522 |
| solar-kite | 129 / 131 | 258 / 262 | 64444 / 64416 | 14 | 0 / 0 | 1.30503 |
| gravity-well | 19 / 17 | 38 / 34 | 21810 / 21767 | 6 | 0 / 0 | 3.58922 |
| spiral-archive | 66 / 66 | 132 / 132 | 48000 / 48000 | 2 | 0 / 0 | 0.0114507 |
| unstable-crown | 254 / 265 | 508 / 530 | 11185 / 11169 | 121 | 0 / 0 | 1.99756 |
| white-noon | 247 / 247 | 494 / 494 | identical | 0 | 0 / 0 | 0.00256872 |

Final state difference is the largest Euclidean position or velocity difference across the three bodies; position and velocity have different normalized units, so the value is only a trajectory divergence diagnostic. Full final states, separate per-voice onset/offset distributions, counts, first differing rows and trace hashes are retained in `runs.jsonl` and `comparisons.json`.

## Interpretation and limits

- Pitch/velocity and onset/offset summaries pair the nth note of each source body with the nth note in the other run. If a trigger is added or removed, later pairs need not identify the same physical event; large ordinal differences are not scheduling-jitter estimates. Full CSV traces permit alternate matching without rerunning the simulation.
- Small numerical trajectory changes can cross discrete musical thresholds or alter trigger/PRNG consumption; later note sequences can diverge. This result neither establishes a convergence order for quantized MIDI nor implies that one sequence is artistically preferable.
- Published-policy respawns can separate trajectories discontinuously. Conservative energy/momentum, sampled separation and half-step position diagnostics already exist for every preset in [numerical-presets.log](../core/numerical-presets.log); their conservative runs must not be conflated with the published-policy music runs here.
- The measurement covers the 24 complete factory systems in their fresh Independent mode, not every combination with the 12 separate voicing presets, every automation/edit, or all accepted extremes. Existing synthetic chord/strum partition and performance tests cover different questions.
- CSV and JSON work are intentionally outside the callback. This is a portable musical/numerical diagnostic, not a processing benchmark, real-time host test, physical MIDI measurement or combined GPU/UI stress session.
- No product source, preset, physical default, musical threshold, release tolerance or CI requirement was changed. The diagnostic reports sensitivity and supplies traceable references for any later compatibility decision.

## Reproduce

```sh
python3 run_refinement.py --repo /absolute/path/to/3bs --source-ref 449f126e6cdea64214374bb0280fa8d16b6d0a82 --output /absolute/path/to/refinement-results
```

The generator stops if the canonical catalogue source changes; review the extraction before updating its expected hash. `provenance.json` records exact commands, environment and SHA-256/git-blob digests. `build.log` and `run.log` retain execution output. `GeneratedFixtures.h` and the exact adapted canonical mapping are retained for inspection.
