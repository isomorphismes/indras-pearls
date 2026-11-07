# Rank-two character coordinates versus the original Kleinian slice

## Mathematical boundary

The current Android application restores the default `dgulotta/kleinian`
`generators(ta,tb)` construction and its CircleQueue raster. Its two complex
controls are **not** a parametrization of the full rank-two character variety.

For a marked pair `A,B ∈ SL(2,C)`, write

- `x = tr(A)`
- `y = tr(B)`
- `z = tr(AB)`

The Fricke trace relation is

```text
κ(x,y,z) = tr(ABA⁻¹B⁻¹) = x² + y² + z² − xyz − 2.
```

The original `original_kleinian_generators(x,y)` computes `z` via

```text
z = (xy − sqrt((xy)² − 4(x² + y²))) / 2
```

and therefore imposes

```text
z² − xyz + x² + y² = 0,    κ(x,y,z) = −2.
```

Thus the historical two-trace UI lies on a **parabolic-commutator slice**,
as for once-punctured-torus groups. The chosen square-root branch is part
of the original reference contract. Do not reinterpret its two disks as
three independent complex generators or change this section while asserting
the original renderer remains bit-identical.

The full affine `SL(2,C)` rank-two character space has three independent
complex coordinates `(x,y,z)`. The locus `κ=2` consists of reducible
characters; a character in this locus need not identify an individual
representation uniquely up to ordinary conjugacy. A general character
does **not** imply the represented group is discrete, faithful, or
geometrically qualified for this application's circle-queue rendering.

Reference: William M. Goldman, *Introduction to the Fricke spaces of
surface groups*, §1.5 (the Fricke–Vogt rank-two trace coordinates and
commutator polynomial),
https://math.umd.edu/~wmg/mcgdynamics.pdf .

## Independent matrix witness (new, host math only)

`original_kleinian_character_matrices(x,y,z,&A,&B)` builds

```text
       [ x  -1 ]                 [ 0    u ]
 A  =  [ 1   0 ]          B  =   [ v    y ]

 u² − zu + 1 = 0;   v = −1/u.
```

Consequently `det A = det B = 1`,
`tr A = x`, `tr B = y`, `tr AB = u − v = z`.
The implementation selects the root of largest modulus and computes
`v = −1/u` to avoid cancellation in the inverse root, rejects
nonfinite/overflow and aliased outputs, and does not modify outputs on
failure. It does not allocate, link Android, or alter the raster producer.

The independent host observer recomputes determinants, products,
commutator matrices, and `κ` from the **returned matrices**, rather
than trusting the constructor's internal intermediates. Its fixtures
include complex coordinates, `z=±2` repeated roots, `κ=2` reducible
characters, a concrete nonparabolic witness `(x,y,z)=(2.2,2.2,0)`
with `κ=7.68`, and rejects invalid input and aliased outputs.
It also checks the unchanged four gesture-relevant original two-trace
fixtures still have `κ=-2`.

## Next extension gate

Do **not** make the three-trace matrix witness a third UI disk yet.
Determine a domain of triples for which the existing circle construction
and queue produce meaningful nondegenerate finite centers, test conjugacy
and scene-coordinate normalization against independently known limit sets,
and require downstream rendered RGB/gesture evidence. At the boundary
`κ=-2`, confirm that any alternate conjugate realization maps to the
historical picture by an explicitly tested Möbius coordinate change.
Never use matching trace polynomials as a proxy for pixel equality.

The DEX/ART advances can eventually own typed Activity and application
state when a concrete requirement emerges. No managed code is necessary
for this matrix witness or the existing C/ICK/EGL/GLES application.
