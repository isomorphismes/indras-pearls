#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "renderer_packet.h"
#include "limit_set_renderer.h"
#include "parameter_controls.h"

#define LOG_TAG "IndrasPearls"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#ifdef INDRAS_ICK_PRODUCER
#define SCHOTTKY_PRODUCER_NAME "ICK C"
#else
#define SCHOTTKY_PRODUCER_NAME "Android NDK C"
#endif

struct camera {
    float center_x;
    float center_y;
    float scale;
};

struct engine {
    struct android_app *app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;

    struct camera camera;
    float renderer_packet[RENDERER_PACKET_FLOAT_COUNT];
    struct complex_parameter_controls parameter_controls;
    struct limit_set_renderer renderer;

    bool dragging;
    bool pinching;
    float last_x;
    float last_y;
    float last_span;
    bool dirty;
};

static void terminate_display(struct engine *engine);

static bool refresh_schottky_renderer_packet(struct engine *engine) {
    float controls_cartesian[SCHOTTKY_PARAMETER_FLOAT_COUNT];
    complex_parameter_controls_flatten(
        &engine->parameter_controls,
        controls_cartesian
    );

    const int status = schottky_three_disk_renderer_packet(
        controls_cartesian,
        engine->renderer_packet
    );
    if (status != RENDERER_PACKET_OK) {
        LOGE(
            "Schottky controls rejected status=%d "
            "u0=(%.4f,%.4f) u1=(%.4f,%.4f) u2=(%.4f,%.4f)",
            status,
            (double)controls_cartesian[0],
            (double)controls_cartesian[1],
            (double)controls_cartesian[2],
            (double)controls_cartesian[3],
            (double)controls_cartesian[4],
            (double)controls_cartesian[5]
        );
        return false;
    }

    LOGI(
        "Schottky controls u0=(%.4f,%.4f) u1=(%.4f,%.4f) u2=(%.4f,%.4f)",
        (double)controls_cartesian[0],
        (double)controls_cartesian[1],
        (double)controls_cartesian[2],
        (double)controls_cartesian[3],
        (double)controls_cartesian[4],
        (double)controls_cartesian[5]
    );
    return true;
}

static float pointer_span(const AInputEvent *event) {
    if (AMotionEvent_getPointerCount(event) < 2) {
        return 0.0f;
    }

    float x0 = AMotionEvent_getX(event, 0);
    float y0 = AMotionEvent_getY(event, 0);
    float x1 = AMotionEvent_getX(event, 1);
    float y1 = AMotionEvent_getY(event, 1);
    float dx = x1 - x0;
    float dy = y1 - y0;
    return sqrtf(dx * dx + dy * dy);
}

static bool initialize_display(struct engine *engine) {
    if (engine->app->window == NULL) {
        return false;
    }

    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return false;
    }

    EGLConfig config = NULL;
    EGLint config_count = 0;
    if (!eglChooseConfig(display, config_attributes, &config, 1, &config_count) ||
        config_count != 1) {
        LOGE("could not choose GLES3 config: 0x%x", eglGetError());
        eglTerminate(display);
        return false;
    }

    EGLint format = 0;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        eglTerminate(display);
        return false;
    }

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%x", eglGetError());
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    engine->display = display;
    engine->surface = surface;
    engine->context = context;
    eglQuerySurface(display, surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &engine->height);

    if (!initialize_limit_set_renderer(&engine->renderer)) {
        LOGE("could not initialize full-screen limit-set renderer");
        terminate_display(engine);
        return false;
    }

    engine->dirty = true;
    LOGI(
        "GLES limit-set renderer ready: %s / %s",
        (const char *)glGetString(GL_VERSION),
        (const char *)glGetString(GL_RENDERER)
    );
    return true;
}

