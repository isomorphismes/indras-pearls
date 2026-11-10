#include "original_raster_worker.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int failures;
#define CHECK(x) do { if (!(x)) { ++failures; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); } } while (0)

struct observed_frame {
    struct original_raster_worker *worker;
    int mutex_was_free;
    int count;
    size_t dark;
};

static bool record_frame(void *context, int width, int height, const uint8_t *rgba) {
    struct observed_frame *record = context;
    if (width != 17 || height != 9 || !rgba) return false;
    /* The upload path must NOT hold the mutex: a slow GL driver would
     * otherwise block new input and recreate the phone regression. */
    if (pthread_mutex_trylock(&record->worker->mutex) == 0) {
        ++record->mutex_was_free;
        pthread_mutex_unlock(&record->worker->mutex);
    }
    ++record->count;
    record->dark = 0;
    for (size_t i = 0; i < (size_t)width * (size_t)height; ++i) {
        if (rgba[i * 4u] == 0 && rgba[i * 4u + 3u] == 255) ++record->dark;
    }
    return record->dark > 0;
}

int main(void) {
    struct original_raster_worker worker = {0};
    CHECK(original_raster_worker_start(&worker, 17, 9, 10000u, NULL, NULL));
    if (!worker.started) return EXIT_FAILURE;
    uint64_t target_revision = 0;
    struct original_kleinian_complex ta = {2.2, 0.0};
    struct original_kleinian_complex tb = {2.2, 0.0};
    for (int i = 0; i < 200; ++i) {
        ta.re = 2.2 + (double)(i % 3) * 0.001;
        target_revision = original_raster_worker_request(&worker, ta, tb, 1);
    }
    CHECK(target_revision == 200u);
    bool latest_observed = false;
    struct observed_frame observed = {0};
    observed.worker = &worker;
    struct timespec pause = {0, 10000000L};
    for (int attempt = 0; attempt < 1000; ++attempt) {
        struct original_raster_result result = {0};
        if (original_raster_worker_consume(
                &worker, record_frame, &observed, &result)) {
            CHECK(result.revision <= target_revision);
            CHECK(result.points == 10000u);
            CHECK(result.rgb_hash != 0);
            if (result.revision == target_revision) {
                CHECK(fabs(result.ta.re - ta.re) < 1.0e-12);
                latest_observed = true;
                break;
            }
        }
        nanosleep(&pause, NULL);
    }
    CHECK(latest_observed); /* pending movements must never lose the last */
    CHECK(observed.count > 0);
    CHECK(observed.dark > 0);
    CHECK(observed.mutex_was_free == observed.count);
    original_raster_worker_stop(&worker);
    CHECK(!worker.started);
    if (failures) return EXIT_FAILURE;
    puts("bounded raster worker: latest request and buffer ownership PASS");
    return EXIT_SUCCESS;
}
