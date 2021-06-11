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

    i = 0;        for(    ;        ;    ) { if (i == 10) break; i++; } assert_int(10, i, "for combinations 1");
    i = 0;        for(    ;        ; i++) { if (i == 10) break; }      assert_int(10, i, "for combinations 2");
    i = 0;        for(    ;  i<10  ;    ) i++;                         assert_int(10, i, "for combinations 3");
    i = 0; c = 0; for(    ;  i<10  ; i++) c++;                         assert_int(10, i, "for combinations 4");
                  for(i=0 ;        ;    ) { if (i == 10) break; i++; } assert_int(10, i, "for combinations 5");
                  for(i=0 ;        ; i++) { if (i == 10) break; }      assert_int(10, i, "for combinations 6");
                  for(i=0 ; i<10   ;    ) i++;                         assert_int(10, i, "for combinations 7");
           c = 0; for(i=0 ; i<10   ; i++) c++;                         assert_int(10, i, "for combinations 8");

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

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_while();
    test_for();
    test_for_statement_combinations();
    test_while_continue();
    test_nested_while_continue();

    finalize();
}
