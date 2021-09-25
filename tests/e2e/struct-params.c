#include <stdio.h>

#include "../test-lib.h"
#include "struct-params.h"

// Test ABI for functions accepting structs
void accept_spf(struct spf spf) { assert_float(1.1, spf.f1, "accept_spf"); }
void accept_spd(struct spd spd) { assert_double(2.1, spd.d1, "accept_spd"); }
void accept_spdf(struct spdf spdf) { assert_float(3.1, spdf.d1, "accept_spf"); assert_double(4.1, spdf.f1, "accept_spdf"); }
void accept_sff(struct sff sff) { assert_float(5.1, sff.f1, "accept_spf"); assert_float(6.1, sff.f2, "accept_sff"); }
void accept_sdd(struct sdd sdd) { assert_float(7.1, sdd.d1, "accept_spf"); assert_float(8.1, sdd.d2, "accept_sdd"); }


void accept_sffff(struct sffff sffff) {
    assert_float(1.1, sffff.f1, "accept_spfff");
    assert_float(2.1, sffff.f2, "accept_sffff");
    assert_float(3.1, sffff.f3, "accept_spfff");
    assert_float(4.1, sffff.f4, "accept_sffff");
}
