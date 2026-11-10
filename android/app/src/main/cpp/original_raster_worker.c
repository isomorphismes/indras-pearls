#include "original_raster_worker.h"
#include "original_android_raster.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Fixed memory ceiling. At 720 x 1600, three RGBA buffers total
 * 13.8 MB; transient gesture traffic allocates nothing. */
#define MAX_RASTER_PIXELS (4u * 1024u * 1024u)

static uint64_t monotonic_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return 0;
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}

static uint64_t rgb_hash(const uint8_t *rgba, size_t pixels) {
    uint64_t h = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < pixels; ++i) {
        for (size_t channel = 0; channel < 3u; ++channel) {
            h ^= rgba[4u * i + channel];
            h *= UINT64_C(1099511628211);
        }
    }
    return h;
}

static size_t dark_pixel_count(const uint8_t *rgba, size_t pixels) {
    size_t n = 0;
    for (size_t i = 0; i < pixels; ++i) {
        const size_t off = 4u * i;
        if (rgba[off] < 32u && rgba[off + 1u] < 32u &&
            rgba[off + 2u] < 32u) ++n;
    }
    return n;
}

static void *raster_thread(void *opaque) {
    struct original_raster_worker *w = opaque;
    uint64_t handled = 0;
    for (;;) {
        pthread_mutex_lock(&w->mutex);
        while (!w->stopping && w->requested_revision == handled) {
            pthread_cond_wait(&w->changed, &w->mutex);
        }
        if (w->stopping) {
            pthread_mutex_unlock(&w->mutex);
            return NULL;
        }
        handled = w->requested_revision;
        const struct original_kleinian_complex ta = w->requested_a;
        const struct original_kleinian_complex tb = w->requested_b;
        const uint64_t input_ns = w->input_ns;
        pthread_mutex_unlock(&w->mutex);

        const uint64_t start_ns = monotonic_ns();
        size_t point_count = 0;
        bool ok = original_kleinian_generate_points_from_traces(
            ta, tb, w->requested_points,
            w->queue, w->point_capacity,
            w->points, w->point_capacity,
            &point_count
        );
        if (ok) {
            ok = original_android_rasterize_rgba(
                w->points, point_count, w->width, w->height,
                w->building_rgba, w->bytes
            );
        }
        const uint64_t hash = ok ? rgb_hash(w->building_rgba, w->width * w->height) : 0;
        const size_t dark = ok ? dark_pixel_count(w->building_rgba, w->width * w->height) : 0;
        const uint64_t finished_ns = monotonic_ns();
        pthread_mutex_lock(&w->mutex);
        if (!w->stopping) {
            if (ok) {
                /* An unconsumed older frame is replaced. No queue grows.
                 * The upload slot is separate: GL never holds this mutex. */
                uint8_t *old_ready = w->ready_rgba;
                w->ready_rgba = w->building_rgba;
                w->building_rgba = old_ready;
                w->ready_result.revision = handled;
                w->ready_result.input_ns = input_ns;
                w->ready_result.build_ns = finished_ns - start_ns;
                w->ready_result.points = point_count;
                w->ready_result.rgb_hash = hash;
                w->ready_result.dark_pixels = dark;
                w->ready_result.ta = ta;
                w->ready_result.tb = tb;
                w->ready = true;
                w->failed = false;
            } else {
                w->failed = true;
                w->failed_revision = handled;
            }
        }
        const bool notify = !w->stopping;
        pthread_mutex_unlock(&w->mutex);
        if (notify && w->wake != NULL) w->wake(w->wake_context);
        /* If requests arrived while building, the next iteration snapshots
         * only the latest one. A completed intermediate may still be shown. */
    }
}

