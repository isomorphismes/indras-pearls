#include "limit_set_renderer.h"

#include <android/log.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define LOG_TAG "IndrasPearls"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char *vertex_shader_source =
    "#version 300 es\n"
    "const vec2 positions[3] = vec2[3](\n"
    "    vec2(-1.0, -1.0),\n"
    "    vec2( 3.0, -1.0),\n"
    "    vec2(-1.0,  3.0)\n"
    ");\n"
    "void main() {\n"
    "    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);\n"
    "}\n";

static const char *fragment_shader_source =
    "#version 300 es\n"
    "precision highp float;\n"
    "\n"
    "const int REGION_COUNT = 4;\n"
    "const int PARAMETER_COUNT = 3;\n"
    "const int MAX_STEPS = 24;\n"
    "\n"
    "uniform vec2 u_center;\n"
    "uniform float u_scale;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec4 u_circle_center_x;\n"
    "uniform vec4 u_circle_center_y;\n"
    "uniform vec4 u_circle_radius_squared;\n"
    "uniform vec2 u_a[REGION_COUNT];\n"
    "uniform vec2 u_b[REGION_COUNT];\n"
    "uniform vec2 u_c[REGION_COUNT];\n"
    "uniform vec2 u_d[REGION_COUNT];\n"
    "uniform vec2 u_parameter_center[PARAMETER_COUNT];\n"
    "uniform vec2 u_parameter_value[PARAMETER_COUNT];\n"
    "uniform float u_parameter_radius;\n"
    "uniform int u_active_parameter;\n"
    "uniform int u_initial_region;\n"
    "\n"
    "out vec4 fragment_color;\n"
    "\n"
    "vec2 complex_multiply(vec2 left, vec2 right) {\n"
    "    return vec2(\n"
    "        left.x * right.x - left.y * right.y,\n"
    "        left.x * right.y + left.y * right.x\n"
    "    );\n"
    "}\n"
    "\n"
    "vec2 complex_divide(vec2 numerator, vec2 denominator) {\n"
    "    float denominator_squared = dot(denominator, denominator);\n"
    "    if (denominator_squared < 1.0e-12) {\n"
    "        return vec2(1.0e12);\n"
    "    }\n"
    "    return vec2(\n"
    "        numerator.x * denominator.x + numerator.y * denominator.y,\n"
    "        numerator.y * denominator.x - numerator.x * denominator.y\n"
    "    ) / denominator_squared;\n"
    "}\n"
    "\n"
    "vec2 apply_mobius(int region, vec2 z) {\n"
    "    vec2 numerator = complex_multiply(u_a[region], z) + u_b[region];\n"
    "    vec2 denominator = complex_multiply(u_c[region], z) + u_d[region];\n"
    "    return complex_divide(numerator, denominator);\n"
    "}\n"
    "\n"
    "int containing_next_region(int previous_region, vec2 z) {\n"
    "    int opposite_pair = 2 - 2 * (previous_region / 2);\n"
    "    ivec3 candidate = ivec3(previous_region, opposite_pair, opposite_pair + 1);\n"
    "    vec3 center_x = vec3(\n"
    "        u_circle_center_x[candidate.x],\n"
    "        u_circle_center_x[candidate.y],\n"
    "        u_circle_center_x[candidate.z]\n"
    "    );\n"
    "    vec3 center_y = vec3(\n"
    "        u_circle_center_y[candidate.x],\n"
    "        u_circle_center_y[candidate.y],\n"
    "        u_circle_center_y[candidate.z]\n"
    "    );\n"
    "    vec3 radius_squared = vec3(\n"
    "        u_circle_radius_squared[candidate.x],\n"
    "        u_circle_radius_squared[candidate.y],\n"
    "        u_circle_radius_squared[candidate.z]\n"
    "    );\n"
    "    vec3 dx = vec3(z.x) - center_x;\n"
    "    vec3 dy = vec3(z.y) - center_y;\n"
    "    vec3 distance_squared = dx * dx + dy * dy;\n"
    "    vec3 inside = vec3(1.0) - step(radius_squared, distance_squared);\n"
    "    float any_region = dot(inside, vec3(1.0));\n"
    "    float selected_region = dot(inside, vec3(candidate));\n"
    "    return int(selected_region + any_region) - 1;\n"
    "}\n"
    "\n"
    "vec3 parameter_color(int index) {\n"
    "    if (index == 0) {\n"
    "        return vec3(0.34, 0.72, 1.00);\n"
    "    }\n"
    "    if (index == 1) {\n"
    "        return vec3(1.00, 0.72, 0.34);\n"
    "    }\n"
    "    return vec3(0.62, 0.96, 0.64);\n"
    "}\n"
    "\n"
    "vec3 draw_parameter_controls(vec3 base_color) {\n"
    "    vec3 color = base_color;\n"
    "    float control_extent = u_parameter_radius * 1.08;\n"
    "    for (int index = 0; index < PARAMETER_COUNT; ++index) {\n"
    "        vec2 delta = gl_FragCoord.xy - u_parameter_center[index];\n"
    "        if (abs(delta.x) > control_extent || abs(delta.y) > control_extent) {\n"
    "            continue;\n"
    "        }\n"
    "        vec2 local = delta / u_parameter_radius;\n"
    "        float distance_from_center = length(local);\n"
    "        if (distance_from_center > 1.08) {\n"
    "            continue;\n"
    "        }\n"
    "\n"
    "        vec3 accent = parameter_color(index);\n"
    "        float inside = 1.0 - smoothstep(0.96, 1.0, distance_from_center);\n"
    "        color = mix(color, vec3(0.025, 0.030, 0.042), inside * 0.72);\n"
    "\n"
    "        float horizontal_axis = 1.0 - smoothstep(0.012, 0.030, abs(local.y));\n"
    "        float vertical_axis = 1.0 - smoothstep(0.012, 0.030, abs(local.x));\n"
    "        float axes = max(horizontal_axis, vertical_axis) * inside;\n"
    "        color = mix(color, vec3(0.56, 0.59, 0.66), axes * 0.30);\n"
    "\n"
    "        float rim = 1.0 - smoothstep(0.018, 0.045, abs(distance_from_center - 1.0));\n"
    "        color = mix(color, accent, rim * 0.88);\n"
    "\n"
    "        float handle_distance = length(local - u_parameter_value[index]);\n"
    "        float handle = 1.0 - smoothstep(0.065, 0.105, handle_distance);\n"
    "        color = mix(color, accent, handle);\n"
    "\n"
    "        if (index == u_active_parameter) {\n"
    "            float active_ring = 1.0 - smoothstep(0.018, 0.050, abs(distance_from_center - 0.90));\n"
    "            color = mix(color, accent, active_ring * 0.48);\n"
    "        }\n"
    "    }\n"
    "    return color;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec3 background = vec3(0.012, 0.014, 0.020);\n"
    "    if (u_initial_region < 0) {\n"
    "        fragment_color = vec4(draw_parameter_controls(background), 1.0);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    vec2 pixel_from_center = gl_FragCoord.xy - 0.5 * u_resolution;\n"
    "    vec2 z = u_center + pixel_from_center * (u_scale / u_resolution.y);\n"
    "    int first_region = u_initial_region;\n"
    "    vec2 first_center = vec2(u_circle_center_x[first_region], u_circle_center_y[first_region]);\n"
    "    vec2 first_offset = z - first_center;\n"
    "    if (dot(first_offset, first_offset) >= u_circle_radius_squared[first_region]) {\n"
    "        discard;\n"
    "    }\n"
    "\n"
    "    int depth = 1;\n"
    "    int last_region = first_region;\n"
    "    z = apply_mobius(first_region, z);\n"
    "    for (int step = 1; step < MAX_STEPS; ++step) {\n"
    "        int region = containing_next_region(last_region, z);\n"
    "        if (region < 0) {\n"
    "            break;\n"
    "        }\n"
    "        last_region = region;\n"
    "        z = apply_mobius(region, z);\n"
    "        depth += 1;\n"
    "    }\n"
    "\n"
    "    float depth_value = float(depth) / float(MAX_STEPS);\n"
    "    float brightness = pow(clamp(depth_value * 3.2, 0.0, 1.0), 0.72);\n"
    "    if (depth == MAX_STEPS) {\n"
    "        brightness = 1.0;\n"
    "    }\n"
    "\n"
    "    vec3 cool = vec3(0.36, 0.66, 0.92);\n"
    "    vec3 warm = vec3(0.92, 0.74, 0.44);\n"
    "    vec3 pearl = (last_region == 0 || last_region == 1) ? cool : warm;\n"
    "    vec3 color = mix(background, pearl, brightness);\n"
    "    fragment_color = vec4(draw_parameter_controls(color), 1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum shader_type, const char *source) {
    GLuint shader = glCreateShader(shader_type);
    if (shader == 0) {
        LOGE("glCreateShader failed: 0x%x", glGetError());
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    char log[2048];
    GLsizei log_length = 0;
    glGetShaderInfoLog(shader, sizeof(log), &log_length, log);
    LOGE("shader compile failed: %.*s", (int)log_length, log);
    glDeleteShader(shader);
    return 0;
}

