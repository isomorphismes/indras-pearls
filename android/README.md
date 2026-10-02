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

The three disk controls are now live Schottky parameters. Each disk stores one
complex value, clamped to radius `0.94`, and all six real coordinates feed the
validated group construction before redraw.

The current chart is a classical-circle chart centered on the bundled group:

- disk 1 moves the antipodal isometric-circle pair for generator `A` around
  the default centers `±1`;
- disk 2 moves the antipodal pair for generator `B` around `±i`;
- disk 3 controls the two pairing phases, one phase per generator.

The common circle radius is recomputed from the minimum separation of the four
centers using a fixed factor below one half. That gives a positive disjointness
margin over the whole control domain instead of accepting arbitrary matrix
perturbations and hoping they remain classical. Zero controls reproduce the
historical `r = 0.7` group.

The renderer remains an independent native implementation. `philogb.md` and
`notes/webgpu.md` are reference notes about Nico Belmonte's public
deployment; they do not assert a license for his application code.

## Build

Use JDK 17, Android SDK 36, NDK `29.0.14206865`, CMake 3.22.1, and Gradle
8.13.

~~~sh
gradle :android:app:assembleDebug
~~~

An ICK-object ARMv7 build uses:

~~~sh
gradle :android:app:assembleDebug \
  -PickArmv7=true \
  -PickMobiusObject=/absolute/path/to/mobius_math.o \
  -PickRendererPacketObject=/absolute/path/to/renderer_packet.o
~~~

The ordinary APK build compiles the C producer with NDK Clang for all supported
ABIs. The ICK ARMv7 build mode instead accepts precompiled ICK objects for
`mobius_math.c` and `renderer_packet.c`, restricts packaging to
`armeabi-v7a`, and lets the same CMake/NDK link produce the actual application
shared library and APK. The NativeActivity logs which producer supplied the
Schottky mathematics.

CI rebuilds the pinned ICK C compiler, compiles those two production translation
units, runs the standalone strict-link probe, then builds and inspects the
MIRO-targeted APK using the same ICK objects.

The APK is written under `android/app/build/outputs/apk/debug/`.

## Touch contract

- drag inside any of the three disks: move that complex parameter and rebuild
  the validated Schottky group/renderer packet;
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

CI tests the full causal chain. Host tests exercise the actual touch-coordinate
state machine and a 343-state three-disk grid. An Android emulator test then
drags each disk through the real input path and requires the GLES framebuffer
below the control overlay to change. A build/link receipt alone is therefore
not treated as interaction integration.
