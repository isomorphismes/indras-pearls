#include "mobius_math.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition, label) do { \
    assertions += 1; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", label, __LINE__); \
        failures += 1; \
    } \
} while (0)

static int failures;
static int assertions;
static const double algebra_tolerance = 1.0e-12;
static const double projective_tolerance = 2.0e-11;
static const double circle_tolerance = 1.0e-12;

static int near(double left, double right, double tolerance) {
    return fabs(left - right) <= tolerance;
}

static int complex_near(struct complex_number left, struct complex_number right, double tolerance) {
    return complex_magnitude(complex_subtract(left, right)) <= tolerance;
}

static double matrix_max_difference(struct mobius_transformation left,
                                    struct mobius_transformation right) {
    const double differences[] = {
        complex_magnitude(complex_subtract(left.a, right.a)),
        complex_magnitude(complex_subtract(left.b, right.b)),
        complex_magnitude(complex_subtract(left.c, right.c)),
        complex_magnitude(complex_subtract(left.d, right.d)),
    };
    double maximum = differences[0];
    for (size_t index = 1; index < sizeof(differences) / sizeof(differences[0]); ++index) {
        if (differences[index] > maximum) {
            maximum = differences[index];
        }
    }
    return maximum;
}

static struct complex_number affine_apply(struct mobius_transformation matrix,
                                          struct complex_number point) {
    const struct complex_number numerator = complex_add(
        complex_multiply(matrix.a, point), matrix.b);
    const struct complex_number denominator = complex_add(
        complex_multiply(matrix.c, point), matrix.d);
    return complex_divide(numerator, denominator);
}

static double circle_equation_value(struct mobius_transformation matrix,
                                    struct complex_number point) {
    return complex_magnitude(complex_add(complex_multiply(matrix.c, point), matrix.d));
}

static void test_canonical_group_and_trace_identities(void) {
    struct marked_rank_two_group group;
    CHECK(symmetric_classical_family_make(0.7, &group) == FAMILY_DOMAIN_INTERIOR,
          "r=0.7 lies inside the open family domain");
    CHECK(mobius_is_sl2(group.A, algebra_tolerance), "det(A)=1");
    CHECK(mobius_is_sl2(group.B, algebra_tolerance), "det(B)=1");

    struct mobius_transformation inverse_A;
    struct mobius_transformation inverse_B;
    CHECK(mobius_inverse(group.A, &inverse_A), "derive A inverse");
    CHECK(mobius_inverse(group.B, &inverse_B), "derive B inverse");
    const struct mobius_transformation identity_A = mobius_multiply(group.A, inverse_A);
    const struct mobius_transformation identity_B = mobius_multiply(group.B, inverse_B);
    const struct complex_number one = complex_make(1.0, 0.0);
    CHECK(complex_near(identity_A.a, one, algebra_tolerance) &&
          complex_near(identity_A.b, complex_make(0.0, 0.0), algebra_tolerance) &&
          complex_near(identity_A.c, complex_make(0.0, 0.0), algebra_tolerance) &&
          complex_near(identity_A.d, one, algebra_tolerance), "A * inverse(A) is identity");
    CHECK(complex_near(identity_B.a, one, algebra_tolerance) &&
          complex_near(identity_B.b, complex_make(0.0, 0.0), algebra_tolerance) &&
          complex_near(identity_B.c, complex_make(0.0, 0.0), algebra_tolerance) &&
          complex_near(identity_B.d, one, algebra_tolerance), "B * inverse(B) is identity");

    struct mobius_transformation raw = group.A;
    raw.a = complex_multiply(raw.a, complex_make(2.0, -3.0));
    raw.b = complex_multiply(raw.b, complex_make(2.0, -3.0));
    raw.c = complex_multiply(raw.c, complex_make(2.0, -3.0));
    raw.d = complex_multiply(raw.d, complex_make(2.0, -3.0));
    struct mobius_transformation normalized;
    CHECK(mobius_normalize_sl2(raw, &normalized), "normalize a nonzero projective matrix lift");
    CHECK(mobius_is_sl2(normalized, algebra_tolerance), "normalized determinant is one");
    struct projective_point normalized_action;
    struct projective_point original_action;
    const struct projective_point sample = projective_finite(complex_make(0.25, -0.75));
    CHECK(mobius_apply_projective(raw, sample, &original_action) &&
          mobius_apply_projective(normalized, sample, &normalized_action) &&
          projective_point_equal(original_action, normalized_action, projective_tolerance),
          "SL normalization preserves the projective action");

    const struct mobius_transformation product = mobius_multiply(group.A, group.B);
    const struct complex_number x = mobius_trace(group.A);
    const struct complex_number y = mobius_trace(group.B);
    const struct complex_number z = mobius_trace(product);
    const struct complex_number expected_ab_inverse = complex_subtract(complex_multiply(x, y), z);
    CHECK(complex_near(mobius_trace(mobius_multiply(group.A, inverse_B)),
                       expected_ab_inverse, projective_tolerance),
          "tr(A B^-1)=tr(A)tr(B)-tr(AB)");
    const struct complex_number fricke = complex_subtract(
        complex_add(complex_add(complex_multiply(x, x), complex_multiply(y, y)),
                    complex_multiply(z, z)),
        complex_add(complex_multiply(complex_multiply(x, y), z), complex_make(2.0, 0.0)));
    CHECK(complex_near(mobius_commutator_trace(group.A, group.B), fricke, projective_tolerance),
          "Fricke commutator trace identity");
    CHECK(complex_near(x, complex_make(20.0 / 7.0, 0.0), algebra_tolerance),
          "bundled trace of A");
    CHECK(complex_near(y, complex_make(0.0, 20.0 / 7.0), algebra_tolerance),
          "bundled trace of B");
    CHECK(complex_near(z, complex_make(-2.0, 200.0 / 49.0), projective_tolerance),
          "bundled trace of AB");
}

