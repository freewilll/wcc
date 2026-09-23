struct spf { float f1;};
struct spd { double d1;};
struct spdf { double d1; float f1; };
struct sff { float f1, f2; };
struct sdd { double d1, d2; };
struct sffff { float f1, f2, f3, f4; };
struct sffii { float f1, f2; int i1, i2; };
struct sffiii { float f1, f2; int i1, i2, i3; };
struct siiff { int i1, i2; float f1, f2; };
struct siifff { int i1, i2; float f1, f2, f3; };
struct sifif { int i1; float f1; int i2; float f2; };
struct sf9 { float f1, f2, f3, f4, f5, f6, f7, f8, f9; };

struct sc1 { char c1; };
struct sc2 { char c1, c2; };
struct sc3 { char c1, c2, c3; };
struct sc4 { char c1, c2, c3, c4; };
struct sc5 { char c1, c2, c3, c4, c5; };
struct sc6 { char c1, c2, c3, c4, c5, c6; };
struct sc7 { char c1, c2, c3, c4, c5, c6, c7; };
struct sc8 { char c1, c2, c3, c4, c5, c6, c7, c8; };
struct sc9 { char c1, c2, c3, c4, c5, c6, c7, c8, c9; };

struct sc3f { char c1, c2, c3; float f1; };

struct si1 { int i1; };
struct si2 { int i1, i2; };
struct si3 { int i1, i2, i3; };
struct si4 { int i1, i2, i3, i4; };
struct si5 { int i1, i2, i3, i4, i5; };
struct si6 { int i1, i2, i3, i4, i5, i6; };
struct si7 { int i1, i2, i3, i4, i5, i6, i7; };
struct si8 { int i1, i2, i3, i4, i5, i6, i7, i8; };
struct si9 { int i1, i2, i3, i4, i5, i6, i7, i8, i9; };

struct sl3 { long l1, l2, l3; };

struct sld2 { long double ld1, ld2; };
struct sld3 { long double ld1, ld2, ld3; };

struct sia2a2 { int i[2][2]; };
struct sia4 { int i[4]; };
struct sfa4 { float f[4]; };
struct sda2 { double d[2]; };
struct slda2 { long double ld[2]; };
struct slda4 { long double ld[4]; };

struct si128 { __int128 i; };

// Example from ABI doc v0.98
typedef struct { int a, b; double d; } structparm;

// An unaligned struct
struct __attribute__ ((__packed__)) us { int i; char c; int j; };

// A struct with a bunch of bit fields
struct bfs { int i1; int i2:16; int i3:3; int i4:3; int i5:3; int i6:5; int i7:5; int i8; };

// Used in union tests

union uii { union { int i1, i2; } u; };
union uff { union { float f1, f2; } u; };
union ufi { union { float f; int i; } u; };

// aarch64: Not HFA
union uf5i1 {
    union {
        struct {float f1, f2;    } s1;
        struct {float f1, f2;    } s2;
        struct {float f3; int i1;} s3;
    } u;
};

// aarch64: HFA
union uf5 {
    union {
        struct {float f1, f2; } s1;
        struct {float f1, f2; } s2;
        struct {float f3;     } s3;
    } u;
};

// aarch64: HFA
union uf6 {
    union {
        struct {float f1, f2; } s1;
        struct {float f1, f2; } s2;
        struct {float f1, f2; } s3;
    } u;
};

// aarch64: HFA
union uf8 {
    union {
        struct {float f1, f2, f3, f4; } s1;
        struct {float f1, f2, f3, f4; } s2;
    } u;
};

// aarch64: HFA
union ud8 {
    union {
        struct {double d1, d2, d3, d4; } s1;
        struct {double d1, d2, d3, d4; } s2;
    } u;
};

// aarch64: HFA
union uld8 {
    union {
        struct {long double ld1, ld2, ld3, ld4; } s1;
        struct {long double ld1, ld2, ld3, ld4; } s2;
    } u;
};

void accept_uii(union uii uii);
void accept_uff(union uff uff);
void accept_ufi(union ufi ufi);
void accept_uf5i1(union uf5i1 uf5i1);
void accept_uf5(union uf5 uf5);
void accept_uf6(union uf6 uf6);
void accept_uf8(union uf8 uf8);
void accept_ud8(union ud8 ud8);
void accept_uld8(union uld8 uld8);

