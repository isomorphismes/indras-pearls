# Notes on Nico Belmonte's WebGPU implementation

Nico Belmonte's *Indra's Pearls* visualization is useful as a reference because it treats the mathematics as something to move through rather than as a sequence of static illustrations. The deployed application is a browser/WebGPU implementation, but the architectural ideas transfer cleanly to a small native renderer.

## What is worth carrying over

### Möbius transformations as the primitive

The right unit of state is the complex Möbius map

`z ↦ (a z + b) / (c z + d)`.

Everything else should be built from a small set of generators and their inverses. That keeps the mathematical boundary small and makes it possible to move group/state code out of the renderer later.

### Iterate on the GPU, not by shipping a giant point cloud

Belmonte's browser renderer uses the GPU as the place where repeated transformations happen. For the first Android slice, the same principle is more valuable than reproducing his WebGPU object structure. A fragment shader can take a screen-space complex point, decide which side/region it lies in, apply the appropriate generator or inverse, and repeat for a bounded number of steps.

That gives us:

- immediate response while panning and zooming;
- no large CPU-side geometry buffer;
- a very small native shell;
- a natural route to the existing Edriç/Idriç GPU path later.

### Texture feedback is useful for the circle/tessellation views

The deployed code uses a render-to-texture/feedback style for iterative constructions and then presents the result both on a flat quad and on a Riemann-sphere view. The important idea is the separation between **the mathematical image being generated** and **the surface used to display it**. We should retain that separation even if the first phone build only shows a flat plane.

### Limit sets want a direct shader path

The most interesting part for a phone toy is the limit-set renderer. Belmonte describes a shader implementation based on Jos Leys' fast algorithm for Kleinian-group limit sets with the Maskit parametrisation. The renderer repeatedly classifies the current point and applies one of the Möbius generators (or its inverse). That is a better first target than trying to reproduce every chapter/scene from the web demo.

### Maskit parameters should be state, not UI logic

The demo exposes parameters such as `µ`, Farey numerator/denominator, projection, phase, and coloring. In our version those should be plain mathematical/render state. Android touch code should only mutate camera parameters at first; mathematical parameter editing can be added without entangling it with input dispatch.

## What I would not carry over

The deployed application is a compiled browser bundle with React/UI machinery and WebGPU setup mixed into a fairly large generated JavaScript artifact. None of that is a useful dependency for a native Android toy. It is better treated as evidence that the rendering ideas work, not as a codebase to port structurally.

Likewise, the first native slice should not reproduce all of:

- Schottky-circle construction;
- Fuchsian and quasi-Fuchsian scenes;
- Apollonian gasket conjugations;
- the Riemann sphere;
- Maskit-slice plotting and cusp solving;
- palette/theme controls.

One good limit-set view with smooth pan/zoom is enough to establish the rendering boundary.

## First native target

1. `NativeActivity` + EGL + GLES 3.
2. Full-screen triangle/quad.
3. A fragment shader containing complex-float Möbius arithmetic and one bundled two-generator example.
4. One-finger drag pans the complex plane.
5. Two-finger pinch changes scale.
6. Renderer state is plain C structs for now, deliberately shaped so the mathematical state can later be generated/owned by Idriç.

Use 32-bit floats unless a concrete limit set visibly needs more precision. The phone interaction should remain camera-only in the initial stub.

## Licensing boundary

Belmonte's public deployment repository currently has no repository-level license. These notes describe ideas and observed architecture; code in this repository should be independently written rather than copied from the deployed JavaScript/WGSL.
