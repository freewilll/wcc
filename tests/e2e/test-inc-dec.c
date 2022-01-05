#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

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
    char c, *pc, **ppc;
    short s, *ps, **pps;
    int i, *pi, **ppi;
    long l, *pl, **ppl;
    unsigned char uc, *upc, **uppc;
    unsigned short us, *ups, **upps;
    unsigned int ui, *upi, **uppi;
    unsigned long ul, *upl, **uppl;

    c = pc = ppc = uc = upc = uppc = 0;;
    s = ps = pps = us = ups = upps = 0;;
    i = pi = ppi = ui = upi = uppi = 0;;
    l = pl = ppl = ul = upl = uppl = 0;;

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

    uc++;   assert_int(1, uc,   "++- size u1a");    uc--; assert_int(0, uc,   "++- size u1b"); ++uc;   assert_int(1, uc,   "++- size u1c");    uc--; assert_int(0, uc,   "++- size u1d");
    upc++;  assert_int(1, upc,  "++- size u2a");   upc--; assert_int(0, upc,  "++- size u2b"); ++upc;  assert_int(1, upc,  "++- size u2c");   upc--; assert_int(0, upc,  "++- size u2d");
    uppc++; assert_int(8, uppc, "++- size u3a");  uppc--; assert_int(0, uppc, "++- size u3b"); ++uppc; assert_int(8, uppc, "++- size u3c");  uppc--; assert_int(0, uppc, "++- size u3d");
    us++;   assert_int(1, us,   "++- size u4a");    us--; assert_int(0, us,   "++- size u4b"); ++us;   assert_int(1, us,   "++- size u4c");    us--; assert_int(0, us,   "++- size u4d");
    ups++;  assert_int(2, ups,  "++- size u5a");   ups--; assert_int(0, ups,  "++- size u5b"); ++ups;  assert_int(2, ups,  "++- size u5c");   ups--; assert_int(0, ups,  "++- size u5d");
    upps++; assert_int(8, upps, "++- size u6a");  upps--; assert_int(0, upps, "++- size u6b"); ++upps; assert_int(8, upps, "++- size u6c");  upps--; assert_int(0, upps, "++- size u6d");
    ui++;   assert_int(1, ui,   "++- size u7a");    ui--; assert_int(0, ui,   "++- size u7b"); ++ui;   assert_int(1, ui,   "++- size u7c");    ui--; assert_int(0, ui,   "++- size u7d");
    upi++;  assert_int(4, upi,  "++- size u8a");   upi--; assert_int(0, upi,  "++- size u8b"); ++upi;  assert_int(4, upi,  "++- size u8c");   upi--; assert_int(0, upi,  "++- size u8d");
    uppi++; assert_int(8, uppi, "++- size u9a");  uppi--; assert_int(0, uppi, "++- size u9b"); ++uppi; assert_int(8, uppi, "++- size u9c");  uppi--; assert_int(0, uppi, "++- size u9d");
    ul++;   assert_int(1, ul,   "++- size uaa");    ul--; assert_int(0, ul,   "++- size uab"); ++ul;   assert_int(1, ul,   "++- size uac");    ul--; assert_int(0, ul,   "++- size uad");
    upl++;  assert_int(8, upl,  "++- size uba");   upl--; assert_int(0, upl,  "++- size ubb"); ++upl;  assert_int(8, upl,  "++- size ubc");   upl--; assert_int(0, upl,  "++- size ubd");
    uppl++; assert_int(8, uppl, "++- size uca");  uppl--; assert_int(0, uppl, "++- size ucb"); ++uppl; assert_int(8, uppl, "++- size ucc");  uppl--; assert_int(0, uppl, "++- size ucd");
}

int test_prefix_inc_bug() {
    unsigned char *puc;
    unsigned int i;

    puc = malloc(2 * sizeof(char));

    i = 1;
    puc[0] = 0;
    puc[1] = 0x81;

    assert_int(129, (i << 7) | (*++puc & 0x7f), "Prefix ++ bug");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_prefix_inc_dec();
    test_postfix_inc_dec();
    test_inc_dec_sizes();
    test_prefix_inc_bug();

    finalize();
}
