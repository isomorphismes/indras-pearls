# Pure C mathematical core tests

Run from the repository root with a C17 compiler and the system math library:

```text
cc -std=c17 -Wall -Wextra -Werror -pedantic -O2 -I android/app/src/main/cpp android/app/src/main/cpp/mobius_math.c tests/mobius_math_test.c -lm -o /tmp/indras-pearls-mobius-math-test
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
