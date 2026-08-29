#include "softfloat.h"


int compare(FpValue *a, FpValue *b) {
    // Zero cmp zero
    if (a->type == TYPE_ZERO && b->type == TYPE_ZERO)
        return 0;

    // Zero cmp infinity
    if (a->type == TYPE_ZERO && b->type == TYPE_INF)
        return b->sign ? 1 : -1;

    // infinity cmp zero
    if (a->type == TYPE_INF && b->type == TYPE_ZERO)
        return a->sign ? -1 : 1;

    // Infinity cmp infinity
    if (a->type == TYPE_INF && b->type == TYPE_INF) {
        if (a->sign == b->sign)
            return 0;
        else if (a->sign < b->sign)
            return 1; // a > b
        else
            return -1; // a < b
    }

    // Infinity cmp finite
    if (a->type == TYPE_INF)
        return a->sign ? -1 : 1;

    // Finite cmp infinity
    if (b->type == TYPE_INF)
        return b->sign ? 1 : -1;

    // Zero cmp finite
    if (a->type == TYPE_ZERO)
        return b->sign ? 1 : -1;

    // Finite cmp zero
    if (b->type == TYPE_ZERO)
        return a->sign ? -1 : 1;

    // Implicit else: finite cmp finite

    // Negative cmp positive
    if (a->sign && !b->sign) {
        return -1;
    }

    // Positive cmp negative
    if (!a->sign && b->sign) {
        return 1;
    }

    // Implicit else: the operands have the same sign.

    int flip = 0;
    int result;

    // If they are both negative, compare as if they were both positive then negate the result.
    if (a->sign && b->sign) flip = 1;

    if (a->exponent < b->exponent)
        result = -1;
    else if (a->exponent > b->exponent)
        result = 1;
    else if (a->significand < b->significand)
        result = -1;
    else if (a->significand > b->significand)
        result = 1;
    else
        result = 0;

    if (flip) result = -result;

    return result;
}

#define COMPARE_FUNC(cmp_op, nan_result) { \
    FpValue fpv_a = load_ld(a); \
    FpValue fpv_b = load_ld(b); \
    if (fpv_a.type == TYPE_NAN || fpv_b.type == TYPE_NAN) return nan_result; \
    int result = compare(&fpv_a, &fpv_b); \
    return result cmp_op 0; \
}

int ld_eq(long double a, long double b) COMPARE_FUNC(==, 0)
int ld_ne(long double a, long double b) COMPARE_FUNC(!=, 1)
int ld_lt(long double a, long double b) COMPARE_FUNC(<,  0)
int ld_gt(long double a, long double b) COMPARE_FUNC(>,  0)
int ld_le(long double a, long double b) COMPARE_FUNC(<=, 0)
int ld_ge(long double a, long double b) COMPARE_FUNC(>=, 0)
