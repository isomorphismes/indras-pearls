# Original Kleinian restoration

## Source of truth

The restored implementation is a literal C translation of the default `oi`
path in `isomorphismes/kleinian-groups`, pinned at:

`117dc5f34353e98cfae2f12bf386db5862110d92`

The fork's later commit only adds `AGENTS.md`; the source files below are
unchanged from that pin.

Source mapping:

- `kleinian/src/lib.rs::generators` -> `original_kleinian_generators`
- `kleinian/src/queue.rs::CircleQueue` -> the private binary heap in
  `original_kleinian.c`
- `kleinian/src/algebra.rs::circle_for_transforms` -> the private
  `circle_for_transforms`
- `kleinian/src/circle.rs` -> `transform_circle`, `circle_radius_inv`,
  and `circle_center`
- `kleinian/src/window.rs` -> `original_kleinian_window_transform`
- `kleinian-web/src/lib.rs::draw` -> point generation plus
  `original_kleinian_rasterize_rgba`
- `kleinian-web/pkg/index.html` supplies the default traces
  `tr(a)=2.2+0i`, `tr(b)=2.2+0i`, and 10,000 iterations.

The **historical reference byte contract** retains `idx = x * height + y`.
`original_kleinian_rasterize_rgba` remains untouched for comparison with the
pinned `dgulotta/kleinian` code. That byte layout is **not** suitable for
upload as a conventional `GL_RGBA` texture: GLES expects contiguous rows,
`idx = y * width + x`. A square-buffer nonblank/count/hash test is not a
pixel-position oracle and did not prevent the C67 failure of 2026-11-07.

`original_android_rasterize_rgba` is the separate **presentation** contract.
It uses the same reference-generated points and `original_kleinian_*window*`
projection, but writes the correctly ordered row-major Android buffer.
The fragment shader's existing vertical sampling flip converts the texture's
bottom-origin addressing into browser-style top-origin image orientation.
Do not apply another flip without a decoded corner/interior pixel oracle.

`tests/original_android_raster_test.c` exercises 17x9, 9x17 and 23x11
asymmetric point sets. It requires the new Android buffer to reproduce each
projected pixel coordinate and requires the historical column-indexed bytes
to **fail** the same display oracle. The historical reference test continues
to run separately and unchanged.

## Current Android boundary

The mathematical core is C with caller-owned storage. It has no heap allocation
or libc dependency and can be compiled freestanding by ICK. Android owns the
working buffers, EGL/GLES context, texture upload, and NativeActivity lifecycle.
The new bounded raster worker retains the last requested trace state and uses
three reusable RGBA buffers; no point generation, allocation or GPU readback is
permitted in touch callbacks. Only the GLES owner thread uploads/presents.
Readback is opt-in via `adb shell setprop debug.indras.gpu_probe 1` in debug
observer lanes. Ordinary interaction leaves it disabled.

The GLES stage does not implement the fractal. It displays the CPU raster
produced by the translated reference algorithm, **not** Nico Belmonte's richer
limit-set/WebGPU scene. Source fidelity to philogb requires a separate,
independently implemented mathematical view; the point-cloud correction alone
must not be presented as that completion.

## Explicitly superseded path

The symmetric-radius classical Schottky family, renderer packet, per-pixel
24-step Möbius fragment shader, and three-complex-disk parameterization are not
the source of truth for this restoration. They remain in history for comparison
but must not be substituted when acceptance says "original Kleinian".

## Acceptance boundary

Before adding Android-specific controls or GPU optimization:

1. the default `2.2, 2.2` generators must match the pinned formula;
2. the priority queue must grow exactly as the Rust `CircleQueue` does,
   including the 4,6,8,... queue-size behavior;
3. generated centers must remain finite and reproduce the reference bounds;
4. the reference raster must contain the reference-style nonblank point set;
5. the separate Android presentation raster must pass asymmetric pixel-position
   oracles, and the old column-indexed upload must fail them;
6. ARMv7 production math must be the same `original_kleinian.c` object when
   built through ICK, with the Android NDK performing the platform link;
7. a single launched process must accept both trace controls repeatedly,
   with measured input-to-swap and independent visible-pixel receipts;
8. API emulators are distinct from physical C67 acceptance. No replacement or
   merge is authorized by an emulator-only result.

New interaction should manipulate the original trace/symmetry parameters rather
than inventing a separate family and then attempting to make it look similar.
