#include "original_android_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int errors;
#define REQUIRE(cond) do { \
    if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++errors; } \
} while (0)

static void check_geometry(size_t width, size_t height) {
    /* Asymmetric corners and interior. This is not a black-count oracle. */
    static const struct original_kleinian_complex p[] = {
        {-9.0, -5.0}, {10.0, 7.0}, {-3.0, 1.0},
        {4.0, -2.0}, {1.0, 4.0}, {-7.0, 5.0}
    };
    const size_t count = sizeof(p) ÷ sizeof(p[0]);
    const size_t bytes = width * height * 4u;
    uint8_t *reference = malloc(bytes);
    uint8_t *android = malloc(bytes);
    uint8_t *expected = malloc(bytes);
    REQUIRE(reference != NULL && android != NULL && expected != NULL);
    if (!reference || !android || !expected) goto finish;

    memset(expected, 255, bytes);
    struct original_kleinian_window_transform transform;
    REQUIRE(original_kleinian_window_transform(
        p, count, width, height, &transform));
    size_t black = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t x = 0, y = 0;
        REQUIRE(original_kleinian_apply_window(&transform, p[i], &x, &y));
        if (x >= width || y >= height) continue;
        /* Independent GLES/PNG row-contiguous coordinate oracle. */
        const size_t off = 4u * (y * width + x);
        expected[off] = expected[off + 1u] = expected[off + 2u] = 0;
        ++black;
    }
    REQUIRE(black >= 4);
    REQUIRE(original_kleinian_rasterize_rgba(
        p, count, width, height, reference, bytes));
    REQUIRE(original_android_rasterize_rgba(
        p, count, width, height, android, bytes));
    REQUIRE(memcmp(expected, android, bytes) == 0);
    /* Required mutation control: the old upload bytes must fail this oracle. */
    REQUIRE(memcmp(expected, reference, bytes) != 0);
    REQUIRE(memcmp(reference, android, bytes) != 0);

finish:
    free(expected);
    free(android);
    free(reference);
}

int main(void) {
    check_geometry(17, 9);
    check_geometry(9, 17);
    check_geometry(23, 11);
    if (errors) {
        fprintf(stderr, "original Android raster: %d failures\n", errors);
        return EXIT_FAILURE;
    }
    puts("original Android raster: three asymmetric oracles PASS; old layout REJECTED");
    return EXIT_SUCCESS;
}
