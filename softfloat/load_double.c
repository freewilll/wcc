#ifdef DEBUG_LOAD
#include <stdio.h>
#endif

#include "softfloat.h"

FpValue __wcc_sf_load_double(double d) {
    #ifdef DEBUG_LOAD
    printf("Loading %15g\n", d);
    #endif

    __uint128_t raw = 0;
    raw = *((uint64_t *) &d);
    return __wcc_sf_load(_wcc_sf_binary64_encoding, raw);
}
