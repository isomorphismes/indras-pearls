#include "mobius_math.h"

#include <math.h>

static const struct complex_number COMPLEX_ZERO = {0.0, 0.0};
static const struct complex_number COMPLEX_ONE = {1.0, 0.0};

struct complex_number complex_make(double real, double imaginary) {
    return (struct complex_number){real, imaginary};
}

struct complex_number complex_add(struct complex_number left, struct complex_number right) {
    return complex_make(left.real + right.real, left.imaginary + right.imaginary);
}

struct complex_number complex_subtract(struct complex_number left, struct complex_number right) {
    return complex_make(left.real - right.real, left.imaginary - right.imaginary);
}

struct complex_number complex_multiply(struct complex_number left, struct complex_number right) {
    return complex_make(
        left.real * right.real - left.imaginary * right.imaginary,
        left.real * right.imaginary + left.imaginary * right.real
    );
}

struct complex_number complex_divide(struct complex_number numerator, struct complex_number denominator) {
    const double scale = fmax(fabs(denominator.real), fabs(denominator.imaginary));
    if (scale == 0.0 || !isfinite(scale)) {
        return complex_make(NAN, NAN);
    }
    const double real = denominator.real / scale;
    const double imaginary = denominator.imaginary / scale;
    const double divisor = real * real + imaginary * imaginary;
    const double scaled_real = numerator.real / scale;
    const double scaled_imaginary = numerator.imaginary / scale;
    return complex_make(
        (scaled_real * real + scaled_imaginary * imaginary) / divisor,
        (scaled_imaginary * real - scaled_real * imaginary) / divisor
    );
}

struct complex_number complex_square_root(struct complex_number value) {
    const double magnitude = hypot(value.real, value.imaginary);
    if (magnitude == 0.0) {
        return COMPLEX_ZERO;
    }
    const double real = sqrt(fmax(0.0, (magnitude + value.real) * 0.5));
    const double imaginary_magnitude = sqrt(fmax(0.0, (magnitude - value.real) * 0.5));
    return complex_make(real, copysign(imaginary_magnitude, value.imaginary));
}

double complex_magnitude(struct complex_number value) {
    return hypot(value.real, value.imaginary);
}

bool complex_is_finite(struct complex_number value) {
    return isfinite(value.real) && isfinite(value.imaginary);
}

struct complex_number mobius_determinant(struct mobius_transformation matrix) {
    return complex_subtract(complex_multiply(matrix.a, matrix.d), complex_multiply(matrix.b, matrix.c));
}

struct complex_number mobius_trace(struct mobius_transformation matrix) {
    return complex_add(matrix.a, matrix.d);
}

struct mobius_transformation mobius_multiply(struct mobius_transformation left,
                                              struct mobius_transformation right) {
    return (struct mobius_transformation){
        .a = complex_add(complex_multiply(left.a, right.a), complex_multiply(left.b, right.c)),
        .b = complex_add(complex_multiply(left.a, right.b), complex_multiply(left.b, right.d)),
        .c = complex_add(complex_multiply(left.c, right.a), complex_multiply(left.d, right.c)),
        .d = complex_add(complex_multiply(left.c, right.b), complex_multiply(left.d, right.d)),
    };
}

bool mobius_inverse(struct mobius_transformation matrix, struct mobius_transformation *inverse) {
    if (inverse == NULL) {
        return false;
    }
    const struct complex_number determinant = mobius_determinant(matrix);
    if (!complex_is_finite(determinant) || complex_magnitude(determinant) == 0.0) {
        return false;
    }
    inverse->a = complex_divide(matrix.d, determinant);
    inverse->b = complex_divide(complex_make(-matrix.b.real, -matrix.b.imaginary), determinant);
    inverse->c = complex_divide(complex_make(-matrix.c.real, -matrix.c.imaginary), determinant);
    inverse->d = complex_divide(matrix.a, determinant);
    return complex_is_finite(inverse->a) && complex_is_finite(inverse->b) &&
        complex_is_finite(inverse->c) && complex_is_finite(inverse->d);
}

