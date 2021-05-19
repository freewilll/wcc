#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int verbose;
int passes;
int failures;

void test_pointer_addition() {
    int *pi1, *pi2;
    int i;
    char c;
    short s;
    long l;

    c = 1;
    s = 1;
    i = 1;
    l = 1;
    pi1 = 0;
    pi2 = 0;

    assert_long(4, pi1 + c, "Pointer addition char");
    assert_long(4, pi1 + s, "Pointer addition short");
    assert_long(4, pi1 + i, "Pointer addition int");
    assert_long(4, pi1 + l, "Pointer addition long");
}

void test_prefix_inc_dec() {
    int i;
    i = 0;
    assert_int(1, ++i, "prefix inc dec 1");
    assert_int(2, ++i, "prefix inc dec 2");
    assert_int(1, --i, "prefix inc dec 3");
    assert_int(0, --i, "prefix inc dec 4");
}

void test_postfix_inc_dec() {
    int i;
    i = 0;
    assert_int(0, i++, "postfix inc dec 1");
    assert_int(1, i++, "postfix inc dec 2");
    assert_int(2, i--, "postfix inc dec 3");
    assert_int(1, i--, "postfix inc dec 4");
}

void test_inc_dec_sizes() {
    char c;
    char *pc;
    char **ppc;
    short s;
    short *ps;
    short **pps;
    int i;
    int *pi;
    int **ppi;
    long l;
    long *pl;
    long **ppl;

    c = 0;
    pc = 0;
    ppc = 0;
    s = 0;
    ps = 0;
    pps = 0;
    i = 0;
    pi = 0;
    ppi = 0;
    l = 0;
    pl = 0;
    ppl = 0;

    c++;   assert_int(1, c,   "++- size 1a");    c--; assert_int(0, c,   "++- size 1b"); ++c;   assert_int(1, c,   "++- size 1c");    c--; assert_int(0, c,   "++- size 1d");
    pc++;  assert_int(1, pc,  "++- size 2a");   pc--; assert_int(0, pc,  "++- size 2b"); ++pc;  assert_int(1, pc,  "++- size 2c");   pc--; assert_int(0, pc,  "++- size 2d");
    ppc++; assert_int(8, ppc, "++- size 3a");  ppc--; assert_int(0, ppc, "++- size 3b"); ++ppc; assert_int(8, ppc, "++- size 3c");  ppc--; assert_int(0, ppc, "++- size 3d");
    s++;   assert_int(1, s,   "++- size 4a");    s--; assert_int(0, s,   "++- size 4b"); ++s;   assert_int(1, s,   "++- size 4c");    s--; assert_int(0, s,   "++- size 4d");
    ps++;  assert_int(2, ps,  "++- size 5a");   ps--; assert_int(0, ps,  "++- size 5b"); ++ps;  assert_int(2, ps,  "++- size 5c");   ps--; assert_int(0, ps,  "++- size 5d");
    pps++; assert_int(8, pps, "++- size 6a");  pps--; assert_int(0, pps, "++- size 6b"); ++pps; assert_int(8, pps, "++- size 6c");  pps--; assert_int(0, pps, "++- size 6d");
    i++;   assert_int(1, i,   "++- size 7a");    i--; assert_int(0, i,   "++- size 7b"); ++i;   assert_int(1, i,   "++- size 7c");    i--; assert_int(0, i,   "++- size 7d");
    pi++;  assert_int(4, pi,  "++- size 8a");   pi--; assert_int(0, pi,  "++- size 8b"); ++pi;  assert_int(4, pi,  "++- size 8c");   pi--; assert_int(0, pi,  "++- size 8d");
    ppi++; assert_int(8, ppi, "++- size 9a");  ppi--; assert_int(0, ppi, "++- size 9b"); ++ppi; assert_int(8, ppi, "++- size 9c");  ppi--; assert_int(0, ppi, "++- size 9d");
    l++;   assert_int(1, l,   "++- size aa");    l--; assert_int(0, l,   "++- size ab"); ++l;   assert_int(1, l,   "++- size ac");    l--; assert_int(0, l,   "++- size ad");
    pl++;  assert_int(8, pl,  "++- size ba");   pl--; assert_int(0, pl,  "++- size bb"); ++pl;  assert_int(8, pl,  "++- size bc");   pl--; assert_int(0, pl,  "++- size bd");
    ppl++; assert_int(8, ppl, "++- size ca");  ppl--; assert_int(0, ppl, "++- size cb"); ++ppl; assert_int(8, ppl, "++- size cc");  ppl--; assert_int(0, ppl, "++- size cd");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_pointer_addition();
    test_prefix_inc_dec();
    test_postfix_inc_dec();
    test_inc_dec_sizes();

    finalize();
}
