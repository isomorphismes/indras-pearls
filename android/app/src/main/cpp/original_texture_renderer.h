#ifndef INDRAS_PEARLS_ORIGINAL_TEXTURE_RENDERER_H
#define INDRAS_PEARLS_ORIGINAL_TEXTURE_RENDERER_H

#include <stdbool.h>
#include <stdint.h>
#include <GLES3/gl3.h>

#include "original_trace_controls.h"

struct original_texture_renderer {
    GLuint program;
    GLuint vertex_array;
    GLuint texture;
    GLint texture_location;
    GLint control_center_location;
    GLint control_value_location;
    GLint control_radius_location;
    GLint active_control_location;
    int width;
    int height;
};

bool initialize_original_texture_renderer(struct original_texture_renderer *renderer);
void terminate_original_texture_renderer(struct original_texture_renderer *renderer);

bool upload_original_texture(
    struct original_texture_renderer *renderer,
    int width,
    int height,
    const uint8_t *rgba
);

void draw_original_texture(
    const struct original_texture_renderer *renderer,
    const struct original_trace_controls *controls,
    int width,
    int height
);

#endif
