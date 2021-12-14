#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"
#include "test-abi.h"

int verbose;
int passes;
int failures;

struct sffff gsffff;

// No test is present for a redeclared symbol with default extern and later extern, but this must compile without errors.
void redeclared_extern(int size);
extern void redeclared_extern (int __size);

void test_struct_in_stack_with_zero_offset() {
    struct si5 si5; si5.i1 = 1; si5.i2 = 2; si5.i3 = 3; si5.i4 = 4; si5.i5 = 5;                                                 accept_si5(si5);
    struct si6 si6; si6.i1 = 1; si6.i2 = 2; si6.i3 = 3; si6.i4 = 4; si6.i5 = 5; si6.i6 = 6;                                     accept_si6(si6);
    struct si7 si7; si7.i1 = 1; si7.i2 = 2; si7.i3 = 3; si7.i4 = 4; si7.i5 = 5; si7.i6 = 6; si7.i7 = 7;                         accept_si7(si7);
    struct si8 si8; si8.i1 = 1; si8.i2 = 2; si8.i3 = 3; si8.i4 = 4; si8.i5 = 5; si8.i6 = 6; si8.i7 = 7; si8.i8 = 8;             accept_si8(si8);
    struct si9 si9; si9.i1 = 1; si9.i2 = 2; si9.i3 = 3; si9.i4 = 4; si9.i5 = 5; si9.i6 = 6; si9.i7 = 7; si9.i8 = 8; si9.i9 = 9; accept_si9(si9);
}

void test_struct_in_stack_with_eight_offset() {
    // Add something to the stack to ensure the tests pass on a stack with an extra
    // offset of 8 bytes.
    int *pi; &pi;

    struct si5 si5; si5.i1 = 1; si5.i2 = 2; si5.i3 = 3; si5.i4 = 4; si5.i5 = 5;                                                 accept_si5(si5);
    struct si6 si6; si6.i1 = 1; si6.i2 = 2; si6.i3 = 3; si6.i4 = 4; si6.i5 = 5; si6.i6 = 6;                                     accept_si6(si6);
    struct si7 si7; si7.i1 = 1; si7.i2 = 2; si7.i3 = 3; si7.i4 = 4; si7.i5 = 5; si7.i6 = 6; si7.i7 = 7;                         accept_si7(si7);
    struct si8 si8; si8.i1 = 1; si8.i2 = 2; si8.i3 = 3; si8.i4 = 4; si8.i5 = 5; si8.i6 = 6; si8.i7 = 7; si8.i8 = 8;             accept_si8(si8);
    struct si9 si9; si9.i1 = 1; si9.i2 = 2; si9.i3 = 3; si9.i4 = 4; si9.i5 = 5; si9.i6 = 6; si9.i7 = 7; si9.i8 = 8; si9.i9 = 9; accept_si9(si9);
}

struct si1 gsi1;
struct si5 gsi5;
struct si9 gsi9;

