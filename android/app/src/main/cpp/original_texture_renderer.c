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
    "in vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    out_color = texture(u_texture, vec2(v_uv.x, 1.0 - v_uv.y));\n"
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

    renderer->texture_location = glGetUniformLocation(renderer->program, "u_texture");
    if (renderer->texture_location < 0) {
        LOGE("u_texture uniform not found");
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

void draw_original_texture(const struct original_texture_renderer *renderer) {
    if (renderer == NULL || renderer->program == 0 || renderer->texture == 0 ||
        renderer->width <= 0 || renderer->height <= 0) {
        return;
    }

    glViewport(0, 0, renderer->width, renderer->height);
    glUseProgram(renderer->program);
    glBindVertexArray(renderer->vertex_array);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glUniform1i(renderer->texture_location, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}
