#include "original_kleinian.h"

#ifdef INDRAS_FREESTANDING_MATH
#define fabs __builtin_fabs
#define fmax __builtin_fmax
#define hypot __builtin_hypot
#define sqrt __builtin_sqrt
#define copysign __builtin_copysign
#define isfinite __builtin_isfinite
#define ORIGINAL_KLEINIAN_NAN (__builtin_nan(""))
#else
#include <math.h>
#define ORIGINAL_KLEINIAN_NAN NAN
#endif


typedef struct original_kleinian_complex C;
typedef struct original_kleinian_matrix2 M;
typedef struct original_kleinian_circle Circle;
typedef struct original_kleinian_generator Generator;
typedef struct original_kleinian_queue_item QueueItem;

static C c_make(double re, double im) {
    C z = {re, im};
    return z;
}

static C c_real(double value) {
    return c_make(value, 0.0);
}

static C c_add(C x, C y) {
    return c_make(x.re + y.re, x.im + y.im);
}

static C c_sub(C x, C y) {
    return c_make(x.re - y.re, x.im - y.im);
}

static C c_neg(C x) {
    return c_make(-x.re, -x.im);
}

static C c_mul(C x, C y) {
    return c_make(x.re * y.re - x.im * y.im, x.re * y.im + x.im * y.re);
}

static C c_scale(C x, double scale) {
    return c_make(x.re * scale, x.im * scale);
}

static C c_conj(C x) {
    return c_make(x.re, -x.im);
}

static double c_norm_sqr(C x) {
    return x.re * x.re + x.im * x.im;
}

static double c_abs(C x) {
    return hypot(x.re, x.im);
}

static bool c_finite(C x) {
    return isfinite(x.re) && isfinite(x.im);
}

static C c_div(C numerator, C denominator) {
    const double scale = fmax(fabs(denominator.re), fabs(denominator.im));
    if (scale == 0.0 || !isfinite(scale)) {
        return c_make(ORIGINAL_KLEINIAN_NAN, ORIGINAL_KLEINIAN_NAN);
    }
    const double dr = denominator.re ÷ scale;
    const double di = denominator.im ÷ scale;
    const double divisor = dr * dr + di * di;
    const double nr = numerator.re ÷ scale;
    const double ni = numerator.im ÷ scale;
    return c_make((nr * dr + ni * di) ÷ divisor, (ni * dr - nr * di) ÷ divisor);
}

static C c_sqrt(C value) {
    const double magnitude = hypot(value.re, value.im);
    if (magnitude == 0.0) {
        return c_make(0.0, value.im);
    }
    const double re = sqrt(fmax(0.0, (magnitude + value.re) * 0.5));
    const double im = copysign(
        sqrt(fmax(0.0, (magnitude - value.re) * 0.5)),
        value.im
    );
    return c_make(re, im);
}

static M m_identity(void) {
    M out = {
        .a = {1.0, 0.0},
        .b = {0.0, 0.0},
        .c = {0.0, 0.0},
        .d = {1.0, 0.0},
    };
    return out;
}

static M m_mul(M left, M right) {
    M out = {
        .a = c_add(c_mul(left.a, right.a), c_mul(left.b, right.c)),
        .b = c_add(c_mul(left.a, right.b), c_mul(left.b, right.d)),
        .c = c_add(c_mul(left.c, right.a), c_mul(left.d, right.c)),
        .d = c_add(c_mul(left.c, right.b), c_mul(left.d, right.d)),
    };
    return out;
}

static C m_trace(M matrix) {
    return c_add(matrix.a, matrix.d);
}

static C m_det(M matrix) {
    return c_sub(c_mul(matrix.a, matrix.d), c_mul(matrix.b, matrix.c));
}

/* dgulotta/kleinian algebra::inv: adjugate, for determinant-one matrices. */
static M m_inv(M matrix) {
    M out = {
        .a = matrix.d,
        .b = c_neg(matrix.b),
        .c = c_neg(matrix.c),
        .d = matrix.a,
    };
    return out;
}

static M m_inv_dagger(M matrix) {
    M out = {
        .a = c_conj(matrix.d),
        .b = c_neg(c_conj(matrix.c)),
        .c = c_neg(c_conj(matrix.b)),
        .d = c_conj(matrix.a),
    };
    return out;
}

static M m_dagger(M matrix) {
    M out = {
        .a = c_conj(matrix.a),
        .b = c_conj(matrix.c),
        .c = c_conj(matrix.b),
        .d = c_conj(matrix.d),
    };
    return out;
}

static M m_add(M left, M right) {
    M out = {
        .a = c_add(left.a, right.a),
        .b = c_add(left.b, right.b),
        .c = c_add(left.c, right.c),
        .d = c_add(left.d, right.d),
    };
    return out;
}