static void terminate_display(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY) {
        return;
    }

    terminate_limit_set_renderer(&engine->renderer);

    eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (engine->context != EGL_NO_CONTEXT) {
        eglDestroyContext(engine->display, engine->context);
    }
    if (engine->surface != EGL_NO_SURFACE) {
        eglDestroySurface(engine->display, engine->surface);
    }
    eglTerminate(engine->display);

    engine->display = EGL_NO_DISPLAY;
    engine->surface = EGL_NO_SURFACE;
    engine->context = EGL_NO_CONTEXT;
    engine->width = 0;
    engine->height = 0;
}

static bool frame_probe_requested(const struct engine *engine) {
#ifndef NDEBUG
    if (engine->app == NULL ||
        engine->app->activity == NULL ||
        engine->app->activity->internalDataPath == NULL) {
        return false;
    }

    char path[1024];
    int written = snprintf(
        path,
        sizeof(path),
        "%s/frame-probe.enable",
        engine->app->activity->internalDataPath
    );
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return false;
    }

    FILE *marker = fopen(path, "rb");
    if (marker == NULL) {
        return false;
    }
    fclose(marker);
    return true;
#else
    (void)engine;
    return false;
#endif
}

static void write_frame_probe(struct engine *engine) {
#ifndef NDEBUG
    if (!frame_probe_requested(engine)) {
        return;
    }

    GLuint framebuffer = 0;
    GLuint color_buffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenRenderbuffers(1, &color_buffer);
    if (framebuffer == 0 || color_buffer == 0) {
        LOGE("frame probe could not allocate GLES framebuffer objects");
        if (color_buffer != 0) {
            glDeleteRenderbuffers(1, &color_buffer);
        }
        if (framebuffer != 0) {
            glDeleteFramebuffers(1, &framebuffer);
        }
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, color_buffer);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_RGBA8,
        engine->width,
        engine->height
    );
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_RENDERBUFFER,
        color_buffer
    );
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("frame probe framebuffer is incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_limit_set(
        &engine->renderer,
        engine->renderer_packet,
        &engine->parameter_controls,
        engine->camera.center_x,
        engine->camera.center_y,
        engine->camera.scale,
        engine->width,
        engine->height
    );
    glFinish();

    const int crop_y = engine->height * 10 / 100;
    const int crop_height = engine->height * 65 / 100;
    if (engine->width <= 0 || crop_height <= 0) {
        return;
    }

    const size_t pixel_count =
        (size_t)engine->width * (size_t)crop_height;
    if (pixel_count > SIZE_MAX / 4 || pixel_count > SIZE_MAX / 3) {
        LOGE("frame probe dimensions overflow");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    unsigned char *rgba = malloc(pixel_count * 4);
    unsigned char *rgb = malloc(pixel_count * 3);
    if (rgba == NULL || rgb == NULL) {
        LOGE("frame probe allocation failed for %zu pixels", pixel_count);
        free(rgb);
        free(rgba);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(
        0,
        crop_y,
        engine->width,
        crop_height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba
    );
    GLenum read_error = glGetError();
    if (read_error != GL_NO_ERROR) {
        LOGE("frame probe glReadPixels failed: 0x%x", read_error);
        free(rgb);
        free(rgba);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t index = 0; index < pixel_count * 4; ++index) {
        hash ^= rgba[index];
        hash *= UINT64_C(1099511628211);
    }

    for (int output_row = 0; output_row < crop_height; ++output_row) {
        const int source_row = crop_height - 1 - output_row;
        for (int x = 0; x < engine->width; ++x) {
            const size_t source =
                ((size_t)source_row * (size_t)engine->width + (size_t)x) * 4;
            const size_t target =
                ((size_t)output_row * (size_t)engine->width + (size_t)x) * 3;
            rgb[target] = rgba[source];
            rgb[target + 1] = rgba[source + 1];
            rgb[target + 2] = rgba[source + 2];
        }
    }

    char temporary_path[1024];
    char final_path[1024];
    int temporary_written = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s/frame-probe.tmp",
        engine->app->activity->internalDataPath
    );
    int final_written = snprintf(
        final_path,
        sizeof(final_path),
        "%s/frame-probe.ppm",
        engine->app->activity->internalDataPath
    );
    if (temporary_written < 0 ||
        final_written < 0 ||
        (size_t)temporary_written >= sizeof(temporary_path) ||
        (size_t)final_written >= sizeof(final_path)) {
        LOGE("frame probe path is too long");
        free(rgb);
        free(rgba);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    FILE *output = fopen(temporary_path, "wb");
    if (output == NULL) {
        LOGE("frame probe could not open output file");
        free(rgb);
        free(rgba);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    bool write_ok =
        fprintf(output, "P6\n%d %d\n255\n", engine->width, crop_height) > 0 &&
        fwrite(rgb, 3, pixel_count, output) == pixel_count &&
        fclose(output) == 0;
    if (!write_ok) {
        LOGE("frame probe write failed");
        remove(temporary_path);
        free(rgb);
        free(rgba);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    if (rename(temporary_path, final_path) != 0) {
        LOGE("frame probe could not publish output file");
        remove(temporary_path);
        free(rgb);
        free(rgba);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        return;
    }

    float controls_cartesian[SCHOTTKY_PARAMETER_FLOAT_COUNT];
    complex_parameter_controls_flatten(
        &engine->parameter_controls,
        controls_cartesian
    );
    LOGI(
        "QEMU frame probe hash=%016llx "
        "u0=(%.4f,%.4f) u1=(%.4f,%.4f) u2=(%.4f,%.4f) "
        "crop=%dx%d+0+%d",
        (unsigned long long)hash,
        (double)controls_cartesian[0],
        (double)controls_cartesian[1],
        (double)controls_cartesian[2],
        (double)controls_cartesian[3],
        (double)controls_cartesian[4],
        (double)controls_cartesian[5],
        engine->width,
        crop_height,
        crop_y
    );

    free(rgb);
    free(rgba);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &color_buffer);
    glDeleteFramebuffers(1, &framebuffer);
    glViewport(0, 0, engine->width, engine->height);
#else
    (void)engine;
#endif
}

static void draw_frame(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY || engine->surface == EGL_NO_SURFACE) {
        return;
    }

    eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    draw_limit_set(
        &engine->renderer,
        engine->renderer_packet,
        &engine->parameter_controls,
        engine->camera.center_x,
        engine->camera.center_y,
        engine->camera.scale,
        engine->width,
        engine->height
    );

    write_frame_probe(engine);

    if (!eglSwapBuffers(engine->display, engine->surface)) {
        LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
    }
    engine->dirty = false;
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    struct engine *engine = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }

    int32_t action = AMotionEvent_getAction(event);
    int32_t masked_action = action & AMOTION_EVENT_ACTION_MASK;
    size_t pointer_count = AMotionEvent_getPointerCount(event);

    switch (masked_action) {
        case AMOTION_EVENT_ACTION_DOWN: {
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            if (begin_complex_parameter_drag(
                    &engine->parameter_controls,
                    x,
                    y,
                    engine->width,
                    engine->height
                )) {
                engine->dragging = false;
                engine->pinching = false;
                if (refresh_schottky_renderer_packet(engine)) {
                    engine->dirty = true;
                }
                return 1;
            }

            engine->dragging = pointer_count == 1;
            engine->pinching = false;
            engine->last_x = x;
            engine->last_y = y;
            return 1;
        }

        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            if (engine->parameter_controls.active_index >= 0) {
                return 1;
            }
            if (pointer_count >= 2) {
                engine->dragging = false;
                engine->pinching = true;
                engine->last_span = pointer_span(event);
            }
            return 1;

        case AMOTION_EVENT_ACTION_MOVE:
            if (engine->parameter_controls.active_index >= 0) {
                if (pointer_count >= 1 && update_complex_parameter_drag(
                        &engine->parameter_controls,
                        AMotionEvent_getX(event, 0),
                        AMotionEvent_getY(event, 0),
                        engine->width,
                        engine->height
                    )) {
                    if (refresh_schottky_renderer_packet(engine)) {
                        engine->dirty = true;
                    }
                }
                return 1;
            }

            if (pointer_count >= 2) {
                float span = pointer_span(event);
                if (!engine->pinching) {
                    engine->pinching = true;
                    engine->dragging = false;
                    engine->last_span = span;
                    return 1;
                }

                if (engine->last_span > 1.0f && span > 1.0f) {
                    float ratio = span / engine->last_span;
                    engine->camera.scale /= ratio;
                    if (engine->camera.scale < 0.0001f) engine->camera.scale = 0.0001f;
                    if (engine->camera.scale > 10000.0f) engine->camera.scale = 10000.0f;
                    engine->dirty = true;
                }
                engine->last_span = span;
                return 1;
            }

            if (engine->dragging && pointer_count == 1) {
                float x = AMotionEvent_getX(event, 0);
                float y = AMotionEvent_getY(event, 0);
                float dx = x - engine->last_x;
                float dy = y - engine->last_y;
                engine->last_x = x;
                engine->last_y = y;

                float height = engine->height > 0 ? (float)engine->height : 1.0f;
                float complex_units_per_pixel = engine->camera.scale / height;
                engine->camera.center_x -= dx * complex_units_per_pixel;
                engine->camera.center_y += dy * complex_units_per_pixel;
                engine->dirty = true;
                return 1;
            }
            break;

        case AMOTION_EVENT_ACTION_POINTER_UP:
            if (engine->parameter_controls.active_index >= 0) {
                end_complex_parameter_drag(&engine->parameter_controls);
                engine->dirty = true;
                return 1;
            }
            engine->pinching = false;
            engine->dragging = false;
            return 1;

        case AMOTION_EVENT_ACTION_UP:
            if (engine->parameter_controls.active_index >= 0 &&
                pointer_count >= 1 &&
                update_complex_parameter_drag(
                    &engine->parameter_controls,
                    AMotionEvent_getX(event, 0),
                    AMotionEvent_getY(event, 0),
                    engine->width,
                    engine->height
                )) {
                refresh_schottky_renderer_packet(engine);
            }
            end_complex_parameter_drag(&engine->parameter_controls);
            engine->dragging = false;
            engine->pinching = false;
            engine->dirty = true;
            return 1;

        case AMOTION_EVENT_ACTION_CANCEL:
            end_complex_parameter_drag(&engine->parameter_controls);
            engine->dragging = false;
            engine->pinching = false;
            engine->dirty = true;
            return 1;

        default:
            break;
    }

    return 0;
}

