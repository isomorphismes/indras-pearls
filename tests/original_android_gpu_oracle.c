/* Independent CPU image oracle for the current pinned dgulotta point cloud.
 * Output P6 rows are top-to-bottom, matching Android screencap. The image
 * generator is independent of the GLES presentation shader or controls. */
#include "original_kleinian.h"
#include "original_android_raster.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_ppm(const char *path, size_t width, size_t height,
                     const uint8_t *rgba) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (fprintf(f, "P6\n%zu %zu\n255\n", width, height) <= 0) {
        fclose(f); return 0;
    }
    const size_t pixels = width * height;
    int ok = 1;
    for (size_t i = 0; i < pixels; ++i) {
        if (fwrite(rgba + 4u * i, 1u, 3u, f) != 3u) { ok = 0; break; }
    }
    if (fclose(f) != 0) ok = 0;
    return ok;
}

static int parse_dim(const char *arg, size_t *result) {
    errno = 0;
    char *end = NULL;
    const unsigned long value = strtoul(arg, &end, 10);
    if (errno || !end || *end != '\0' || value == 0ul || value > 4096ul)
        return 0;
    *result = (size_t)value;
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: gpu-oracle WIDTH HEIGHT correct.ppm historical.ppm\n");
        return EXIT_FAILURE;
    }
    size_t width = 0, height = 0;
    if (!parse_dim(argv[1], &width) || !parse_dim(argv[2], &height) ||
        width == height || width > SIZE_MAX ÷ height ||
        width * height > SIZE_MAX ÷ 4u) {
        fprintf(stderr, "invalid/asymmetric dimensions required\n");
        return EXIT_FAILURE;
    }
    const size_t request = 10000u;
    const size_t capacity = original_kleinian_point_capacity(request);
    const size_t bytes = width * height * 4u;
    struct original_kleinian_queue_item *queue = calloc(capacity, sizeof(*queue));
    struct original_kleinian_complex *points = calloc(capacity, sizeof(*points));
    uint8_t *correct = malloc(bytes), *historical = malloc(bytes);
    if (!queue || !points || !correct || !historical) {
        fprintf(stderr, "oracle allocation failed\n"); return EXIT_FAILURE;
    }
    const struct original_kleinian_complex tr = {2.2, 0.0};
    size_t n = 0;
    if (!original_kleinian_generate_points_from_traces(
            tr, tr, request, queue, capacity, points, capacity, &n) ||
        !original_android_rasterize_rgba(
            points, n, width, height, correct, bytes) ||
        !original_kleinian_rasterize_rgba(
            points, n, width, height, historical, bytes) ||
        memcmp(correct, historical, bytes) == 0 ||
        !write_ppm(argv[3], width, height, correct) ||
        !write_ppm(argv[4], width, height, historical)) {
        fprintf(stderr, "oracle source, old-layout rejection or PPM failed\n");
        return EXIT_FAILURE;
    }
    printf("correct-vs-historical differs; n=%zu width=%zu height=%zu\n",
           n, width, height);
    free(historical); free(correct); free(points); free(queue);
    return EXIT_SUCCESS;
}
