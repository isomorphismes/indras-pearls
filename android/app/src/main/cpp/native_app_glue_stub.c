#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "original_kleinian.h"
#include "original_texture_renderer.h"
#include "original_trace_controls.h"

#define LOG_TAG "IndrasPearls"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define ORIGINAL_POINT_REQUEST 10000u

struct engine {
    struct android_app *app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    struct original_texture_renderer renderer;
    struct original_trace_controls trace_controls;
    bool dirty;
};

static void terminate_display(struct engine *engine);

static uint64_t rgb_hash(
    const uint8_t *rgba,
    size_t pixel_count
) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const size_t offset = pixel * 4u;
        for (size_t channel = 0; channel < 3u; ++channel) {
            hash ^= rgba[offset + channel];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static size_t count_dark_rgb(
    const uint8_t *rgba,
    size_t pixel_count
) {
    size_t count = 0;
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const size_t offset = pixel * 4u;
        if (rgba[offset] < 32u &&
            rgba[offset + 1u] < 32u &&
            rgba[offset + 2u] < 32u) {
            ++count;
        }
    }
    return count;
}

static bool rebuild_original_raster(struct engine *engine) {
    if (engine->width <= 0 || engine->height <= 0) {
        return false;
    }

    const size_t point_capacity =
        original_kleinian_point_capacity(ORIGINAL_POINT_REQUEST);
    if (point_capacity == 0) {
        return false;
    }

    struct original_kleinian_queue_item *queue =
        calloc(point_capacity, sizeof(*queue));
    struct original_kleinian_complex *points =
        calloc(point_capacity, sizeof(*points));

    const size_t width = (size_t)engine->width;
    const size_t height = (size_t)engine->height;
    if (width > SIZE_MAX / height) {
        free(points);
        free(queue);
        return false;
    }
    const size_t pixel_count = width * height;
    if (pixel_count > SIZE_MAX / 4u) {
        free(points);
        free(queue);
        return false;
    }
    const size_t rgba_size = pixel_count * 4u;
    uint8_t *rgba = malloc(rgba_size);

    if (queue == NULL || points == NULL || rgba == NULL) {
        LOGE("could not allocate original Kleinian working buffers");
        free(rgba);
        free(points);
        free(queue);
        return false;
    }

    struct original_kleinian_complex ta;
    struct original_kleinian_complex tb;
    original_trace_controls_to_traces(
        &engine->trace_controls,
        &ta,
        &tb
    );

    size_t point_count = 0;
    bool ok = original_kleinian_generate_points_from_traces(
        ta,
        tb,
        ORIGINAL_POINT_REQUEST,
        queue,
        point_capacity,
        points,
        point_capacity,
        &point_count
    );
    if (ok) {
        ok = original_kleinian_rasterize_rgba(
            points,
            point_count,
            width,
            height,
            rgba,
            rgba_size
        );
    }

    const uint64_t hash = ok ? rgb_hash(rgba, pixel_count) : 0;
    const size_t dark_pixels = ok ? count_dark_rgb(rgba, pixel_count) : 0;

    if (ok) {
        ok = upload_original_texture(
            &engine->renderer,
            engine->width,
            engine->height,
            rgba
        );
    }

    if (ok) {
        LOGI(
            "original dgulotta/kleinian raster ready: "
            "tr(a)=(%.6f,%.6f) tr(b)=(%.6f,%.6f) "
            "points=%zu rgbhash=%016llx dark=%zu size=%dx%d",
            ta.re,
            ta.im,
            tb.re,
            tb.im,
            point_count,
            (unsigned long long)hash,
            dark_pixels,
            engine->width,
            engine->height
        );
    } else {
        LOGE(
            "could not build original dgulotta/kleinian raster "
            "for tr(a)=(%.6f,%.6f) tr(b)=(%.6f,%.6f)",
            ta.re,
            ta.im,
            tb.re,
            tb.im
        );
    }

    free(rgba);
    free(points);
    free(queue);
    return ok;
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

    EGLSurface surface =
        eglCreateWindowSurface(display, config, engine->app->window, NULL);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        eglTerminate(display);
        return false;
    }

    EGLContext context =
        eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
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

    if (!initialize_original_texture_renderer(&engine->renderer)) {
        LOGE("could not initialize original-raster texture renderer");
        terminate_display(engine);
        return false;
    }

    if (!rebuild_original_raster(engine)) {
        terminate_display(engine);
        return false;
    }

    engine->dirty = true;
    LOGI(
        "GLES original-raster renderer ready: %s / %s",
        (const char *)glGetString(GL_VERSION),
        (const char *)glGetString(GL_RENDERER)
    );
    return true;
}