static void handle_command(struct android_app *app, int32_t command) {
    struct engine *engine = app->userData;

    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != NULL && engine->display == EGL_NO_DISPLAY) {
                initialize_display(engine);
            }
            break;

        case APP_CMD_TERM_WINDOW:
            terminate_display(engine);
            break;

        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_GAINED_FOCUS:
            engine->dirty = true;
            break;

        default:
            break;
    }
}

void android_main(struct android_app *app) {
    struct engine engine;
    memset(&engine, 0, sizeof(engine));
    engine.app = app;
    engine.display = EGL_NO_DISPLAY;
    engine.surface = EGL_NO_SURFACE;
    engine.context = EGL_NO_CONTEXT;
    engine.camera.center_x = 0.0f;
    engine.camera.center_y = 0.0f;
    engine.camera.scale = 4.0f;
    initialize_complex_parameter_controls(&engine.parameter_controls);
    if (!refresh_schottky_renderer_packet(&engine)) {
        return;
    }
    LOGI("Schottky math producer: %s", SCHOTTKY_PRODUCER_NAME);
    engine.dirty = true;

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        int timeout = engine.dirty ? 0 : -1;
        int poll_result = ALooper_pollOnce(timeout, NULL, &events, (void **)&source);

        if (poll_result >= 0 && source != NULL) {
            source->process(app, source);
        }

        if (app->destroyRequested != 0) {
            terminate_display(&engine);
            return;
        }

        if (engine.dirty && engine.display != EGL_NO_DISPLAY) {
            draw_frame(&engine);
        }
    }
}