void test_struct_params() {
    // Struct with only floats and doubles
    struct spf spf; spf.f1 = 1.1; accept_spf(spf);
    struct spd spd; spd.d1 = 2.1; accept_spd(spd);
    struct spdf spdf; spdf.d1 = 3.1; spdf.f1 = 4.1; accept_spdf(spdf);
    struct sff sff; sff.f1 = 5.1; sff.f2 = 6.1; accept_sff(sff);
    struct sdd sdd; sdd.d1 = 7.1; sdd.d2 = 8.1; accept_sdd(sdd);
    struct sffff sffff; sffff.f1 = 1.1; sffff.f2 = 2.1; sffff.f3 = 3.1; sffff.f4 = 4.1; accept_sffff(sffff);
    struct sffii sffii; sffii.f1 = 1.1; sffii.f2 = 2.1; sffii.i1 = 3; sffii.i2 = 4; accept_sffii(sffii);
    struct sffiii sffiii; sffiii.f1 = 1.1; sffiii.f2 = 2.1; sffiii.i1 = 3; sffiii.i2 = 4; sffiii.i3 = 5; accept_sffiii(sffiii);
    struct siiff siiff; siiff.i1 = 1; siiff.i2 = 2; siiff.f1 = 3.1; siiff.f2 = 4.1; accept_siiff(siiff);
    struct siifff siifff; siifff.i1 = 1; siifff.i2 = 2; siifff.f1 = 3.1; siifff.f2 = 4.1; siifff.f3 = 5.1; accept_siifff(siifff);
    struct sifif sifif; sifif.i1 = 1; sifif.f1 = 2.1; sifif.i2 = 3; sifif.f2 = 4.1; accept_sifif(sifif);

    // From lvalue in register
    accept_sffff(*&sffff);

    // From global
    gsffff.f1 = 1.1; gsffff.f2 = 2.1;  gsffff.f3 = 3.1; gsffff.f4 = 4.1; accept_sffff(gsffff);

    // Structs with chars of different sizes
    struct sc1 sc1; sc1.c1 = 1;                                                                                                 accept_sc1(sc1);
    struct sc2 sc2; sc2.c1 = 1; sc2.c2 = 2;                                                                                     accept_sc2(sc2);
    struct sc3 sc3; sc3.c1 = 1; sc3.c2 = 2; sc3.c3 = 3;                                                                         accept_sc3(sc3);
    struct sc4 sc4; sc4.c1 = 1; sc4.c2 = 2; sc4.c3 = 3; sc4.c4 = 4;                                                             accept_sc4(sc4);
    struct sc5 sc5; sc5.c1 = 1; sc5.c2 = 2; sc5.c3 = 3; sc5.c4 = 4; sc5.c5 = 5;                                                 accept_sc5(sc5);
    struct sc6 sc6; sc6.c1 = 1; sc6.c2 = 2; sc6.c3 = 3; sc6.c4 = 4; sc6.c5 = 5; sc6.c6 = 6;                                     accept_sc6(sc6);
    struct sc7 sc7; sc7.c1 = 1; sc7.c2 = 2; sc7.c3 = 3; sc7.c4 = 4; sc7.c5 = 5; sc7.c6 = 6; sc7.c7 = 7;                         accept_sc7(sc7);
    struct sc8 sc8; sc8.c1 = 1; sc8.c2 = 2; sc8.c3 = 3; sc8.c4 = 4; sc8.c5 = 5; sc8.c6 = 6; sc8.c7 = 7; sc8.c8 = 8;             accept_sc8(sc8);
    struct sc9 sc9; sc9.c1 = 1; sc9.c2 = 2; sc9.c3 = 3; sc9.c4 = 4; sc9.c5 = 5; sc9.c6 = 6; sc9.c7 = 7; sc9.c8 = 8; sc9.c9 = 9; accept_sc9(sc9);

    // From lvalue in register
    accept_sc9(*&sc9);

    // Local in stack
    test_struct_in_stack_with_zero_offset();
    test_struct_in_stack_with_eight_offset();

    // Global in stack
    gsi5.i1 = 1; gsi5.i2 = 2; gsi5.i3 = 3; gsi5.i4 = 4; gsi5.i5 = 5;
    accept_si5(gsi5);

    gsi9.i1 = 1; gsi9.i2 = 2; gsi9.i3 = 3; gsi9.i4 = 4; gsi9.i5 = 5; gsi9.i6 = 6; gsi9.i7 = 7; gsi9.i8 = 8; gsi9.i9 = 9; accept_si9(gsi9);

    // From lvalue in register
    accept_si5(*&gsi5); // <= 32 bytes, done with registers
    accept_si9(*&gsi9); // > 32 bytes, done with memcpy

    // Forcing of si4 onto the stack due to partial register exhaustion
    struct si4 si4; si4.i1 = 6; si4.i2 = 7; si4.i3 = 8; si4.i4 = 9;
    accept_i5si4(1, 2, 3, 4, 5, si4);

    struct sia4 sia4; sia4.i[0] = 6; sia4.i[1] = 7; sia4.i[2] = 8; sia4.i[3] = 9;
    accept_i5sia4(1, 2, 3, 4, 5, sia4);

    struct sia2a2 sia2a2; sia2a2.i[0][0] = 1; sia2a2.i[0][1] = 2; sia2a2.i[1][0] = 3; sia2a2.i[1][1] = 4;
    accept_sia2a2(sia2a2);

    sia2a2.i[0][0] = 6; sia2a2.i[0][1] = 7; sia2a2.i[1][0] = 8; sia2a2.i[1][1] = 9;
    accept_i5sia2a2(1, 2, 3, 4, 5, sia2a2);

    // Forcing of sffff onto the stack due to partial register exhaustion
    sffff.f1 = 6.1; sffff.f2 = 7.1; sffff.f3 = 8.1; sffff.f4 = 9.1;
    accept_f5sffff(1.1, 2.1, 3.1, 4.1, 5.1, sffff);

    struct sfa4 sfa4; sfa4.f[0] = 6.1; sfa4.f[1] = 7.1; sfa4.f[2] = 8.1; sfa4.f[3] = 9.1;
    accept_f5sfa4(1.1, 2.1, 3.1, 4.1, 5.1, sfa4);

    // Alignment 16 struct with 8 bytes on the stack underneath it
    struct sld2 sld2; sld2.ld1 = 8.1; sld2.ld2 = 9.1; accept_i7sld2(1, 2, 3, 4, 5, 6, 7, sld2);

    // Example from ABI doc v0.98
    structparm s;
    int e = 1; int f = 2; int g = 3; int h = 4; int i = 5; int j = 6; int k = 7;
    long double ld = 8.1;
    double m = 9.1; double n = 10.1;
    s.a = 11; s.b = 12; s.d = 13.1;
    accept_abi_example(e, f, s, g, h, ld, m, n, i, j, k);
}

