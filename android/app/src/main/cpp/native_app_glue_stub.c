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
#include <string.h>

#include "group_state.h"
#include "limit_set_renderer.h"

#define LOG_TAG "IndrasPearls"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

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
    struct limit_set_group group;
    struct limit_set_renderer renderer;

    bool dragging;
    bool pinching;
    float last_x;
    float last_y;
    float last_span;
    bool dirty;
};

static void terminate_display(struct engine *engine);

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
        &engine->group,
        engine->camera.center_x,
        engine->camera.center_y,
        engine->camera.scale,
        engine->width,
        engine->height
    );

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
        case AMOTION_EVENT_ACTION_DOWN:
            engine->dragging = pointer_count == 1;
            engine->pinching = false;
            engine->last_x = AMotionEvent_getX(event, 0);
            engine->last_y = AMotionEvent_getY(event, 0);
            return 1;

        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            if (pointer_count >= 2) {
                engine->dragging = false;
                engine->pinching = true;
                engine->last_span = pointer_span(event);
            }
            return 1;

        case AMOTION_EVENT_ACTION_MOVE:
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
            engine->pinching = false;
            engine->dragging = false;
            return 1;

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            engine->dragging = false;
            engine->pinching = false;
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
    initialize_bundled_limit_set_group(&engine.group);
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
