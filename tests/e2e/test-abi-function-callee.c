#include <stdio.h>

#include "../test-lib.h"
#include "test-abi.h"

// These functions are called from test-abi-function-caller.c

void accept_spf(struct spf spf) { assert_float(1.1, spf.f1, "accept_spf"); }
void accept_spd(struct spd spd) { assert_double(2.1, spd.d1, "accept_spd"); }
void accept_spdf(struct spdf spdf) { assert_float(3.1, spdf.d1, "accept_spdf"); assert_double(4.1, spdf.f1, "accept_spdf"); }
void accept_sff(struct sff sff) { assert_float(5.1, sff.f1, "accept_spf"); assert_float(6.1, sff.f2, "accept_sff"); }
void accept_sdd(struct sdd sdd) { assert_float(7.1, sdd.d1, "accept_spf"); assert_float(8.1, sdd.d2, "accept_sdd"); }

void accept_sffff(struct sffff sffff) {
    assert_float(1.1, sffff.f1, "accept_sffff");
    assert_float(2.1, sffff.f2, "accept_sffff");
    assert_float(3.1, sffff.f3, "accept_sffff");
    assert_float(4.1, sffff.f4, "accept_sffff");
}

void accept_sf9(struct sf9 sf9) {
    assert_float(1.1, sf9.f1, "accept_sf9");
    assert_float(2.1, sf9.f2, "accept_sf9");
    assert_float(3.1, sf9.f3, "accept_sf9");
    assert_float(4.1, sf9.f4, "accept_sf9");
    assert_float(5.1, sf9.f5, "accept_sf9");
    assert_float(6.1, sf9.f6, "accept_sf9");
    assert_float(7.1, sf9.f7, "accept_sf9");
    assert_float(8.1, sf9.f8, "accept_sf9");
    assert_float(9.1, sf9.f9, "accept_sf9");
}

// In aarch64, all args go into the registers
void accept_ffffsffff(float f1, float f2, float f3, float f4, struct sffff sffff) {
    assert_float(1.1, f1,       "accept_ffffsffff");
    assert_float(2.1, f2,       "accept_ffffsffff");
    assert_float(3.1, f3,       "accept_ffffsffff");
    assert_float(4.1, f4,       "accept_ffffsffff");
    assert_float(5.1, sffff.f1, "accept_ffffsffff");
    assert_float(6.1, sffff.f2, "accept_ffffsffff");
    assert_float(7.1, sffff.f3, "accept_ffffsffff");
    assert_float(8.1, sffff.f4, "accept_ffffsffff");
}

void accept_sffii(struct sffii sffii) {
    assert_float(1.1, sffii.f1, "accept_sffii");
    assert_float(2.1, sffii.f2, "accept_sffii");
    assert_int(3, sffii.i1, "accept_sffii");
    assert_int(4, sffii.i2, "accept_sffii");
}

void accept_sffiii(struct sffiii sffiii) {
    assert_float(1.1, sffiii.f1, "accept_sffiii");
    assert_float(2.1, sffiii.f2, "accept_sffiii");
    assert_int(3, sffiii.i1, "accept_sffiii");
    assert_int(4, sffiii.i2, "accept_sffiii");
    assert_int(5, sffiii.i3, "accept_sffiii");
}

void accept_siiff(struct siiff siiff) {
    assert_int(1, siiff.i1, "accept_siiff");
    assert_int(2, siiff.i2, "accept_siiff");
    assert_float(3.1, siiff.f1, "accept_siiff");
    assert_float(4.1, siiff.f2, "accept_siiff");
}

