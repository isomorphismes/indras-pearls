# Android native limit-set slice

This native path now has two deliberately separate layers: the existing playable limit-set renderer, and an experimental three-disk complex-parameter control surface.

The bundled mathematical state is still the independently specified classical rank-2 Schottky group from the first slice. Four disjoint isometric circles and four determinant-one Möbius maps live in plain C state. The fragment shader repeatedly moves a screen-space complex point out of the circle that contains it; bounded escape depth gives the visible limit-set approximation.

The three new controls are ordinary unit disks. Each disk stores one complex value directly as its horizontal and vertical handle position. Values are clamped to radius `0.94`, so the interaction stays strictly inside the unit disk rather than pretending the rim is an ordinary finite value.

This experiment does **not** yet map those three complex values into the Schottky group. That separation is intentional: first test whether three compact two-dimensional controls are usable on the phone, then choose the mathematical chart. Android input dispatch may edit camera state or the independent control state, but it still does not contain or mutate the group construction.

The renderer is an independent native implementation. `philogb.md` and `notes/webgpu.md` remain reference notes about Nico Belmonte's public deployment; they do not assert a license for his application code, and this slice does not structurally port the browser bundle.

## Build

Use JDK 17, Android SDK 36, NDK `29.0.14206865`, CMake 3.22.1, and Gradle 8.13.

```sh
gradle :android:app:assembleDebug
```

The APK is written under `android/app/build/outputs/apk/debug/`.

## Touch contract

- drag inside one of the three disks: move that complex parameter handle;
- one finger outside the disks: pan the complex plane;
- two fingers outside the disks: zoom;
- lift: leave both camera and parameter values where they are.

A parameter drag captures that gesture so it does not accidentally pan or pinch the limit-set view.

## Control layout experiment

The three disks sit across the top of the framebuffer at one-sixth, one-half, and five-sixths of the width. Each has a faint real/imaginary crosshair, a colored rim, and a draggable handle. The layout is computed from the framebuffer dimensions rather than Android view widgets, so it stays inside the same NativeActivity/GLES boundary as the renderer.

The three stored values are currently UI state only. No claim is made that they are already a coordinate chart on Schottky space.

## Current renderer boundary

- one full-screen triangle; no CPU-side point cloud or mesh for the fractal;
- GLES 3 fragment shader with 32-bit complex arithmetic;
- one bundled group uploaded as uniforms;
- bounded 24-step circle classification / Möbius iteration;
- three analytic disk controls drawn in the same fragment pass;
- event-driven redraws when the camera, controls, or window change.

The next mathematical step, after the control layout is physically tested, is to decide how the three disk values map into a normalized three-complex-parameter description of the rank-2 Schottky group.
