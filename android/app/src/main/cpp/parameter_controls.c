#include "parameter_controls.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static float smaller(float left, float right) {
    return left < right ? left : right;
}

float complex_parameter_control_radius(int width, int height) {
    if (width <= 0 || height <= 0) {
        return 0.0f;
    }

    return smaller((float)width / 8.0f, (float)height / 12.0f);
}

void complex_parameter_control_center(
    int index,
    int width,
    int height,
    float *x,
    float *y
) {
    float slot = ((float)index + 0.5f) / (float)COMPLEX_PARAMETER_COUNT;
    float radius = complex_parameter_control_radius(width, height);

    if (x != NULL) {
        *x = slot * (float)width;
    }
    if (y != NULL) {
        *y = (float)height - 1.45f * radius;
    }
}

void initialize_complex_parameter_controls(struct complex_parameter_controls *controls) {
    memset(controls, 0, sizeof(*controls));
    controls->active_index = -1;
}

static void set_control_from_input(
    struct complex_parameter_controls *controls,
    int index,
    float input_x,
    float input_y,
    int width,
    int height
) {
    float center_x = 0.0f;
    float center_y = 0.0f;
    float radius = complex_parameter_control_radius(width, height);
    if (radius <= 0.0f) {
        return;
    }

    complex_parameter_control_center(index, width, height, &center_x, &center_y);

    float framebuffer_y = (float)height - input_y;
    float real = (input_x - center_x) / radius;
    float imaginary = (framebuffer_y - center_y) / radius;
    float magnitude_squared = real * real + imaginary * imaginary;
    const float maximum_radius = 0.94f;

    if (magnitude_squared > maximum_radius * maximum_radius) {
        float magnitude = sqrtf(magnitude_squared);
        real *= maximum_radius / magnitude;
        imaginary *= maximum_radius / magnitude;
    }

    controls->value[index].real = real;
    controls->value[index].imaginary = imaginary;
}

bool begin_complex_parameter_drag(
    struct complex_parameter_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
) {
    float radius = complex_parameter_control_radius(width, height);
    if (radius <= 0.0f) {
        return false;
    }

    float framebuffer_y = (float)height - input_y;
    float best_distance_squared = radius * radius * 1.10f * 1.10f;
    int best_index = -1;

    for (int index = 0; index < COMPLEX_PARAMETER_COUNT; ++index) {
        float center_x = 0.0f;
        float center_y = 0.0f;
        complex_parameter_control_center(index, width, height, &center_x, &center_y);

        float dx = input_x - center_x;
        float dy = framebuffer_y - center_y;
        float distance_squared = dx * dx + dy * dy;
        if (distance_squared <= best_distance_squared) {
            best_distance_squared = distance_squared;
            best_index = index;
        }
    }

    if (best_index < 0) {
        return false;
    }

    controls->active_index = best_index;
    set_control_from_input(controls, best_index, input_x, input_y, width, height);
    return true;
}

bool update_complex_parameter_drag(
    struct complex_parameter_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
) {
    if (controls->active_index < 0 || controls->active_index >= COMPLEX_PARAMETER_COUNT) {
        return false;
    }

    set_control_from_input(
        controls,
        controls->active_index,
        input_x,
        input_y,
        width,
        height
    );
    return true;
}

void end_complex_parameter_drag(struct complex_parameter_controls *controls) {
    controls->active_index = -1;
}
