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
    assert_float(1.1, sffff.f1, "accept_sffff");
    assert_float(2.1, sffff.f2, "accept_sffff");
    assert_float(3.1, sffff.f3, "accept_sffff");
    assert_float(4.1, sffff.f4, "accept_sffff");
}

void accept_sffii(struct sffii sffii) {
    assert_float(1.1, sffii.f1, "accept_sffii");
    assert_float(2.1, sffii.f2, "accept_sffii");
    assert_int(3, sffii.i1, "accept_sffii");
    assert_int(4, sffii.i2, "accept_sffii");
}

void accept_siiff(struct siiff siiff) {
    assert_int(1, siiff.i1, "accept_siiff");
    assert_int(2, siiff.i2, "accept_siiff");
    assert_float(3.1, siiff.f1, "accept_siiff");
    assert_float(4.1, siiff.f2, "accept_siiff");
}

void accept_sifif(struct sifif sifif) {
    assert_int(1, sifif.i1, "accept_sifif");
    assert_float(2.1, sifif.f1, "accept_sifif");
    assert_int(3, sifif.i2, "accept_sifif");
    assert_float(4.1, sifif.f2, "accept_sifif");
}

void accept_sc1(struct sc1 sc1) {
    assert_int(1, sc1.c1, "accept_sc1 1");
}

void accept_sc2(struct sc2 sc2) {
    assert_int(1, sc2.c1, "accept_sc2 1");
    assert_int(2, sc2.c2, "accept_sc2 2");
}

void accept_sc3(struct sc3 sc3) {
    assert_int(1, sc3.c1, "accept_sc3 1");
    assert_int(2, sc3.c2, "accept_sc3 2");
    assert_int(3, sc3.c3, "accept_sc3 3");
}

void accept_sc4(struct sc4 sc4) {
    assert_int(1, sc4.c1, "accept_sc4 1");
    assert_int(2, sc4.c2, "accept_sc4 2");
    assert_int(3, sc4.c3, "accept_sc4 3");
    assert_int(4, sc4.c4, "accept_sc4 4");
}

void accept_sc5(struct sc5 sc5) {
    assert_int(1, sc5.c1, "accept_sc5 1");
    assert_int(2, sc5.c2, "accept_sc5 2");
    assert_int(3, sc5.c3, "accept_sc5 3");
    assert_int(4, sc5.c4, "accept_sc5 4");
    assert_int(5, sc5.c5, "accept_sc5 5");
}

void accept_sc6(struct sc6 sc6) {
    assert_int(1, sc6.c1, "accept_sc6 1");
    assert_int(2, sc6.c2, "accept_sc6 2");
    assert_int(3, sc6.c3, "accept_sc6 3");
    assert_int(4, sc6.c4, "accept_sc6 4");
    assert_int(5, sc6.c5, "accept_sc6 5");
    assert_int(6, sc6.c6, "accept_sc6 6");
}

void accept_sc7(struct sc7 sc7) {
    assert_int(1, sc7.c1, "accept_sc7 1");
    assert_int(2, sc7.c2, "accept_sc7 2");
    assert_int(3, sc7.c3, "accept_sc7 3");
    assert_int(4, sc7.c4, "accept_sc7 4");
    assert_int(5, sc7.c5, "accept_sc7 5");
    assert_int(6, sc7.c6, "accept_sc7 6");
    assert_int(7, sc7.c7, "accept_sc7 7");
}

void accept_sc8(struct sc8 sc8) {
    assert_int(1, sc8.c1, "accept_sc8 1");
    assert_int(2, sc8.c2, "accept_sc8 2");
    assert_int(3, sc8.c3, "accept_sc8 3");
    assert_int(4, sc8.c4, "accept_sc8 4");
    assert_int(5, sc8.c5, "accept_sc8 5");
    assert_int(6, sc8.c6, "accept_sc8 6");
    assert_int(7, sc8.c7, "accept_sc8 7");
    assert_int(8, sc8.c8, "accept_sc8 8");
}

void accept_sc9(struct sc9 sc9) {
    assert_int(1, sc9.c1, "accept_sc9 1");
    assert_int(2, sc9.c2, "accept_sc9 2");
    assert_int(3, sc9.c3, "accept_sc9 3");
    assert_int(4, sc9.c4, "accept_sc9 4");
    assert_int(5, sc9.c5, "accept_sc9 5");
    assert_int(6, sc9.c6, "accept_sc9 6");
    assert_int(7, sc9.c7, "accept_sc9 7");
    assert_int(8, sc9.c8, "accept_sc9 8");
    assert_int(9, sc9.c9, "accept_sc9 9");
}
