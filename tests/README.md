# Pure C mathematical core tests

Run from the repository root with a C17 compiler and the system math library:

```text
cc -std=c17 -Wall -Wextra -Werror -pedantic -O2 -I android/app/src/main/cpp android/app/src/main/cpp/mobius_math.c android/app/src/main/cpp/renderer_packet.c tests/mobius_math_test.c -lm -o /tmp/indras-pearls-mobius-math-test
/tmp/indras-pearls-mobius-math-test
```

The host test uses explicit binary64 tolerances:

| Check | Tolerance |
|---|---:|
| Determinant-one checks | `1e-12 × max(1, |ad| + |bc|)` for determinant roundoff |
| Inverse residuals and bundled coefficients | `1e-12` absolute |
| Projective fixed-point, action, trace-identity, and circle-equation residuals | `2e-11` absolute |
| Circle relation classification, in affine coordinate units | `1e-12` absolute |
| Continuity probe | `r` changes by `1e-6`; matrix and circle-center changes stay below `1e-5`, radius below `2e-6`, and the circle gap remains positive |

The looser projective bound covers the binary64 complex square root and division
used to recover matrix eigenvectors, while remaining small compared with the
bundled fixed-point separation. Circle classification uses an absolute
coordinate tolerance because the symmetric family is tested in one fixed
affine frame; it does not claim a scale-independent tolerance policy for
arbitrary presentations. Tests do not call Android, EGL, GLES, or renderer code.

The executable prints the number of assertions it ran. Its cases cover:

| Test family | Mathematical fact checked |
|---|---|
| Canonical group and traces | `det(A)=det(B)=1`, derived inverse laws, projective normalization, `tr(AB⁻¹)=tr(A)tr(B)−tr(AB)`, Fricke commutator identity, and the bundled trace values |
| Projective action | Matrix composition order, agreement with finite affine action, inverse action, and exact infinity at a pole |
| Fixed points | Projective fixed-point equation, bundled closed forms, and local multipliers |
| Circle presentation | `|cz+d|=1`, boundary-to-paired-boundary mapping, interior-to-exterior mapping, four-circle pairing, positive disjointness margin, and disjoint/tangent/overlap classification |
| Family boundary and reconstruction | Strict `0<r<1/√2` interior status, tangent boundary, overlap rejection, exact `r=0.7` renderer coefficients, and continuity away from the boundary |
| Renderer packet | Flattening of the validated four-circle presentation into the scalar/array GLES boundary, four independent radius-squared slots, rejection outside the classical domain, and output preservation on failure |

The family constructor reports a positive parameter that cannot be represented
with a determinant-one binary64 matrix as `FAMILY_NUMERICALLY_UNREPRESENTABLE`;
that numerical limit is separate from the mathematical open domain.

## ICK C / Android NDK boundary

CI also builds the pinned ICK C compiler for ARMv7, compiles `mobius_math.c`
and `renderer_packet.c` as freestanding `armeabi-v7a` objects, and lets
Android NDK r29 Clang/lld perform the final shared-library link with
`tests/renderer_packet_ndk_probe.c`.

Only an ordinary `float` radius, integer status, and flat float array cross
between ICK-compiled code and NDK-compiled code. No C aggregate or compiler
private complex representation crosses that boundary.


## Live control and emulator acceptance

The live-control host test exercises the actual touch-coordinate state machine,
maps the horizontal handle to the classical-family radius, rebuilds two renderer
packets, and requires the packet to change:

```text
cc -std=c17 -Wall -Wextra -Werror -pedantic -O2 -I android/app/src/main/cpp \
  android/app/src/main/cpp/mobius_math.c \
  android/app/src/main/cpp/renderer_packet.c \
  android/app/src/main/cpp/parameter_controls.c \
  tests/control_packet_test.c -lm -o /tmp/indras-pearls-control-test
/tmp/indras-pearls-control-test
```

CI then runs the built APK in an Android x86-64 emulator (QEMU), enables a
debug-only framebuffer probe, renders through the same GLES shader path into a
deterministic offscreen framebuffer at `r = 0.7`, performs an ADB drag to
`r = 0.4`, and renders that same path again. The probe reads only the lower
renderer region, excluding the control overlay. The
test requires the native semantic receipt, different framebuffer hashes, and a
nontrivial pixel difference between the two GPU frames. Ordinary Android
`screencap` images are retained only as diagnostics because a headless QEMU
compositor can return a black display capture even while the app's accelerated
surface is rendering.
