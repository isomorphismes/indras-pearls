#include "parameter_controls.h"
#include "renderer_packet.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;
static int assertions = 0;

#define CHECK(condition, message) do { \
    ++assertions; \
    if (!(condition)) { \
        ++failures; \
        fprintf(stderr, "FAIL: %s\n", message); \
    } \
} while (0)

static int nearf(float left, float right, float tolerance) {
    return fabsf(left - right) <= tolerance;
}

int main(void) {
    struct complex_parameter_controls controls;
    initialize_complex_parameter_controls(&controls);

    CHECK(nearf(schottky_radius_from_controls(&controls), 0.7f, 1.0e-6f),
          "default live control reproduces bundled r=0.7");

    const int width = 600;
    const int height = 1200;
    const float control_radius = complex_parameter_control_radius(width, height);
    float center_x = 0.0f;
    float center_y = 0.0f;
    complex_parameter_control_center(
        0, width, height, &center_x, &center_y
    );
    const float input_y = (float)height - center_y;
    const float initial_handle_x = center_x + 0.94f * control_radius;

    CHECK(begin_complex_parameter_drag(
              &controls,
              initial_handle_x,
              input_y,
              width,
              height),
          "touching the live r handle captures the gesture");
    CHECK(update_complex_parameter_drag(
              &controls,
              center_x,
              input_y,
              width,
              height),
          "dragging the live r handle updates control state");
    CHECK(nearf(controls.value[0].imaginary, 0.0f, 1.0e-7f),
          "live r control stays on its horizontal diameter");
    CHECK(nearf(schottky_radius_from_controls(&controls), 0.4f, 1.0e-6f),
          "centered live handle maps to r=0.4");
    end_complex_parameter_drag(&controls);

    float bundled[RENDERER_PACKET_FLOAT_COUNT];
    float moved[RENDERER_PACKET_FLOAT_COUNT];

    struct complex_parameter_controls bundled_controls;
    initialize_complex_parameter_controls(&bundled_controls);
    CHECK(symmetric_classical_renderer_packet(
              schottky_radius_from_controls(&bundled_controls),
              bundled) == RENDERER_PACKET_OK,
          "default control builds the bundled renderer packet");
    CHECK(symmetric_classical_renderer_packet(
              schottky_radius_from_controls(&controls),
              moved) == RENDERER_PACKET_OK,
          "moved control builds a second validated renderer packet");

    CHECK(nearf(
              bundled[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET],
              0.49f,
              2.0e-6f),
          "bundled packet keeps r^2=0.49");
    CHECK(nearf(
              moved[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET],
              0.16f,
              2.0e-6f),
          "moved control changes renderer circle radius to r^2=0.16");

    int packet_changed = 0;
    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        if (!nearf(bundled[index], moved[index], 1.0e-6f)) {
            packet_changed = 1;
            break;
        }
    }
    CHECK(packet_changed,
          "moving the live control changes the validated renderer packet");

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    printf("PASS: %d live-control assertions\n", assertions);
    return EXIT_SUCCESS;
}
