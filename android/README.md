# Android native limit-set slice

This is the first playable native rendering boundary: NativeActivity + `android_native_app_glue`, EGL/GLES 3, one-finger camera pan, two-finger pinch zoom, and one full-screen limit-set shader.

The bundled mathematical state is a small independently specified classical rank-2 Schottky group. Four disjoint isometric circles and four determinant-one Möbius maps live in plain C state. The fragment shader repeatedly moves a screen-space complex point out of the circle that contains it; bounded escape depth gives the visible limit-set approximation.

Android input dispatch still mutates camera state only. It does not contain or edit group parameters, so the mathematical state can later move into Idriç/Edriç ownership without turning touch dispatch into the group model.

The renderer is an independent native implementation. `philogb.md` and `notes/webgpu.md` remain reference notes about Nico Belmonte's public deployment; they do not assert a license for his application code, and this slice does not structurally port the browser bundle.

## Build

Use the same Android toolchain as the other native math toys: JDK 17, Android SDK 36, NDK `29.0.14206865`, CMake 3.22.1, and Gradle 8.13.

```sh
gradle :android:app:assembleDebug
```

The APK is written under `android/app/build/outputs/apk/debug/`.

## Touch contract

- one finger: pan the complex plane
- two fingers: zoom
- lift: leave the camera where it is

No group-parameter gestures are wired yet.

## Current renderer boundary

- one full-screen triangle; no CPU-side point cloud or mesh for the fractal;
- GLES 3 fragment shader with 32-bit complex arithmetic;
- one bundled group uploaded as uniforms;
- bounded 24-step circle classification / Möbius iteration;
- event-driven redraws when the camera or window changes.

This is deliberately only one group and one flat limit-set view. Schottky construction UI, Maskit parameter editing, feedback textures, Riemann-sphere projection, and the browser application's scene structure are not part of this slice.
