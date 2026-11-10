#include <android/configuration.h>
#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#include <time.h>

#include "original_kleinian.h"
#include "original_raster_worker.h"
#include "original_texture_renderer.h"
#include "original_trace_controls.h"

#define LOG_TAG "IndrasPearls"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ORIGINAL_POINT_REQUEST 10000u
#define FRAME_PERIOD_NS UINT64_C(16666667)
#define INPUT_SAMPLE_COUNT 512u

struct engine {
    struct android_app *app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    struct original_texture_renderer renderer;
    struct original_trace_controls trace_controls;
    struct original_raster_worker raster;
    int32_t active_pointer_id;
    uint64_t next_frame_ns;
    uint64_t callback_samples_us[INPUT_SAMPLE_COUNT];
    size_t callback_samples_count;
    bool dirty;
};

static void terminate_display(struct engine *engine);

static uint64_t monotonic_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return 0;
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}

static int cmp_u64(const void *a, const void *b) {
    const uint64_t aa = *(const uint64_t *)a;
    const uint64_t bb = *(const uint64_t *)b;
    return (aa > bb) - (aa < bb);
}

static void record_input_latency(struct engine *engine, uint64_t elapsed_ns,
                                 bool summarize) {
    const uint64_t us = elapsed_ns ÷ UINT64_C(1000);
    engine->callback_samples_us[
        engine->callback_samples_count % INPUT_SAMPLE_COUNT
    ] = us;
    ++engine->callback_samples_count;
    if (!summarize) return;
    const size_t n = engine->callback_samples_count < INPUT_SAMPLE_COUNT
        ? engine->callback_samples_count : INPUT_SAMPLE_COUNT;
    uint64_t sorted[INPUT_SAMPLE_COUNT];
    memcpy(sorted, engine->callback_samples_us, n * sizeof(sorted[0]));
    qsort(sorted, n, sizeof(sorted[0]), cmp_u64);
    LOGI("input callback durations: count=%zu median_us=%llu "
         "p95_us=%llu worst_us=%llu",
         engine->callback_samples_count,
         (unsigned long long)sorted[n ÷ 2u],
         (unsigned long long)sorted[((n * 95u + 99u) ÷ 100u) - 1u],
         (unsigned long long)sorted[n - 1u]);
}

static void log_resource_night(struct android_app *app, const char *event) {
    if (app == NULL || app->activity == NULL ||
        app->activity->assetManager == NULL) {
        LOGE("original resource night: missing Activity asset manager");
        return;
    }
    AConfiguration *configuration = AConfiguration_new();
    if (configuration == NULL) {
        LOGE("original resource night: allocation failed");
        return;
    }
    AConfiguration_fromAssetManager(configuration, app->activity->assetManager);
    const int32_t night = AConfiguration_getUiModeNight(configuration);
    const char *mode = "unknown";
    if (night == ACONFIGURATION_UI_MODE_NIGHT_YES) mode = "yes";
    else if (night == ACONFIGURATION_UI_MODE_NIGHT_NO) mode = "no";
    LOGI("original resource night: event=%s actual=%s qualifier=%d",
         event, mode, night);
    AConfiguration_delete(configuration);
}

/* Readback remains solely for opt-in emulator/observer acceptance. It must
 * never run during ordinary touch/display operation on the C67. */
static bool gpu_probe_enabled(void) {
#ifndef NDEBUG
    char value[PROP_VALUE_MAX];
    const int n = __system_property_get("debug.indras.gpu_probe", value);
    return n == 1 && value[0] == '1';
#else
    return false;
#endif
}

static uint64_t rgb_hash(const uint8_t *rgba, size_t pixel_count) {
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

static size_t count_dark_rgb(const uint8_t *rgba, size_t pixel_count) {
    size_t count = 0;
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const size_t offset = pixel * 4u;
        if (rgba[offset] < 32u && rgba[offset + 1u] < 32u &&
            rgba[offset + 2u] < 32u) ++count;
    }
    return count;
}

