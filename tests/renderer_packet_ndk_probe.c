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

    if (symmetric_classical_renderer_packet(0.72f, packet) !=
        RENDERER_PACKET_OUTSIDE_CLASSICAL_DOMAIN) {
        return 4;
    }
    return 0;
}
