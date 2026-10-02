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
    bool dirty;
};

static void terminate_display(struct engine *engine);

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

    const struct original_kleinian_complex ta = {2.2, 0.0};
    const struct original_kleinian_complex tb = {2.2, 0.0};
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
            "original dgulotta/kleinian raster ready: tr(a)=2.2 tr(b)=2.2 points=%zu size=%dx%d",
            point_count,
            engine->width,
            engine->height
        );
    } else {
        LOGE("could not build original dgulotta/kleinian raster");
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

static void probe_original_frame(struct engine *engine) {
#ifndef NDEBUG
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    const size_t width = (size_t)engine->width;
    const size_t height = (size_t)engine->height;
    if (width > SIZE_MAX / height) {
        LOGE("original GPU probe dimensions overflow");
        return;
    }
    const size_t pixel_count = width * height;
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
        0, 0, engine->width, engine->height,
        GL_RGBA, GL_UNSIGNED_BYTE, rgba
    );
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOGE("original GPU probe glReadPixels failed: 0x%x", error);
        free(rgba);
        return;
    }

    uint64_t hash = UINT64_C(1469598103934665603);
    size_t dark_pixels = 0;
    size_t bright_pixels = 0;
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const size_t offset = pixel * 4u;
        const unsigned r = rgba[offset];
        const unsigned g = rgba[offset + 1u];
        const unsigned b = rgba[offset + 2u];
        hash ^= r; hash *= UINT64_C(1099511628211);
        hash ^= g; hash *= UINT64_C(1099511628211);
        hash ^= b; hash *= UINT64_C(1099511628211);
        if (r < 32u && g < 32u && b < 32u) {
            ++dark_pixels;
        }
        if (r > 223u && g > 223u && b > 223u) {
            ++bright_pixels;
        }
    }

    LOGI(
        "original GPU frame probe rgbhash=%016llx dark=%zu bright=%zu pixels=%zu",
        (unsigned long long)hash,
        dark_pixels,
        bright_pixels,
        pixel_count
    );
    free(rgba);
#else
    (void)engine;
#endif
}

static void draw_frame(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY || engine->surface == EGL_NO_SURFACE) {
        return;
    }

    int32_t old_width = engine->width;
    int32_t old_height = engine->height;
    eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    if ((engine->width != old_width || engine->height != old_height) &&
        !rebuild_original_raster(engine)) {
        return;
    }

    draw_original_texture(&engine->renderer);
    probe_original_frame(engine);

    if (!eglSwapBuffers(engine->display, engine->surface)) {
        LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
    }
    engine->dirty = false;
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
    engine.dirty = true;

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = NULL;

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
