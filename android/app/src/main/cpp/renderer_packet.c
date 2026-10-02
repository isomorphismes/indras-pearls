#include "renderer_packet.h"

#include "mobius_math.h"

_Static_assert(INDRAS_CIRCLE_COUNT == LIMIT_SET_REGION_COUNT,
               "renderer packet and mathematical circle counts must match");

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

static int renderer_packet_from_group(
    struct marked_rank_two_group group,
    float output[RENDERER_PACKET_FLOAT_COUNT]
) {
    struct classical_circle_presentation presentation;
    if (output == 0 ||
        !classical_circle_presentation_derive(
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

int symmetric_classical_renderer_packet(
    float radius,
    float output[RENDERER_PACKET_FLOAT_COUNT]
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

    return renderer_packet_from_group(group, output);
}

static int parameter_value_is_valid(float real, float imaginary) {
    if (!__builtin_isfinite(real) || !__builtin_isfinite(imaginary)) {
        return 0;
    }
    const double magnitude_squared =
        (double)real * (double)real + (double)imaginary * (double)imaginary;
    const double limit = (double)SCHOTTKY_PARAMETER_LIMIT + 1.0e-6;
    return magnitude_squared <= limit * limit;
}

static double minimum4(double a, double b, double c, double d) {
    double minimum = a < b ? a : b;
    minimum = minimum < c ? minimum : c;
    return minimum < d ? minimum : d;
}

static struct mobius_transformation circle_pair_generator(
    struct complex_number positive_center,
    double radius,
    double phase
) {
    const struct complex_number phase_unit =
        complex_make(__builtin_cos(phase), __builtin_sin(phase));
    const struct complex_number c =
        complex_divide(phase_unit, complex_make(radius, 0.0));
    const struct complex_number a = complex_multiply(c, positive_center);
    const struct complex_number d = a;
    const struct complex_number ad = complex_multiply(a, d);
    const struct complex_number b =
        complex_divide(
            complex_subtract(ad, complex_make(1.0, 0.0)),
            c
        );

    return (struct mobius_transformation){
        .a = a,
        .b = b,
        .c = c,
        .d = d,
    };
}

int schottky_three_disk_renderer_packet(
    const float controls_cartesian[SCHOTTKY_PARAMETER_FLOAT_COUNT],
    float output[RENDERER_PACKET_FLOAT_COUNT]
) {
    if (controls_cartesian == 0 || output == 0) {
        return RENDERER_PACKET_INVALID_PARAMETER;
    }

    for (int index = 0; index < SCHOTTKY_PARAMETER_COUNT; ++index) {
        if (!parameter_value_is_valid(
                controls_cartesian[index * 2],
                controls_cartesian[index * 2 + 1]
            )) {
            return RENDERER_PACKET_INVALID_PARAMETER;
        }
    }

    const double center_scale = 0.35;
    const struct complex_number u0 = complex_make(
        (double)controls_cartesian[0],
        (double)controls_cartesian[1]
    );
    const struct complex_number u1 = complex_make(
        (double)controls_cartesian[2],
        (double)controls_cartesian[3]
    );

    const struct complex_number p = complex_add(
        complex_make(1.0, 0.0),
        complex_multiply(complex_make(center_scale, 0.0), u0)
    );
    const struct complex_number q = complex_add(
        complex_make(0.0, 1.0),
        complex_multiply(complex_make(center_scale, 0.0), u1)
    );

    const double pair_a_distance = 2.0 * complex_magnitude(p);
    const double pair_b_distance = 2.0 * complex_magnitude(q);
    const double cross_minus_distance =
        complex_magnitude(complex_subtract(p, q));
    const double cross_plus_distance =
        complex_magnitude(complex_add(p, q));
    const double minimum_center_distance = minimum4(
        pair_a_distance,
        pair_b_distance,
        cross_minus_distance,
        cross_plus_distance
    );

    if (!__builtin_isfinite(minimum_center_distance) ||
        minimum_center_distance <= 0.0) {
        return RENDERER_PACKET_NUMERIC_FAILURE;
    }

    /*
     * 0.7/sqrt(2). It is strictly below 1/2, so choosing
     * radius = factor * minimum_center_distance proves every pair of the four
     * circles is disjoint. At u0=u1=0 the minimum distance is sqrt(2), hence
     * the historical radius 0.7 is reproduced exactly up to binary64 roundoff.
     */
    const double radius_factor = 0.4949747468305832;
    const double radius = radius_factor * minimum_center_distance;
    if (!__builtin_isfinite(radius) || radius <= 0.0) {
        return RENDERER_PACKET_NUMERIC_FAILURE;
    }

    const double phase_scale = 3.342119844244461;
    const double phase_a =
        phase_scale * (double)controls_cartesian[4];
    const double phase_b =
        phase_scale * (double)controls_cartesian[5];

    const struct marked_rank_two_group group = {
        .A = circle_pair_generator(p, radius, phase_a),
        .B = circle_pair_generator(q, radius, phase_b),
    };

    return renderer_packet_from_group(group, output);
}
