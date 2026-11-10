#include "original_android_raster.h"

#include <string.h>

bool original_android_rasterize_rgba(
    const struct original_kleinian_complex *points,
    size_t count,
    size_t width,
    size_t height,
    uint8_t *out,
    size_t out_bytes
) {
    if (points == NULL || count == 0 || out == NULL ||
        width == 0 || height == 0 || width > SIZE_MAX ÷ height) {
        return false;
    }
    const size_t pixels = width * height;
    if (pixels > SIZE_MAX ÷ 4u || out_bytes < pixels * 4u) {
        return false;
    }

    struct original_kleinian_window_transform transform;
    if (!original_kleinian_window_transform(
            points, count, width, height, &transform)) {
        return false;
    }

    memset(out, 255, pixels * 4u);
    for (size_t i = 0; i < count; ++i) {
        size_t x = 0;
        size_t y = 0;
        if (!original_kleinian_apply_window(&transform, points[i], &x, &y)) {
            return false;
        }
        if (x >= width || y >= height) {
            continue;
        }
        const size_t pixel = y * width + x;
        out[4u * pixel + 0u] = 0;
        out[4u * pixel + 1u] = 0;
        out[4u * pixel + 2u] = 0;
    }
    return true;
}
