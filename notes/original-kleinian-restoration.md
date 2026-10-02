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

The historical raster indexing expression `idx = x * height + y` is preserved
deliberately. Do not "correct" it while claiming bit/behavioral parity with the
reference.

## Current Android boundary

The mathematical core is C with caller-owned storage. It has no heap allocation
or libc dependency and can be compiled freestanding by ICK. Android owns the
working buffers, EGL/GLES context, texture upload, and NativeActivity lifecycle.

The GLES stage does not implement the fractal. It displays the CPU raster
produced by the translated reference algorithm.

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
4. the raster must contain the reference-style nonblank point set;
5. ARMv7 production math must be the same `original_kleinian.c` object when
   built through ICK, with the Android NDK performing the platform link.

New interaction should manipulate the original trace/symmetry parameters rather
than inventing a separate family and then attempting to make it look similar.
