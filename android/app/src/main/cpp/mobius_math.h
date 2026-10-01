#ifndef INDRAS_PEARLS_MOBIUS_MATH_H
#define INDRAS_PEARLS_MOBIUS_MATH_H

#include <stdbool.h>
#include <stddef.h>

#define INDRAS_CIRCLE_COUNT 4
#define INDRAS_CIRCLE_PAIR_COUNT 6

struct complex_number {
    double real;
    double imaginary;
};

struct mobius_transformation {
    struct complex_number a;
    struct complex_number b;
    struct complex_number c;
    struct complex_number d;
};

/* A projective point [numerator:denominator] in CP^1. */
struct projective_point {
    struct complex_number numerator;
    struct complex_number denominator;
};

struct marked_rank_two_group {
    struct mobius_transformation A;
    struct mobius_transformation B;
};

struct fixed_point_diagnostic {
    struct projective_point point;
    /* Local derivative at point, computed as det(M) / eigenvalue(M, point)^2. */
    struct complex_number multiplier;
};

struct mobius_fixed_points {
    struct fixed_point_diagnostic point[2];
};

struct affine_isometric_circle {
    struct complex_number center;
    double radius;
};

enum circle_relation {
    CIRCLE_RELATION_DISJOINT,
    CIRCLE_RELATION_TANGENT,
    CIRCLE_RELATION_OVERLAPPING,
    CIRCLE_RELATION_INVALID
};

enum isometric_circle_status {
    ISOMETRIC_CIRCLE_AFFINE,
    ISOMETRIC_CIRCLE_GENERALIZED_LINE,
    ISOMETRIC_CIRCLE_INVALID_MAP
};

enum family_domain_status {
    FAMILY_PARAMETER_INVALID,
    FAMILY_DOMAIN_INTERIOR,
    FAMILY_DOMAIN_TANGENT_BOUNDARY,
    FAMILY_DOMAIN_OVERLAPPING,
    FAMILY_NUMERICALLY_UNREPRESENTABLE
};

enum presentation_status {
    PRESENTATION_VALID,
    PRESENTATION_TANGENT,
    PRESENTATION_OVERLAPPING,
    PRESENTATION_UNSUPPORTED_AFFINE_LINE,
    PRESENTATION_INVALID_GROUP
};

struct classical_circle_presentation {
    /* These are derived from group.A and group.B; they are not canonical state. */
    struct mobius_transformation exit_map[INDRAS_CIRCLE_COUNT];
    struct affine_isometric_circle circle[INDRAS_CIRCLE_COUNT];
    enum circle_relation pair_relation[INDRAS_CIRCLE_PAIR_COUNT];
    double minimum_pair_gap;
    enum presentation_status status;
};

struct complex_number complex_make(double real, double imaginary);
struct complex_number complex_add(struct complex_number left, struct complex_number right);
struct complex_number complex_subtract(struct complex_number left, struct complex_number right);
struct complex_number complex_multiply(struct complex_number left, struct complex_number right);
struct complex_number complex_divide(struct complex_number numerator, struct complex_number denominator);
struct complex_number complex_square_root(struct complex_number value);
double complex_magnitude(struct complex_number value);
bool complex_is_finite(struct complex_number value);

struct complex_number mobius_determinant(struct mobius_transformation matrix);
struct complex_number mobius_trace(struct mobius_transformation matrix);
struct mobius_transformation mobius_multiply(
    struct mobius_transformation left,
    struct mobius_transformation right
);
bool mobius_inverse(
    struct mobius_transformation matrix,
    struct mobius_transformation *inverse
);
bool mobius_normalize_sl2(
    struct mobius_transformation matrix,
    struct mobius_transformation *normalized
);
bool mobius_is_sl2(struct mobius_transformation matrix, double tolerance);
struct complex_number mobius_commutator_trace(
    struct mobius_transformation A,
    struct mobius_transformation B
);

bool projective_point_normalize(
    struct projective_point point,
    struct projective_point *normalized
);
struct projective_point projective_finite(struct complex_number value);
struct projective_point projective_infinity(void);
bool projective_point_is_infinity(struct projective_point point);
bool projective_point_equal(
    struct projective_point left,
    struct projective_point right,
    double tolerance
);
bool mobius_apply_projective(
    struct mobius_transformation matrix,
    struct projective_point point,
    struct projective_point *image
);
bool mobius_fixed_points_compute(
    struct mobius_transformation matrix,
    double determinant_tolerance,
    struct mobius_fixed_points *fixed_points
);

enum isometric_circle_status mobius_derive_isometric_circle(
    struct mobius_transformation matrix,
    double determinant_tolerance,
    struct affine_isometric_circle *circle
);
enum circle_relation circle_compare(
    struct affine_isometric_circle left,
    struct affine_isometric_circle right,
    double tolerance
);
bool classical_circle_presentation_derive(
    struct marked_rank_two_group group,
    double determinant_tolerance,
    double circle_tolerance,
    struct classical_circle_presentation *presentation
);

enum family_domain_status symmetric_classical_family_make(
    double radius,
    struct marked_rank_two_group *group
);

#endif