bool mobius_normalize_sl2(struct mobius_transformation matrix,
                          struct mobius_transformation *normalized) {
    if (normalized == NULL) {
        return false;
    }
    const struct complex_number determinant = mobius_determinant(matrix);
    if (!complex_is_finite(determinant) || complex_magnitude(determinant) == 0.0) {
        return false;
    }
    const struct complex_number root = complex_square_root(determinant);
    if (complex_magnitude(root) == 0.0 || !complex_is_finite(root)) {
        return false;
    }
    normalized->a = complex_divide(matrix.a, root);
    normalized->b = complex_divide(matrix.b, root);
    normalized->c = complex_divide(matrix.c, root);
    normalized->d = complex_divide(matrix.d, root);
    return complex_is_finite(normalized->a) && complex_is_finite(normalized->b) &&
        complex_is_finite(normalized->c) && complex_is_finite(normalized->d);
}

bool mobius_is_sl2(struct mobius_transformation matrix, double tolerance) {
    if (tolerance < 0.0 || !isfinite(tolerance)) {
        return false;
    }
    const struct complex_number determinant = mobius_determinant(matrix);
    const double product_scale = fmax(
        1.0,
        complex_magnitude(complex_multiply(matrix.a, matrix.d)) +
            complex_magnitude(complex_multiply(matrix.b, matrix.c))
    );
    return complex_is_finite(determinant) && isfinite(product_scale) &&
        complex_magnitude(complex_subtract(determinant, COMPLEX_ONE)) <= tolerance * product_scale;
}

struct complex_number mobius_commutator_trace(struct mobius_transformation A,
                                              struct mobius_transformation B) {
    struct mobius_transformation inverse_A;
    struct mobius_transformation inverse_B;
    if (!mobius_inverse(A, &inverse_A) || !mobius_inverse(B, &inverse_B)) {
        return complex_make(NAN, NAN);
    }
    return mobius_trace(mobius_multiply(mobius_multiply(mobius_multiply(A, B), inverse_A), inverse_B));
}

bool projective_point_normalize(struct projective_point point, struct projective_point *normalized) {
    if (normalized == NULL || !complex_is_finite(point.numerator) ||
        !complex_is_finite(point.denominator)) {
        return false;
    }
    if (complex_magnitude(point.denominator) == 0.0) {
        if (complex_magnitude(point.numerator) == 0.0) {
            return false;
        }
        *normalized = projective_infinity();
        return true;
    }
    normalized->numerator = complex_divide(point.numerator, point.denominator);
    normalized->denominator = COMPLEX_ONE;
    return complex_is_finite(normalized->numerator);
}

struct projective_point projective_finite(struct complex_number value) {
    return (struct projective_point){value, COMPLEX_ONE};
}

struct projective_point projective_infinity(void) {
    return (struct projective_point){COMPLEX_ONE, COMPLEX_ZERO};
}

bool projective_point_is_infinity(struct projective_point point) {
    return complex_magnitude(point.denominator) == 0.0 &&
        complex_magnitude(point.numerator) != 0.0;
}

bool projective_point_equal(struct projective_point left, struct projective_point right,
                            double tolerance) {
    struct projective_point normalized_left;
    struct projective_point normalized_right;
    if (tolerance < 0.0 || !isfinite(tolerance) ||
        !projective_point_normalize(left, &normalized_left) ||
        !projective_point_normalize(right, &normalized_right)) {
        return false;
    }
    return complex_magnitude(complex_subtract(normalized_left.numerator, normalized_right.numerator)) <= tolerance &&
        complex_magnitude(complex_subtract(normalized_left.denominator, normalized_right.denominator)) <= tolerance;
}

bool mobius_apply_projective(struct mobius_transformation matrix, struct projective_point point,
                             struct projective_point *image) {
    if (image == NULL || !complex_is_finite(point.numerator) || !complex_is_finite(point.denominator)) {
        return false;
    }
    const struct complex_number numerator = complex_add(
        complex_multiply(matrix.a, point.numerator), complex_multiply(matrix.b, point.denominator));
    const struct complex_number denominator = complex_add(
        complex_multiply(matrix.c, point.numerator), complex_multiply(matrix.d, point.denominator));
    return projective_point_normalize((struct projective_point){numerator, denominator}, image);
}