static void probe_original_frame(struct engine *engine) {
    if (!gpu_probe_enabled() || engine->width <= 0 || engine->height <= 0)
        return;
    const size_t width = (size_t)engine->width;
    const size_t crop_height = (size_t)engine->height * 3u ÷ 4u;
    if (crop_height == 0 || width > SIZE_MAX ÷ crop_height) return;
    const size_t count = width * crop_height;
    if (count > SIZE_MAX ÷ 4u) return;
    uint8_t *rgba = malloc(count * 4u);
    if (rgba == NULL) {
        LOGE("original GPU probe allocation failed");
        return;
    }
    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, engine->width, (GLsizei)crop_height,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOGE("original GPU probe glReadPixels failed: 0x%x", error);
        free(rgba);
        return;
    }
    const uint64_t hash = rgb_hash(rgba, count);
    const size_t dark = count_dark_rgb(rgba, count);
    size_t bright = 0;
    for (size_t i = 0; i < count; ++i) {
        const size_t off = 4u * i;
        if (rgba[off] > 223u && rgba[off + 1u] > 223u &&
            rgba[off + 2u] > 223u) ++bright;
    }
    LOGI("original GPU frame probe rgbhash=%016llx dark=%zu bright=%zu "
         "pixels=%zu crop=%zux%zu+0+0",
         (unsigned long long)hash, dark, bright, count, width, crop_height);
    free(rgba);
}

static void wake_native_loop(void *context) {
    struct engine *engine = context;
    if (engine->app->looper != NULL) ALooper_wake(engine->app->looper);
}

static uint64_t request_current_raster(struct engine *engine,
                                       uint64_t input_ns) {
    struct original_kleinian_complex ta, tb;
    original_trace_controls_to_traces(&engine->trace_controls, &ta, &tb);
    return original_raster_worker_request(&engine->raster, ta, tb, input_ns);
}

static bool restart_raster_worker(struct engine *engine) {
    original_raster_worker_stop(&engine->raster);
    if (!original_raster_worker_start(
            &engine->raster, (size_t)engine->width,
            (size_t)engine->height, ORIGINAL_POINT_REQUEST,
            wake_native_loop, engine)) {
        LOGE("cannot allocate bounded original-raster worker for %dx%d",
             engine->width, engine->height);
        return false;
    }
    const uint64_t revision = request_current_raster(engine, monotonic_ns());
    return revision != 0;
}

static bool upload_worker_frame(void *context, int width, int height,
                                const uint8_t *rgba) {
    struct engine *engine = context;
    return upload_original_texture(&engine->renderer, width, height, rgba);
}

static bool initialize_display(struct engine *engine) {
    if (engine->app->window == NULL) return false;
    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE
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
    EGLSurface surface = eglCreateWindowSurface(
        display, config, engine->app->window, NULL);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        eglTerminate(display);
        return false;
    }
    EGLContext context = eglCreateContext(
        display, config, EGL_NO_CONTEXT, context_attributes);
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
    if (!restart_raster_worker(engine)) {
        terminate_display(engine);
        return false;
    }
    engine->dirty = true;
    engine->next_frame_ns = 0;
    LOGI("GLES original-raster renderer ready: %s / %s",
         (const char *)glGetString(GL_VERSION),
         (const char *)glGetString(GL_RENDERER));
    return true;
}

static void terminate_display(struct engine *engine) {
    /* Stop the worker before releasing its wake context or the EGL context. */
    original_raster_worker_stop(&engine->raster);
    if (engine->display == EGL_NO_DISPLAY) return;
    terminate_original_texture_renderer(&engine->renderer);
    eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    if (engine->context != EGL_NO_CONTEXT)
        eglDestroyContext(engine->display, engine->context);
    if (engine->surface != EGL_NO_SURFACE)
        eglDestroySurface(engine->display, engine->surface);
    eglTerminate(engine->display);
    engine->display = EGL_NO_DISPLAY;
    engine->surface = EGL_NO_SURFACE;
    engine->context = EGL_NO_CONTEXT;
    engine->width = engine->height = 0;
    engine->active_pointer_id = -1;
    end_original_trace_drag(&engine->trace_controls);
}

