struct spf { float f1;};
struct spd { double d1;};
struct spdf { double d1; float f1; };
struct sff { float f1, f2; };
struct sdd { double d1, d2; };
struct sffff { float f1, f2, f3, f4; };
struct sffii { float f1, f2; int i1, i2; };
struct siiff { int i1, i2; float f1, f2; };
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

void accept_spf(struct spf spf);
void accept_spd(struct spd spd);
void accept_spdf(struct spdf spdf);
void accept_sff(struct sff sff);
void accept_sdd(struct sdd sdd);
void accept_sffff(struct sffff sffff);
void accept_sffii(struct sffii sffii);
void accept_siiff(struct siiff siiff);
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
