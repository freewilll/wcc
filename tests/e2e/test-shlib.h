extern char c;
extern int i;
extern short s;
extern long l;

extern unsigned char uc;
extern unsigned int ui;
extern unsigned short us;
extern unsigned long ul;

extern float f;
extern double d;
extern long double ld;

extern int *pi;

extern struct st { int i, j; } st;

extern char ca[2];
extern short sa[2];
extern int ia[2];
extern long la[2];
extern float fa[2];
extern double da[2];
extern long double lda[2];

typedef int (*FuncReturningInt)();

extern FuncReturningInt fri;

int inc_c();
int inc_s();
int inc_i();
int inc_l();

int inc_uc();
int inc_us();
int inc_ui();
int inc_ul();

int inc_f();
int inc_d();
int inc_ld();

int inc_pi();

int inc_st();
int inc_ia();

void test_address_of();

int add_one(int i);