void accept_spf(struct spf spf);
void accept_spd(struct spd spd);
void accept_spdf(struct spdf spdf);
void accept_sff(struct sff sff);
void accept_sdd(struct sdd sdd);
void accept_sffff(struct sffff sffff);
void accept_sf9(struct sf9 sf9);
void accept_ffffsffff(float f1, float f2, float f3, float f4, struct sffff sffff);
void accept_sffii(struct sffii sffii);
void accept_sffiii(struct sffiii sffiii);
void accept_siiff(struct siiff siiff);
void accept_siifff(struct siifff siifff);
void accept_sifif(struct sifif sifif);

void accept_sc1(struct sc1 sc1);
void accept_sc2(struct sc2 sc2);
void accept_sc3(struct sc3 sc3);
void accept_sc4(struct sc4 sc4);
void accept_sc5(struct sc5 sc5);
void accept_sc6(struct sc6 sc6);
void accept_sc7(struct sc7 sc7);
void accept_sc8(struct sc8 sc8);
void accept_sc9(struct sc9 sc9);

void accept_sc3f(struct sc3f sc3f);

void accept_si5(struct si5 si5);
void accept_si6(struct si6 si6);
void accept_si7(struct si7 si7);
void accept_si8(struct si8 si8);
void accept_si9(struct si9 si9);

void accept_si9_i6(int i1, int i2, int i3, int i4, int i5, int i6, struct si9 si91, struct si9 si92);
void accept_si9_i7(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct si9 si91, struct si9 si92);
void accept_si9_2_f9(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9, struct si9 si91, struct si9 si92);
void accept_si9_2_f10(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9, float f10, struct si9 si91, struct si9 si92);

void accept_i5si4(int i1, int i2, int i3, int i4, int i5, struct si4 si4);
void accept_i5sia4(int i1, int i2, int i3, int i4, int i5, struct sia4 sia4);
void accept_i7sia4(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct sia4 sia4);
void accept_i7sia4i1(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct sia4 sia4, int i8);
void accept_sia2a2(struct sia2a2 sia2a2);
void accept_i5sia2a2(int i1, int i2, int i3, int i4, int i5, struct sia2a2 sia2a2);
void accept_f5sffff(float f1, float f2, float f3, float f4, float f5, struct sffff sffff);
long accept_sl3(struct sl3 s);

void accept_sfa4(struct sfa4 sfa4);
void accept_sda2_2(struct sda2 sda21, struct sda2 sda22);
void accept_lda2i9(struct slda2 s1, struct slda2 s2, struct slda2 s3, struct slda2 s4, struct slda2 s5, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9);
void accept_f5sfa4(float f1, float f2, float f3, float f4, float f5, struct sfa4 sfa4);
void accept_f5sfa4f1(float f1, float f2, float f3, float f4, float f5, struct sfa4 sfa4, float f6);

void accept_i7sld2(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct sld2 sld2);

void accept_slda4(struct slda4 slda4);

void accept_abi_example(int e, int f, structparm s, int g, int h, long double ld, double m, double n, int i, int j, int k);

void accept_us(struct us us);

struct spf return_spf();
struct spf return_spf_from_global();
struct spf return_spf_from_temp();
struct spf return_spf_with_params(int i);
struct spd return_spd();
struct spdf return_spdf();
struct sff return_sff();
struct sdd return_sdd();
struct sffff return_sffff();
struct sffii return_sffii();
struct sffiii return_sffiii();
struct siiff return_siiff();
struct siifff return_siifff();
struct sifif return_sifif();

struct si1 return_si1();
struct si2 return_si2();
struct si3 return_si3();
struct si4 return_si4();
struct si5 return_si5();

struct si5 return_si5_with_params(int i, float f);

struct sld3 return_sld3();

void accept_array(int a[4]);

int get_linked_object();
int get_unlinked_object();
void set_linked_object(int i);
void set_unlinked_object(int i);
int get_sei();
void set_sei(int i);
void test_bitfield_struct_fields(struct bfs *bfs);
void test_int128_in_registers(__int128 i128);
void test_int128_in_registers_i5(int i1, int i2, int i3, int i4, int i5, __int128 i128);
void test_int128_in_registers_i6(int i1, int i2, int i3, int i4, int i5, int i6, __int128 i128);
void test_int128_in_registers_i7(int i1, int i2, int i3, int i4, int i5, int i6, int i7, __int128 i128);
void accept_i_and_int128(int i, struct si128 s);
__int128_t return_int128();

void accept_uii(union uii uii);
void accept_uff(union uff uff);
void accept_ufi(union ufi ufi);
void accept_uf5i1(union uf5i1 uf5i1);
void accept_uf5(union uf5 uf5);
void accept_uf6(union uf6 uf6);
void accept_uf8(union uf8 uf8);
void accept_ud8(union ud8 ud8);
void accept_uld8(union uld8 uld8);
