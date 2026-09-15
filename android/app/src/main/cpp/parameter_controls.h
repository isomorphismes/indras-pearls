#ifndef INDRAS_PEARLS_PARAMETER_CONTROLS_H
#define INDRAS_PEARLS_PARAMETER_CONTROLS_H

#include <stdbool.h>

#include "group_state.h"

#define COMPLEX_PARAMETER_COUNT 3

struct complex_parameter_controls {
    struct complex_value value[COMPLEX_PARAMETER_COUNT];
    int active_index;
};

void initialize_complex_parameter_controls(struct complex_parameter_controls *controls);
float complex_parameter_control_radius(int width, int height);
void complex_parameter_control_center(
    int index,
    int width,
    int height,
    float *x,
    float *y
);
bool begin_complex_parameter_drag(
    struct complex_parameter_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
);
bool update_complex_parameter_drag(
    struct complex_parameter_controls *controls,
    float input_x,
    float input_y,
    int width,
    int height
);
void end_complex_parameter_drag(struct complex_parameter_controls *controls);

#endif
