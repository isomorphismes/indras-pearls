#include "parameter_controls.h"
#include "renderer_packet.h"

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

static int packet_differs(const float *left, const float *right, float tolerance) {
    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        if (!nearf(left[index], right[index], tolerance)) {
            return 1;
        }
    }
    return 0;
}

static int packet_circles_are_disjoint(const float *packet) {
    for (int i = 0; i < LIMIT_SET_REGION_COUNT; ++i) {
        const float xi =
            packet[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + i * 2];
        const float yi =
            packet[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + i * 2 + 1];
        const float ri = sqrtf(
            packet[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET + i]
        );
        if (!isfinite(xi) || !isfinite(yi) || !isfinite(ri) || ri <= 0.0f) {
            return 0;
        }

        for (int j = i + 1; j < LIMIT_SET_REGION_COUNT; ++j) {
            const float xj =
                packet[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + j * 2];
            const float yj =
                packet[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + j * 2 + 1];
            const float rj = sqrtf(
                packet[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET + j]
            );
            const float distance = hypotf(xi - xj, yi - yj);
            if (!(distance > ri + rj)) {
                return 0;
            }
        }
    }
    return 1;
}

static double reference_output_rmse(const float *a, const float *b);\n\nstatic int packet_is_finite(const float *packet) {
    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        if (!isfinite(packet[index])) {
            return 0;
        }
    }
    return 1;
}

static void flatten_and_build(
    const struct complex_parameter_controls *controls,
    float packet[RENDERER_PACKET_FLOAT_COUNT],
    const char *label
) {
    float values[SCHOTTKY_PARAMETER_FLOAT_COUNT];
    complex_parameter_controls_flatten(controls, values);
    CHECK(
        schottky_three_disk_renderer_packet(values, packet) ==
            RENDERER_PACKET_OK,
        label
    );
}

static void drag_one_axis(
    int control_index,
    int imaginary_axis,
    float target,
    float packet[RENDERER_PACKET_FLOAT_COUNT]
) {
    struct complex_parameter_controls controls;
    initialize_complex_parameter_controls(&controls);

    const int width = 600;
    const int height = 1200;
    const float radius = complex_parameter_control_radius(width, height);
    float center_x = 0.0f;
    float center_y = 0.0f;
    complex_parameter_control_center(
        control_index,
        width,
        height,
        &center_x,
        &center_y
    );

    const float input_center_y = (float)height - center_y;
    CHECK(
        begin_complex_parameter_drag(
            &controls,
            center_x,
            input_center_y,
            width,
            height
        ),
        "touch captures the selected complex disk"
    );

    const float target_x =
        center_x + (imaginary_axis ? 0.0f : target * radius);
    const float target_y =
        input_center_y - (imaginary_axis ? target * radius : 0.0f);

    CHECK(
        update_complex_parameter_drag(
            &controls,
            target_x,
            target_y,
            width,
            height
        ),
        "drag updates selected complex disk"
    );
    end_complex_parameter_drag(&controls);

    CHECK(
        nearf(
            imaginary_axis
                ? controls.value[control_index].imaginary
                : controls.value[control_index].real,
            target,
            2.0e-5f
        ),
        "touch coordinates reach requested complex parameter axis"
    );

    flatten_and_build(
        &controls,
        packet,
        "dragged control builds a validated Schottky packet"
    );
}

static void test_default_reproduces_bundled_group(void) {
    struct complex_parameter_controls controls;
    initialize_complex_parameter_controls(&controls);

    float live[RENDERER_PACKET_FLOAT_COUNT];
    float bundled[RENDERER_PACKET_FLOAT_COUNT];
    flatten_and_build(
        &controls,
        live,
        "zero controls build the live Schottky packet"
    );
    CHECK(
        symmetric_classical_renderer_packet(0.7f, bundled) ==
            RENDERER_PACKET_OK,
        "historical bundled packet still builds"
    );

    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        CHECK(
            nearf(live[index], bundled[index], 3.0e-5f),
            "zero controls reproduce historical r=0.7 packet"
        );
    }
    CHECK(
        packet_circles_are_disjoint(live),
        "default live packet has four disjoint circles"
    );
}

static void test_every_real_coordinate_is_live(void) {
    struct complex_parameter_controls controls;
    initialize_complex_parameter_controls(&controls);
    float baseline[RENDERER_PACKET_FLOAT_COUNT];
    flatten_and_build(
        &controls,
        baseline,
        "baseline packet builds before six-axis test"
    );

    for (int control = 0; control < COMPLEX_PARAMETER_COUNT; ++control) {
        for (int axis = 0; axis < 2; ++axis) {
            float moved[RENDERER_PACKET_FLOAT_COUNT];
            drag_one_axis(control, axis, 0.45f, moved);
            CHECK(
                packet_differs(baseline, moved, 1.0e-6f),
                "each of six real control coordinates changes renderer packet"
            );
            CHECK(
                packet_circles_are_disjoint(moved),
                "each one-axis drag preserves disjoint classical circles"
            );
            double output_rmse = reference_output_rmse(baseline, moved);
            CHECK(
                output_rmse > 0.001,
                "each real control direction changes reference output RMS"
            );
            printf(
                "control=%d axis=%d output_rmse=%.8f\n",
                control,
                axis,
                output_rmse
            );
        }
    }
}

