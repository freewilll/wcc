#include <stdio.h>
#include <stdlib.h>

#include "../test-lib.h"
#include "test-shlib.h"

int verbose;
int passes;
int failures;

void test_direct_access() {
    assert_int(1, c,  "Increment c 1");  inc_c();  assert_int(2, c,  "Increment c 2");
    assert_int(2, s,  "Increment i 1");  inc_s();  assert_int(3, s,  "Increment i 2");
    assert_int(3, i,  "Increment s 1");  inc_i();  assert_int(4, i,  "Increment s 2");
    assert_int(4, l,  "Increment l 1");  inc_l();  assert_int(5, l,  "Increment l 2");
    assert_int(5, uc, "Increment uc 1"); inc_uc(); assert_int(6, uc, "Increment uc 2");
    assert_int(6, us, "Increment ui 1"); inc_us(); assert_int(7, us, "Increment ui 2");
    assert_int(7, ui, "Increment us 1"); inc_ui(); assert_int(8, ui, "Increment us 2");
    assert_int(8, ul, "Increment ul 1"); inc_ul(); assert_int(9, ul, "Increment ul 2");

    assert_float( 1.1, f, "Increment f 1"); inc_f(); assert_float( 2.1, f, "Increment f 2");
    assert_double(2.1, d, "Increment d 1"); inc_d(); assert_double(3.1, d, "Increment d 2");

    assert_long_double(3.1, ld, "Increment d 1"); inc_ld(); assert_long_double(4.1, ld, "Increment d 2");

    pi = malloc(2 * sizeof(int));
    pi[0] = 10;
    pi[1] = 20;

    // Pointer
    assert_int(10, *pi, "Increment pi1 1"); inc_pi(); assert_int(20, *pi, "Increment pi1 2");

    // Struct
    assert_int(1, st.i, "Increment st.i 1");
    assert_int(2, st.j, "Increment st.j 1");
    inc_st();
    assert_int(2, st.i, "Increment st.i 2");
    assert_int(3, st.j, "Increment st.j 2");

    // Array
    assert_int(1, ia[0], "Increment ia[0] 1");
    assert_int(2, ia[1], "Increment ia[1] 1");
    inc_ia();
    assert_int(2, ia[0], "Increment ia[0] 2");
    assert_int(3, ia[1], "Increment ia[1] 2");

    i = 42;
    assert_int(42, fri(), "Pointer to function");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_direct_access();
    test_address_of();

    finalize();
}
