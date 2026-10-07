#ifdef DEBUG_LOAD
#include <stdio.h>
#endif

#include "softfloat.h"

FpValue __wcc_sf_load_float(float f) {
    #ifdef DEBUG_LOAD
    printf("Loading %15g\n", f);
    #endif

    __uint128_t raw = 0;
    raw = *((uint32_t *) &f);
    return __wcc_sf_load(_wcc_sf_binary32_encoding, raw);
}
