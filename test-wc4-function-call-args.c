#include "test-lib.h"

int verbose;
int passes;
int failures;

int fca0() {
    assert_int(1, 1, "function call with 0 args");
}

int fca1(int i1) {
    assert_int(1, i1, "function call with 1 arg");
}

int fca2(int i1, int i2) {
    assert_int(12, 10 * i1 + i2, "function call with 2 args");
}

int fca3(int i1, int i2, int i3) {
    assert_int(123, 100 * i1 + 10 * i2 + i3, "function call with 3 args");
}

int fca4(int i1, int i2, int i3, int i4) {
    assert_int(1234, 1000 * i1 + 100 * i2 + 10 * i3 + i4, "function call with 4 args");
}

int fca5(int i1, int i2, int i3, int i4, int i5) {
    assert_int(12345, 10000 * i1 + 1000 * i2 + 100 * i3 + 10 * i4 + i5, "function call with 5 args");
}

int fca6(int i1, int i2, int i3, int i4, int i5, int i6) {
    assert_int(123456, 100000 * i1 + 10000 * i2 + 1000 * i3 + 100 * i4 + 10 * i5 + i6, "function call with 6 args");
}

int fca7(int i1, int i2, int i3, int i4, int i5, int i6, int i7) {
    assert_int(1234567, 1000000 * i1 + 100000 * i2 + 10000 * i3 + 1000 * i4 + 100 * i5 + 10 * i6 + i7, "function call with 7 args");
}

int fca8(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8) {
    assert_int(12345678, 10000000 * i1 + 1000000 * i2 + 100000 * i3 + 10000 * i4 + 1000 * i5 + 100 * i6 + 10 * i7 + i8, "function call with 8 args");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    fca0();
    fca1(1);
    fca2(1, 2);
    fca3(1, 2, 3);
    fca4(1, 2, 3, 4);
    fca5(1, 2, 3, 4, 5);
    fca6(1, 2, 3, 4, 5, 6);
    fca7(1, 2, 3, 4, 5, 6, 7);
    fca8(1, 2, 3, 4, 5, 6, 7, 8);

    finalize();
}