void test_struct_return_values() {
    // Struct with only floats and doubles
    struct spf spf   = return_spf();              assert_float(1.1,  spf.f1,  "return_spf");
               spf   = return_spf_from_global();  assert_float(1.1,  spf.f1,  "return_spf_from_global");
               spf   = return_spf_from_temp();    assert_float(1.1,  spf.f1,  "return_spf_from_temp");
               spf   = return_spf_with_params(1); assert_float(1.1,  spf.f1,  "return_spf_with_params");
    struct spd spd   = return_spd();              assert_double(2.1, spd.d1,  "accept_spd");
    struct spdf spdf = return_spdf();             assert_float(3.1,  spdf.d1, "accept_spf");  assert_double(4.1, spdf.f1, "accept_spdf");
    struct sff sff   = return_sff();              assert_float(5.1,  sff.f1,  "accept_spf");  assert_float(6.1,  sff.f2,  "accept_sff");
    struct sdd sdd   = return_sdd();              assert_float(7.1,  sdd.d1,  "accept_spf");  assert_float(8.1,  sdd.d2,  "accept_sdd");

    struct si1 si1 = return_si1();
    assert_int(1, si1.i1, "return_si1");

    struct si2 si2 = return_si2();
    assert_int(1, si2.i1, "return_si2");
    assert_int(2, si2.i2, "return_si2");

    struct si3 si3 = return_si3();
    assert_int(1, si3.i1, "return_si3");
    assert_int(2, si3.i2, "return_si3");
    assert_int(3, si3.i3, "return_si3");

    struct si4 si4 = return_si4();
    assert_int(1, si4.i1, "return_si4");
    assert_int(2, si4.i2, "return_si4");
    assert_int(3, si4.i3, "return_si4");
    assert_int(4, si4.i4, "return_si4");

    struct si5 si5 = return_si5();
    assert_int(1, si5.i1, "return_si5");
    assert_int(2, si5.i2, "return_si5");
    assert_int(3, si5.i3, "return_si5");
    assert_int(4, si5.i4, "return_si5");
    assert_int(5, si5.i5, "return_si5");

    si5 = return_si5_with_params(1, 1.1);
    assert_int(1, si5.i1, "return_si5");
    assert_int(2, si5.i2, "return_si5");
    assert_int(3, si5.i3, "return_si5");
    assert_int(4, si5.i4, "return_si5");
    assert_int(5, si5.i5, "return_si5");

    gsi1 = return_si1();
    assert_int(1, gsi1.i1, "return_si1 with global");

    gsi1.i1 = 2;
    *&gsi1 = return_si1();
    assert_int(1, gsi1.i1, "return_si1 with temp");

    struct sffff sffff = return_sffff();
    assert_float(1.1, sffff.f1, "return_sffff");
    assert_float(2.1, sffff.f2, "return_sffff");
    assert_float(3.1, sffff.f3, "return_sffff");
    assert_float(4.1, sffff.f4, "return_sffff");

    struct sffii sffii = return_sffii();
    assert_float(1.1, sffii.f1, "return_sffii");
    assert_float(2.1, sffii.f2, "return_sffii");
    assert_int(3, sffii.i1, "return_sffii");
    assert_int(4, sffii.i2, "return_sffii");

    struct sffiii sffiii = return_sffiii();
    assert_float(1.1, sffiii.f1, "return_sffiii");
    assert_float(2.1, sffiii.f2, "return_sffiii");
    assert_int(3, sffiii.i1, "return_sffiii");
    assert_int(4, sffiii.i2, "return_sffiii");
    assert_int(5, sffiii.i3, "return_sffiii");

    struct siiff siiff = return_siiff();
    assert_int(1, siiff.i1, "return_siiff");
    assert_int(2, siiff.i2, "return_siiff");
    assert_float(3.1, siiff.f1, "return_siiff");
    assert_float(4.1, siiff.f2, "return_siiff");

    struct siifff siifff = return_siifff();
    assert_int(1, siifff.i1, "return_siifff");
    assert_int(2, siifff.i2, "return_siifff");
    assert_float(3.1, siifff.f1, "return_siifff");
    assert_float(4.1, siifff.f2, "return_siifff");
    assert_float(5.1, siifff.f3, "return_siifff");

    struct sifif sifif = return_sifif();
    assert_int(1, sifif.i1, "return_sifif");
    assert_float(2.1, sifif.f1, "return_sifif");
    assert_int(3, sifif.i2, "return_sifif");
    assert_float(4.1, sifif.f2, "return_sifif");
}

