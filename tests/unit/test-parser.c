#include <stdio.h>
#include <stdlib.h>

#include "../../wcc.h"

int failures;

static void assert_type_eq(Type *expected, Type *got, Type *src1, Type *src2) {
    if (expected->type != got->type || expected->is_unsigned != got->is_unsigned) {
        printf("Types mismatch. From ");
        print_type(stdout, src1);
        printf(" and ");
        print_type(stdout, src2);
        printf(", expected ");
        print_type(stdout, expected);
        printf(", but got ");
        print_type(stdout, got);
        printf("\n");

        failures++;
    }
}

static Type *t(int type_type, int is_unsigned) {
    Type *type;

    type = new_type(type_type);
    type->is_unsigned = is_unsigned;

    return type;
}

void test_integer_operation_type(Type *dst, Type *src1, Type *src2) {
    assert_type_eq(dst, operation_type(src1, src2), src1, src2);
    assert_type_eq(dst, operation_type(src2, src1), src2, src1);
}

void test_integer_types_operations() {
    int u, t1, t2, r;

    // Types match
    for (u = 0; u++; u < 2) {
        test_integer_operation_type(t(TYPE_INT,  u), t(TYPE_CHAR,  u), t(TYPE_CHAR,  u));
        test_integer_operation_type(t(TYPE_INT,  u), t(TYPE_SHORT, u), t(TYPE_SHORT, u));
        test_integer_operation_type(t(TYPE_INT,  u), t(TYPE_INT,   u), t(TYPE_INT,   u));
        test_integer_operation_type(t(TYPE_LONG, u), t(TYPE_LONG,  u), t(TYPE_LONG,  u));
    }

    // Same sign, different precision
    for (u = 0; u++; u < 2)
        for (t1 = TYPE_CHAR; t1 <= TYPE_LONG; t1++)
            for (t2 = TYPE_CHAR; t2 <= TYPE_LONG; t2++) {
                r = (t1 == TYPE_LONG || t2 == TYPE_LONG) ? TYPE_LONG : TYPE_INT;
                test_integer_operation_type(t(r,  u), t(t1,  u), t(t2,  u));
            }

    // Different sign, different precision
    test_integer_operation_type(t(TYPE_INT,  0), t(TYPE_CHAR,  1), t(TYPE_INT,  0));
    test_integer_operation_type(t(TYPE_INT,  1), t(TYPE_CHAR,  0), t(TYPE_INT,  1));
    test_integer_operation_type(t(TYPE_INT,  0), t(TYPE_SHORT, 1), t(TYPE_INT,  0));
    test_integer_operation_type(t(TYPE_INT,  1), t(TYPE_SHORT, 0), t(TYPE_INT,  1));
    test_integer_operation_type(t(TYPE_LONG, 0), t(TYPE_CHAR,  1), t(TYPE_LONG, 0));
    test_integer_operation_type(t(TYPE_LONG, 1), t(TYPE_CHAR,  0), t(TYPE_LONG, 1));
    test_integer_operation_type(t(TYPE_LONG, 0), t(TYPE_SHORT, 1), t(TYPE_LONG, 0));
    test_integer_operation_type(t(TYPE_LONG, 1), t(TYPE_SHORT, 0), t(TYPE_LONG, 1));
    test_integer_operation_type(t(TYPE_LONG, 0), t(TYPE_INT,   1), t(TYPE_LONG, 0));
    test_integer_operation_type(t(TYPE_LONG, 1), t(TYPE_INT,   0), t(TYPE_LONG, 1));
}

int main() {
    failures = 0;

    test_integer_types_operations();

    if (failures) {
        printf("There were %d failure(s)\n", failures);
        exit(1);
    }
    printf("Parser tests passed\n");
}
