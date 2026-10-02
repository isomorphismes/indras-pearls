#include "original_kleinian.h"

int indras_original_kleinian_ndk_probe(void) {
    struct original_kleinian_generator generators[ORIGINAL_KLEINIAN_GENERATOR_COUNT];
    const struct original_kleinian_complex ta = {2.2, 0.0};
    const struct original_kleinian_complex tb = {2.2, 0.0};
    if (original_kleinian_point_capacity(5) != 6) {
        return 1;
    }
    if (!original_kleinian_generators(ta, tb, generators)) {
        return 2;
    }
    return generators[0].matrix.a.re > 1.09 &&
           generators[0].matrix.a.re < 1.11 ? 0 : 3;
}