static bool find_uniforms(struct limit_set_renderer *renderer) {
    renderer->center_location = glGetUniformLocation(renderer->program, "u_center");
    renderer->scale_location = glGetUniformLocation(renderer->program, "u_scale");
    renderer->resolution_location = glGetUniformLocation(renderer->program, "u_resolution");
    renderer->circle_center_x_location = glGetUniformLocation(renderer->program, "u_circle_center_x");
    renderer->circle_center_y_location = glGetUniformLocation(renderer->program, "u_circle_center_y");
    renderer->circle_radius_squared_location = glGetUniformLocation(renderer->program, "u_circle_radius_squared");
    renderer->a_location = glGetUniformLocation(renderer->program, "u_a[0]");
    renderer->b_location = glGetUniformLocation(renderer->program, "u_b[0]");
    renderer->c_location = glGetUniformLocation(renderer->program, "u_c[0]");
    renderer->d_location = glGetUniformLocation(renderer->program, "u_d[0]");
    renderer->parameter_center_location = glGetUniformLocation(renderer->program, "u_parameter_center[0]");
    renderer->parameter_value_location = glGetUniformLocation(renderer->program, "u_parameter_value[0]");
    renderer->parameter_radius_location = glGetUniformLocation(renderer->program, "u_parameter_radius");
    renderer->active_parameter_location = glGetUniformLocation(renderer->program, "u_active_parameter");
    renderer->initial_region_location = glGetUniformLocation(renderer->program, "u_initial_region");

    return renderer->center_location >= 0 &&
        renderer->scale_location >= 0 &&
        renderer->resolution_location >= 0 &&
        renderer->circle_center_x_location >= 0 &&
        renderer->circle_center_y_location >= 0 &&
        renderer->circle_radius_squared_location >= 0 &&
        renderer->a_location >= 0 &&
        renderer->b_location >= 0 &&
        renderer->c_location >= 0 &&
        renderer->d_location >= 0 &&
        renderer->parameter_center_location >= 0 &&
        renderer->parameter_value_location >= 0 &&
        renderer->parameter_radius_location >= 0 &&
        renderer->active_parameter_location >= 0 &&
        renderer->initial_region_location >= 0;
}