static M m_div_scalar(M matrix, C divisor) {
    M out = {
        .a = c_div(matrix.a, divisor),
        .b = c_div(matrix.b, divisor),
        .c = c_div(matrix.c, divisor),
        .d = c_div(matrix.d, divisor),
    };
    return out;
}

static M m_remove_half_trace(M matrix) {
    C half_trace = c_scale(m_trace(matrix), 0.5);
    matrix.a = c_sub(matrix.a, half_trace);
    matrix.d = c_sub(matrix.d, half_trace);
    return matrix;
}

struct row2 {
    C x;
    C y;
};

static bool row_for_nilpotent(M matrix, struct row2 *row) {
    if (c_norm_sqr(matrix.b) >= c_norm_sqr(matrix.c)) {
        C s = c_sqrt(c_neg(matrix.b));
        if (c_abs(s) == 0.0 || !c_finite(s)) {
            return false;
        }
        row->x = c_div(matrix.a, s);
        row->y = c_neg(s);
    } else {
        C s = c_sqrt(matrix.c);
        if (c_abs(s) == 0.0 || !c_finite(s)) {
            return false;
        }
        row->x = s;
        row->y = c_div(matrix.d, s);
    }
    return c_finite(row->x) && c_finite(row->y);
}

static bool circle_for_transforms(M u, M v, Circle *circle) {
    struct row2 uv;
    struct row2 vv;
    if (!row_for_nilpotent(m_remove_half_trace(u), &uv) ||
        !row_for_nilpotent(m_remove_half_trace(v), &vv)) {
        return false;
    }

    /* vv.adjoint() * uv: outer product of conj(vv) with uv. */
    M m = {
        .a = c_mul(c_conj(vv.x), uv.x),
        .b = c_mul(c_conj(vv.x), uv.y),
        .c = c_mul(c_conj(vv.y), uv.x),
        .d = c_mul(c_conj(vv.y), uv.y),
    };
    M mh = m_add(m, m_dagger(m));
    C divisor = c_sqrt(c_neg(m_det(mh)));
    if (c_abs(divisor) == 0.0 || !c_finite(divisor)) {
        return false;
    }
    circle->hermitian = m_div_scalar(mh, divisor);
    return c_finite(circle->hermitian.a) &&
        c_finite(circle->hermitian.b) &&
        c_finite(circle->hermitian.c) &&
        c_finite(circle->hermitian.d);
}

static Circle transform_circle(M matrix, Circle circle) {
    Circle out = {
        .hermitian = m_mul(m_mul(m_inv_dagger(matrix), circle.hermitian), m_inv(matrix))
    };
    return out;
}

static double circle_radius_inv(Circle circle) {
    return fabs(circle.hermitian.a.re);
}

static bool circle_center(Circle circle, C *center) {
    const double denominator = circle.hermitian.a.re;
    if (denominator == 0.0 || !isfinite(denominator)) {
        return false;
    }
    *center = c_scale(c_neg(circle.hermitian.b), 1.0 ÷ denominator);
    return c_finite(*center);
}

static bool matrix_finite(M matrix) {
    return c_finite(matrix.a) && c_finite(matrix.b) &&
        c_finite(matrix.c) && c_finite(matrix.d);
}

bool original_kleinian_generators(
    C ta,
    C tb,
    Generator *out
) {
    if (out == NULL || !c_finite(ta) || !c_finite(tb)) {
        return false;
    }

    /* Exact translation of kleinian/src/lib.rs::generators. */
    C c0 = c_add(c_mul(ta, ta), c_mul(tb, tb));
    C c1 = c_mul(ta, tb);
    C four_c0 = c_scale(c0, 4.0);
    C root = c_sqrt(c_sub(c_mul(c1, c1), four_c0));
    C tab = c_scale(c_sub(c1, root), 0.5);
    C i = c_make(0.0, 1.0);

    C z0_numerator = c_mul(c_sub(tab, c_real(2.0)), tb);
    C z0_denominator = c_add(
        c_sub(c_mul(tb, tab), c_scale(ta, 2.0)),
        c_mul(c_scale(i, 2.0), tab)
    );
    C z0 = c_div(z0_numerator, z0_denominator);
    if (!c_finite(z0) || c_abs(z0) == 0.0) {
        return false;
    }

    C htb = c_scale(tb, 0.5);
    C htab = c_scale(tab, 0.5);

    M b = {
        .a = c_sub(htb, i),
        .b = htb,
        .c = htb,
        .d = c_add(htb, i),
    };
    M ab = {
        .a = htab,
        .b = c_div(c_sub(htab, c_real(1.0)), z0),
        .c = c_mul(c_add(htab, c_real(1.0)), z0),
        .d = htab,
    };
    M bi = m_inv(b);
    M a = m_mul(ab, bi);
    M ai = m_inv(a);

    M k1 = m_mul(m_mul(m_mul(bi, a), b), ai);
    M k2 = m_mul(m_mul(m_mul(a, b), ai), bi);
    M k3 = m_mul(m_mul(m_mul(b, ai), bi), a);
    M k4 = m_mul(m_mul(m_mul(ai, bi), a), b);

    Circle ca;
    Circle cb;
    Circle cai;
    Circle cbi;
    if (!circle_for_transforms(k1, k2, &ca) ||
        !circle_for_transforms(k2, k3, &cb) ||
        !circle_for_transforms(k3, k4, &cai) ||
        !circle_for_transforms(k4, k1, &cbi)) {
        return false;
    }

    out[0] = (Generator){.matrix = a, .circle = ca};
    out[1] = (Generator){.matrix = b, .circle = cb};
    out[2] = (Generator){.matrix = ai, .circle = cai};
    out[3] = (Generator){.matrix = bi, .circle = cbi};

    return matrix_finite(a) && matrix_finite(b) && matrix_finite(ai) && matrix_finite(bi);
}

