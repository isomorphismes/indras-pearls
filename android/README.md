# Android original Kleinian renderer

The current application renders the restored algorithm from
`isomorphismes/kleinian-groups@117dc5f34353e98cfae2f12bf386db5862110d92`.
`original_kleinian.c` builds the points and raster; `original_texture_renderer.c`
uploads that raster to GLES. The historical circle/renderer-packet implementation
remains available for its independent mathematics tests.

Two complex disk controls set the traces of the original generators:

- first disk: real and imaginary parts of `tr(a)`;
- second disk: real and imaginary parts of `tr(b)`.

The default traces are both `2.2`. NativeActivity touch events update the control
state, convert it to traces, regenerate the original raster, and upload the new
texture. A captured parameter drag does not also move the camera. A gesture
outside the controls retains the existing camera behavior.

## Build and signing boundaries

The ARMv7 production-core stage uses ICK C at
`ea034a574097e81d1027660b39c3e1c185d11b80`, with Thumb instructions and the
Android softfp calling convention. Android NDK `29.0.14206865` compiles the
platform/control glue and links the ICK object against Android/EGL/GLES/Bionic.
The emulator and host-test C stages currently use that NDK. Gradle packages the
existing native application; no application Java/Kotlin or DEX is introduced.

The ICK qualification on this branch covers the freestanding ARMv7 mathematics
object. Qualification of the Android platform glue, final platform link, and
x86_64 emulator/host stages through ICK remains absent. Those stages currently
use NDK; this is a qualification gap, not proof that ICK cannot implement them.

Installable test artifacts use the existing public Wegert test signer at
`isomorphismes/wegert@89dcfb840cec1a66ee04c7f7404954cbd6c09839`.
The central ai-ci finished-APK gate checks the registered package and certificate
after packaging. No missing-key generation or runner-local debug identity is
permitted. Test signing does not authorize a production/store release.

## Acceptance

`tests/qemu-original-raster.sh` requires nontrivial GPU and decoded display
content. `tests/qemu-original-trace-controls.sh` drives Android touchscreen
gestures for all four trace coordinates. Each coordinate must change the
mathematical raster, GPU RGB hash, and a decoded RGB display crop that excludes
the controls and navigation chrome. AE and normalized RMSE thresholds reject
incidental pixel changes; PNG file-byte differences are not the visual oracle.

These tests run on an x86_64 Android emulator. They do not establish that the
ARMv7 APK behaves correctly on the physical MIRO A1, or establish update
continuity on that device. Keep those acceptance boundaries open.