static void test_projective_action_and_composition(void) {
    struct marked_rank_two_group group;
    (void)symmetric_classical_family_make(0.7, &group);
    struct mobius_transformation inverse_A;
    CHECK(mobius_inverse(group.A, &inverse_A), "inverse available for action tests");
    const struct projective_point points[] = {
        projective_finite(complex_make(0.2, 0.4)),
        projective_finite(complex_make(-1.3, 0.1)),
        projective_infinity(),
    };
    const size_t point_count = sizeof(points) / sizeof(points[0]);
    for (size_t index = 0; index < point_count; ++index) {
        struct projective_point image;
        struct projective_point recovered;
        CHECK(mobius_apply_projective(group.A, points[index], &image) &&
              mobius_apply_projective(inverse_A, image, &recovered) &&
              projective_point_equal(points[index], recovered, projective_tolerance),
              "A inverse action recovers finite points and infinity");

        const struct mobius_transformation composition = mobius_multiply(group.A, group.B);
        struct projective_point sequential;
        struct projective_point composed;
        CHECK(mobius_apply_projective(group.B, points[index], &sequential) &&
              mobius_apply_projective(group.A, sequential, &sequential) &&
              mobius_apply_projective(composition, points[index], &composed) &&
              projective_point_equal(sequential, composed, projective_tolerance),
              "(A B)(p) equals A(B(p)) on CP1");
        if (!projective_point_is_infinity(points[index])) {
            CHECK(mobius_apply_projective(group.A, points[index], &image) &&
                  complex_near(image.numerator,
                               affine_apply(group.A, points[index].numerator),
                               projective_tolerance),
                  "projective action agrees with affine formula");
        }
    }

    const struct projective_point pole = projective_finite(complex_make(-1.0, 0.0));
    struct projective_point pole_image;
    CHECK(mobius_apply_projective(group.A, pole, &pole_image) &&
          projective_point_is_infinity(pole_image), "zero denominator maps exactly to infinity");
}