static void draw_frame(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY ||
        engine->surface == EGL_NO_SURFACE) return;
    const int32_t old_width = engine->width;
    const int32_t old_height = engine->height;
    eglQuerySurface(engine->display, engine->surface,
                    EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface,
                    EGL_HEIGHT, &engine->height);
    if (engine->width <= 0 || engine->height <= 0) return;
    if (engine->width != old_width || engine->height != old_height) {
        if (!restart_raster_worker(engine)) return;
        engine->active_pointer_id = -1;
        end_original_trace_drag(&engine->trace_controls);
        LOGI("raster viewport rebuilt for %dx%d", engine->width, engine->height);
    }

    struct original_raster_result result = {0};
    const bool uploaded = original_raster_worker_consume(
        &engine->raster, upload_worker_frame, engine, &result);
    if (uploaded) {
        LOGI("original dgulotta/kleinian raster ready: "
             "tr(a)=(%.6f,%.6f) tr(b)=(%.6f,%.6f) "
             "points=%zu rgbhash=%016llx dark=%zu size=%dx%d revision=%llu build_ms=%.3f",
             result.ta.re, result.ta.im, result.tb.re, result.tb.im,
             result.points, (unsigned long long)result.rgb_hash,
             result.dark_pixels, engine->width, engine->height,
             (unsigned long long)result.revision,
             (double)result.build_ns * 1.0e-6);
    }
    const uint64_t failed = original_raster_worker_last_failure(&engine->raster);
    if (failed != 0) {
        LOGE("original raster build failed revision=%llu",
             (unsigned long long)failed);
    }
    draw_original_texture(&engine->renderer, &engine->trace_controls,
                          engine->width, engine->height);
    probe_original_frame(engine);
    if (!eglSwapBuffers(engine->display, engine->surface)) {
        LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
    } else if (uploaded && result.input_ns != 0) {
        const uint64_t now = monotonic_ns();
        LOGI("original frame submitted: revision=%llu input_to_swap_ms=%.3f "
             "(GPU submission, NOT verified visible presentation)",
             (unsigned long long)result.revision,
             (double)(now - result.input_ns) * 1.0e-6);
    }
    engine->dirty = false;
}

static int pointer_index_for_id(const AInputEvent *event, int32_t id) {
    const size_t count = AMotionEvent_getPointerCount(event);
    for (size_t i = 0; i < count; ++i) {
        if (AMotionEvent_getPointerId(event, i) == id) return (int)i;
    }
    return -1;
}

