char c;
int i;
short s;
long l;

unsigned char uc;
unsigned int ui;
unsigned short us;
unsigned long ul;

float f;
double d;
long double ld;

int *pi;

struct st { int i, j; } st;

char ca[2];
short sa[2];
int ia[2];
long la[2];
float fa[2];
double da[2];
long double lda[2];

typedef int (*FuncReturningInt)();

FuncReturningInt fri;

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
