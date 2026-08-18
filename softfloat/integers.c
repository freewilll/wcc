#include "softfloat.h"

#if defined(DEBUG_LOAD) || defined(DEBUG_STORE)
#include <stdio.h>
#include <inttypes.h>

#include "testlib.h"
#endif

int64_t convert_fpv_to_int64(FpValue *fpv) {
    #ifdef DEBUG_STORE
    printf("Converting to int64         ");
    print_fpv(fpv);
    #endif

    if (fpv->type == TYPE_ZERO || fpv->exponent < 0)
        return 0;
    else if (fpv->type == TYPE_INF)
        return fpv->sign ? INT64_MIN : INT64_MAX;
    else if (fpv->type == TYPE_NAN)
        return fpv->sign ? 0 : INT64_MAX;
    else if (fpv->exponent >= 63)
        // Overflow
        return fpv->sign ? INT64_MIN : INT64_MAX;

    // Implicit else, the value is a normal number
    __uint128_t result = fpv->significand >> 1;
    SET_SBIT(result, 0);
    int shift_amount = SIGNIFICAND_BITS - fpv->exponent - 1;
    if (shift_amount >= SIGNIFICAND_BITS) return 0;
    result >>=  shift_amount;

    // Flip the sign if it's negative
    if (fpv->sign) result = -result;

    #ifdef DEBUG_STORE
    printf("%ld\n", (int64_t) result);
    #endif

    return result;
}

uint64_t convert_fpv_to_uint64(FpValue *fpv) {
    #ifdef DEBUG_STORE
    printf("Converting to uint64        ");
    print_fpv(fpv);
    #endif

    if (fpv->type == TYPE_ZERO || fpv->exponent < 0)
        return 0;
    else if (fpv->type == TYPE_INF)
        return fpv->sign ? 0 : UINT64_MAX;
    else if (fpv->type == TYPE_NAN)
        return fpv->sign ? 0 : UINT64_MAX;
    else if (fpv->sign)
        // Negative numbers are rounded up to zero
        return 0;
    else if (fpv->exponent >= 64)
        // Overflow
        return UINT64_MAX;

    // Implicit else, the value is a normal number
    __uint128_t result = fpv->significand >> 1;
    SET_SBIT(result, 0);
    int shift_amount = SIGNIFICAND_BITS - fpv->exponent - 1;
    if (shift_amount >= SIGNIFICAND_BITS) return 0;
    result >>=  shift_amount;

    #ifdef DEBUG_STORE
    printf("%" PRIu64 "\n", (uint64_t) result);
    #endif

    return result;
}

// Convert a signed or unsigned 64 bit integer into a floating point value.
static FpValue convert_common_int64_to_fpv(uint64_t i, int is_signed) {
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

FpValue convert_int64_to_fpv(int64_t i) {
    return convert_common_int64_to_fpv(i, 1);
}

FpValue convert_uint64_to_fpv(uint64_t i) {
    return convert_common_int64_to_fpv(i, 0);
}