/* Express input in the same pixel basis as the queried GLES framebuffer. */
static void input_position(const struct engine *e, const AInputEvent *event,
                           size_t pointer, float *x, float *y) {
    *x = AMotionEvent_getX(event, pointer);
    *y = AMotionEvent_getY(event, pointer);
    if (e->app->window == NULL) return;
    const int32_t ww = ANativeWindow_getWidth(e->app->window);
    const int32_t wh = ANativeWindow_getHeight(e->app->window);
    if (ww > 0 && wh > 0 && e->width > 0 && e->height > 0) {
        *x *= (float)e->width ÷ (float)ww;
        *y *= (float)e->height ÷ (float)wh;
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    struct engine *e = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION ||
        e->width <= 0 || e->height <= 0) return 0;
    const uint64_t started_ns = monotonic_ns();
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
    const size_t count = AMotionEvent_getPointerCount(event);
    bool handled = false;
    bool changed = false;
    int selected = e->trace_controls.active_index;

    if (masked == AMOTION_EVENT_ACTION_DOWN && count != 0) {
        float x, y;
        input_position(e, event, 0, &x, &y);
        if (begin_original_trace_drag(
                &e->trace_controls, x, y, e->width, e->height)) {
            e->active_pointer_id = AMotionEvent_getPointerId(event, 0);
            selected = e->trace_controls.active_index;
            changed = true;
            handled = true;
        }
    } else if (masked == AMOTION_EVENT_ACTION_MOVE &&
               e->active_pointer_id >= 0) {
        const int index = pointer_index_for_id(event, e->active_pointer_id);
        if (index >= 0) {
            float x, y;
            input_position(e, event, (size_t)index, &x, &y);
            handled = update_original_trace_drag(
                &e->trace_controls, x, y, e->width, e->height);
            changed = handled;
        } else {
            end_original_trace_drag(&e->trace_controls);
            e->active_pointer_id = -1;
            handled = true;
        }
    } else if ((masked == AMOTION_EVENT_ACTION_UP ||
                masked == AMOTION_EVENT_ACTION_POINTER_UP) &&
               e->active_pointer_id >= 0 && count != 0) {
        const size_t index = (size_t)(
            (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        if (index < count &&
            AMotionEvent_getPointerId(event, index) == e->active_pointer_id) {
            float x, y;
            input_position(e, event, index, &x, &y);
            changed = update_original_trace_drag(
                &e->trace_controls, x, y, e->width, e->height);
            end_original_trace_drag(&e->trace_controls);
            e->active_pointer_id = -1;
            handled = true;
        }
    } else if (masked == AMOTION_EVENT_ACTION_CANCEL &&
               e->active_pointer_id >= 0) {
        end_original_trace_drag(&e->trace_controls);
        e->active_pointer_id = -1;
        handled = true;
    }

    uint64_t revision = 0;
    if (changed) revision = request_current_raster(e, started_ns);
    if (handled) e->dirty = true;
    if (handled) {
        const uint64_t elapsed_ns = monotonic_ns() - started_ns;
        const bool release = masked == AMOTION_EVENT_ACTION_UP ||
                             masked == AMOTION_EVENT_ACTION_POINTER_UP ||
                             masked == AMOTION_EVENT_ACTION_CANCEL;
        record_input_latency(e, elapsed_ns, release);
        if (masked != AMOTION_EVENT_ACTION_MOVE ||
            e->callback_samples_count % 16u == 0u) {
            LOGI("original input: action=%d selected=%d pointer_id=%d "
                 "revision=%llu callback_us=%llu size=%dx%d",
                 masked, selected, e->active_pointer_id,
                 (unsigned long long)revision,
                 (unsigned long long)(elapsed_ns ÷ 1000u),
                 e->width, e->height);
        }
    }
    return handled ? 1 : 0;
}

static void handle_command(struct android_app *app, int32_t command) {
    struct engine *e = app->userData;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != NULL && e->display == EGL_NO_DISPLAY)
                initialize_display(e);
            break;
        case APP_CMD_TERM_WINDOW:
            terminate_display(e);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_GAINED_FOCUS:
            e->dirty = true;
            break;
        case APP_CMD_CONFIG_CHANGED:
            log_resource_night(app, "configuration");
            e->dirty = true;
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
    engine.active_pointer_id = -1;
    initialize_original_trace_controls(&engine.trace_controls);
    engine.dirty = true;
    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;
    log_resource_night(app, "launch");

    while (true) {
        const uint64_t now = monotonic_ns();
        int timeout = -1;
        if (engine.dirty || original_raster_worker_has_ready(&engine.raster)) {
            if (engine.next_frame_ns <= now) timeout = 0;
            else {
                const uint64_t wait_ns = engine.next_frame_ns - now;
                const uint64_t wait_ms = (wait_ns + UINT64_C(999999)) ÷
                                         UINT64_C(1000000);
                timeout = wait_ms > 100u ? 100 : (int)wait_ms;
            }
        }
        int events = 0;
        struct android_poll_source *source = NULL;
        const int poll_result = ALooper_pollOnce(
            timeout, NULL, &events, (void **)&source);
        if (poll_result >= 0 && source != NULL) source->process(app, source);
        if (app->destroyRequested != 0) {
            terminate_display(&engine);
            return;
        }
        if (original_raster_worker_has_ready(&engine.raster))
            engine.dirty = true;
        if (engine.dirty && engine.display != EGL_NO_DISPLAY &&
            monotonic_ns() >= engine.next_frame_ns) {
            draw_frame(&engine);
            engine.next_frame_ns = monotonic_ns() + FRAME_PERIOD_NS;
        }
    }
}
