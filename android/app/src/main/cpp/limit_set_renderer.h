#ifndef INDRAS_PEARLS_LIMIT_SET_RENDERER_H
#define INDRAS_PEARLS_LIMIT_SET_RENDERER_H

#include <GLES3/gl3.h>
#include <stdbool.h>

#include "group_state.h"
#include "parameter_controls.h"

struct limit_set_renderer {
    GLuint program;
    GLuint vertex_array;
    GLint center_location;
    GLint scale_location;
    GLint resolution_location;
    GLint circle_center_location;
    GLint circle_radius_squared_location;
    GLint a_location;
    GLint b_location;
    GLint c_location;
    GLint d_location;
    GLint parameter_center_location;
    GLint parameter_value_location;
    GLint parameter_radius_location;
    GLint active_parameter_location;
};

bool initialize_limit_set_renderer(struct limit_set_renderer *renderer);
void terminate_limit_set_renderer(struct limit_set_renderer *renderer);
void draw_limit_set(
    const struct limit_set_renderer *renderer,
    const struct limit_set_group *group,
    const struct complex_parameter_controls *controls,
    float center_x,
    float center_y,
    float scale,
    int width,
    int height
);

#endif
