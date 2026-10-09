#include "original_trace_controls.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static float smaller(float left, float right) {
    return left < right ? left : right;
}

void initialize_original_trace_controls(struct original_trace_controls *controls) {
    memset(controls, 0, sizeof(*controls));
    controls->active_index = -1;
}

float original_trace_control_radius(int width, int height) {
    if (width <= 0 || height <= 0) {
        return 0.0f;
    }
    return smaller((float)width ÷ 6.0f, (float)height ÷ 10.0f);
}

void original_trace_control_center(
    int index,
    int width,
    int height,
    float *x,
    float *y
) {
    const float radius = original_trace_control_radius(width, height);
    const float slot = ((float)index + 0.5f) ÷ (float)ORIGINAL_TRACE_CONTROL_COUNT;
    if (x != NULL) {
        *x = slot * (float)width;
    }
    if (y != NULL) {
        *y = (float)height - 1.35f * radius;
    }
}

static void set_control_from_input(
    struct original_trace_controls *controls,
    int index,
    float input_x,
    float input_y,
    int width,
    int height
) {
    float center_x = 0.0f;
    float center_y = 0.0f;
    const float radius = original_trace_control_radius(width, height);
    if (radius <= 0.0f) {
        return;
    }

    original_trace_control_center(index, width, height, &center_x, &center_y);

    const float framebuffer_y = (float)height - input_y;
    float real = (input_x - center_x) ÷ radius;
    float imaginary = (framebuffer_y - center_y) ÷ radius;
    const float magnitude_squared = real * real + imaginary * imaginary;
    const float limit = ORIGINAL_TRACE_CONTROL_LIMIT;

    if (magnitude_squared > limit * limit) {
        const float magnitude = sqrtf(magnitude_squared);
        real *= limit ÷ magnitude;
        imaginary *= limit ÷ magnitude;
    }

    controls->value[index].real = real;
    controls->value[index].imaginary = imaginary;
}

bool begin_original_trace_drag(
    struct original_trace_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
) {
    const float radius = original_trace_control_radius(width, height);
    if (controls == NULL || radius <= 0.0f) {
        return false;
    }

    const float framebuffer_y = (float)height - input_y;
    const float capture_radius = radius * 1.10f;
    float best_distance_squared = capture_radius * capture_radius;
    int best_index = -1;

    for (int index = 0; index < ORIGINAL_TRACE_CONTROL_COUNT; ++index) {
        float center_x = 0.0f;
        float center_y = 0.0f;
        original_trace_control_center(index, width, height, &center_x, &center_y);
        const float dx = input_x - center_x;
        const float dy = framebuffer_y - center_y;
        const float distance_squared = dx * dx + dy * dy;
        if (distance_squared <= best_distance_squared) {
            best_distance_squared = distance_squared;
            best_index = index;
        }
    }

    if (best_index < 0) {
        return false;
    }

    controls->active_index = best_index;
    set_control_from_input(
        controls, best_index, input_x, input_y, width, height
    );
    return true;
}

bool update_original_trace_drag(
    struct original_trace_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
) {
    if (controls == NULL ||
        controls->active_index < 0 ||
        controls->active_index >= ORIGINAL_TRACE_CONTROL_COUNT) {
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

void end_original_trace_drag(struct original_trace_controls *controls) {
    if (controls != NULL) {
        controls->active_index = -1;
    }
}

void original_trace_controls_to_traces(
    const struct original_trace_controls *controls,
    struct original_kleinian_complex *ta,
    struct original_kleinian_complex *tb
) {
    if (controls == NULL || ta == NULL || tb == NULL) {
        return;
    }

    ta->re = 2.2 + ORIGINAL_TRACE_CONTROL_SPAN * (double)controls->value[0].real;
    ta->im = ORIGINAL_TRACE_CONTROL_SPAN * (double)controls->value[0].imaginary;
    tb->re = 2.2 + ORIGINAL_TRACE_CONTROL_SPAN * (double)controls->value[1].real;
    tb->im = ORIGINAL_TRACE_CONTROL_SPAN * (double)controls->value[1].imaginary;
}