static void test_control_domain_grid(void) {
    static const struct complex_value samples[] = {
        {0.0f, 0.0f},
        {0.90f, 0.0f},
        {-0.90f, 0.0f},
        {0.0f, 0.90f},
        {0.0f, -0.90f},
        {0.66f, 0.66f},
        {-0.66f, 0.66f},
    };

    struct complex_parameter_controls controls;
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        for (size_t j = 0; j < sizeof(samples) / sizeof(samples[0]); ++j) {
            for (size_t k = 0; k < sizeof(samples) / sizeof(samples[0]); ++k) {
                initialize_complex_parameter_controls(&controls);
                controls.value[0] = samples[i];
                controls.value[1] = samples[j];
                controls.value[2] = samples[k];

                float packet[RENDERER_PACKET_FLOAT_COUNT];
                float values[SCHOTTKY_PARAMETER_FLOAT_COUNT];
                complex_parameter_controls_flatten(&controls, values);
                CHECK(
                    schottky_three_disk_renderer_packet(values, packet) ==
                        RENDERER_PACKET_OK,
                    "representative three-disk grid stays in classical domain"
                );
                CHECK(
                    packet_is_finite(packet),
                    "representative three-disk grid stays finite"
                );
                CHECK(
                    packet_circles_are_disjoint(packet),
                    "representative three-disk grid keeps circles disjoint"
                );
            }
        }
    }
}

typedef struct { float x; float y; } rcpx;

static rcpx rcpx_mul(rcpx a, rcpx b) {
    return (rcpx){a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x};
}

static rcpx packet_value(const float *p, int offset, int region) {
    return (rcpx){p[offset + 2 * region], p[offset + 2 * region + 1]};
}

static rcpx packet_map(const float *p, int region, rcpx z) {
    rcpx a = packet_value(p, RENDERER_PACKET_A_OFFSET, region);
    rcpx b = packet_value(p, RENDERER_PACKET_B_OFFSET, region);
    rcpx c = packet_value(p, RENDERER_PACKET_C_OFFSET, region);
    rcpx d = packet_value(p, RENDERER_PACKET_D_OFFSET, region);
    rcpx az = rcpx_mul(a, z);
    rcpx cz = rcpx_mul(c, z);
    rcpx n = {az.x + b.x, az.y + b.y};
    rcpx q = {cz.x + d.x, cz.y + d.y};
    float q2 = q.x * q.x + q.y * q.y;
    if (q2 < 1.0e-12f) return (rcpx){1.0e12f, 1.0e12f};
    return (rcpx){
        (n.x * q.x + n.y * q.y) / q2,
        (n.y * q.x - n.x * q.y) / q2
    };
}

static int packet_region(const float *p, rcpx z) {
    for (int r = 0; r < LIMIT_SET_REGION_COUNT; ++r) {
        float dx = z.x - p[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + 2 * r];
        float dy = z.y - p[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + 2 * r + 1];
        if (dx * dx + dy * dy <
            p[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET + r]) return r;
    }
    return -1;
}

static float reference_value(const float *p, int x, int y) {
    const int width = 48;
    const int height = 96;
    rcpx z = {
        ((float)x + 0.5f - 0.5f * width) * (4.0f / height),
        ((float)y + 0.5f - 0.5f * height) * (4.0f / height)
    };
    int depth = 0;
    int last = 0;
    for (int step = 0; step < 24; ++step) {
        int r = packet_region(p, z);
        if (r < 0) break;
        last = r;
        z = packet_map(p, r, z);
        ++depth;
    }
    float b = powf(fminf(((float)depth / 24.0f) * 3.2f, 1.0f), 0.72f);
    if (depth == 24) b = 1.0f;
    return b * ((last < 2) ? 0.66f : 0.74f);
}

static double reference_output_rmse(const float *a, const float *b) {
    const int width = 48;
    const int height = 96;
    double sum = 0.0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double d = (double)reference_value(b, x, y) -
                       (double)reference_value(a, x, y);
            sum += d * d;
        }
    }
    return sqrt(sum / (double)(width * height));
}

static void test_invalid_input_fails_closed(void) {
    float values[SCHOTTKY_PARAMETER_FLOAT_COUNT] = {
        0.95f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
    };
    float packet[RENDERER_PACKET_FLOAT_COUNT];
    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        packet[index] = -123.0f;
    }

    CHECK(
        schottky_three_disk_renderer_packet(values, packet) ==
            RENDERER_PACKET_INVALID_PARAMETER,
        "control outside unit-disk contract is rejected"
    );

    int unchanged = 1;
    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        if (packet[index] != -123.0f) {
            unchanged = 0;
            break;
        }
    }
    CHECK(
        unchanged,
        "rejected live parameter state preserves previous renderer packet"
    );
}

int main(void) {
    test_default_reproduces_bundled_group();
    test_every_real_coordinate_is_live();
    test_control_domain_grid();
    test_invalid_input_fails_closed();

    if (failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    printf("PASS: %d three-disk integration assertions\n", assertions);
    return EXIT_SUCCESS;
}
