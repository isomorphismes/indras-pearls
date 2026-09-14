#include "group_state.h"

static struct complex_value complex_value(float real, float imaginary) {
    return (struct complex_value){
        .real = real,
        .imaginary = imaginary,
    };
}

static struct mobius_map mobius_map(
    struct complex_value a,
    struct complex_value b,
    struct complex_value c,
    struct complex_value d
) {
    return (struct mobius_map){
        .a = a,
        .b = b,
        .c = c,
        .d = d,
    };
}

void initialize_bundled_limit_set_group(struct limit_set_group *group) {
    const float ten_sevenths = 10.0f / 7.0f;
    const float fifty_one_seventieths = 51.0f / 70.0f;
    const float one_hundred_forty_nine_seventieths = 149.0f / 70.0f;

    group->circle_radius = 0.7f;
    group->circle_center[0] = complex_value(-1.0f, 0.0f);
    group->circle_center[1] = complex_value(1.0f, 0.0f);
    group->circle_center[2] = complex_value(0.0f, -1.0f);
    group->circle_center[3] = complex_value(0.0f, 1.0f);

    /*
     * The four circles are paired isometric circles for two determinant-one
     * Mobius generators. Each exit map sends its circle interior back toward
     * the common fundamental domain. Repeatedly applying those maps gives a
     * bounded escape-depth test for the group's limit set.
     */
    group->exit_map[0] = mobius_map(
        complex_value(ten_sevenths, 0.0f),
        complex_value(fifty_one_seventieths, 0.0f),
        complex_value(ten_sevenths, 0.0f),
        complex_value(ten_sevenths, 0.0f)
    );
    group->exit_map[1] = mobius_map(
        complex_value(ten_sevenths, 0.0f),
        complex_value(-fifty_one_seventieths, 0.0f),
        complex_value(-ten_sevenths, 0.0f),
        complex_value(ten_sevenths, 0.0f)
    );
    group->exit_map[2] = mobius_map(
        complex_value(0.0f, ten_sevenths),
        complex_value(-one_hundred_forty_nine_seventieths, 0.0f),
        complex_value(ten_sevenths, 0.0f),
        complex_value(0.0f, ten_sevenths)
    );
    group->exit_map[3] = mobius_map(
        complex_value(0.0f, ten_sevenths),
        complex_value(one_hundred_forty_nine_seventieths, 0.0f),
        complex_value(-ten_sevenths, 0.0f),
        complex_value(0.0f, ten_sevenths)
    );
}
