# Coxeter word handoff

This repository is the intended rendering side of an optional handoff from `isomorphismes/coxeter`.

The Coxeter side now has a design-time prompt that may appear only when an upstream caller has already established that a word has an associated renderable hyperbolic surface. The A2/A3 reducer itself must not infer hyperbolic geometry from a finite spherical word.

## Intended boundary

```text
Coxeter word + certified geometric context
  -> optional render suggestion
  -> Indra's Pearls render request
  -> image / interactive view
```

The rendering repository should receive explicit geometric data, not try to reverse-engineer it from a bare generator list. A future request should therefore carry enough information to identify the actual representation, for example:

```text
{
  schema: "coxeter-hyperbolic-render/v0",
  source: "isomorphismes/coxeter",
  word: [...],
  representation: <explicit hyperbolic/Kleinian data>,
  provenance: <how that representation was established>
}
```

`representation` is deliberately unspecified in this design stub. It may eventually be a reflection representation, a matrix/Mobius-generator representation, a fundamental-domain description, or another exact object suitable for rendering.

## Current status

As of August 26, 2026 there is no automatic renderer handoff. Coxeter can only print the suggestion. Indra's Pearls should not claim it can render a word until the geometric representation and provenance are supplied explicitly.
