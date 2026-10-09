#ifndef INDRAS_PEARLS_ORIGINAL_KLEINIAN_H
#define INDRAS_PEARLS_ORIGINAL_KLEINIAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ORIGINAL_KLEINIAN_GENERATOR_COUNT 4

struct original_kleinian_complex {
    double re;
    double im;
};

struct original_kleinian_matrix2 {
    struct original_kleinian_complex a;
    struct original_kleinian_complex b;
    struct original_kleinian_complex c;
    struct original_kleinian_complex d;
};

struct original_kleinian_circle {
    struct original_kleinian_matrix2 hermitian;
};

struct original_kleinian_generator {
    struct original_kleinian_matrix2 matrix;
    struct original_kleinian_circle circle;
};

struct original_kleinian_queue_item {
    struct original_kleinian_matrix2 matrix;
    uint8_t last;
    double priority;
};

struct original_kleinian_window_transform {
    double scale;
    double xoff;
    double yoff;
};

/*
 * Literal C port of dgulotta/kleinian's default "oi" path:
 *   generators(ta, tb) -> CircleQueue -> circle centers -> window transform.
 *
 * The caller owns all storage so this core remains suitable for ICK/freestanding
 * compilation. generate_points() may return one point more than requested when
 * num_points is odd, matching the Rust queue-growth rule (4, 6, 8, ...).
 */
size_t original_kleinian_point_capacity(size_t num_points);

/* Non-null generator pointers designate ORIGINAL_KLEINIAN_GENERATOR_COUNT
 * elements. Both entrypoints reject a null generator pointer. */
bool original_kleinian_generators(
    struct original_kleinian_complex ta,
    struct original_kleinian_complex tb,
    struct original_kleinian_generator *out
);

bool original_kleinian_generate_points(
    const struct original_kleinian_generator *generators,
    size_t num_points,
    struct original_kleinian_queue_item *queue_storage,
    size_t queue_capacity,
    struct original_kleinian_complex *points,
    size_t point_capacity,
    size_t *point_count
);

bool original_kleinian_generate_points_from_traces(
    struct original_kleinian_complex ta,
    struct original_kleinian_complex tb,
    size_t num_points,
    struct original_kleinian_queue_item *queue_storage,
    size_t queue_capacity,
    struct original_kleinian_complex *points,
    size_t point_capacity,
    size_t *point_count
);

bool original_kleinian_window_transform(
    const struct original_kleinian_complex *points,
    size_t point_count,
    size_t width,
    size_t height,
    struct original_kleinian_window_transform *transform
);

bool original_kleinian_apply_window(
    const struct original_kleinian_window_transform *transform,
    struct original_kleinian_complex point,
    size_t *x,
    size_t *y
);

/*
 * Reproduces kleinian-web's ImageData construction, including its historical
 * column-major-looking index expression: idx = x * height + y.
 */
bool original_kleinian_rasterize_rgba(
    const struct original_kleinian_complex *points,
    size_t point_count,
    size_t width,
    size_t height,
    uint8_t *rgba,
    size_t rgba_size
);

#endif