static void test_fixed_points_and_multipliers(void) {
    struct marked_rank_two_group group;
    (void)symmetric_classical_family_make(0.7, &group);
    const struct mobius_transformation matrices[] = {group.A, group.B};
    const struct complex_number expected[2][2] = {
        {{sqrt(51.0) / 10.0, 0.0}, {-sqrt(51.0) / 10.0, 0.0}},
        {{0.0, sqrt(149.0) / 10.0}, {0.0, -sqrt(149.0) / 10.0}},
    };
    for (size_t matrix_index = 0; matrix_index < 2; ++matrix_index) {
        struct mobius_fixed_points fixed_points;
        CHECK(mobius_fixed_points_compute(matrices[matrix_index], algebra_tolerance, &fixed_points),
              "compute two fixed-point diagnostics");
        for (size_t point_index = 0; point_index < 2; ++point_index) {
            struct projective_point image;
            CHECK(mobius_apply_projective(matrices[matrix_index],
                                          fixed_points.point[point_index].point, &image) &&
                  projective_point_equal(image, fixed_points.point[point_index].point,
                                         projective_tolerance),
                  "fixed point satisfies the projective fixed-point equation");
            CHECK(complex_near(fixed_points.point[point_index].point.numerator,
                               expected[matrix_index][point_index], 2.0e-11),
                  "fixed point agrees with the bundled closed form");
            const struct complex_number affine_denominator = complex_add(
                complex_multiply(matrices[matrix_index].c,
                                 fixed_points.point[point_index].point.numerator),
                matrices[matrix_index].d);
            const struct complex_number derivative = complex_divide(
                mobius_determinant(matrices[matrix_index]),
                complex_multiply(affine_denominator, affine_denominator));
            CHECK(complex_near(fixed_points.point[point_index].multiplier, derivative,
                               projective_tolerance),
                  "fixed-point multiplier equals the local Mobius derivative");
        }
    }
}

