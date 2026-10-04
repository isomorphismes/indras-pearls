#include "original_texture_renderer.h"

#include <android/log.h>
#include <string.h>

#define LOG_TAG "IndrasPearls"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char *vertex_shader_source =
    "#version 300 es\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    vec2 p;\n"
    "    if (gl_VertexID == 0) p = vec2(-1.0, -1.0);\n"
    "    else if (gl_VertexID == 1) p = vec2(3.0, -1.0);\n"
    "    else p = vec2(-1.0, 3.0);\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "    v_uv = p * 0.5 + 0.5;\n"
    "}\n";

static const char *fragment_shader_source =
    "#version 300 es\n"
    "precision highp float;\n"
    "const int CONTROL_COUNT = 2;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_control_center[CONTROL_COUNT];\n"
    "uniform vec2 u_control_value[CONTROL_COUNT];\n"
    "uniform float u_control_radius;\n"
    "uniform int u_active_control;\n"
    "layout(location = 0) out vec4 out_color;\n"
    "\n"
    "vec3 control_accent(int index) {\n"
    "    if (index == 0) return vec3(0.15, 0.54, 0.92);\n"
    "    return vec3(0.92, 0.45, 0.12);\n"
    "}\n"
    "\n"
    "vec3 draw_trace_controls(vec3 base_color) {\n"
    "    vec3 color = base_color;\n"
    "    float extent = u_control_radius * 1.08;\n"
    "    for (int index = 0; index < CONTROL_COUNT; ++index) {\n"
    "        vec2 delta = gl_FragCoord.xy - u_control_center[index];\n"
    "        if (abs(delta.x) > extent || abs(delta.y) > extent) continue;\n"
    "        vec2 local = delta / u_control_radius;\n"
    "        float radius = length(local);\n"
    "        if (radius > 1.08) continue;\n"
    "\n"
    "        vec3 accent = control_accent(index);\n"
    "        float inside = 1.0 - smoothstep(0.96, 1.0, radius);\n"
    "        color = mix(color, vec3(0.055, 0.060, 0.070), inside * 0.76);\n"
    "\n"
    "        float horizontal = 1.0 - smoothstep(0.010, 0.028, abs(local.y));\n"
    "        float vertical = 1.0 - smoothstep(0.010, 0.028, abs(local.x));\n"
    "        float axes = max(horizontal, vertical) * inside;\n"
    "        color = mix(color, vec3(0.66), axes * 0.34);\n"
    "\n"
    "        float rim = 1.0 - smoothstep(0.018, 0.050, abs(radius - 1.0));\n"
    "        color = mix(color, accent, rim * 0.94);\n"
    "\n"
    "        float handle_distance = length(local - u_control_value[index]);\n"
    "        float handle = 1.0 - smoothstep(0.060, 0.110, handle_distance);\n"
    "        color = mix(color, accent, handle);\n"
    "\n"
    "        if (index == u_active_control) {\n"
    "            float active_ring = 1.0 - smoothstep(0.018, 0.052, abs(radius - 0.88));\n"
    "            color = mix(color, accent, active_ring * 0.55);\n"
    "        }\n"
    "    }\n"
    "    return color;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec4 texel = texture(u_texture, vec2(v_uv.x, 1.0 - v_uv.y));\n"
    "    out_color = vec4(draw_trace_controls(texel.rgb), texel.a);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        LOGE("glCreateShader failed: 0x%x", glGetError());
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char log[2048];
        GLsizei log_length = 0;
        glGetShaderInfoLog(shader, sizeof(log), &log_length, log);
        LOGE("shader compile failed: %.*s", (int)log_length, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool initialize_original_texture_renderer(struct original_texture_renderer *renderer) {
    if (renderer == NULL) {
        return false;
    }
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

    renderer->program = glCreateProgram();
    if (renderer->program == 0) {
        glDeleteShader(fragment_shader);
        glDeleteShader(vertex_shader);
        return false;
    }
    glAttachShader(renderer->program, vertex_shader);
    glAttachShader(renderer->program, fragment_shader);
    glLinkProgram(renderer->program);
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(renderer->program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[2048];
        GLsizei log_length = 0;
        glGetProgramInfoLog(renderer->program, sizeof(log), &log_length, log);
        LOGE("shader link failed: %.*s", (int)log_length, log);
        terminate_original_texture_renderer(renderer);
        return false;
    }

    renderer->texture_location =
        glGetUniformLocation(renderer->program, "u_texture");
    renderer->control_center_location =
        glGetUniformLocation(renderer->program, "u_control_center[0]");
    renderer->control_value_location =
        glGetUniformLocation(renderer->program, "u_control_value[0]");
    renderer->control_radius_location =
        glGetUniformLocation(renderer->program, "u_control_radius");
    renderer->active_control_location =
        glGetUniformLocation(renderer->program, "u_active_control");

    if (renderer->texture_location < 0 ||
        renderer->control_center_location < 0 ||
        renderer->control_value_location < 0 ||
        renderer->control_radius_location < 0 ||
        renderer->active_control_location < 0) {
        LOGE("original texture/control shader uniform not found");
        terminate_original_texture_renderer(renderer);
        return false;
    }

    glGenVertexArrays(1, &renderer->vertex_array);
    glGenTextures(1, &renderer->texture);
    if (renderer->vertex_array == 0 || renderer->texture == 0) {
        LOGE("could not allocate GLES texture renderer objects: 0x%x", glGetError());
        terminate_original_texture_renderer(renderer);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void terminate_original_texture_renderer(struct original_texture_renderer *renderer) {
    if (renderer == NULL) {
        return;
    }
    if (renderer->texture != 0) {
        glDeleteTextures(1, &renderer->texture);
    }
    if (renderer->vertex_array != 0) {
        glDeleteVertexArrays(1, &renderer->vertex_array);
    }
    if (renderer->program != 0) {
        glDeleteProgram(renderer->program);
    }
    memset(renderer, 0, sizeof(*renderer));
}

bool upload_original_texture(
    struct original_texture_renderer *renderer,
    int width,
    int height,
    const uint8_t *rgba
) {
    if (renderer == NULL || rgba == NULL || width <= 0 || height <= 0 ||
        renderer->texture == 0) {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba
    );
    GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        LOGE("glTexImage2D failed: 0x%x", error);
        return false;
    }

    renderer->width = width;
    renderer->height = height;
    return true;
}

void draw_original_texture(
    const struct original_texture_renderer *renderer,
    const struct original_trace_controls *controls,
    int width,
    int height
) {
    if (renderer == NULL || controls == NULL ||
        renderer->program == 0 || renderer->texture == 0 ||
        renderer->width <= 0 || renderer->height <= 0 ||
        width <= 0 || height <= 0) {
        return;
    }

    float centers[ORIGINAL_TRACE_CONTROL_COUNT * 2];
    float values[ORIGINAL_TRACE_CONTROL_COUNT * 2];
    for (int index = 0; index < ORIGINAL_TRACE_CONTROL_COUNT; ++index) {
        original_trace_control_center(
            index,
            width,
            height,
            &centers[index * 2],
            &centers[index * 2 + 1]
        );
        values[index * 2] = controls->value[index].real;
        values[index * 2 + 1] = controls->value[index].imaginary;
    }

    glViewport(0, 0, width, height);
    glUseProgram(renderer->program);
    glBindVertexArray(renderer->vertex_array);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glUniform1i(renderer->texture_location, 0);
    glUniform2fv(
        renderer->control_center_location,
        ORIGINAL_TRACE_CONTROL_COUNT,
        centers
    );
    glUniform2fv(
        renderer->control_value_location,
        ORIGINAL_TRACE_CONTROL_COUNT,
        values
    );
    glUniform1f(
        renderer->control_radius_location,
        original_trace_control_radius(width, height)
    );
    glUniform1i(renderer->active_control_location, controls->active_index);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}
