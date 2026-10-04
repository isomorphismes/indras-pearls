#include "original_kleinian.h"
#include "original_trace_controls.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int assertions;

#define CHECK(condition, message) do { \
    ++assertions; \
    if (!(condition)) { \
        ++failures; \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
    } \
} while (0)

static int nearf(float left, float right, float tolerance) {
    return fabsf(left - right) <= tolerance;
}

static int raster_for_controls(
    const struct original_trace_controls *controls,
    size_t width,
    size_t height,
    unsigned char *rgba,
    size_t rgba_size
) {
    const size_t request = 10000;
    const size_t capacity = original_kleinian_point_capacity(request);
    struct original_kleinian_queue_item *queue =
        calloc(capacity, sizeof(*queue));
    struct original_kleinian_complex *points =
        calloc(capacity, sizeof(*points));

    if (queue == NULL || points == NULL) {
        free(points);
        free(queue);
        return 0;
    }

    struct original_kleinian_complex ta;
    struct original_kleinian_complex tb;
    original_trace_controls_to_traces(controls, &ta, &tb);

    size_t point_count = 0;
    int ok = original_kleinian_generate_points_from_traces(
        ta, tb, request,
        queue, capacity,
        points, capacity,
        &point_count
    );
    if (ok) {
        ok = original_kleinian_rasterize_rgba(
            points, point_count, width, height, rgba, rgba_size
        );
    }

    free(points);
    free(queue);
    return ok;
}

static size_t changed_pixels(
    const unsigned char *left,
    const unsigned char *right,
    size_t pixel_count
) {
    size_t changed = 0;
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const size_t offset = pixel * 4u;
        if (left[offset] != right[offset] ||
            left[offset + 1u] != right[offset + 1u] ||
            left[offset + 2u] != right[offset + 2u]) {
            ++changed;
        }
    }
    return changed;
}

static void test_default_trace_mapping(void) {
    struct original_trace_controls controls;
    initialize_original_trace_controls(&controls);

    struct original_kleinian_complex ta;
    struct original_kleinian_complex tb;
    original_trace_controls_to_traces(&controls, &ta, &tb);

    CHECK(fabs(ta.re - 2.2) < 1.0e-12, "default tr(a).re is original 2.2");
    CHECK(fabs(ta.im) < 1.0e-12, "default tr(a).im is original zero");
    CHECK(fabs(tb.re - 2.2) < 1.0e-12, "default tr(b).re is original 2.2");
    CHECK(fabs(tb.im) < 1.0e-12, "default tr(b).im is original zero");
    CHECK(controls.active_index == -1, "no trace control starts active");
}

static void test_touch_mapping(void) {
    struct original_trace_controls controls;
    initialize_original_trace_controls(&controls);

    const int width = 600;
    const int height = 1200;
    const float radius = original_trace_control_radius(width, height);

    for (int control = 0; control < ORIGINAL_TRACE_CONTROL_COUNT; ++control) {
        float center_x = 0.0f;
        float center_y = 0.0f;
        original_trace_control_center(
            control, width, height, &center_x, &center_y
        );
        const float input_center_y = (float)height - center_y;

        initialize_original_trace_controls(&controls);
        CHECK(
            begin_original_trace_drag(
                &controls, center_x, input_center_y, width, height
            ),
            "touch captures original trace disk"
        );
        CHECK(
            update_original_trace_drag(
                &controls,
                center_x + 0.5f * radius,
                input_center_y - 0.25f * radius,
                width,
                height
            ),
            "drag updates original trace disk"
        );
        CHECK(
            nearf(controls.value[control].real, 0.5f, 2.0e-5f),
            "trace disk real coordinate follows touch"
        );
        CHECK(
            nearf(controls.value[control].imaginary, 0.25f, 2.0e-5f),
            "trace disk imaginary coordinate follows touch"
        );
        end_original_trace_drag(&controls);
    }
}

static void test_each_trace_axis_changes_original_raster(void) {
    const size_t width = 128;
    const size_t height = 96;
    const size_t pixel_count = width * height;
    const size_t rgba_size = pixel_count * 4u;

    unsigned char *baseline = malloc(rgba_size);
    unsigned char *moved = malloc(rgba_size);
    CHECK(baseline != NULL, "allocate baseline raster");
    CHECK(moved != NULL, "allocate moved raster");
    if (baseline == NULL || moved == NULL) {
        free(moved);
        free(baseline);
        return;
    }

    struct original_trace_controls controls;
    initialize_original_trace_controls(&controls);
    CHECK(
        raster_for_controls(&controls, width, height, baseline, rgba_size),
        "default original trace raster builds"
    );

    for (int control = 0; control < ORIGINAL_TRACE_CONTROL_COUNT; ++control) {
        for (int axis = 0; axis < 2; ++axis) {
            initialize_original_trace_controls(&controls);
            if (axis == 0) {
                controls.value[control].real = 0.50f;
            } else {
                controls.value[control].imaginary = 0.50f;
            }

            memset(moved, 0, rgba_size);
            CHECK(
                raster_for_controls(&controls, width, height, moved, rgba_size),
                "moved original trace raster builds"
            );
            const size_t changed =
                changed_pixels(baseline, moved, pixel_count);
            CHECK(
                changed > 100,
                "each original trace coordinate materially changes raster"
            );
        }
    }

    free(moved);
    free(baseline);
}

int main(void) {
    test_default_trace_mapping();
    test_touch_mapping();
    test_each_trace_axis_changes_original_raster();

    if (failures != 0) {
        fprintf(stderr, "%d/%d original trace-control assertions failed\n",
                failures, assertions);
        return 1;
    }

    printf("%d original trace-control assertions passed\n", assertions);
    return 0;
}
