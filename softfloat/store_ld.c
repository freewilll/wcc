#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

long double __wcc_sf_store_ld(FpValue *fpv) {
    #ifdef DEBUG_STORE
    printf("Storing                     ");
    print_fpv(fpv);
    #endif

    __wcc_sf_convert_to_inf_if_needed(_wcc_sf_binary128_encoding, fpv);
    __wcc_sf_convert_to_subnormal_if_needed(_wcc_sf_binary128_encoding, fpv);

    __uint128_t raw = __wcc_sf_store(_wcc_sf_binary128_encoding, fpv);
    long double ld = *((long double *) &raw);

    #ifdef DEBUG_STORE
    printf("Stored  %15Lg\n", ld);
    #endif

    return ld;
}
