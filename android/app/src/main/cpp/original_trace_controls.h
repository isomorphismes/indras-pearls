#ifndef INDRAS_PEARLS_ORIGINAL_TRACE_CONTROLS_H
#define INDRAS_PEARLS_ORIGINAL_TRACE_CONTROLS_H

#include <stdbool.h>

#include "original_kleinian.h"

#define ORIGINAL_TRACE_CONTROL_COUNT 2
#define ORIGINAL_TRACE_CONTROL_LIMIT 0.90f
#define ORIGINAL_TRACE_CONTROL_SPAN 0.40

struct original_trace_control_value {
    float real;
    float imaginary;
};

struct original_trace_controls {
    struct original_trace_control_value value[ORIGINAL_TRACE_CONTROL_COUNT];
    int active_index;
};

void initialize_original_trace_controls(struct original_trace_controls *controls);

float original_trace_control_radius(int width, int height);

void original_trace_control_center(
    int index,
    int width,
    int height,
    float *x,
    float *y
);

bool begin_original_trace_drag(
    struct original_trace_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
);

bool update_original_trace_drag(
    struct original_trace_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
);

void end_original_trace_drag(struct original_trace_controls *controls);

void original_trace_controls_to_traces(
    const struct original_trace_controls *controls,
    struct original_kleinian_complex *ta,
    struct original_kleinian_complex *tb
);

#endif
