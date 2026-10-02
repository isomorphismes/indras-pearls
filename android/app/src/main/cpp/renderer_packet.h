#ifndef INDRAS_PEARLS_RENDERER_PACKET_H
#define INDRAS_PEARLS_RENDERER_PACKET_H

#define LIMIT_SET_REGION_COUNT 4
#define RENDERER_COMPLEX_COMPONENT_COUNT 2

#define SCHOTTKY_PARAMETER_COUNT 3
#define SCHOTTKY_PARAMETER_COMPONENT_COUNT 2
#define SCHOTTKY_PARAMETER_FLOAT_COUNT \
    (SCHOTTKY_PARAMETER_COUNT * SCHOTTKY_PARAMETER_COMPONENT_COUNT)
#define SCHOTTKY_PARAMETER_LIMIT 0.94f

#define RENDERER_PACKET_CIRCLE_CENTER_OFFSET 0
#define RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET \
    (RENDERER_PACKET_CIRCLE_CENTER_OFFSET + LIMIT_SET_REGION_COUNT * RENDERER_COMPLEX_COMPONENT_COUNT)
#define RENDERER_PACKET_A_OFFSET \
    (RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET + LIMIT_SET_REGION_COUNT)
#define RENDERER_PACKET_B_OFFSET \
    (RENDERER_PACKET_A_OFFSET + LIMIT_SET_REGION_COUNT * RENDERER_COMPLEX_COMPONENT_COUNT)
#define RENDERER_PACKET_C_OFFSET \
    (RENDERER_PACKET_B_OFFSET + LIMIT_SET_REGION_COUNT * RENDERER_COMPLEX_COMPONENT_COUNT)
#define RENDERER_PACKET_D_OFFSET \
    (RENDERER_PACKET_C_OFFSET + LIMIT_SET_REGION_COUNT * RENDERER_COMPLEX_COMPONENT_COUNT)
#define RENDERER_PACKET_FLOAT_COUNT \
    (RENDERER_PACKET_D_OFFSET + LIMIT_SET_REGION_COUNT * RENDERER_COMPLEX_COMPONENT_COUNT)

enum renderer_packet_status {
    RENDERER_PACKET_OK = 0,
    RENDERER_PACKET_INVALID_PARAMETER,
    RENDERER_PACKET_OUTSIDE_CLASSICAL_DOMAIN,
    RENDERER_PACKET_INVALID_PRESENTATION,
    RENDERER_PACKET_NUMERIC_FAILURE
};

/*
 * Compiler-neutral Android boundaries. Only ordinary scalar/array C data
 * crosses between ICK-compiled mathematics and NDK-compiled application code.
 */
int symmetric_classical_renderer_packet(
    float radius,
    float output[RENDERER_PACKET_FLOAT_COUNT]
);

/*
 * Three live complex controls, stored as
 *   [u0.re, u0.im, u1.re, u1.im, u2.re, u2.im].
 *
 * u0 moves the antipodal A-circle pair around ±1.
 * u1 moves the antipodal B-circle pair around ±i.
 * u2.re and u2.im change the phase of the two generator pairings.
 *
 * The common circle radius is derived from the minimum separation of the four
 * centers with a fixed factor below one half, so every accepted state remains
 * a disjoint classical four-circle presentation. Zero reproduces the bundled
 * r=0.7 group.
 */
int schottky_three_disk_renderer_packet(
    const float controls_cartesian[SCHOTTKY_PARAMETER_FLOAT_COUNT],
    float output[RENDERER_PACKET_FLOAT_COUNT]
);

#endif
