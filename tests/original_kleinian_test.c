#include "original_kleinian.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;
static int assertions = 0;

#define CHECK(expr) do {     ++assertions;     if (!(expr)) {         ++failures;         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);     } } while (0)

static bool near(double actual, double expected, double tolerance) {
    return fabs(actual - expected) <= tolerance;
}

static void check_complex(
    struct original_kleinian_complex actual,
    double re,
    double im,
    double tolerance
) {
    CHECK(near(actual.re, re, tolerance));
    CHECK(near(actual.im, im, tolerance));
}

static void test_original_default_generators(void) {
    struct original_kleinian_generator generators[ORIGINAL_KLEINIAN_GENERATOR_COUNT];
    const struct original_kleinian_complex ta = {2.2, 0.0};
    const struct original_kleinian_complex tb = {2.2, 0.0};

    CHECK(!original_kleinian_generators(ta, tb, NULL));
    CHECK(original_kleinian_generators(ta, tb, generators));

    /*
     * Golden values from dgulotta/kleinian's generators() formula at the
     * web UI defaults.  These test the literal trace parameterization, not
     * the superseded symmetric-radius family.
     */
    check_complex(generators[0].matrix.a, 1.1, 0.0, 2e-12);
    check_complex(generators[0].matrix.b, 0.0, 0.11118055826844087, 2e-12);
    check_complex(generators[0].matrix.c, 0.0, -1.8888194417315578, 2e-12);
    check_complex(generators[0].matrix.d, 1.1, 0.0, 2e-12);

    check_complex(generators[1].matrix.a, 1.1, -1.0, 2e-12);
    check_complex(generators[1].matrix.b, 1.1, 0.0, 2e-12);
    check_complex(generators[1].matrix.c, 1.1, 0.0, 2e-12);
    check_complex(generators[1].matrix.d, 1.1, 1.0, 2e-12);

    /* The fork's generator order is exactly [a, b, a^-1, b^-1]. */
    check_complex(generators[2].matrix.a, generators[0].matrix.d.re,
                  generators[0].matrix.d.im, 2e-12);
    check_complex(generators[2].matrix.b, -generators[0].matrix.b.re,
                  -generators[0].matrix.b.im, 2e-12);
    check_complex(generators[2].matrix.c, -generators[0].matrix.c.re,
                  -generators[0].matrix.c.im, 2e-12);
    check_complex(generators[2].matrix.d, generators[0].matrix.a.re,
                  generators[0].matrix.a.im, 2e-12);

    check_complex(generators[3].matrix.a, generators[1].matrix.d.re,
                  generators[1].matrix.d.im, 2e-12);
    check_complex(generators[3].matrix.b, -generators[1].matrix.b.re,
                  -generators[1].matrix.b.im, 2e-12);
    check_complex(generators[3].matrix.c, -generators[1].matrix.c.re,
                  -generators[1].matrix.c.im, 2e-12);
    check_complex(generators[3].matrix.d, generators[1].matrix.a.re,
                  generators[1].matrix.a.im, 2e-12);
}

static void test_original_circle_queue(void) {
    CHECK(original_kleinian_point_capacity(1) == 4);
    CHECK(original_kleinian_point_capacity(4) == 4);
    CHECK(original_kleinian_point_capacity(5) == 6);
    CHECK(original_kleinian_point_capacity(100) == 100);

    const size_t request = 100;
    const size_t capacity = original_kleinian_point_capacity(request);
    struct original_kleinian_queue_item *queue =
        calloc(capacity, sizeof(*queue));
    struct original_kleinian_complex *points =
        calloc(capacity, sizeof(*points));
    CHECK(queue != NULL);
    CHECK(points != NULL);
    if (queue == NULL || points == NULL) {
        free(points);
        free(queue);
        return;
    }

    const struct original_kleinian_complex ta = {2.2, 0.0};
    const struct original_kleinian_complex tb = {2.2, 0.0};
    size_t count = 0;
    CHECK(!original_kleinian_generate_points(
        NULL, request, queue, capacity, points, capacity, &count
    ));
    CHECK(original_kleinian_generate_points_from_traces(
        ta, tb, request, queue, capacity, points, capacity, &count
    ));
    CHECK(count == request);

    double xmin = points[0].re;
    double xmax = points[0].re;
    double ymin = points[0].im;
    double ymax = points[0].im;
    for (size_t i = 0; i < count; ++i) {
        CHECK(isfinite(points[i].re));
        CHECK(isfinite(points[i].im));
        if (points[i].re < xmin) xmin = points[i].re;
        if (points[i].re > xmax) xmax = points[i].re;
        if (points[i].im < ymin) ymin = points[i].im;
        if (points[i].im > ymax) ymax = points[i].im;
    }

    CHECK(xmin < -0.99);
    CHECK(xmax > 0.99);
    CHECK(ymin < -0.90);
    CHECK(ymax > 0.90);
    CHECK(xmin > -1.01);
    CHECK(xmax < 1.01);
    CHECK(ymin > -0.92);
    CHECK(ymax < 0.92);

    free(points);
    free(queue);
}

static void test_original_web_raster_contract(void) {
    const size_t request = 10000;
    const size_t capacity = original_kleinian_point_capacity(request);
    const size_t width = 128;
    const size_t height = 128;
    const size_t rgba_size = width * height * 4u;

    struct original_kleinian_queue_item *queue =
        calloc(capacity, sizeof(*queue));
    struct original_kleinian_complex *points =
        calloc(capacity, sizeof(*points));
    unsigned char *rgba = malloc(rgba_size);
    CHECK(queue != NULL);
    CHECK(points != NULL);
    CHECK(rgba != NULL);
    if (queue == NULL || points == NULL || rgba == NULL) {
        free(rgba);
        free(points);
        free(queue);
        return;
    }

    const struct original_kleinian_complex ta = {2.2, 0.0};
    const struct original_kleinian_complex tb = {2.2, 0.0};
    size_t count = 0;
    CHECK(original_kleinian_generate_points_from_traces(
        ta, tb, request, queue, capacity, points, capacity, &count
    ));
    CHECK(count == request);
    CHECK(original_kleinian_rasterize_rgba(
        points, count, width, height, rgba, rgba_size
    ));

    size_t black_pixels = 0;
    for (size_t i = 0; i < width * height; ++i) {
        if (rgba[4u * i] == 0 &&
            rgba[4u * i + 1u] == 0 &&
            rgba[4u * i + 2u] == 0 &&
            rgba[4u * i + 3u] == 255) {
            ++black_pixels;
        }
    }

    /*
     * The exact heap order for equal-priority symmetric items is intentionally
     * not treated as an oracle.  The working reference produces about 1,076
     * black pixels here; this range catches blank/solid/degenerate output while
     * allowing equivalent tie ordering.
     */
    CHECK(black_pixels > 800);
    CHECK(black_pixels < 1400);

    free(rgba);
    free(points);
    free(queue);
}

int main(void) {
    test_original_default_generators();
    test_original_circle_queue();
    test_original_web_raster_contract();

    if (failures != 0) {
        fprintf(stderr, "%d/%d original-kleinian assertions failed\n",
                failures, assertions);
        return 1;
    }

    printf("%d original-kleinian assertions passed\n", assertions);
    return 0;
}