bool initialize_limit_set_renderer(struct limit_set_renderer *renderer) {
    memset(renderer, 0, sizeof(*renderer));

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    if (vertex_shader == 0) {
        return false;
    }

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return false;
    }

    GLuint program = glCreateProgram();
    if (program == 0) {
        LOGE("glCreateProgram failed: 0x%x", glGetError());
        glDeleteShader(fragment_shader);
        glDeleteShader(vertex_shader);
        return false;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[2048];
        GLsizei log_length = 0;
        glGetProgramInfoLog(program, sizeof(log), &log_length, log);
        LOGE("shader link failed: %.*s", (int)log_length, log);
        glDeleteProgram(program);
        return false;
    }

    renderer->program = program;
    if (!find_uniforms(renderer)) {
        LOGE("required limit-set shader uniform was optimized out or not found");
        terminate_limit_set_renderer(renderer);
        return false;
    }

    glGenVertexArrays(1, &renderer->vertex_array);
    if (renderer->vertex_array == 0) {
        LOGE("glGenVertexArrays failed: 0x%x", glGetError());
        terminate_limit_set_renderer(renderer);
        return false;
    }

    return true;
}

void terminate_limit_set_renderer(struct limit_set_renderer *renderer) {
    if (renderer->vertex_array != 0) {
        glDeleteVertexArrays(1, &renderer->vertex_array);
    }
    if (renderer->program != 0) {
        glDeleteProgram(renderer->program);
    }
    memset(renderer, 0, sizeof(*renderer));
}

static void copy_parameter_controls(
    float centers[COMPLEX_PARAMETER_COUNT * 2],
    float values[COMPLEX_PARAMETER_COUNT * 2],
    const struct complex_parameter_controls *controls,
    int width,
    int height
) {
    for (int index = 0; index < COMPLEX_PARAMETER_COUNT; ++index) {
        complex_parameter_control_center(
            index,
            width,
            height,
            &centers[index * 2],
            &centers[index * 2 + 1]
        );
        values[index * 2] = controls->value[index].real;
        values[index * 2 + 1] = controls->value[index].imaginary;
    }
}

