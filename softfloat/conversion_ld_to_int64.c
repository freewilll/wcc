#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

int64_t __wcc_sf_convert_ld_to_int64(long double ld) {
    FpValue fpv = __wcc_sf_load_ld(ld);

    #ifdef DEBUG_STORE
    printf("Converting to int64         ");
    print_fpv(&fpv);
    #endif

    if (fpv.type == TYPE_ZERO || fpv.exponent < 0)
        return 0;
    else if (fpv.type == TYPE_INF)
        return fpv.sign ? INT64_MIN : INT64_MAX;
    else if (fpv.type == TYPE_NAN)
        return fpv.sign ? 0 : INT64_MAX;
    else if (fpv.exponent >= 63)
        // Overflow
        return fpv.sign ? INT64_MIN : INT64_MAX;

    // Implicit else, the value is a normal number
    __uint128_t result = fpv.significand >> 1;
    SET_SBIT(result, 0);
    int shift_amount = SIGNIFICAND_BITS - fpv.exponent - 1;
    if (shift_amount >= SIGNIFICAND_BITS) return 0;
    result >>=  shift_amount;

    // Flip the sign if it's negative
    if (fpv.sign) result = -result;

    #ifdef DEBUG_STORE
    printf("%ld\n", (int64_t) result);
    #endif

    return result;
}
