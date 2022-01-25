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

struct sc1 { char c1; };
struct sc2 { char c1, c2; };
struct sc3 { char c1, c2, c3; };
struct sc4 { char c1, c2, c3, c4; };
struct sc5 { char c1, c2, c3, c4, c5; };
struct sc6 { char c1, c2, c3, c4, c5, c6; };
struct sc7 { char c1, c2, c3, c4, c5, c6, c7; };
struct sc8 { char c1, c2, c3, c4, c5, c6, c7, c8; };
struct sc9 { char c1, c2, c3, c4, c5, c6, c7, c8, c9; };

struct si1 { int i1; };
struct si2 { int i1, i2; };
struct si3 { int i1, i2, i3; };
struct si4 { int i1, i2, i3, i4; };
struct si5 { int i1, i2, i3, i4, i5; };
struct si6 { int i1, i2, i3, i4, i5, i6; };
struct si7 { int i1, i2, i3, i4, i5, i6, i7; };
struct si8 { int i1, i2, i3, i4, i5, i6, i7, i8; };
struct si9 { int i1, i2, i3, i4, i5, i6, i7, i8, i9; };

struct sld2 { long double ld1, ld2; };

struct sia2a2 { int i[2][2]; };
struct sia4 { int i[4]; };
struct sfa4 { float f[4]; };

struct ld3 { long double ld1, ld2, ld3; };

// Example from ABI doc v0.98
typedef struct { int a, b; double d; } structparm;

// An unaligned struct
struct __attribute__ ((__packed__)) us { int i; char c; int j; }; // TODO revive packed structs

void accept_spf(struct spf spf);
void accept_spd(struct spd spd);
void accept_spdf(struct spdf spdf);
void accept_sff(struct sff sff);
void accept_sdd(struct sdd sdd);
void accept_sffff(struct sffff sffff);
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

void accept_si5(struct si5 si5);
void accept_si6(struct si6 si6);
void accept_si7(struct si7 si7);
void accept_si8(struct si8 si8);
void accept_si9(struct si9 si9);

void accept_i5si4(int i1, int i2, int i3, int i4, int i5, struct si4 si4);
void accept_i5sia4(int i1, int i2, int i3, int i4, int i5, struct sia4 sia4);
void accept_sia2a2(struct sia2a2 sia2a2);
void accept_i5sia2a2(int i1, int i2, int i3, int i4, int i5, struct sia2a2 sia2a2);
void accept_f5sffff(float f1, float f2, float f3, float f4, float f5, struct sffff sffff);
void accept_f5sfa4(float f1, float f2, float f3, float f4, float f5, struct sfa4 sfa4);

void accept_i7sld2(int i1, int i2, int i3, int i4, int i5, int i6, int i7, struct sld2 sld2);

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

struct ld3 return_ld3();

void accept_array(int a[4]);

int get_linked_object();
int get_unlinked_object();
void set_linked_object(int i);
void set_unlinked_object(int i);
int get_sei();
void set_sei(int i);