static bool fixed_point_for_eigenvalue(struct mobius_transformation matrix,
                                       struct complex_number eigenvalue,
                                       struct complex_number determinant,
                                       struct fixed_point_diagnostic *diagnostic) {
    const struct complex_number row_1_left = complex_subtract(matrix.a, eigenvalue);
    const struct complex_number row_1_right = matrix.b;
    const struct complex_number row_2_left = matrix.c;
    const struct complex_number row_2_right = complex_subtract(matrix.d, eigenvalue);
    const double row_1_size = hypot(complex_magnitude(row_1_left), complex_magnitude(row_1_right));
    const double row_2_size = hypot(complex_magnitude(row_2_left), complex_magnitude(row_2_right));
    struct complex_number vector_numerator;
    struct complex_number vector_denominator;
    if (row_1_size >= row_2_size) {
        vector_numerator = complex_make(-row_1_right.real, -row_1_right.imaginary);
        vector_denominator = row_1_left;
    } else {
        vector_numerator = complex_make(-row_2_right.real, -row_2_right.imaginary);
        vector_denominator = row_2_left;
    }
    if (!projective_point_normalize(
            (struct projective_point){vector_numerator, vector_denominator}, &diagnostic->point)) {
        return false;
    }
    const struct complex_number eigenvalue_squared = complex_multiply(eigenvalue, eigenvalue);
    if (complex_magnitude(eigenvalue_squared) == 0.0) {
        return false;
    }
    diagnostic->multiplier = complex_divide(determinant, eigenvalue_squared);
    return complex_is_finite(diagnostic->multiplier);
}

bool mobius_fixed_points_compute(struct mobius_transformation matrix, double determinant_tolerance,
                                 struct mobius_fixed_points *fixed_points) {
    if (fixed_points == NULL || !mobius_is_sl2(matrix, determinant_tolerance)) {
        return false;
    }
    const struct complex_number trace = mobius_trace(matrix);
    const struct complex_number determinant = mobius_determinant(matrix);
    const struct complex_number discriminant = complex_subtract(
        complex_multiply(trace, trace), complex_make(4.0 * determinant.real, 4.0 * determinant.imaginary));
    const struct complex_number root = complex_square_root(discriminant);
    const struct complex_number eigenvalue_plus = complex_multiply(complex_add(trace, root), complex_make(0.5, 0.0));
    const struct complex_number eigenvalue_minus = complex_multiply(complex_subtract(trace, root), complex_make(0.5, 0.0));
    return fixed_point_for_eigenvalue(matrix, eigenvalue_plus, determinant, &fixed_points->point[0]) &&
        fixed_point_for_eigenvalue(matrix, eigenvalue_minus, determinant, &fixed_points->point[1]);
}

enum isometric_circle_status mobius_derive_isometric_circle(
    struct mobius_transformation matrix,
    double determinant_tolerance,
    struct affine_isometric_circle *circle
) {
    if (circle == NULL || !mobius_is_sl2(matrix, determinant_tolerance)) {
        return ISOMETRIC_CIRCLE_INVALID_MAP;
    }
    const double lower_left_magnitude = complex_magnitude(matrix.c);
    if (lower_left_magnitude == 0.0) {
        return ISOMETRIC_CIRCLE_GENERALIZED_LINE;
    }
    if (!isfinite(lower_left_magnitude)) {
        return ISOMETRIC_CIRCLE_INVALID_MAP;
    }
    const struct complex_number center_ratio = complex_divide(matrix.d, matrix.c);
    circle->center = complex_make(-center_ratio.real, -center_ratio.imaginary);
    circle->radius = 1.0 / lower_left_magnitude;
    if (!complex_is_finite(circle->center) || !isfinite(circle->radius) || circle->radius <= 0.0) {
        return ISOMETRIC_CIRCLE_INVALID_MAP;
    }
    return ISOMETRIC_CIRCLE_AFFINE;
}

enum circle_relation circle_compare(struct affine_isometric_circle left,
                                    struct affine_isometric_circle right, double tolerance) {
    if (!complex_is_finite(left.center) || !complex_is_finite(right.center) ||
        !isfinite(left.radius) || !isfinite(right.radius) || left.radius < 0.0 ||
        right.radius < 0.0 || tolerance < 0.0 || !isfinite(tolerance)) {
        return CIRCLE_RELATION_INVALID;
    }
    const double center_distance = complex_magnitude(complex_subtract(left.center, right.center));
    const double radius_sum = left.radius + right.radius;
    if (center_distance > radius_sum + tolerance) {
        return CIRCLE_RELATION_DISJOINT;
    }
    if (fabs(center_distance - radius_sum) <= tolerance) {
        return CIRCLE_RELATION_TANGENT;
    }
    return CIRCLE_RELATION_OVERLAPPING;
}

