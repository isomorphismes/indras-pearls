#include "renderer_packet.h"

#include "mobius_math.h"

static int store_complex(
    float output[static RENDERER_PACKET_FLOAT_COUNT],
    int offset,
    struct complex_number value
) {
    const float real = (float)value.real;
    const float imaginary = (float)value.imaginary;
    if (!__builtin_isfinite(real) || !__builtin_isfinite(imaginary)) {
        return 0;
    }
    output[offset] = real;
    output[offset + 1] = imaginary;
    return 1;
}

int symmetric_classical_renderer_packet(
    float radius,
    float output[static RENDERER_PACKET_FLOAT_COUNT]
) {
    if (output == 0 || !__builtin_isfinite(radius) || radius <= 0.0f) {
        return RENDERER_PACKET_INVALID_PARAMETER;
    }

    struct marked_rank_two_group group;
    const enum family_domain_status family_status =
        symmetric_classical_family_make((double)radius, &group);
    if (family_status == FAMILY_PARAMETER_INVALID) {
        return RENDERER_PACKET_INVALID_PARAMETER;
    }
    if (family_status == FAMILY_NUMERICALLY_UNREPRESENTABLE) {
        return RENDERER_PACKET_NUMERIC_FAILURE;
    }
    if (family_status != FAMILY_DOMAIN_INTERIOR) {
        return RENDERER_PACKET_OUTSIDE_CLASSICAL_DOMAIN;
    }

    struct classical_circle_presentation presentation;
    if (!classical_circle_presentation_derive(
            group,
            1.0e-12,
            1.0e-12,
            &presentation
        ) ||
        presentation.status != PRESENTATION_VALID) {
        return RENDERER_PACKET_INVALID_PRESENTATION;
    }

    float packet[RENDERER_PACKET_FLOAT_COUNT];
    for (int region = 0; region < LIMIT_SET_REGION_COUNT; ++region) {
        const int complex_offset = region * RENDERER_COMPLEX_COMPONENT_COUNT;
        const struct affine_isometric_circle circle = presentation.circle[region];
        const float circle_radius = (float)circle.radius;
        const float radius_squared = circle_radius * circle_radius;
        if (!__builtin_isfinite(circle_radius) ||
            circle_radius <= 0.0f ||
            !__builtin_isfinite(radius_squared)) {
            return RENDERER_PACKET_NUMERIC_FAILURE;
        }

        if (!store_complex(
                packet,
                RENDERER_PACKET_CIRCLE_CENTER_OFFSET + complex_offset,
                circle.center
            ) ||
            !store_complex(
                packet,
                RENDERER_PACKET_A_OFFSET + complex_offset,
                presentation.exit_map[region].a
            ) ||
            !store_complex(
                packet,
                RENDERER_PACKET_B_OFFSET + complex_offset,
                presentation.exit_map[region].b
            ) ||
            !store_complex(
                packet,
                RENDERER_PACKET_C_OFFSET + complex_offset,
                presentation.exit_map[region].c
            ) ||
            !store_complex(
                packet,
                RENDERER_PACKET_D_OFFSET + complex_offset,
                presentation.exit_map[region].d
            )) {
            return RENDERER_PACKET_NUMERIC_FAILURE;
        }

        packet[RENDERER_PACKET_CIRCLE_RADIUS_SQUARED_OFFSET + region] =
            radius_squared;
    }

    for (int index = 0; index < RENDERER_PACKET_FLOAT_COUNT; ++index) {
        output[index] = packet[index];
    }
    return RENDERER_PACKET_OK;
}
