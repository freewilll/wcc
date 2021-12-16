#include "../test-lib.h"

int verbose;
int passes;
int failures;

void test_while() {
    int i;
    int c1, c2;

    c1 = c2 = 0;
    i = 0; while (i < 3) { c1 = c1 * 10 + i; i++; }
    i = 0; while (i++ < 3) c2 = c2 * 10 + i;
    assert_int( 12, c1, "while 1");
    assert_int(123, c2, "while 2");
}

void test_for() {
    int i;
    int c1, c2;

    c1 = c2 = 0;

    for (i = 0; i < 10; i++) c1 = c1 * 10 + i;

    for (i = 0; i < 10; i++) {
        if (i == 5) continue;
        c2 = c2 * 10 + i;
    }

    assert_int(123456789, c1, "for 1");
    assert_int(12346789,  c2, "for 2");
}

void test_for_statement_combinations() {
    int i;
    int c;

    i = 0; c = 0; for(    ;        ;    ) { c++; if (i == 10) break; i++; } assert_int(10, i, "for combinations i 1"); assert_int(11, c, "for combinations c 1");
    i = 0; c = 0; for(    ;        ; i++) { c++; if (i == 10) break; }      assert_int(10, i, "for combinations i 2"); assert_int(11, c, "for combinations c 2");
    i = 0; c = 0; for(    ; i<10   ;    ) { c++; i++; }                     assert_int(10, i, "for combinations i 3"); assert_int(10, c, "for combinations c 3");
    i = 0; c = 0; for(    ; i<10   ; i++) { c++; }                          assert_int(10, i, "for combinations i 4"); assert_int(10, c, "for combinations c 4");
           c = 0; for(i=0 ;        ;    ) { c++; if (i == 10) break; i++; } assert_int(10, i, "for combinations i 5"); assert_int(11, c, "for combinations c 5");
           c = 0; for(i=0 ;        ; i++) { c++; if (i == 10) break; }      assert_int(10, i, "for combinations i 6"); assert_int(11, c, "for combinations c 6");
           c = 0; for(i=0 ; i<10   ;    ) { c++; i++; }                     assert_int(10, i, "for combinations i 7"); assert_int(10, c, "for combinations c 7");
           c = 0; for(i=0 ; i<10   ; i++) { c++; }                          assert_int(10, i, "for combinations i 8"); assert_int(10, c, "for combinations c 8");
}

void test_while_continue() {
    int i, c1;

    c1 = i = 0;
    while (i++ < 5) {
        c1 = c1 * 10 + i;
        continue;
        c1 = c1 * 10 + 9;
    }
    assert_int(12345, c1, "while continue");
}

void test_nested_while_continue() {
    int i, c1;
    char *s;

    i = c1 = 0;
    s = "foo";

    while (s[i++] != 0) {
        c1 = c1 * 10 + i;
        while (0);
        continue;
    }

    assert_int(123, c1, "nested while/continue");
}

void test_do_while() {
    int i = 1;
    int c1 = 0;
    int c2 = 0;

    do {
        c1++;
        i++;
        if (i == 5) continue;
        c2++;
    } while (i != 10);

    assert_int(9, c1, "do_while 1");
    assert_int(8, c2, "do_while 2");

    i = c1 = c2 = 0;
    do {
        c1++;
        i++;
        if (i == 5) break;
        c2++;
    } while (i != 10);

    assert_int(5, c1, "do_while 3");
    assert_int(4, c2, "do_while 4");
}

void test_goto() {
    int i = 0, c = 0;

loop:
    i++;
    c = c * 10 + i;
    if (i < 10) goto loop;
    c--;
    goto done;

    c--;

done:;
    assert_int(1234567899, c, "Goto");
}

int test_compilation_crash_on_unreachable_code() {
    // A bug in SSA caused this to fail to compile.
    do continue; while(1);
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_while();
    test_for();
    test_for_statement_combinations();
    test_while_continue();
    test_nested_while_continue();
    test_do_while();
    test_goto();

    finalize();
}
