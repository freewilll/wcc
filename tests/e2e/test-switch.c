#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

static int run_switch_without_default(int i) {
    int result = -10;

    switch(i) {
        result *= 11;

        case 1:
            result += 1;
        case 2:
            result *= 2;
            break;
        case 3:
            result += 3;
        case 4:
            result *= 5;
    }

    return result;
}

static int run_switch_with_default(int i) {
    int result = -10;

    switch(i) {
        result *= 11;

        case 1:
            result += 1;
        case 2:
            result *= 2;
            break;
        case 3:
            result += 3;
        case 4:
            result *= 5;
        default:
            result += 100;
    }

    return result;
}

static int run_nested_switch(int i, int j) {
    int ri = 1, rj = 1;
    switch(i) {
        case 1:
            ri++;
            switch (j) {
                case 1:
                    rj += 1;
                case 2:
                    rj += 1;
                    break;
                case 3:
                    rj += 1;
            }
        case 2:
            ri++;
            switch (j) {
                case 1:
                    rj += 2;
                case 2:
                    rj += 2;
                    break;
                case 3:
                    rj += 2;
            }
            break;
        case 3:
            switch (j) {
                case 1:
                    rj += 3;
                case 2:
                    rj += 3;
                    break;
                case 3:
                    rj += 3;
            }
            ri++;
    }
    return ri * 10 + rj;
}

int run_deeply_nested_switch(int i) {
    switch(i) {
        {
            assert_int(0, 1, "Should not get here; it's dead code");

            case 1:
                return 1;
            case 3:
                return 3;
            {
                case 4:
                    return 4;
                {
                    case 5:
                        return 5;
                    default:
                        return -1;
                }
            }

        }
        case 2:
            return 2;
    }
}


static int test_int_switch_with_mixed_type_cases(int i) {
    int result = 0;
    // The case statements should be truncated to ints
    switch(i) {
        case 0x000000000:
            result |= 1;
            break;
        case 0x000000001:
            result |= 2;
            break;
        case 0x100000002:   // This should be truncated to 0x2
            result |= 8;
            break;
        default:
            result |= 16;
    }
    return result;
}

static int test_long_switch_with_mixed_type_cases(long ll) {
    int result = 0;
    switch(ll) {
        case 0x000000000:
            result |= 1;
            break;
        case 0x000000001:
            result |= 2;
            break;
        case 0x000000002:
            result |= 4;
            break;
        case 0x100000000:
            result |= 8;
            break;
        case 0x100000001:
            result |= 16;
            break;
        case 0x100000002:
            result |= 32;
            break;
        default:
            result |= 64;
    }
    return result;
}

typedef enum { I, J } E;

int test_switch_with_enum(E e) {
    int i = 0;

    switch (e) {
        case I:
            i = 1;
            break;
        case J:
            i = 2;
            break;
    }

    return i;
}

int test_multiple_cases(int i) {
    switch (i) {
        case 1:
        case 2:
            return 12;
        case 3:
            return 3;
        default:
            return 0;
    }
}