void test_arrays() {
    int a[4];
    for (int i = 0; i < 4; i++) a[i] = i + 1;

    accept_array(a);
}

int linked_object;
static int unlinked_object;
static int initialized_unlinked_object = 1;

// A object with internal linkage can be redeclared with extern, but with unchanged linkage
static int sei;
extern int sei;

void test_global_object_linkage() {
    // Ensure values in linked object are the same

    set_linked_object(1);
    assert_int(1, linked_object, "Linked object in this translation unit");
    assert_int(1, get_linked_object(), "Linked object in other translation unit 1");
    linked_object = 2;
    assert_int(2, get_linked_object(), "Linked object in other translation unit 2");

    // Ensure unlinked objects have distinct values
    unlinked_object = 1;
    assert_int(0, get_unlinked_object(), "Unlinked object in other translation unit 1");
    set_unlinked_object(2);
    assert_int(2, get_unlinked_object(), "Unlinked object in other translation unit 2");
    assert_int(1, unlinked_object, "Unlinked object in this translation unit 1");

    sei = 1;
    assert_int(0, get_sei(), "Redeclared unlinked object in other translation unit 1");
    set_sei(2);
    assert_int(2, get_sei(), "Redeclared unlinked object in other translation unit 2");
    assert_int(1, sei, "Redeclared unlinked object in this translation unit 1");
}

int global_int;
extern int extern_global_int;

void test_block_extern_object_linkage() {
    // With internally defined global object
    int global_int = 10;
    {
        extern int global_int;
        global_int = 20;
        assert_int(20, global_int, "extern int in block");
    }
    assert_int(10, global_int, "extern int outside of block");

    // With externally defined global object
    int extern_global_int = 10;
    {
        extern int extern_global_int;
        extern_global_int = 20;
        assert_int(20, extern_global_int, "extern int in block");
    }
    assert_int(10, extern_global_int, "extern int outside of block");
}

// Wacky gcc syntax, seen e.g. in fscanf
extern int intern_renamed_func(int i) __asm__ ("extern_renamed_func");

void test_extern_renamed_func() {
    assert_int(2, intern_renamed_func(1), "Linkage of a renamed extern function");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);
    test_struct_params();
    test_struct_return_values();
    test_arrays();
    test_global_object_linkage();
    test_block_extern_object_linkage();
    test_extern_renamed_func();

    finalize();
}