static void test_isometric_circles_and_validation(void) {
    const double radius = 0.7;
    struct marked_rank_two_group group;
    (void)symmetric_classical_family_make(radius, &group);
    struct classical_circle_presentation presentation;
    CHECK(classical_circle_presentation_derive(group, algebra_tolerance, circle_tolerance,
                                               &presentation),
          "derive four-circle classical presentation");
    CHECK(presentation.status == PRESENTATION_VALID, "r=0.7 circles form a valid presentation");
    const struct complex_number centers[4] = {
        {-1.0, 0.0}, {1.0, 0.0}, {0.0, -1.0}, {0.0, 1.0},
    };
    for (size_t index = 0; index < INDRAS_CIRCLE_COUNT; ++index) {
        const struct affine_isometric_circle circle = presentation.circle[index];
        CHECK(complex_near(circle.center, centers[index], algebra_tolerance),
              "derived isometric circle has the bundled center");
        CHECK(near(circle.radius, radius, algebra_tolerance), "derived circle radius is r");
        const struct complex_number boundary = complex_add(circle.center, complex_make(circle.radius, 0.0));
        CHECK(near(circle_equation_value(presentation.exit_map[index], boundary), 1.0,
                   projective_tolerance), "circle boundary satisfies |cz+d|=1");

        for (size_t sample_index = 0; sample_index < 8; ++sample_index) {
            const double angle = (acos(-1.0) * 2.0 * (double)sample_index) / 8.0;
            const struct complex_number sampled_boundary = complex_add(
                circle.center,
                complex_make(circle.radius * cos(angle), circle.radius * sin(angle)));
            CHECK(near(circle_equation_value(presentation.exit_map[index], sampled_boundary), 1.0,
                       projective_tolerance), "sampled boundary satisfies |cz+d|=1");
        }

        const struct complex_number interior = complex_add(circle.center, complex_make(circle.radius * 0.5, 0.0));
        struct projective_point image;
        CHECK(mobius_apply_projective(presentation.exit_map[index], projective_finite(interior), &image),
              "map an interior point projectively");
        const size_t paired_index = index ^ 1U;
        struct projective_point boundary_image;
        CHECK(mobius_apply_projective(presentation.exit_map[index], projective_finite(boundary),
                                      &boundary_image) &&
              !projective_point_is_infinity(boundary_image) &&
              near(circle_equation_value(presentation.exit_map[paired_index],
                                         boundary_image.numerator), 1.0,
                   projective_tolerance),
              "boundary maps to paired inverse boundary");
        CHECK(!projective_point_is_infinity(image) &&
              circle_equation_value(presentation.exit_map[paired_index], image.numerator) > 1.0,
              "paired map sends source-circle interior outside inverse circle");
    }
    CHECK(presentation.minimum_pair_gap > 0.0, "all circle interiors have a positive gap");
    for (size_t index = 0; index < INDRAS_CIRCLE_PAIR_COUNT; ++index) {
        CHECK(presentation.pair_relation[index] == CIRCLE_RELATION_DISJOINT,
              "each of six circle pairs is disjoint");
    }

    const struct affine_isometric_circle unit_left = {{0.0, 0.0}, 1.0};
    const struct affine_isometric_circle disjoint = {{3.0, 0.0}, 1.0};
    const struct affine_isometric_circle tangent = {{2.0, 0.0}, 1.0};
    const struct affine_isometric_circle overlap = {{1.5, 0.0}, 1.0};
    CHECK(circle_compare(unit_left, disjoint, circle_tolerance) == CIRCLE_RELATION_DISJOINT,
          "circle validation identifies disjoint pair");
    CHECK(circle_compare(unit_left, tangent, circle_tolerance) == CIRCLE_RELATION_TANGENT,
          "circle validation identifies tangent pair");
    CHECK(circle_compare(unit_left, overlap, circle_tolerance) == CIRCLE_RELATION_OVERLAPPING,
          "circle validation identifies overlapping pair");

    const struct mobius_transformation translation = {
        {1.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {1.0, 0.0},
    };
    struct affine_isometric_circle unsupported_circle;
    CHECK(mobius_derive_isometric_circle(translation, algebra_tolerance, &unsupported_circle) ==
              ISOMETRIC_CIRCLE_GENERALIZED_LINE,
          "c=0 is reported as a generalized circle through infinity");
    struct marked_rank_two_group line_group = {translation, group.B};
    struct classical_circle_presentation line_presentation;
    CHECK(classical_circle_presentation_derive(line_group, algebra_tolerance, circle_tolerance,
                                               &line_presentation) &&
          line_presentation.status == PRESENTATION_UNSUPPORTED_AFFINE_LINE,
          "the four-affine-circle renderer certificate reports its unsupported line case");
}

static void test_domain_reconstruction_and_continuity(void) {
    struct marked_rank_two_group invalid_group;
    CHECK(symmetric_classical_family_make(0.0, &invalid_group) ==
              FAMILY_PARAMETER_INVALID,
          "r=0 is rejected");
    CHECK(symmetric_classical_family_make(-0.1, &invalid_group) ==
              FAMILY_PARAMETER_INVALID,
          "negative r is rejected");
    CHECK(symmetric_classical_family_make(DBL_MIN, &invalid_group) ==
              FAMILY_NUMERICALLY_UNREPRESENTABLE,
          "positive r below binary64 construction range is distinct from domain invalidity");
    const double tangent_radius = 1.0 / sqrt(2.0);
    struct marked_rank_two_group boundary_group;
    CHECK(symmetric_classical_family_make(tangent_radius, &boundary_group) ==
              FAMILY_DOMAIN_TANGENT_BOUNDARY,
          "r=1/sqrt(2) is the tangent boundary");
    struct classical_circle_presentation boundary_presentation;
    CHECK(classical_circle_presentation_derive(boundary_group, algebra_tolerance,
                                               circle_tolerance, &boundary_presentation) &&
          boundary_presentation.status == PRESENTATION_TANGENT,
          "tangent family state is distinguished from interior validity");
    struct marked_rank_two_group overlap_group;
    CHECK(symmetric_classical_family_make(0.72, &overlap_group) == FAMILY_DOMAIN_OVERLAPPING,
          "r above 1/sqrt(2) is outside the family domain");
    struct classical_circle_presentation overlap_presentation;
    CHECK(classical_circle_presentation_derive(overlap_group, algebra_tolerance,
                                               circle_tolerance, &overlap_presentation) &&
          overlap_presentation.status == PRESENTATION_OVERLAPPING,
          "overlapping family state is rejected as a classical presentation");
    const double interior_radii[] = {0.1, 0.4, 0.7, 0.7070};
    for (size_t index = 0; index < sizeof(interior_radii) / sizeof(interior_radii[0]); ++index) {
        struct marked_rank_two_group group;
        CHECK(symmetric_classical_family_make(interior_radii[index], &group) == FAMILY_DOMAIN_INTERIOR,
              "representative radius belongs to the open interval");
    }

    struct marked_rank_two_group bundled;
    CHECK(symmetric_classical_family_make(0.7, &bundled) == FAMILY_DOMAIN_INTERIOR,
          "construct exact bundled radius family member");
    const double ten_sevenths = 10.0 / 7.0;
    const double fifty_one_seventieths = 51.0 / 70.0;
    const double one_hundred_forty_nine_seventieths = 149.0 / 70.0;
    CHECK(complex_near(bundled.A.a, complex_make(ten_sevenths, 0.0), algebra_tolerance) &&
          complex_near(bundled.A.b, complex_make(fifty_one_seventieths, 0.0), algebra_tolerance) &&
          complex_near(bundled.A.c, complex_make(ten_sevenths, 0.0), algebra_tolerance) &&
          complex_near(bundled.A.d, complex_make(ten_sevenths, 0.0), algebra_tolerance),
          "r=0.7 reconstructs existing A matrix coefficients");
    CHECK(complex_near(bundled.B.a, complex_make(0.0, ten_sevenths), algebra_tolerance) &&
          complex_near(bundled.B.b, complex_make(-one_hundred_forty_nine_seventieths, 0.0), algebra_tolerance) &&
          complex_near(bundled.B.c, complex_make(ten_sevenths, 0.0), algebra_tolerance) &&
          complex_near(bundled.B.d, complex_make(0.0, ten_sevenths), algebra_tolerance),
          "r=0.7 reconstructs existing B matrix coefficients");
    struct mobius_transformation inverse_A;
    struct mobius_transformation inverse_B;
    CHECK(mobius_inverse(bundled.A, &inverse_A) && mobius_inverse(bundled.B, &inverse_B),
          "derive bundled inverse matrices");
    CHECK(complex_near(inverse_A.a, complex_make(ten_sevenths, 0.0), algebra_tolerance) &&
          complex_near(inverse_A.b, complex_make(-fifty_one_seventieths, 0.0), algebra_tolerance) &&
          complex_near(inverse_A.c, complex_make(-ten_sevenths, 0.0), algebra_tolerance) &&
          complex_near(inverse_A.d, complex_make(ten_sevenths, 0.0), algebra_tolerance),
          "derived A inverse reproduces existing renderer coefficients");
    CHECK(complex_near(inverse_B.a, complex_make(0.0, ten_sevenths), algebra_tolerance) &&
          complex_near(inverse_B.b, complex_make(one_hundred_forty_nine_seventieths, 0.0), algebra_tolerance) &&
          complex_near(inverse_B.c, complex_make(-ten_sevenths, 0.0), algebra_tolerance) &&
          complex_near(inverse_B.d, complex_make(0.0, ten_sevenths), algebra_tolerance),
          "derived B inverse reproduces existing renderer coefficients");

    struct marked_rank_two_group nearby;
    CHECK(symmetric_classical_family_make(0.700001, &nearby) == FAMILY_DOMAIN_INTERIOR,
          "nearby family member remains valid");
    CHECK(matrix_max_difference(bundled.A, nearby.A) < 1.0e-5 &&
          matrix_max_difference(bundled.B, nearby.B) < 1.0e-5,
          "small radius change makes a small matrix change");
    struct classical_circle_presentation bundled_presentation;
    struct classical_circle_presentation nearby_presentation;
    CHECK(classical_circle_presentation_derive(bundled, algebra_tolerance, circle_tolerance,
                                               &bundled_presentation) &&
          classical_circle_presentation_derive(nearby, algebra_tolerance, circle_tolerance,
                                               &nearby_presentation),
          "derive continuous circle presentations");
    CHECK(complex_magnitude(complex_subtract(bundled_presentation.circle[0].center,
                                             nearby_presentation.circle[0].center)) < 1.0e-5 &&
          fabs(bundled_presentation.circle[0].radius - nearby_presentation.circle[0].radius) < 2.0e-6 &&
          nearby_presentation.minimum_pair_gap > 0.0,
          "circle data vary continuously while the presentation remains valid");
    struct mobius_fixed_points bundled_fixed;
    struct mobius_fixed_points nearby_fixed;
    CHECK(mobius_fixed_points_compute(bundled.A, algebra_tolerance, &bundled_fixed) &&
          mobius_fixed_points_compute(nearby.A, algebra_tolerance, &nearby_fixed),
          "compute nearby fixed points");
    CHECK(projective_point_equal(bundled_fixed.point[0].point, nearby_fixed.point[0].point,
                                 2.0e-5),
          "fixed points vary continuously away from the domain boundary");
}

int main(void) {
    test_canonical_group_and_trace_identities();
    test_projective_action_and_composition();
    test_fixed_points_and_multipliers();
    test_isometric_circles_and_validation();
    test_domain_reconstruction_and_continuity();
    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("PASS: %d deterministic mathematical assertions\n", assertions);
    return EXIT_SUCCESS;
}