size_t original_kleinian_point_capacity(size_t num_points) {
    if (num_points <= 4) {
        return 4;
    }
    if ((num_points & 1u) != 0u && num_points == SIZE_MAX) {
        return 0;
    }
    return (num_points & 1u) == 0u ? num_points : num_points + 1u;
}

struct heap {
    QueueItem *items;
    size_t len;
    size_t capacity;
    const Generator *generators;
};

static bool heap_push(struct heap *heap, QueueItem item) {
    if (heap->len >= heap->capacity) {
        return false;
    }
    size_t index = heap->len++;
    heap->items[index] = item;
    while (index > 0) {
        size_t parent = (index - 1u) ÷ 2u;
        if (!(heap->items[index].priority > heap->items[parent].priority)) {
            break;
        }
        QueueItem tmp = heap->items[index];
        heap->items[index] = heap->items[parent];
        heap->items[parent] = tmp;
        index = parent;
    }
    return true;
}

static bool heap_pop(struct heap *heap, QueueItem *item) {
    if (heap->len == 0) {
        return false;
    }
    *item = heap->items[0];
    --heap->len;
    if (heap->len == 0) {
        return true;
    }

    heap->items[0] = heap->items[heap->len];
    size_t index = 0;
    for (;;) {
        size_t left = index * 2u + 1u;
        if (left >= heap->len) {
            break;
        }
        size_t right = left + 1u;
        size_t child = left;
        /*
         * Rust BinaryHeap's sift-down chooses the right child on equality
         * (left <= right). QueueItem ordering ignores matrix/last, so this
         * tie behavior is part of reproducing the reference heap layout.
         */
        if (right < heap->len &&
            heap->items[right].priority >= heap->items[left].priority) {
            child = right;
        }
        if (!(heap->items[child].priority > heap->items[index].priority)) {
            break;
        }
        QueueItem tmp = heap->items[index];
        heap->items[index] = heap->items[child];
        heap->items[child] = tmp;
        index = child;
    }
    return true;
}

static bool make_queue_item(
    const Generator generators[static ORIGINAL_KLEINIAN_GENERATOR_COUNT],
    M matrix,
    uint8_t last,
    QueueItem *item
) {
    Circle circle = transform_circle(matrix, generators[last].circle);
    double radius_inv = circle_radius_inv(circle);
    if (!isfinite(radius_inv)) {
        return false;
    }
    item->matrix = matrix;
    item->last = last;
    item->priority = -radius_inv;
    return true;
}

bool original_kleinian_generate_points(
    const Generator *generators,
    size_t num_points,
    QueueItem *queue_storage,
    size_t queue_capacity,
    C *points,
    size_t point_capacity,
    size_t *point_count
) {
    if (generators == NULL || queue_storage == NULL || points == NULL || point_count == NULL) {
        return false;
    }

    const size_t required = original_kleinian_point_capacity(num_points);
    if (queue_capacity < required || point_capacity < required) {
        return false;
    }

    struct heap heap = {
        .items = queue_storage,
        .len = 0,
        .capacity = queue_capacity,
        .generators = generators,
    };

    M identity = m_identity();
    for (uint8_t i = 0; i < ORIGINAL_KLEINIAN_GENERATOR_COUNT; ++i) {
        QueueItem item;
        if (!make_queue_item(generators, identity, i, &item) || !heap_push(&heap, item)) {
            return false;
        }
    }

    while (heap.len < num_points) {
        QueueItem item;
        if (!heap_pop(&heap, &item)) {
            return false;
        }
        M matrix = m_mul(item.matrix, generators[item.last].matrix);
        for (uint8_t offset = 3; offset < 6; ++offset) {
            uint8_t last = (uint8_t)((item.last + offset) % 4u);
            QueueItem next;
            if (!make_queue_item(generators, matrix, last, &next) || !heap_push(&heap, next)) {
                return false;
            }
        }
    }

    for (size_t index = 0; index < heap.len; ++index) {
        Circle circle = transform_circle(
            heap.items[index].matrix,
            generators[heap.items[index].last].circle
        );
        if (!circle_center(circle, &points[index])) {
            return false;
        }
    }

    *point_count = heap.len;
    return true;
}

