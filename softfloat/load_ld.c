#ifdef DEBUG_LOAD
#include <stdio.h>
#endif

#include "softfloat.h"

FpValue __wcc_sf_load_ld(long double ld) {
    #ifdef DEBUG_LOAD
    printf("Loading %15Lg\n", ld);
    #endif

    __uint128_t raw = *((__uint128_t *) &ld);
    return __wcc_sf_load(_wcc_sf_binary128_encoding, raw);
}
