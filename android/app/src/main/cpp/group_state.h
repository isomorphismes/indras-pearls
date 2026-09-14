#ifndef INDRAS_PEARLS_GROUP_STATE_H
#define INDRAS_PEARLS_GROUP_STATE_H

#define LIMIT_SET_REGION_COUNT 4

struct complex_value {
    float real;
    float imaginary;
};

struct mobius_map {
    struct complex_value a;
    struct complex_value b;
    struct complex_value c;
    struct complex_value d;
};

struct limit_set_group {
    struct complex_value circle_center[LIMIT_SET_REGION_COUNT];
    float circle_radius;
    struct mobius_map exit_map[LIMIT_SET_REGION_COUNT];
};

void initialize_bundled_limit_set_group(struct limit_set_group *group);

#endif