bool original_raster_worker_start(
    struct original_raster_worker *w,
    size_t width, size_t height, size_t requested_points,
    void (*wake)(void *), void *wake_context
) {
    if (w == NULL || width == 0 || height == 0 ||
        width > SIZE_MAX ÷ height ||
        width * height > MAX_RASTER_PIXELS || requested_points == 0) {
        return false;
    }
    const size_t capacity = original_kleinian_point_capacity(requested_points);
    if (!capacity || capacity > SIZE_MAX ÷ sizeof(*w->queue) ||
        capacity > SIZE_MAX ÷ sizeof(*w->points)) {
        return false;
    }
    memset(w, 0, sizeof(*w));
    w->width = width;
    w->height = height;
    w->bytes = width * height * 4u;
    w->requested_points = requested_points;
    w->point_capacity = capacity;
    w->wake = wake;
    w->wake_context = wake_context;
    w->queue = calloc(capacity, sizeof(*w->queue));
    w->points = calloc(capacity, sizeof(*w->points));
    w->building_rgba = malloc(w->bytes);
    w->ready_rgba = malloc(w->bytes);
    w->upload_rgba = malloc(w->bytes);
    if (!w->queue || !w->points || !w->building_rgba ||
        !w->ready_rgba || !w->upload_rgba) {
        goto allocation_failed;
    }
    if (pthread_mutex_init(&w->mutex, NULL) != 0) goto allocation_failed;
    if (pthread_cond_init(&w->changed, NULL) != 0) {
        pthread_mutex_destroy(&w->mutex);
        goto allocation_failed;
    }
    if (pthread_create(&w->thread, NULL, raster_thread, w) != 0) {
        pthread_cond_destroy(&w->changed);
        pthread_mutex_destroy(&w->mutex);
        goto allocation_failed;
    }
    w->started = true;
    return true;

allocation_failed:
    free(w->upload_rgba);
    free(w->ready_rgba);
    free(w->building_rgba);
    free(w->points);
    free(w->queue);
    memset(w, 0, sizeof(*w));
    return false;
}

void original_raster_worker_stop(struct original_raster_worker *w) {
    if (!w || !w->started) return;
    pthread_mutex_lock(&w->mutex);
    w->stopping = true;
    pthread_cond_signal(&w->changed);
    pthread_mutex_unlock(&w->mutex);
    pthread_join(w->thread, NULL);
    pthread_cond_destroy(&w->changed);
    pthread_mutex_destroy(&w->mutex);
    free(w->upload_rgba);
    free(w->ready_rgba);
    free(w->building_rgba);
    free(w->points);
    free(w->queue);
    memset(w, 0, sizeof(*w));
}

uint64_t original_raster_worker_request(
    struct original_raster_worker *w,
    struct original_kleinian_complex ta,
    struct original_kleinian_complex tb,
    uint64_t input_ns
) {
    if (!w || !w->started) return 0;
    pthread_mutex_lock(&w->mutex);
    if (w->stopping || w->requested_revision == UINT64_MAX) {
        pthread_mutex_unlock(&w->mutex);
        return 0;
    }
    w->requested_a = ta;
    w->requested_b = tb;
    w->input_ns = input_ns;
    const uint64_t revision = ++w->requested_revision;
    pthread_cond_signal(&w->changed);
    pthread_mutex_unlock(&w->mutex);
    return revision;
}

bool original_raster_worker_has_ready(struct original_raster_worker *w) {
    if (!w || !w->started) return false;
    pthread_mutex_lock(&w->mutex);
    const bool ready = w->ready;
    pthread_mutex_unlock(&w->mutex);
    return ready;
}

bool original_raster_worker_consume(
    struct original_raster_worker *w,
    bool (*upload)(void *, int, int, const uint8_t *),
    void *ctx,
    struct original_raster_result *result
) {
    if (!w || !w->started || !upload) return false;
    pthread_mutex_lock(&w->mutex);
    if (!w->ready) {
        pthread_mutex_unlock(&w->mutex);
        return false;
    }
    if (result) *result = w->ready_result;
    uint8_t *previous_upload = w->upload_rgba;
    w->upload_rgba = w->ready_rgba;
    w->ready_rgba = previous_upload;
    w->ready = false;
    const int width = (int)w->width;
    const int height = (int)w->height;
    pthread_mutex_unlock(&w->mutex);
    /* GLES upload and its driver stalls must not block input requests. */
    return upload(ctx, width, height, w->upload_rgba);
}

uint64_t original_raster_worker_last_failure(struct original_raster_worker *w) {
    if (!w || !w->started) return 0;
    pthread_mutex_lock(&w->mutex);
    const uint64_t failed = w->failed ? w->failed_revision : 0;
    pthread_mutex_unlock(&w->mutex);
    return failed;
}
