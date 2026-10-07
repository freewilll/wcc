#ifdef DEBUG_STORE
#include <inttypes.h>
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

uint64_t __wcc_sf_convert_ld_to_uint64(long double ld) {
    FpValue fpv = __wcc_sf_load_ld(ld);

    #ifdef DEBUG_STORE
    printf("Converting to uint64        ");
    print_fpv(&fpv);
    #endif

    if (fpv.type == TYPE_ZERO || fpv.exponent < 0)
        return 0;
    else if (fpv.type == TYPE_INF)
        return fpv.sign ? 0 : UINT64_MAX;
    else if (fpv.type == TYPE_NAN)
        return fpv.sign ? 0 : UINT64_MAX;
    else if (fpv.sign)
        // Negative numbers are rounded up to zero
        return 0;
    else if (fpv.exponent >= 64)
        // Overflow
        return UINT64_MAX;

    // Implicit else, the value is a normal number
    __uint128_t result = fpv.significand >> 1;
    SET_SBIT(result, 0);
    int shift_amount = SIGNIFICAND_BITS - fpv.exponent - 1;
    if (shift_amount >= SIGNIFICAND_BITS) return 0;
    result >>=  shift_amount;

    #ifdef DEBUG_STORE
    printf("%" PRIu64 "\n", (uint64_t) result);
    #endif

    return result;
}
