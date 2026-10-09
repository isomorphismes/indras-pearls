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

The maintained application and host-test C frontend is ICK at
`c61e448251744a2f40ad743ebef1a027bdcd2f9d`. The shared compiler and Android
header qualification is pinned to
`isomorphisms/ai-ci@4ea071a96239f3a29ca6d98454feb59947d87cfe`.
Every application-owned C translation unit is compiled to assembly by ICK;
Android NDK `29.0.14206865` assembles it and links Android/EGL/GLES/Bionic.
Unmodified NDK NativeActivity glue remains an NDK C stage. Those upstream,
assembler, and platform-link stages remain explicitly declared qualification
gaps in `ci/build-toolchain.tsv`.

The normal APK retains its three ABIs and API 26 floor, inherited Fortify 2,
stack protection, format checks, and debug information. ARMv7 retains Thumb-2,
NEON, and Android softfp; ARM64 reserves Android's x18 register. The separate
freestanding ARMv7 core remains a distinct build and passes the original strict
API-21 standalone link probe before Gradle packages it with ICK-produced
application glue. GNU `readelf` retains the original ARM attribute oracle;
NDK tools inspect the linked and packaged ELF files.

Gradle still packages the existing native application with its existing package
and version. No application Java/Kotlin or DEX is introduced. Local native
compile/link qualification is separate from the finished-APK signer and emulator
checks in the exact-head workflow.

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