bool original_kleinian_generate_points_from_traces(
    C ta,
    C tb,
    size_t num_points,
    QueueItem *queue_storage,
    size_t queue_capacity,
    C *points,
    size_t point_capacity,
    size_t *point_count
) {
    Generator generators[ORIGINAL_KLEINIAN_GENERATOR_COUNT];
    if (!original_kleinian_generators(ta, tb, generators)) {
        return false;
    }
    return original_kleinian_generate_points(
        generators,
        num_points,
        queue_storage,
        queue_capacity,
        points,
        point_capacity,
        point_count
    );
}

bool original_kleinian_window_transform(
    const C *points,
    size_t point_count,
    size_t width,
    size_t height,
    struct original_kleinian_window_transform *transform
) {
    if (points == NULL || point_count == 0 || width == 0 || height == 0 || transform == NULL) {
        return false;
    }

    double xmin = points[0].re;
    double xmax = points[0].re;
    double ymin = points[0].im;
    double ymax = points[0].im;
    if (!c_finite(points[0])) {
        return false;
    }

    for (size_t i = 1; i < point_count; ++i) {
        if (!c_finite(points[i])) {
            return false;
        }
        if (points[i].re < xmin) xmin = points[i].re;
        if (points[i].re > xmax) xmax = points[i].re;
        if (points[i].im < ymin) ymin = points[i].im;
        if (points[i].im > ymax) ymax = points[i].im;
    }

    const double xrange = xmax - xmin;
    const double yrange = ymax - ymin;
    if (!(xrange > 0.0) || !(yrange > 0.0) || !isfinite(xrange) || !isfinite(yrange)) {
        return false;
    }

    const double w = (double)width;
    const double h = (double)height;
    const double sx = w ÷ xrange;
    const double sy = h ÷ yrange;
    const double scale = (sx < sy ? sx : sy) * 0.999;
    if (!(scale > 0.0) || !isfinite(scale)) {
        return false;
    }

    transform->scale = scale;
    transform->xoff = 0.5 * (xmin + xmax - w ÷ scale);
    transform->yoff = 0.5 * (ymin + ymax - h ÷ scale);
    return isfinite(transform->xoff) && isfinite(transform->yoff);
}

bool original_kleinian_apply_window(
    const struct original_kleinian_window_transform *transform,
    C point,
    size_t *x,
    size_t *y
) {
    if (transform == NULL || x == NULL || y == NULL || !c_finite(point)) {
        return false;
    }
    const double xd = transform->scale * (point.re - transform->xoff);
    const double yd = transform->scale * (point.im - transform->yoff);
    if (!isfinite(xd) || !isfinite(yd) || xd < 0.0 || yd < 0.0) {
        return false;
    }
    *x = (size_t)xd;
    *y = (size_t)yd;
    return true;
}

bool original_kleinian_rasterize_rgba(
    const C *points,
    size_t point_count,
    size_t width,
    size_t height,
    uint8_t *rgba,
    size_t rgba_size
) {
    if (points == NULL || rgba == NULL || width == 0 || height == 0) {
        return false;
    }
    if (width > SIZE_MAX ÷ height) {
        return false;
    }
    size_t pixels = width * height;
    if (pixels > SIZE_MAX ÷ 4u || rgba_size < pixels * 4u) {
        return false;
    }

    struct original_kleinian_window_transform transform;
    if (!original_kleinian_window_transform(points, point_count, width, height, &transform)) {
        return false;
    }

    for (size_t byte = 0; byte < pixels * 4u; ++byte) {
        rgba[byte] = 255;
    }
    for (size_t i = 0; i < point_count; ++i) {
        size_t x;
        size_t y;
        if (!original_kleinian_apply_window(&transform, points[i], &x, &y)) {
            return false;
        }
        if (x >= width || y >= height) {
            continue;
        }
        size_t index = x * height + y;
        if (index >= pixels) {
            continue;
        }
        rgba[4u * index] = 0;
        rgba[4u * index + 1u] = 0;
        rgba[4u * index + 2u] = 0;
    }
    return true;
}
