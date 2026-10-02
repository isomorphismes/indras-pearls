#include "renderer_packet.h"

__attribute__((visibility("default")))
int indras_ick_renderer_packet_probe(void) {
    float packet[RENDERER_PACKET_FLOAT_COUNT];
    if (symmetric_classical_renderer_packet(0.7f, packet) != RENDERER_PACKET_OK) {
        return 1;
    }

    const float tolerance = 2.0e-5f;
    float error = packet[RENDERER_PACKET_CIRCLE_CENTER_OFFSET] + 1.0f;
    if (error < 0.0f) {
        error = -error;
    }
    if (error > tolerance) {
        return 2;
    }

    error = packet[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET] - 0.49f;
    if (error < 0.0f) {
        error = -error;
    }
    if (error > tolerance) {
        return 3;
    }

    float controls[SCHOTTKY_PARAMETER_FLOAT_COUNT] = {
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
    };
    float live[RENDERER_PACKET_FLOAT_COUNT];
    if (schottky_three_disk_renderer_packet(controls, live) !=
        RENDERER_PACKET_OK) {
        return 4;
    }

    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        float delta = live[index] - packet[index];
        if (delta < 0.0f) {
            delta = -delta;
        }
        if (delta > 3.0e-5f) {
            return 5;
        }
    }

    controls[0] = 0.45f;
    if (schottky_three_disk_renderer_packet(controls, live) !=
        RENDERER_PACKET_OK) {
        return 6;
    }

    int changed = 0;
    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        float delta = live[index] - packet[index];
        if (delta < 0.0f) {
            delta = -delta;
        }
        if (delta > 1.0e-6f) {
            changed = 1;
            break;
        }
    }
    if (!changed) {
        return 7;
    }

    controls[0] = 0.0f;
    controls[4] = 0.45f;
    if (schottky_three_disk_renderer_packet(controls, live) !=
        RENDERER_PACKET_OK) {
        return 8;
    }

    changed = 0;
    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        float delta = live[index] - packet[index];
        if (delta < 0.0f) {
            delta = -delta;
        }
        if (delta > 1.0e-6f) {
            changed = 1;
            break;
        }
    }
    if (!changed) {
        return 9;
    }

    if (symmetric_classical_renderer_packet(0.72f, packet) !=
        RENDERER_PACKET_OUTSIDE_CLASSICAL_DOMAIN) {
        return 10;
    }
    return 0;
}