void accept_siifff(struct siifff siifff) {
    assert_int(1, siifff.i1, "accept_siifff");
    assert_int(2, siifff.i2, "accept_siifff");
    assert_float(3.1, siifff.f1, "accept_siifff");
    assert_float(4.1, siifff.f2, "accept_siifff");
    assert_float(5.1, siifff.f3, "accept_siifff");
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

void accept_sc3f(struct sc3f sc3f) {
    assert_int( 1,    sc3f.c1, "accept_sc3f 1");
    assert_int( 2,    sc3f.c2, "accept_sc3f 2");
    assert_int( 3,    sc3f.c3, "accept_sc3f 3");
    assert_float(4.1, sc3f.f1, "accept_sc3f 3");
}

void accept_si5(struct si5 si5) {
    assert_int(1, si5.i1, "accept_si5 1");
    assert_int(2, si5.i2, "accept_si5 2");
    assert_int(3, si5.i3, "accept_si5 3");
    assert_int(4, si5.i4, "accept_si5 4");
    assert_int(5, si5.i5, "accept_si5 5");
}

void accept_si6(struct si6 si6) {
    assert_int(1, si6.i1, "accept_si6 1");
    assert_int(2, si6.i2, "accept_si6 2");
    assert_int(3, si6.i3, "accept_si6 3");
    assert_int(4, si6.i4, "accept_si6 4");
    assert_int(5, si6.i5, "accept_si6 5");
    assert_int(6, si6.i6, "accept_si6 6");
}

void accept_si7(struct si7 si7) {
    assert_int(1, si7.i1, "accept_si7 1");
    assert_int(2, si7.i2, "accept_si7 2");
    assert_int(3, si7.i3, "accept_si7 3");
    assert_int(4, si7.i4, "accept_si7 4");
    assert_int(5, si7.i5, "accept_si7 5");
    assert_int(6, si7.i6, "accept_si7 6");
    assert_int(7, si7.i7, "accept_si7 7");
}

void accept_si8(struct si8 si8) {
    assert_int(1, si8.i1, "accept_si8 1");
    assert_int(2, si8.i2, "accept_si8 2");
    assert_int(3, si8.i3, "accept_si8 3");
    assert_int(4, si8.i4, "accept_si8 4");
    assert_int(5, si8.i5, "accept_si8 5");
    assert_int(6, si8.i6, "accept_si8 6");
    assert_int(7, si8.i7, "accept_si8 7");
    assert_int(8, si8.i8, "accept_si8 8");
}

void accept_si9(struct si9 si9) {
    assert_int(1, si9.i1, "accept_si9 1");
    assert_int(2, si9.i2, "accept_si9 2");
    assert_int(3, si9.i3, "accept_si9 3");
    assert_int(4, si9.i4, "accept_si9 4");
    assert_int(5, si9.i5, "accept_si9 5");
    assert_int(6, si9.i6, "accept_si9 6");
    assert_int(7, si9.i7, "accept_si9 7");
    assert_int(8, si9.i8, "accept_si9 8");
    assert_int(9, si9.i9, "accept_si9 9");
}

// Like accept_si9, except with a bunch of integer args first.
// This is meant for aarch64. Both structs end up being pointers
// to the stack in registers
void accept_si9_i6(int i1, int i2, int i3, int i4, int i5, int i6, struct si9 si91, struct si9 si92) {
    assert_int(1, i1, "accept_si9_6 1");
    assert_int(2, i2, "accept_si9_6 2");
    assert_int(3, i3, "accept_si9_6 3");
    assert_int(4, i4, "accept_si9_6 4");
    assert_int(5, i5, "accept_si9_6 5");
    assert_int(6, i6, "accept_si9_6 6");

    assert_int(11, si91.i1, "accept_si9_6 11");
    assert_int(12, si91.i2, "accept_si9_6 12");
    assert_int(13, si91.i3, "accept_si9_6 13");
    assert_int(14, si91.i4, "accept_si9_6 14");
    assert_int(15, si91.i5, "accept_si9_6 15");
    assert_int(16, si91.i6, "accept_si9_6 16");
    assert_int(17, si91.i7, "accept_si9_6 17");
    assert_int(18, si91.i8, "accept_si9_6 18");
    assert_int(19, si91.i9, "accept_si9_6 19");

    assert_int(21, si92.i1, "accept_si9_6 21");
    assert_int(22, si92.i2, "accept_si9_6 22");
    assert_int(23, si92.i3, "accept_si9_6 23");
    assert_int(24, si92.i4, "accept_si9_6 24");
    assert_int(25, si92.i5, "accept_si9_6 25");
    assert_int(26, si92.i6, "accept_si9_6 26");
    assert_int(27, si92.i7, "accept_si9_6 27");
    assert_int(28, si92.i8, "accept_si9_6 28");
    assert_int(29, si92.i9, "accept_si9_6 29");
}

// In aarch64, one struct has a pointer in a register, the other one a pointer in the stack
void accept_si9_i7(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct si9 si91, struct si9 si92) {
    assert_int(1, i1, "accept_si9_7 1");
    assert_int(2, i2, "accept_si9_7 2");
    assert_int(3, i3, "accept_si9_7 3");
    assert_int(4, i4, "accept_si9_7 4");
    assert_int(5, i5, "accept_si9_7 5");
    assert_int(6, i6, "accept_si9_7 6");
    assert_int(7, i7, "accept_si9_7 7");

    assert_int(11, si91.i1, "accept_si9_7 11");
    assert_int(12, si91.i2, "accept_si9_7 12");
    assert_int(13, si91.i3, "accept_si9_7 13");
    assert_int(14, si91.i4, "accept_si9_7 14");
    assert_int(15, si91.i5, "accept_si9_7 15");
    assert_int(16, si91.i6, "accept_si9_7 16");
    assert_int(17, si91.i7, "accept_si9_7 17");
    assert_int(18, si91.i8, "accept_si9_7 18");
    assert_int(19, si91.i9, "accept_si9_7 19");

    assert_int(21, si92.i1, "accept_si9_7 21");
    assert_int(22, si92.i2, "accept_si9_7 22");
    assert_int(23, si92.i3, "accept_si9_7 23");
    assert_int(24, si92.i4, "accept_si9_7 24");
    assert_int(25, si92.i5, "accept_si9_7 25");
    assert_int(26, si92.i6, "accept_si9_7 26");
    assert_int(27, si92.i7, "accept_si9_7 27");
    assert_int(28, si92.i8, "accept_si9_7 28");
    assert_int(29, si92.i9, "accept_si9_7 29");
}

// With 2 structs and 1 float in the stack in aarch64
void accept_si9_2_f9(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9, struct si9 si91, struct si9 si92) {
    assert_float(1.1, f1, "accept_si9_2_f9 1");
    assert_float(2.1, f2, "accept_si9_2_f9 2");
    assert_float(3.1, f3, "accept_si9_2_f9 3");
    assert_float(4.1, f4, "accept_si9_2_f9 4");
    assert_float(5.1, f5, "accept_si9_2_f9 5");
    assert_float(6.1, f6, "accept_si9_2_f9 6");
    assert_float(7.1, f7, "accept_si9_2_f9 7");
    assert_float(8.1, f8, "accept_si9_2_f9 8");
    assert_float(9.1, f9, "accept_si9_2_f9 9");

    assert_int(11, si91.i1, "accept_si9_2 11");
    assert_int(12, si91.i2, "accept_si9_2 12");
    assert_int(13, si91.i3, "accept_si9_2 13");
    assert_int(14, si91.i4, "accept_si9_2 14");
    assert_int(15, si91.i5, "accept_si9_2 15");
    assert_int(16, si91.i6, "accept_si9_2 16");
    assert_int(17, si91.i7, "accept_si9_2 17");
    assert_int(18, si91.i8, "accept_si9_2 18");
    assert_int(19, si91.i9, "accept_si9_2 19");

    assert_int(21, si92.i1, "accept_si9_2 21");
    assert_int(22, si92.i2, "accept_si9_2 22");
    assert_int(23, si92.i3, "accept_si9_2 23");
    assert_int(24, si92.i4, "accept_si9_2 24");
    assert_int(25, si92.i5, "accept_si9_2 25");
    assert_int(26, si92.i6, "accept_si9_2 26");
    assert_int(27, si92.i7, "accept_si9_2 27");
    assert_int(28, si92.i8, "accept_si9_2 28");
    assert_int(29, si92.i9, "accept_si9_2 29");
}

// With 2 structs and 2 floats in the stack in aarch64
void accept_si9_2_f10(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9, float f10, struct si9 si91, struct si9 si92) {
    assert_float(1.1, f1, "accept_si9_2_f10 1");
    assert_float(2.1, f2, "accept_si9_2_f10 2");
    assert_float(3.1, f3, "accept_si9_2_f10 3");
    assert_float(4.1, f4, "accept_si9_2_f10 4");
    assert_float(5.1, f5, "accept_si9_2_f10 5");
    assert_float(6.1, f6, "accept_si9_2_f10 6");
    assert_float(7.1, f7, "accept_si9_2_f10 7");
    assert_float(8.1, f8, "accept_si9_2_f10 8");
    assert_float(9.1, f9, "accept_si9_2_f10 9");
    assert_float(10.1, f10, "accept_si9_2_f10 10");

    assert_int(11, si91.i1, "accept_si9_2_f10 11");
    assert_int(12, si91.i2, "accept_si9_2_f10 12");
    assert_int(13, si91.i3, "accept_si9_2_f10 13");
    assert_int(14, si91.i4, "accept_si9_2_f10 14");
    assert_int(15, si91.i5, "accept_si9_2_f10 15");
    assert_int(16, si91.i6, "accept_si9_2_f10 16");
    assert_int(17, si91.i7, "accept_si9_2_f10 17");
    assert_int(18, si91.i8, "accept_si9_2_f10 18");
    assert_int(19, si91.i9, "accept_si9_2_f10 19");

    assert_int(21, si92.i1, "accept_si9_2_f10 21");
    assert_int(22, si92.i2, "accept_si9_2_f10 22");
    assert_int(23, si92.i3, "accept_si9_2_f10 23");
    assert_int(24, si92.i4, "accept_si9_2_f10 24");
    assert_int(25, si92.i5, "accept_si9_2_f10 25");
    assert_int(26, si92.i6, "accept_si9_2_f10 26");
    assert_int(27, si92.i7, "accept_si9_2_f10 27");
    assert_int(28, si92.i8, "accept_si9_2_f10 28");
    assert_int(29, si92.i9, "accept_si9_2_f10 29");
}

void accept_i5si4(int i1, int i2, int i3, int i4, int i5, struct si4 si4) {
    assert_int(1, i1,     "accept_i5si4 1");
    assert_int(2, i2,     "accept_i5si4 2");
    assert_int(3, i3,     "accept_i5si4 3");
    assert_int(4, i4,     "accept_i5si4 4");
    assert_int(5, i5,     "accept_i5si4 5");
    assert_int(6, si4.i1, "accept_i5si4 6");
    assert_int(7, si4.i2, "accept_i5si4 7");
    assert_int(8, si4.i3, "accept_i5si4 8");
    assert_int(9, si4.i4, "accept_i5si4 9");
}

void accept_i5sia4(int i1, int i2, int i3, int i4, int i5, struct sia4 sia4) {
    assert_int(1, i1,        "accept_i5sia4 1");
    assert_int(2, i2,        "accept_i5sia4 2");
    assert_int(3, i3,        "accept_i5sia4 3");
    assert_int(4, i4,        "accept_i5sia4 4");
    assert_int(5, i5,        "accept_i5sia4 5");
    assert_int(6, sia4.i[0], "accept_i5sia4 6");
    assert_int(7, sia4.i[1], "accept_i5sia4 7");
    assert_int(8, sia4.i[2], "accept_i5sia4 8");
    assert_int(9, sia4.i[3], "accept_i5sia4 9");
}

void accept_i7sia4(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct sia4 sia4) {
    assert_int(1,  i1,        "accept_i7sia4 1");
    assert_int(2,  i2,        "accept_i7sia4 2");
    assert_int(3,  i3,        "accept_i7sia4 3");
    assert_int(4,  i4,        "accept_i7sia4 4");
    assert_int(5,  i5,        "accept_i7sia4 5");
    assert_int(6,  i6,        "accept_i7sia4 6");
    assert_int(7,  i7,        "accept_i7sia4 7");
    assert_int(8,  sia4.i[0], "accept_i7sia4 8");
    assert_int(9,  sia4.i[1], "accept_i7sia4 9");
    assert_int(10, sia4.i[2], "accept_i7sia4 10");
    assert_int(11, sia4.i[3], "accept_i7sia4 11");
}

// This tests that the last arg ends up in the stack in aarch64
void accept_i7sia4i1(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct sia4 sia4, int i8) {
    assert_int(1,  i1,        "accept_i7sia4i1 1");
    assert_int(2,  i2,        "accept_i7sia4i1 2");
    assert_int(3,  i3,        "accept_i7sia4i1 3");
    assert_int(4,  i4,        "accept_i7sia4i1 4");
    assert_int(5,  i5,        "accept_i7sia4i1 5");
    assert_int(6,  i6,        "accept_i7sia4i1 6");
    assert_int(7,  i7,        "accept_i7sia4i1 7");
    assert_int(8,  i8,        "accept_i7sia4i1 8");
    assert_int(9,  sia4.i[0], "accept_i7sia4i1 9");
    assert_int(10, sia4.i[1], "accept_i7sia4i1 10");
    assert_int(11, sia4.i[2], "accept_i7sia4i1 11");
    assert_int(12, sia4.i[3], "accept_i7sia4i1 12");
}

void accept_sia2a2(struct sia2a2 sia2a2) {
    assert_int(1, sia2a2.i[0][0], "accept_sia2a2 1");
    assert_int(2, sia2a2.i[0][1], "accept_sia2a2 2");
    assert_int(3, sia2a2.i[1][0], "accept_sia2a2 3");
    assert_int(4, sia2a2.i[1][1], "accept_sia2a2 4");
}

void accept_i5sia2a2(int i1, int i2, int i3, int i4, int i5, struct sia2a2 sia2a2) {
    assert_int(1, i1,             "accept_i5sia2a2 1");
    assert_int(2, i2,             "accept_i5sia2a2 2");
    assert_int(3, i3,             "accept_i5sia2a2 3");
    assert_int(4, i4,             "accept_i5sia2a2 4");
    assert_int(5, i5,             "accept_i5sia2a2 5");
    assert_int(6, sia2a2.i[0][0], "accept_i5sia2a2 6");
    assert_int(7, sia2a2.i[0][1], "accept_i5sia2a2 7");
    assert_int(8, sia2a2.i[1][0], "accept_i5sia2a2 8");
    assert_int(9, sia2a2.i[1][1], "accept_i5sia2a2 9");
}

// This tests a bug where code was being inserted at the top of the function instead of in the IR.
long accept_sl3(struct sl3 s) {
    s.l1 = 42;
    return s.l1;
}

void accept_f5sffff(float f1, float f2, float f3, float f4, float f5, struct sffff sffff) {
    assert_int(1.1, f1,       "accept_f5sffff 1");
    assert_int(2.1, f2,       "accept_f5sffff 2");
    assert_int(3.1, f3,       "accept_f5sffff 3");
    assert_int(4.1, f4,       "accept_f5sffff 4");
    assert_int(5.1, f5,       "accept_f5sffff 5");
    assert_int(6.1, sffff.f1, "accept_f5sffff 6");
    assert_int(7.1, sffff.f2, "accept_f5sffff 7");
    assert_int(8.1, sffff.f3, "accept_f5sffff 8");
    assert_int(9.1, sffff.f4, "accept_f5sffff 9");
}

void accept_sfa4(struct sfa4 sfa4) {
    assert_float(6.1, sfa4.f[0], "accept_sfa4 6");
    assert_float(7.1, sfa4.f[1], "accept_sfa4 7");
    assert_float(8.1, sfa4.f[2], "accept_sfa4 8");
    assert_float(9.1, sfa4.f[3], "accept_sfa4 9");
}

void accept_f5sfa4(float f1, float f2, float f3, float f4, float f5, struct sfa4 sfa4) {
    assert_float(1.1, f1,        "accept_i5sfa4 1");
    assert_float(2.1, f2,        "accept_i5sfa4 2");
    assert_float(3.1, f3,        "accept_i5sfa4 3");
    assert_float(4.1, f4,        "accept_i5sfa4 4");
    assert_float(5.1, f5,        "accept_i5sfa4 5");
    assert_float(6.1, sfa4.f[0], "accept_i5sfa4 6");
    assert_float(7.1, sfa4.f[1], "accept_i5sfa4 7");
    assert_float(8.1, sfa4.f[2], "accept_i5sfa4 8");
    assert_float(9.1, sfa4.f[3], "accept_i5sfa4 9");
}

// This tests that the last arg ends up in the stack in aarch64
void accept_f5sfa4f1(float f1, float f2, float f3, float f4, float f5, struct sfa4 sfa4, float f6) {
    assert_float(1.1,  f1,        "accept_f5sfa4f1 1");
    assert_float(2.1,  f2,        "accept_f5sfa4f1 2");
    assert_float(3.1,  f3,        "accept_f5sfa4f1 3");
    assert_float(4.1,  f4,        "accept_f5sfa4f1 4");
    assert_float(5.1,  f5,        "accept_f5sfa4f1 5");
    assert_float(6.1,  f6,        "accept_f5sfa4f1 6");
    assert_float(7.1,  sfa4.f[0], "accept_f5sfa4f1 7");
    assert_float(8.1,  sfa4.f[1], "accept_f5sfa4f1 8");
    assert_float(9.1,  sfa4.f[2], "accept_f5sfa4f1 9");
    assert_float(10.1, sfa4.f[3], "accept_f5sfa4f1 10");
}

#ifdef __x86_64__

void accept_i7sld2(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct sld2 sld2) {
    assert_int(1, i1, "accept_i7sld2 1");
    assert_int(2, i2, "accept_i7sld2 2");
    assert_int(3, i3, "accept_i7sld2 3");
    assert_int(4, i4, "accept_i7sld2 4");
    assert_int(5, i5, "accept_i7sld2 5");
    assert_int(6, i6, "accept_i7sld2 6");
    assert_int(7, i7, "accept_i7sld2 7");
    assert_long_double(8.1, sld2.ld1, "accept_i7sld2 8");
    assert_long_double(9.1, sld2.ld2, "accept_i7sld2 9");
}

#endif

void accept_sda2_2(struct sda2 sda21, struct sda2 sda22) {
    assert_double(1.1, sda21.d[0], "accept_sda2_2 1");
    assert_double(2.1, sda21.d[1], "accept_sda2_2 1");
    assert_double(3.1, sda22.d[0], "accept_sda2_2 3");
    assert_double(4.1, sda22.d[1], "accept_sda2_2 4");
}

// Convoluted test for aarch64 to check the stack layout is correct.
// Struct 5 should be on the stack first, then i9.
void accept_lda2i9(struct slda2 s1, struct slda2 s2, struct slda2 s3, struct slda2 s4, struct slda2 s5, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9) {
    assert_long_double(1.1,  s1.ld[0], "accept_lda2i9 1");
    assert_long_double(2.1,  s1.ld[1], "accept_lda2i9 2");
    assert_long_double(3.1,  s2.ld[0], "accept_lda2i9 3");
    assert_long_double(4.1,  s2.ld[1], "accept_lda2i9 4");
    assert_long_double(5.1,  s3.ld[0], "accept_lda2i9 5");
    assert_long_double(6.1,  s3.ld[1], "accept_lda2i9 6");
    assert_long_double(7.1,  s4.ld[0], "accept_lda2i9 7");
    assert_long_double(8.1,  s4.ld[1], "accept_lda2i9 8");
    assert_long_double(9.1,  s5.ld[0], "accept_lda2i9 9");
    assert_long_double(10.1, s5.ld[1], "accept_lda2i9 10");

    assert_int(1, i1, "accept_lda2i9 11");
    assert_int(2, i2, "accept_lda2i9 12");
    assert_int(3, i3, "accept_lda2i9 13");
    assert_int(4, i4, "accept_lda2i9 14");
    assert_int(5, i5, "accept_lda2i9 15");
    assert_int(6, i6, "accept_lda2i9 16");
    assert_int(7, i7, "accept_lda2i9 17");
    assert_int(8, i8, "accept_lda2i9 18");
    assert_int(9, i9, "accept_lda2i9 19");
}

void accept_slda4(struct slda4 slda4) {
    assert_long_double(6.1, slda4.ld[0], "accept_slda4 6");
    assert_long_double(7.1, slda4.ld[1], "accept_slda4 7");
    assert_long_double(8.1, slda4.ld[2], "accept_slda4 8");
    assert_long_double(9.1, slda4.ld[3], "accept_slda4 9");
}

// Example from ABI doc v0.98
void accept_abi_example(int e, int f, structparm s, int g, int h, long double ld, double m, double n, int i, int j, int k) {
    assert_int(1, e, "abi_example e");
    assert_int(2, f, "abi_example f");
    assert_int(3, g, "abi_example g");
    assert_int(4, h, "abi_example h");
    assert_int(5, i, "abi_example i");
    assert_int(6, j, "abi_example j");
    assert_int(7, k, "abi_example k");
    assert_long_double(8.1, ld, "abi_example ld");
    assert_double(9.1, m, "abi_example m");
    assert_double(10.1, n, "abi_example n");
    assert_int(11, s.a, "abi_example s.a");
    assert_int(12, s.b, "abi_example s.b");
    assert_double(13.1, s.d, "abi_example s.d");
}

void accept_us(struct us us) {
    assert_int(1, us.i, "accept-us i");
    assert_int(2, us.c, "accept-us c");
    assert_int(3, us.j, "accept-us j");
}

struct spf gspf;

struct spf    return_spf() { struct spf spf; spf.f1 = 1.1; return spf; }
struct spf    return_spf_from_global() { gspf.f1 = 1.1; return gspf; }
struct spf    return_spf_from_temp() { struct spf spf; spf.f1 = 1.1; return *&spf; }
struct spf    return_spf_with_params(int i) { assert_int(1, i, "return_spf_with_params i"); struct spf spf; spf.f1 = 1.1; return *&spf; }

struct spd    return_spd()    { struct spd spd;         spd.d1 = 2.1; return spd; }
struct spdf   return_spdf()   { struct spdf spdf;       spdf.d1 = 3.1; spdf.f1 = 4.1; return spdf; }
struct sff    return_sff()    { struct sff sff;         sff.f1 = 5.1; sff.f2 = 6.1; return sff; }
struct sdd    return_sdd()    { struct sdd sdd;         sdd.d1 = 7.1; sdd.d2 = 8.1; return sdd; }
struct sffff  return_sffff()  { struct sffff sffff;     sffff.f1 = 1.1; sffff.f2 = 2.1; sffff.f3 = 3.1; sffff.f4 = 4.1; return  sffff; }
struct sffii  return_sffii()  { struct sffii sffii;     sffii.f1 = 1.1; sffii.f2 = 2.1; sffii.i1 = 3; sffii.i2 = 4; return  sffii; }
struct sffiii return_sffiii() { struct sffiii sffiii;   sffiii.f1 = 1.1; sffiii.f2 = 2.1; sffiii.i1 = 3; sffiii.i2 = 4; sffiii.i3 = 5; return  sffiii; }
struct siiff  return_siiff()  { struct siiff siiff;     siiff.i1 = 1; siiff.i2 = 2; siiff.f1 = 3.1; siiff.f2 = 4.1; return  siiff; }
struct siifff return_siifff() { struct siifff siifff;   siifff.i1 = 1; siifff.i2 = 2; siifff.f1 = 3.1; siifff.f2 = 4.1; siifff.f3 = 5.1; return  siifff; }
struct sifif  return_sifif()  { struct sifif sifif;     sifif.i1 = 1; sifif.f1 = 2.1; sifif.i2 = 3; sifif.f2 = 4.1; return  sifif; }
struct si1    return_si1()    { struct si1 si1;         si1.i1 = 1; return si1; }
struct si2    return_si2()    { struct si2 si2;         si2.i1 = 1; si2.i2 = 2; return si2; }
struct si3    return_si3()    { struct si3 si3;         si3.i1 = 1; si3.i2 = 2; si3.i3 = 3; return si3; }
struct si4    return_si4()    { struct si4 si4;         si4.i1 = 1; si4.i2 = 2; si4.i3 = 3; si4.i4 = 4; return si4; }
struct si5    return_si5()    { struct si5 si5;         si5.i1 = 1; si5.i2 = 2; si5.i3 = 3; si5.i4 = 4; si5.i5 = 5; return si5; }
struct sld3   return_sld3()   { struct sld3 ld3;        ld3.ld1 = 1.1; ld3.ld2 = 2.1; ld3.ld3 = 3.1; return ld3; }

struct si5 return_si5_with_params(int i, float f) {
    assert_int(1, i, "return_si5_with_params i");
    assert_float(1.1, f, "return_si5_with_params f");
    struct si5 si5;
    si5.i1 = 1;
    si5.i2 = 2;
    si5.i3 = 3;
    si5.i4 = 4;
    si5.i5 = 5;
    return *&si5;
}

void accept_array(int a[4]) {
    assert_int(1, a[0], "Accept array 1");
    assert_int(2, a[1], "Accept array 2");
    assert_int(3, a[2], "Accept array 3");
    assert_int(4, a[3], "Accept array 4");
}

#ifdef __x86_64__

int linked_object;

static int unlinked_object;
int initialized_linked_object = 1;
static int initialized_unlinked_object = 1;

int get_linked_object() { return linked_object; }
int get_unlinked_object() { return unlinked_object; }
void set_linked_object(int i) { linked_object = i; }
void set_unlinked_object(int i) { unlinked_object = i; }

// A object with internal linkage can be redeclared with extern, but with unchanged linkage
static int sei;
extern int sei;

int get_sei() { return sei; }
void set_sei(int i) { sei = i; }

int extern_global_int;

int extern_renamed_func(int i) {
    return i + 1;
}

extern int extern_func(int i) { return i + 1; }

#endif

void test_bitfield_struct_fields(struct bfs *bfs) {
    assert_int(1,  bfs->i1, "Test bfs 1");
    assert_int(2 , bfs->i2, "Test bfs 2");
    assert_int(3,  bfs->i3, "Test bfs 3");
    assert_int(-4, bfs->i4, "Test bfs 4"); // The high bit is set
    assert_int(-3, bfs->i5, "Test bfs 5"); // The high bit is set
    assert_int(6,  bfs->i6, "Test bfs 6");
    assert_int(7,  bfs->i7, "Test bfs 7");
    assert_int(8,  bfs->i8, "Test bfs 8");
}

// Test an int128 that is passed in registers
void test_int128_in_registers(__int128 i128) {
    ASSERT_INT128(1, 2, i128, "An int128 in registers");
}

// Test an int128 with 5 registers already taken
void test_int128_in_registers_i5(int i1, int i2, int i3, int i4, int i5, __int128 i128) {
    assert_int(   -1,   i1,   "A int128 on the stack 1");
    assert_int(   -2,   i2,   "A int128 on the stack 2");
    assert_int(   -3,   i3,   "A int128 on the stack 3");
    assert_int(   -4,   i4,   "A int128 on the stack 4");
    assert_int(   -5,   i5,   "A int128 on the stack 5");
    ASSERT_INT128(1, 2, i128, "A int128 on the stack 6");
}

// Test an int128 with 6 registers already taken
void test_int128_in_registers_i6(int i1, int i2, int i3, int i4, int i5, int i6, __int128 i128) {
    assert_int(   -1,   i1,   "A int128 on the stack 1");
    assert_int(   -2,   i2,   "A int128 on the stack 2");
    assert_int(   -3,   i3,   "A int128 on the stack 3");
    assert_int(   -4,   i4,   "A int128 on the stack 4");
    assert_int(   -5,   i5,   "A int128 on the stack 5");
    assert_int(   -6,   i6,   "A int128 on the stack 6");
    ASSERT_INT128(1, 2, i128, "A int128 on the stack 7");
}

// Test an int128 with 7 registers already taken
void test_int128_in_registers_i7(int i1, int i2, int i3, int i4, int i5, int i6, int i7, __int128 i128) {
    assert_int(   -1,   i1,   "A int128 on the stack 1");
    assert_int(   -2,   i2,   "A int128 on the stack 2");
    assert_int(   -3,   i3,   "A int128 on the stack 3");
    assert_int(   -4,   i4,   "A int128 on the stack 4");
    assert_int(   -5,   i5,   "A int128 on the stack 5");
    assert_int(   -6,   i6,   "A int128 on the stack 6");
    assert_int(   -7,   i7,   "A int128 on the stack 7");
    ASSERT_INT128(1, 2, i128, "A int128 on the stack 8");
}

__int128_t return_int128() {
    __int128_t r = MAKE_SINT128(1, 2);
    return r;
}

