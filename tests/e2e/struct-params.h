struct spf { float f1;};
struct spd { double d1;};
struct spdf { double d1; float f1; };
struct sff { float f1, f2; };
struct sdd { double d1, d2; };
struct sffff { float f1, f2, f3, f4; };

void accept_spf(struct spf spf);
void accept_spd(struct spd spd);
void accept_spdf(struct spdf spdf);
void accept_sff(struct sff sff);
void accept_sdd(struct sdd sdd);
void accept_sffff(struct sffff sffff);
