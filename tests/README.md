# Mathematical and interaction acceptance

The maintained executable commands live in
[`android.yml`](../.github/workflows/android.yml). Host C tests use Android NDK
`29.0.14206865` in host mode. The production ARMv7 mathematics object uses ICK C
`ea034a574097e81d1027660b39c3e1c185d11b80`; NDK performs the platform link.
Qualification of the host/emulator and platform-glue stages through ICK remains
an explicit gap. Generic system `cc` is not the maintained test route.

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
