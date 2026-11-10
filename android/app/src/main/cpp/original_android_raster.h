#ifndef INDRAS_PEARLS_ORIGINAL_ANDROID_RASTER_H
#define INDRAS_PEARLS_ORIGINAL_ANDROID_RASTER_H

#include "original_kleinian.h"

/* GLES upload buffer: RGBA pixels are row-contiguous, x varies fastest.
 * The reference's x * height + y byte layout remains independently available
 * through original_kleinian_rasterize_rgba(), which this file does not alter. */
bool original_android_rasterize_rgba(
    const struct original_kleinian_complex *points,
    size_t count,
    size_t width,
    size_t height,
    uint8_t *out,
    size_t out_bytes
);

#endif