static void terminate_display(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY) {
        return;
    }

    terminate_original_texture_renderer(&engine->renderer);

    eglMakeCurrent(
        engine->display,
        EGL_NO_SURFACE,
        EGL_NO_SURFACE,
        EGL_NO_CONTEXT
    );
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

static void probe_original_frame(struct engine *engine) {
#ifndef NDEBUG
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    const size_t width = (size_t)engine->width;
    const size_t crop_height = (size_t)engine->height * 3u / 4u;
    if (crop_height == 0 || width > SIZE_MAX / crop_height) {
        LOGE("original GPU probe dimensions overflow");
        return;
    }
    const size_t pixel_count = width * crop_height;
    if (pixel_count == 0 || pixel_count > SIZE_MAX / 4u) {
        LOGE("original GPU probe pixel count overflow");
        return;
    }

    uint8_t *rgba = malloc(pixel_count * 4u);
    if (rgba == NULL) {
        LOGE("original GPU probe allocation failed");
        return;
    }

    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(
        0,
        0,
        engine->width,
        (GLsizei)crop_height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba
    );
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOGE("original GPU probe glReadPixels failed: 0x%x", error);
        free(rgba);
        return;
    }

    const uint64_t hash = rgb_hash(rgba, pixel_count);
    const size_t dark_pixels = count_dark_rgb(rgba, pixel_count);
    size_t bright_pixels = 0;
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const size_t offset = pixel * 4u;
        if (rgba[offset] > 223u &&
            rgba[offset + 1u] > 223u &&
            rgba[offset + 2u] > 223u) {
            ++bright_pixels;
        }
    }

    LOGI(
        "original GPU frame probe rgbhash=%016llx dark=%zu bright=%zu "
        "pixels=%zu crop=%zux%zu+0+0",
        (unsigned long long)hash,
        dark_pixels,
        bright_pixels,
        pixel_count,
        width,
        crop_height
    );
    free(rgba);
#else
    (void)engine;
#endif
}

static void draw_frame(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY ||
        engine->surface == EGL_NO_SURFACE) {
        return;
    }

    const int32_t old_width = engine->width;
    const int32_t old_height = engine->height;
    eglQuerySurface(
        engine->display,
        engine->surface,
        EGL_WIDTH,
        &engine->width
    );
    eglQuerySurface(
        engine->display,
        engine->surface,
        EGL_HEIGHT,
        &engine->height
    );
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    if ((engine->width != old_width || engine->height != old_height) &&
        !rebuild_original_raster(engine)) {
        return;
    }

    draw_original_texture(
        &engine->renderer,
        &engine->trace_controls,
        engine->width,
        engine->height
    );
    probe_original_frame(engine);

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

    const int32_t action = AMotionEvent_getAction(event);
    const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
    const size_t pointer_count = AMotionEvent_getPointerCount(event);

    switch (masked) {
        case AMOTION_EVENT_ACTION_DOWN: {
            const float x = AMotionEvent_getX(event, 0);
            const float y = AMotionEvent_getY(event, 0);
            if (!begin_original_trace_drag(
                    &engine->trace_controls,
                    x,
                    y,
                    engine->width,
                    engine->height
                )) {
                return 0;
            }
            if (rebuild_original_raster(engine)) {
                engine->dirty = true;
            }
            return 1;
        }

        case AMOTION_EVENT_ACTION_MOVE:
            if (engine->trace_controls.active_index >= 0 &&
                pointer_count >= 1 &&
                update_original_trace_drag(
                    &engine->trace_controls,
                    AMotionEvent_getX(event, 0),
                    AMotionEvent_getY(event, 0),
                    engine->width,
                    engine->height
                )) {
                if (rebuild_original_raster(engine)) {
                    engine->dirty = true;
                }
                return 1;
            }
            break;

        case AMOTION_EVENT_ACTION_UP:
            if (engine->trace_controls.active_index >= 0) {
                if (pointer_count >= 1 &&
                    update_original_trace_drag(
                        &engine->trace_controls,
                        AMotionEvent_getX(event, 0),
                        AMotionEvent_getY(event, 0),
                        engine->width,
                        engine->height
                    )) {
                    (void)rebuild_original_raster(engine);
                }
                end_original_trace_drag(&engine->trace_controls);
                engine->dirty = true;
                return 1;
            }
            break;

        case AMOTION_EVENT_ACTION_CANCEL:
            if (engine->trace_controls.active_index >= 0) {
                end_original_trace_drag(&engine->trace_controls);
                engine->dirty = true;
                return 1;
            }
            break;

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
    initialize_original_trace_controls(&engine.trace_controls);
    engine.dirty = true;

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        const int timeout = engine.dirty ? 0 : -1;
        const int poll_result =
            ALooper_pollOnce(timeout, NULL, &events, (void **)&source);

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
