#include "softfloat.h"

#if defined(DEBUG_LOAD) || defined(DEBUG_STORE)
#include <stdio.h>
#include <inttypes.h>

#include "testlib.h"
#endif

// Convert a signed or unsigned 64 bit integer into a floating point value.
FpValue __wcc_sf_convert_common_int64_to_fpv(uint64_t i, int is_signed) {
    #ifdef DEBUG_LOAD
    printf("%ld\n", (int64_t) i);
    #endif

    FpValue fpv = {0, 0, 0, TYPE_ZERO};

    if (i == 0) return fpv;

    fpv.type = TYPE_NORMAL;

    // If we're handling a signed integer and it's negative, convert the
    // value to a positive and set sign to 1.
    if (is_signed && i & (1L << 63)) {
        i = - (int64_t) i;
        fpv.sign = 1;
    }

    int leading_bits = __builtin_clzll(i);
    fpv.exponent = 63 - leading_bits;

    // Shift it over enough so that the leading zero is shifted off
    int shift_amount = SIGNIFICAND_BITS + leading_bits - 63;
    fpv.significand = i;
    if (shift_amount < SIGNIFICAND_BITS) fpv.significand <<= shift_amount;

    #ifdef DEBUG_LOAD
    printf("Loaded                      ");
    print_fpv(&fpv);
    #endif

    return fpv;
}
