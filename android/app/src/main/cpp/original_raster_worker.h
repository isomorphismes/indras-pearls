#ifndef INDRAS_PEARLS_ORIGINAL_RASTER_WORKER_H
#define INDRAS_PEARLS_ORIGINAL_RASTER_WORKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "original_kleinian.h"

/* A three-buffer latest-state worker: build, ready, and upload storage.
 * No GL operations or Android input APIs run on the worker thread.
 * The upload callback executes without holding the worker mutex. */
struct original_raster_result {
    uint64_t revision;
    uint64_t input_ns;
    uint64_t build_ns;
    size_t points;
    uint64_t rgb_hash;
    size_t dark_pixels;
    struct original_kleinian_complex ta;
    struct original_kleinian_complex tb;
};

struct original_raster_worker {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    bool started;
    bool stopping;
    bool ready;
    bool failed;
    size_t width;
    size_t height;
    size_t bytes;
    size_t point_capacity;
    size_t requested_points;
    uint64_t requested_revision;
    uint64_t failed_revision;
    uint64_t input_ns;
    struct original_kleinian_complex requested_a;
    struct original_kleinian_complex requested_b;
    struct original_raster_result ready_result;
    struct original_kleinian_queue_item *queue;
    struct original_kleinian_complex *points;
    uint8_t *building_rgba;
    uint8_t *ready_rgba;
    uint8_t *upload_rgba;
    void (*wake)(void *);
    void *wake_context;
};

bool original_raster_worker_start(
    struct original_raster_worker *worker,
    size_t width, size_t height, size_t requested_points,
    void (*wake)(void *), void *wake_context
);
void original_raster_worker_stop(struct original_raster_worker *worker);
uint64_t original_raster_worker_request(
    struct original_raster_worker *worker,
    struct original_kleinian_complex ta,
    struct original_kleinian_complex tb,
    uint64_t input_ns
);
bool original_raster_worker_has_ready(struct original_raster_worker *worker);
bool original_raster_worker_consume(
    struct original_raster_worker *worker,
    bool (*upload)(void *ctx, int width, int height, const uint8_t *rgba),
    void *ctx,
    struct original_raster_result *result
);
uint64_t original_raster_worker_last_failure(struct original_raster_worker *worker);

#endif
