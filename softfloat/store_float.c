#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

float __wcc_sf_store_float(FpValue *fpv) {
    #ifdef DEBUG_STORE
    printf("Storing                     ");
    print_fpv(fpv);
    #endif

    __wcc_sf_convert_to_inf_if_needed(_wcc_sf_binary32_encoding, fpv);
    __wcc_sf_convert_to_subnormal_if_needed(_wcc_sf_binary32_encoding, fpv);

    // Subnormals have already been rounded and don't need rounding done again.
    if (fpv->type != TYPE_SUBNORMAL)
        __wcc_sf_round_to_nearest_even_at_bits(_wcc_sf_binary32_encoding, fpv, _wcc_sf_binary32_encoding.significand_bits);

    __uint128_t raw = __wcc_sf_store(_wcc_sf_binary32_encoding, fpv);

    float f = *((float *) &raw);

    #ifdef DEBUG_STORE
    printf("Stored  %15g\n", f);
    #endif

    return f;
}