static bool set_region_scissor(
    const float renderer_packet[static RENDERER_PACKET_FLOAT_COUNT],
    int region,
    float center_x,
    float center_y,
    float scale,
    int width,
    int height
) {
    if (width <= 0 || height <= 0 || !isfinite(scale) || scale == 0.0f) {
        return false;
    }

    const float pixels_per_world = (float)height / scale;
    if (!isfinite(pixels_per_world)) {
        return false;
    }

    const int complex_offset = region * RENDERER_COMPLEX_COMPONENT_COUNT;
    const float circle_x =
        renderer_packet[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + complex_offset];
    const float circle_y =
        renderer_packet[RENDERER_PACKET_CIRCLE_CENTER_OFFSET + complex_offset + 1];
    const float radius_squared =
        renderer_packet[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET + region];
    const float screen_x =
        (circle_x - center_x) * pixels_per_world + 0.5f * (float)width;
    const float screen_y =
        (circle_y - center_y) * pixels_per_world + 0.5f * (float)height;
    const float radius_pixels = sqrtf(radius_squared) * fabsf(pixels_per_world);

    const float left_f = fmaxf(0.0f, floorf(screen_x - radius_pixels));
    const float bottom_f = fmaxf(0.0f, floorf(screen_y - radius_pixels));
    const float right_f = fminf((float)width, ceilf(screen_x + radius_pixels));
    const float top_f = fminf((float)height, ceilf(screen_y + radius_pixels));

    if (!(left_f < right_f && bottom_f < top_f)) {
        return false;
    }

    glScissor(
        (GLint)left_f,
        (GLint)bottom_f,
        (GLsizei)(right_f - left_f),
        (GLsizei)(top_f - bottom_f)
    );
    return true;
}


void draw_limit_set(
    const struct limit_set_renderer *renderer,
    const float renderer_packet[static RENDERER_PACKET_FLOAT_COUNT],
    const struct complex_parameter_controls *controls,
    float center_x,
    float center_y,
    float scale,
    int width,
    int height
) {
    float parameter_centers[COMPLEX_PARAMETER_COUNT * 2];
    float parameter_values[COMPLEX_PARAMETER_COUNT * 2];

    copy_parameter_controls(parameter_centers, parameter_values, controls, width, height);

    glViewport(0, 0, width, height);
    glUseProgram(renderer->program);
    glBindVertexArray(renderer->vertex_array);

    glUniform2f(renderer->center_location, center_x, center_y);
    glUniform1f(renderer->scale_location, scale);
    glUniform2f(renderer->resolution_location, (float)width, (float)height);
    const float *circle_centers =
        renderer_packet + RENDERER_PACKET_CIRCLE_CENTER_OFFSET;
    glUniform4f(
        renderer->circle_center_x_location,
        circle_centers[0],
        circle_centers[2],
        circle_centers[4],
        circle_centers[6]
    );
    glUniform4f(
        renderer->circle_center_y_location,
        circle_centers[1],
        circle_centers[3],
        circle_centers[5],
        circle_centers[7]
    );
    glUniform4fv(
        renderer->circle_radius_squared_location,
        1,
        renderer_packet + RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET
    );
    glUniform2fv(
        renderer->a_location,
        LIMIT_SET_REGION_COUNT,
        renderer_packet + RENDERER_PACKET_A_OFFSET
    );
    glUniform2fv(
        renderer->b_location,
        LIMIT_SET_REGION_COUNT,
        renderer_packet + RENDERER_PACKET_B_OFFSET
    );
    glUniform2fv(
        renderer->c_location,
        LIMIT_SET_REGION_COUNT,
        renderer_packet + RENDERER_PACKET_C_OFFSET
    );
    glUniform2fv(
        renderer->d_location,
        LIMIT_SET_REGION_COUNT,
        renderer_packet + RENDERER_PACKET_D_OFFSET
    );
    glUniform2fv(
        renderer->parameter_center_location,
        COMPLEX_PARAMETER_COUNT,
        parameter_centers
    );
    glUniform2fv(
        renderer->parameter_value_location,
        COMPLEX_PARAMETER_COUNT,
        parameter_values
    );
    glUniform1f(
        renderer->parameter_radius_location,
        complex_parameter_control_radius(width, height)
    );
    glUniform1i(renderer->active_parameter_location, controls->active_index);

    glDisable(GL_SCISSOR_TEST);
    glUniform1i(renderer->initial_region_location, -1);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_SCISSOR_TEST);
    for (int region = 0; region < LIMIT_SET_REGION_COUNT; ++region) {
        if (!set_region_scissor(
                renderer_packet,
                region,
                center_x,
                center_y,
                scale,
                width,
                height)) {
            continue;
        }
        glUniform1i(renderer->initial_region_location, region);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
}