bool classical_circle_presentation_derive(struct marked_rank_two_group group,
                                          double determinant_tolerance,
                                          double circle_tolerance,
                                          struct classical_circle_presentation *presentation) {
    if (presentation == NULL || !mobius_is_sl2(group.A, determinant_tolerance) ||
        !mobius_is_sl2(group.B, determinant_tolerance) || circle_tolerance < 0.0 ||
        !isfinite(circle_tolerance)) {
        return false;
    }
    struct mobius_transformation inverse_A;
    struct mobius_transformation inverse_B;
    if (!mobius_inverse(group.A, &inverse_A) || !mobius_inverse(group.B, &inverse_B)) {
        return false;
    }
    presentation->exit_map[0] = group.A;
    presentation->exit_map[1] = inverse_A;
    presentation->exit_map[2] = group.B;
    presentation->exit_map[3] = inverse_B;
    presentation->minimum_pair_gap = INFINITY;
    presentation->status = PRESENTATION_VALID;
    for (size_t i = 0; i < INDRAS_CIRCLE_COUNT; ++i) {
        const enum isometric_circle_status circle_status =
            mobius_derive_isometric_circle(presentation->exit_map[i], determinant_tolerance,
                                           &presentation->circle[i]);
        if (circle_status != ISOMETRIC_CIRCLE_AFFINE) {
            presentation->status = circle_status == ISOMETRIC_CIRCLE_GENERALIZED_LINE
                ? PRESENTATION_UNSUPPORTED_AFFINE_LINE
                : PRESENTATION_INVALID_GROUP;
            return true;
        }
    }
    size_t pair_index = 0;
    for (size_t i = 0; i < INDRAS_CIRCLE_COUNT; ++i) {
        for (size_t j = i + 1; j < INDRAS_CIRCLE_COUNT; ++j) {
            const struct affine_isometric_circle left = presentation->circle[i];
            const struct affine_isometric_circle right = presentation->circle[j];
            const double distance = complex_magnitude(complex_subtract(left.center, right.center));
            const double gap = distance - left.radius - right.radius;
            if (gap < presentation->minimum_pair_gap) {
                presentation->minimum_pair_gap = gap;
            }
            const enum circle_relation relation = circle_compare(left, right, circle_tolerance);
            presentation->pair_relation[pair_index++] = relation;
            if (relation == CIRCLE_RELATION_OVERLAPPING) {
                presentation->status = PRESENTATION_OVERLAPPING;
            } else if (relation == CIRCLE_RELATION_TANGENT && presentation->status == PRESENTATION_VALID) {
                presentation->status = PRESENTATION_TANGENT;
            } else if (relation == CIRCLE_RELATION_INVALID) {
                presentation->status = PRESENTATION_INVALID_GROUP;
            }
        }
    }
    return true;
}

enum family_domain_status symmetric_classical_family_make(double radius,
                                                           struct marked_rank_two_group *group) {
    if (group == NULL || !isfinite(radius) || radius <= 0.0) {
        return FAMILY_PARAMETER_INVALID;
    }
    const double inverse_radius = 1.0 / radius;
    const double radius_squared = radius * radius;
    const struct complex_number real_inverse_radius = complex_make(inverse_radius, 0.0);
    const struct complex_number imaginary_inverse_radius = complex_make(0.0, inverse_radius);
    group->A = (struct mobius_transformation){
        .a = real_inverse_radius,
        .b = complex_make((1.0 - radius_squared) * inverse_radius, 0.0),
        .c = real_inverse_radius,
        .d = real_inverse_radius,
    };
    group->B = (struct mobius_transformation){
        .a = imaginary_inverse_radius,
        .b = complex_make(-(1.0 + radius_squared) * inverse_radius, 0.0),
        .c = real_inverse_radius,
        .d = imaginary_inverse_radius,
    };
    if (!mobius_is_sl2(group->A, 1.0e-12) || !mobius_is_sl2(group->B, 1.0e-12)) {
        return FAMILY_NUMERICALLY_UNREPRESENTABLE;
    }
    const double tangent_radius = 1.0 / sqrt(2.0);
    if (radius < tangent_radius) {
        return FAMILY_DOMAIN_INTERIOR;
    }
    if (radius == tangent_radius) {
        return FAMILY_DOMAIN_TANGENT_BOUNDARY;
    }
    return FAMILY_DOMAIN_OVERLAPPING;
}
