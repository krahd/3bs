# Runtime profiling

The profiling checks are development validation, not release guarantees. Run
them from an initialized recursive checkout.

## Audio processing

Configure and build the plugin-debug tree, then run:

```sh
cmake --build --preset plugin-debug --target threebs_realtime_profile
./build/plugin-debug/src/plugin/threebs_realtime_profile
```

The test warms the processor and host MIDI buffer first, then measures 2,048
512-sample `processBlock` calls at 48 kHz. Global allocation hooks count heap
allocations only while `processBlock` executes. The test fails if any
steady-state allocation is observed and reports average/max CPU time relative to
the 10.67 ms block duration. Timing is intentionally informational because CI
load and hardware vary.

On 2026-10-06 the current Mac measured 0 allocations, about 491 us average and
2.12 ms maximum process time.

The engine, render snapshot, command, and note-visualization paths use bounded
or fixed-capacity storage. The current audited core/audio processing path has no
explicit mutex/lock primitives.

## Renderer

`MetalSceneComponent` records callback cadence plus smoothed and maximum CPU
submission time for successful Metal frames using atomically published metrics.
The normal control deck displays measured FPS and smoothed CPU frame time while
the Metal status is active.

This measures CPU submission/callback cadence, not GPU completion time. Final
renderer acceptance therefore still requires hands-on observation and, when
needed, Instruments/Metal profiling under representative presets, window sizes,
and interaction.

## Validation

Run the complete debug gate with:

```sh
ctest --preset plugin-debug --output-on-failure
```

The 2026-10-06 profiling checkpoint passes all seven plugin-debug tests.
