#include "limit_set_renderer.h"

#include <android/log.h>
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
    "const int MAX_STEPS = 24;\n"
    "\n"
    "uniform vec2 u_center;\n"
    "uniform float u_scale;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec2 u_circle_center[REGION_COUNT];\n"
    "uniform float u_circle_radius_squared;\n"
    "uniform vec2 u_a[REGION_COUNT];\n"
    "uniform vec2 u_b[REGION_COUNT];\n"
    "uniform vec2 u_c[REGION_COUNT];\n"
    "uniform vec2 u_d[REGION_COUNT];\n"
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
    "int containing_region(vec2 z) {\n"
    "    for (int region = 0; region < REGION_COUNT; ++region) {\n"
    "        vec2 offset = z - u_circle_center[region];\n"
    "        if (dot(offset, offset) < u_circle_radius_squared) {\n"
    "            return region;\n"
    "        }\n"
    "    }\n"
    "    return -1;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 pixel_from_center = gl_FragCoord.xy - 0.5 * u_resolution;\n"
    "    vec2 z = u_center + pixel_from_center * (u_scale / u_resolution.y);\n"
    "\n"
    "    int depth = 0;\n"
    "    int last_region = 0;\n"
    "    for (int step = 0; step < MAX_STEPS; ++step) {\n"
    "        int region = containing_region(z);\n"
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
    "    vec3 background = vec3(0.012, 0.014, 0.020);\n"
    "    vec3 cool = vec3(0.36, 0.66, 0.92);\n"
    "    vec3 warm = vec3(0.92, 0.74, 0.44);\n"
    "    vec3 pearl = (last_region == 0 || last_region == 1) ? cool : warm;\n"
    "    fragment_color = vec4(mix(background, pearl, brightness), 1.0);\n"
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
    renderer->circle_center_location = glGetUniformLocation(renderer->program, "u_circle_center[0]");
    renderer->circle_radius_squared_location = glGetUniformLocation(renderer->program, "u_circle_radius_squared");
    renderer->a_location = glGetUniformLocation(renderer->program, "u_a[0]");
    renderer->b_location = glGetUniformLocation(renderer->program, "u_b[0]");
    renderer->c_location = glGetUniformLocation(renderer->program, "u_c[0]");
    renderer->d_location = glGetUniformLocation(renderer->program, "u_d[0]");

    return renderer->center_location >= 0 &&
        renderer->scale_location >= 0 &&
        renderer->resolution_location >= 0 &&
        renderer->circle_center_location >= 0 &&
        renderer->circle_radius_squared_location >= 0 &&
        renderer->a_location >= 0 &&
        renderer->b_location >= 0 &&
        renderer->c_location >= 0 &&
        renderer->d_location >= 0;
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

static void copy_complex_values(
    float output[LIMIT_SET_REGION_COUNT * 2],
    const struct complex_value input[LIMIT_SET_REGION_COUNT]
) {
    for (int i = 0; i < LIMIT_SET_REGION_COUNT; ++i) {
        output[i * 2] = input[i].real;
        output[i * 2 + 1] = input[i].imaginary;
    }
}

static void copy_mobius_coefficients(
    float a[LIMIT_SET_REGION_COUNT * 2],
    float b[LIMIT_SET_REGION_COUNT * 2],
    float c[LIMIT_SET_REGION_COUNT * 2],
    float d[LIMIT_SET_REGION_COUNT * 2],
    const struct mobius_map maps[LIMIT_SET_REGION_COUNT]
) {
    for (int i = 0; i < LIMIT_SET_REGION_COUNT; ++i) {
        a[i * 2] = maps[i].a.real;
        a[i * 2 + 1] = maps[i].a.imaginary;
        b[i * 2] = maps[i].b.real;
        b[i * 2 + 1] = maps[i].b.imaginary;
        c[i * 2] = maps[i].c.real;
        c[i * 2 + 1] = maps[i].c.imaginary;
        d[i * 2] = maps[i].d.real;
        d[i * 2 + 1] = maps[i].d.imaginary;
    }
}

void draw_limit_set(
    const struct limit_set_renderer *renderer,
    const struct limit_set_group *group,
    float center_x,
    float center_y,
    float scale,
    int width,
    int height
) {
    float circle_centers[LIMIT_SET_REGION_COUNT * 2];
    float a[LIMIT_SET_REGION_COUNT * 2];
    float b[LIMIT_SET_REGION_COUNT * 2];
    float c[LIMIT_SET_REGION_COUNT * 2];
    float d[LIMIT_SET_REGION_COUNT * 2];

    copy_complex_values(circle_centers, group->circle_center);
    copy_mobius_coefficients(a, b, c, d, group->exit_map);

    glViewport(0, 0, width, height);
    glUseProgram(renderer->program);
    glBindVertexArray(renderer->vertex_array);

    glUniform2f(renderer->center_location, center_x, center_y);
    glUniform1f(renderer->scale_location, scale);
    glUniform2f(renderer->resolution_location, (float)width, (float)height);
    glUniform2fv(renderer->circle_center_location, LIMIT_SET_REGION_COUNT, circle_centers);
    glUniform1f(
        renderer->circle_radius_squared_location,
        group->circle_radius * group->circle_radius
    );
    glUniform2fv(renderer->a_location, LIMIT_SET_REGION_COUNT, a);
    glUniform2fv(renderer->b_location, LIMIT_SET_REGION_COUNT, b);
    glUniform2fv(renderer->c_location, LIMIT_SET_REGION_COUNT, c);
    glUniform2fv(renderer->d_location, LIMIT_SET_REGION_COUNT, d);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