static void test_switch() {
    assert_int(-10, run_switch_without_default(0), "Test switch without default 1");
    assert_int(-18, run_switch_without_default(1), "Test switch without default 2");
    assert_int(-20, run_switch_without_default(2), "Test switch without default 3");
    assert_int(-35, run_switch_without_default(3), "Test switch without default 4");
    assert_int(-50, run_switch_without_default(4), "Test switch without default 5");

    assert_int( 90, run_switch_with_default(0), "Test switch with default 1");
    assert_int(-18, run_switch_with_default(1), "Test switch with default 2");
    assert_int(-20, run_switch_with_default(2), "Test switch with default 3");
    assert_int(65,  run_switch_with_default(3), "Test switch with default 4");
    assert_int(50,  run_switch_with_default(4), "Test switch with default 5");

    assert_int(11,  run_nested_switch(0, 0), "Test nested switch 0, 0");
    assert_int(11,  run_nested_switch(0, 1), "Test nested switch 0, 1");
    assert_int(11,  run_nested_switch(0, 2), "Test nested switch 0, 2");
    assert_int(11,  run_nested_switch(0, 3), "Test nested switch 0, 3");
    assert_int(31,  run_nested_switch(1, 0), "Test nested switch 1, 0");
    assert_int(37,  run_nested_switch(1, 1), "Test nested switch 1, 1");
    assert_int(34,  run_nested_switch(1, 2), "Test nested switch 1, 2");
    assert_int(34,  run_nested_switch(1, 3), "Test nested switch 1, 3");
    assert_int(21,  run_nested_switch(2, 0), "Test nested switch 2, 0");
    assert_int(25,  run_nested_switch(2, 1), "Test nested switch 2, 1");
    assert_int(23,  run_nested_switch(2, 2), "Test nested switch 2, 2");
    assert_int(23,  run_nested_switch(2, 3), "Test nested switch 2, 3");
    assert_int(21,  run_nested_switch(3, 0), "Test nested switch 3, 0");
    assert_int(27,  run_nested_switch(3, 1), "Test nested switch 3, 1");
    assert_int(24,  run_nested_switch(3, 2), "Test nested switch 3, 2");
    assert_int(24,  run_nested_switch(3, 3), "Test nested switch 3, 3");

    assert_int(-1, run_deeply_nested_switch(-1), "Test deeply nested switch -1");
    assert_int( 1, run_deeply_nested_switch( 1), "Test deeply nested switch 1");
    assert_int( 2, run_deeply_nested_switch( 2), "Test deeply nested switch 2");
    assert_int( 3, run_deeply_nested_switch( 3), "Test deeply nested switch 3");
    assert_int( 4, run_deeply_nested_switch( 4), "Test deeply nested switch 4");
    assert_int( 5, run_deeply_nested_switch( 5), "Test deeply nested switch 5");

    assert_int(1,  test_int_switch_with_mixed_type_cases(0),            "Test int switch with mixed type cases 1");
    assert_int(2,  test_int_switch_with_mixed_type_cases(1),            "Test int switch with mixed type cases 2");
    assert_int(8,  test_int_switch_with_mixed_type_cases(2),            "Test int switch with mixed type cases 3");
    assert_int(2,  test_int_switch_with_mixed_type_cases(0x100000001),  "Test int switch with mixed type cases 4");
    assert_int(8,  test_int_switch_with_mixed_type_cases(0x100000002),  "Test int switch with mixed type cases 5");
    assert_int(16, test_int_switch_with_mixed_type_cases(3),            "Test int switch with mixed type cases 6");

    assert_int(1,  test_long_switch_with_mixed_type_cases(0),           "Test long switch with mixed type cases 1");
    assert_int(2,  test_long_switch_with_mixed_type_cases(1),           "Test long switch with mixed type cases 2");
    assert_int(4,  test_long_switch_with_mixed_type_cases(2),           "Test long switch with mixed type cases 3");
    assert_int(64, test_long_switch_with_mixed_type_cases(3),           "Test long switch with mixed type cases 4");
    assert_int(1,  test_long_switch_with_mixed_type_cases(0x000000000), "Test long switch with mixed type cases 5");
    assert_int(2,  test_long_switch_with_mixed_type_cases(0x000000001), "Test long switch with mixed type cases 6");
    assert_int(4,  test_long_switch_with_mixed_type_cases(0x000000002), "Test long switch with mixed type cases 7");
    assert_int(8,  test_long_switch_with_mixed_type_cases(0x100000000), "Test long switch with mixed type cases 8");
    assert_int(16, test_long_switch_with_mixed_type_cases(0x100000001), "Test long switch with mixed type cases 9");
    assert_int(32, test_long_switch_with_mixed_type_cases(0x100000002), "Test long switch with mixed type cases 10");

    assert_int(1, test_switch_with_enum(I), "Switch with enum I");
    assert_int(2, test_switch_with_enum(J), "Switch with enum J");

    int i;
    i = 0; switch(1) case 1: i = 1; assert_int(1, i, "Silly switch case without curlies 1");
    i = 0; switch(1) case 2: i = 1; assert_int(0, i, "Silly switch case without curlies 2");

    assert_int(12, test_multiple_cases(1), "Multiple switch cases 1");
    assert_int(12, test_multiple_cases(2), "Multiple switch cases 2");
    assert_int(3,  test_multiple_cases(3), "Multiple switch cases 3");
    assert_int(0,  test_multiple_cases(4), "Multiple switch cases 4");
}

int main(int argc, char **argv) {
    parse_args(argc, argv, &verbose);

    test_switch();

    finalize();
}
