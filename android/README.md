# Android native limit-set slice

The Android path now has three deliberate layers:

1. the binary64 Schottky/Möbius mathematics core in plain C;
2. a flat scalar renderer packet derived from a validated classical circle presentation;
3. the NativeActivity/EGL/GLES renderer and touch UI.

For the bundled view, the app constructs the symmetric classical family member
at `r = 0.7`, derives its four isometric circles and four exit maps through
`mobius_math.c`, and flattens that validated state through
`renderer_packet.c`. The old hand-entered `group_state.c` copy is no longer
the renderer's source of truth.

The packet carries separate center and radius-squared values for all four
circles plus the four complex coefficients of every exit map. The fragment
shader therefore no longer assumes that all four circles share one radius.

The packet boundary is intentionally friendly to both ICK C and the Android
NDK. Its externally visible construction call uses an ordinary `float`
parameter, integer status, and a flat float array. C aggregates and compiler
private complex representations stay on the producer side. CI proves an
`armeabi-v7a` path in which ICK C compiles the Möbius and packet translation
units and Android NDK r29 Clang/lld performs the final Android shared-library
link.

The three disk controls remain an independent UI experiment. Each disk stores
one complex value directly as its horizontal and vertical handle position,
clamped to radius `0.94`. They do **not** yet map into the Schottky group.
That keeps the parameter-chart decision separate from the now-working
mathematics-to-renderer path.

The renderer remains an independent native implementation. `philogb.md` and
`notes/webgpu.md` are reference notes about Nico Belmonte's public
deployment; they do not assert a license for his application code.

## Build

Use JDK 17, Android SDK 36, NDK `29.0.14206865`, CMake 3.22.1, and Gradle
8.13.

```sh
gradle :android:app:assembleDebug
```

The ordinary APK build may compile the C producer with NDK Clang. The separate
CI boundary job recompiles the same producer sources with ICK C and proves that
those objects link through the NDK Android boundary.

The APK is written under `android/app/build/outputs/apk/debug/`.

## Touch contract

- drag inside one of the three disks: move that complex parameter handle;
- one finger outside the disks: pan the complex plane;
- two fingers outside the disks: zoom;
- lift: leave both camera and parameter values where they are.

A parameter drag captures that gesture so it does not accidentally pan or
pinch the limit-set view.

## Current renderer boundary

- one full-screen triangle; no CPU-side fractal point cloud or mesh;
- GLES 3 fragment shader with 32-bit complex arithmetic;
- validated binary64 group construction before narrowing to renderer floats;
- four independent circle centers and radius-squared values uploaded as uniforms;
- four exit maps uploaded as ordinary complex coefficient arrays;
- bounded 24-step circle classification / Möbius iteration;
- three analytic disk controls drawn in the same fragment pass;
- event-driven redraws when camera, controls, or window state changes.

The next parameter step is to let a chosen control mode request a new validated
group and packet rather than editing renderer coefficients directly.
