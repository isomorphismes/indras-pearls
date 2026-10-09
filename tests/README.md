# Mathematical and interaction acceptance

The maintained executable commands live in
[`android.yml`](../.github/workflows/android.yml) and [`Makefile`](Makefile).
Host tests and application-owned Android C use ICK
`c61e448251744a2f40ad743ebef1a027bdcd2f9d`, with the qualified stage pinned to
`isomorphisms/ai-ci@4ea071a96239f3a29ca6d98454feb59947d87cfe`.
Android NDK `29.0.14206865` assembles and links the emitted Android assembly;
the host stage declares its native runtime separately. The original unmodified
NDK glue and platform-link boundaries are recorded in `ci/build-toolchain.tsv`.

## Division migration qualification

The account-wide migration replaces 59 binary C divisions in six files. The
Idriç source scanner checked all 20 tracked C/header files: no C arithmetic `/`
or `/=` remains. Include paths and embedded GLSL division stay in their own
lexical languages. The historical parameter controls are compiled independently
by the host target and remain outside the restored application's UI.

The prior NDK host producer passed 257 original-core and 24 trace-control
assertions; the ICK baseline passed 148 historical mathematics assertions.
The migrated ICK producer passes the same assertions and two explicit null-input
rejections, for 431 total. The two public generator-pointer declarations now
match their existing null-rejection implementation, instead of making a
contradictory non-null `static` array promise. Strict warnings stay enabled.

Local qualification also links the complete debug library for ARMv7, ARM64, and
x86-64 using the actual r29 NDK and inherited hardening/debug flags. The separate
freestanding ARM core passes the unchanged ARMv7/Thumb-2/softfp ELF checks,
strict API-21 shared-library probe, and full application-library link. These
results are recorded in `ci/division-glyph-local.tsv`; finished APKs, signer
checks, and the two emulator oracles run independently in CI.

## Original algorithm and trace controls

`original_kleinian_test.c` tests the C translation of
`isomorphismes/kleinian-groups@117dc5f34353e98cfae2f12bf386db5862110d92`,
including generator traces, CircleQueue growth, finite bounds, and the default
web raster. `original_trace_controls_test.c` tests control-to-trace mapping,
gesture capture, clamping, and changes to the actual original raster.

`qemu-original-raster.sh` installs the finished APK in an Android emulator and
requires dark and bright mathematical content on the GPU and decoded display.
`qemu-original-trace-controls.sh` separately moves `tr(a).re`, `tr(a).im`,
`tr(b).re`, and `tr(b).im` through Android touchscreen gestures. Every case must
change the regenerated raster, GPU RGB hash, and a control-free display crop.
Decoded RGB hashes, AE with a 2% fuzz threshold, a minimum changed-pixel count,
and normalized RMSE above `0.005` form the visual test. Alpha and PNG metadata
do not supply acceptance evidence.

The workflow verifies the existing central signer before exposing each APK.
Emulator success is distinct from physical MIRO A1 acceptance and from an
old-to-new replacement installation on that device.

## Historical circle/packet mathematics

`mobius_math_test.c` retains the binary64 determinant, inverse, projective-action,
fixed-point, trace-identity, circle-pairing, domain, and renderer-packet tests.
Determinant tolerance is `1e-12 × max(1, |ad| + |bc|)`; inverse and bundled
coefficient tolerance is `1e-12`; projective residual tolerance is `2e-11`.
These checks cover the historical circle family independently. They are not
evidence that its old three-disk UI controls drive the restored application.

`original_kleinian_ndk_probe.c` is the ABI/link probe for the current ICK-produced
original core. The older renderer-packet probe is retained for that older
boundary and must not stand in for the production original-core probe.
