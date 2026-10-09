typedef va_list __gnuc_va_list;
typedef va_list __builtin_va_list;

typedef signed __int128 __int128_t;
typedef unsigned __int128 __uint128_t;

extern void *memcpy(void *s1, const void *s2, unsigned long n);
extern void *memset(void *str, int c, unsigned long n);

long double __wcc_sf_add_ld(long double a, long double b);
long double __wcc_sf_subtract_ld(long double a, long double b);
long double __wcc_sf_multiply_ld(long double a, long double b);
long double __wcc_sf_divide_ld(long double a, long double b);

int __wcc_sf_ld_eq(long double a, long double b);
int __wcc_sf_ld_ne(long double a, long double b);
int __wcc_sf_ld_lt(long double a, long double b);
int __wcc_sf_ld_gt(long double a, long double b);
int __wcc_sf_ld_le(long double a, long double b);
int __wcc_sf_ld_ge(long double a, long double b);

long double  __wcc_sf_convert_float_to_ld     (float f);
long double  __wcc_sf_convert_double_to_ld    (double d);
float        __wcc_sf_convert_ld_to_float     (long double ld);
double       __wcc_sf_convert_ld_to_double    (long double ld);
long         __wcc_sf_convert_ld_to_int64     (long double ld);
unsigned long __wcc_sf_convert_ld_to_uint64   (long double ld);
long double   __wcc_sf_convert_int64_to_ld    (long i);
long double   __wcc_sf_convert_uint64_to_ld   (unsigned long i);
