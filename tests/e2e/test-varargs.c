#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

struct sca {char c[3];};

int vp(char *template, char *expected, ...) {
    va_list ap;
    va_start(ap, expected);
    char buffer[100];
    vsprintf(buffer, template, ap);
    assert_string(expected, buffer, expected);
}

// Test va_start and va_list are correct by handing va_list over to vsprintf
// and asserting the result is correct.
int test_va_start_va_list_abi() {
    vp("%d %d %d %d",          "1 2 3 4",       1, 2, 3, 4);
    vp("%d %d %d %d %d",       "1 2 3 4 5",     1, 2, 3, 4, 5);
    vp("%d %d %d %d %d %d",    "1 2 3 4 5 6",   1, 2, 3, 4, 5, 6);
    vp("%d %d %d %d %d %d %d", "1 2 3 4 5 6 7", 1, 2, 3, 4, 5, 6, 7);

    vp("%.1f %.1f %.1f %.1f",                "1.1 2.1 3.1 4.1",             1.1, 2.1, 3.1, 4.1);
    vp("%.1f %.1f %.1f %.1f %.1f",           "1.1 2.1 3.1 4.1 5.1",         1.1, 2.1, 3.1, 4.1, 5.1);
    vp("%.1f %.1f %.1f %.1f %.1f %.1f",      "1.1 2.1 3.1 4.1 5.1 6.1",     1.1, 2.1, 3.1, 4.1, 5.1, 6.1);
    vp("%.1f %.1f %.1f %.1f %.1f %.1f %.1f", "1.1 2.1 3.1 4.1 5.1 6.1 7.1", 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1);

    vp("%.1Lf %.1Lf %.1Lf %.1Lf",                   "1.1 2.1 3.1 4.1",             1.1L, 2.1L, 3.1L, 4.1L);
    vp("%.1Lf %.1Lf %.1Lf %.1Lf %.1Lf",             "1.1 2.1 3.1 4.1 5.1",         1.1L, 2.1L, 3.1L, 4.1L, 5.1L);
    vp("%.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf",       "1.1 2.1 3.1 4.1 5.1 6.1",     1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L);
    vp("%.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf", "1.1 2.1 3.1 4.1 5.1 6.1 7.1", 1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L);

    vp("%d %.1f", "1 1.1", 1, 1.1);
    vp("%d %.1f %.1Lf", "1 1.1 2.1", 1, 1.1, 2.1L);
    vp("%d %.1f %.1Lf %s", "1 1.1 2.1 foo", 1, 1.1, 2.1L, "foo");
}

typedef long double LD;

int   add_ini(int   c, ...) { va_list ap; va_start(ap, c); int   r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, int   ); va_end(ap); return r; }
float add_fli(int   c, ...) { va_list ap; va_start(ap, c); float r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, double); va_end(ap); return r; }
LD    add_ldi(int   c, ...) { va_list ap; va_start(ap, c); LD    r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, LD    ); va_end(ap); return r; }
int   add_inf(float c, ...) { va_list ap; va_start(ap, c); int   r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, int   ); va_end(ap); return r; }
float add_flf(float c, ...) { va_list ap; va_start(ap, c); float r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, double); va_end(ap); return r; }
LD    add_ldf(float c, ...) { va_list ap; va_start(ap, c); LD    r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, LD    ); va_end(ap); return r; }
int   add_inl(LD    c, ...) { va_list ap; va_start(ap, c); int   r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, int   ); va_end(ap); return r; }
float add_fll(LD    c, ...) { va_list ap; va_start(ap, c); float r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, double); va_end(ap); return r; }
LD    add_ldl(LD    c, ...) { va_list ap; va_start(ap, c); LD    r = 0; for (int i = 0; i < c; i++) r += va_arg(ap, LD    ); va_end(ap); return r; }

void test_additions() {
    // With an integer as a parameter
    assert_int(0,  add_ini(0),                                 "add_ini 0");
    assert_int(1,  add_ini(1,  1),                             "add_ini 1");
    assert_int(3,  add_ini(2,  1, 2),                          "add_ini 2");
    assert_int(6,  add_ini(3,  1, 2, 3),                       "add_ini 3");
    assert_int(10, add_ini(4,  1, 2, 3, 4),                    "add_ini 4");
    assert_int(15, add_ini(5,  1, 2, 3, 4, 5),                 "add_ini 5");
    assert_int(21, add_ini(6,  1, 2, 3, 4, 5, 6),              "add_ini 6");
    assert_int(28, add_ini(7,  1, 2, 3, 4, 5, 6, 7),           "add_ini 7");
    assert_int(36, add_ini(8,  1, 2, 3, 4, 5, 6, 7, 8),        "add_ini 8");
    assert_int(45, add_ini(9,  1, 2, 3, 4, 5, 6, 7, 8, 9),     "add_ini 9");
    assert_int(55, add_ini(10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10), "add_ini10");

    assert_float(0.0,  add_fli(0),                                                           "add_fli 0");
    assert_float(1.1,  add_fli(1,  1.1),                                                     "add_fli 1");
    assert_float(3.2,  add_fli(2,  1.1, 2.1),                                                "add_fli 3");
    assert_float(6.3,  add_fli(3,  1.1, 2.1, 3.1),                                           "add_fli 6");
    assert_float(10.4, add_fli(4,  1.1, 2.1, 3.1, 4.1),                                      "add_fli 10");
    assert_float(15.5, add_fli(5,  1.1, 2.1, 3.1, 4.1, 5.1),                                 "add_fli 15");
    assert_float(21.6, add_fli(6,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1),                            "add_fli 21");
    assert_float(28.7, add_fli(7,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1),                       "add_fli 28");
    assert_float(36.8, add_fli(8,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1),                  "add_fli 36");
    assert_float(45.9, add_fli(9,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1),             "add_fli 45");
    assert_float(56.0, add_fli(10, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1),       "add_fli 56");
    assert_float(67.1, add_fli(11, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1, 11.1), "add_fli 67");

    assert_long_double(0.0,  add_ldi(0),                                                               "add_ldi 0");
    assert_long_double(1.1,  add_ldi(1,  1.1L),                                                        "add_ldi 1");
    assert_long_double(3.2,  add_ldi(2,  1.1L, 2.1L),                                                  "add_ldi 3");
    assert_long_double(6.3,  add_ldi(3,  1.1L, 2.1L, 3.1L),                                            "add_ldi 6");
    assert_long_double(10.4, add_ldi(4,  1.1L, 2.1L, 3.1L, 4.1L),                                      "add_ldi 10");
    assert_long_double(15.5, add_ldi(5,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L),                                "add_ldi 15");
    assert_long_double(21.6, add_ldi(6,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L),                          "add_ldi 21");
    assert_long_double(28.7, add_ldi(7,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L),                    "add_ldi 28");
    assert_long_double(36.8, add_ldi(8,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L),              "add_ldi 36");
    assert_long_double(45.9, add_ldi(9,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L, 9.1L),        "add_ldi 45");
    assert_long_double(56.0, add_ldi(10, 1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L, 9.1L, 10.1L), "add_ldi 56");

    // With a float as a parameter
    assert_int(0,  add_inf(0),                                 "add_inf 0");
    assert_int(1,  add_inf(1,  1),                             "add_inf 1");
    assert_int(3,  add_inf(2,  1, 2),                          "add_inf 2");
    assert_int(6,  add_inf(3,  1, 2, 3),                       "add_inf 3");
    assert_int(10, add_inf(4,  1, 2, 3, 4),                    "add_inf 4");
    assert_int(15, add_inf(5,  1, 2, 3, 4, 5),                 "add_inf 5");
    assert_int(21, add_inf(6,  1, 2, 3, 4, 5, 6),              "add_inf 6");
    assert_int(28, add_inf(7,  1, 2, 3, 4, 5, 6, 7),           "add_inf 7");
    assert_int(36, add_inf(8,  1, 2, 3, 4, 5, 6, 7, 8),        "add_inf 8");
    assert_int(45, add_inf(9,  1, 2, 3, 4, 5, 6, 7, 8, 9),     "add_inf 9");
    assert_int(55, add_inf(10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10), "add_inf 10");

    assert_float(0.0,  add_flf(0),                                                           "add_flf 0");
    assert_float(1.1,  add_flf(1,  1.1),                                                     "add_flf 1");
    assert_float(3.2,  add_flf(2,  1.1, 2.1),                                                "add_flf 3");
    assert_float(6.3,  add_flf(3,  1.1, 2.1, 3.1),                                           "add_flf 6");
    assert_float(10.4, add_flf(4,  1.1, 2.1, 3.1, 4.1),                                      "add_flf 10");
    assert_float(15.5, add_flf(5,  1.1, 2.1, 3.1, 4.1, 5.1),                                 "add_flf 15");
    assert_float(21.6, add_flf(6,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1),                            "add_flf 21");
    assert_float(28.7, add_flf(7,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1),                       "add_flf 28");
    assert_float(36.8, add_flf(8,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1),                  "add_flf 36");
    assert_float(45.9, add_flf(9,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1),             "add_flf 45");
    assert_float(56.0, add_flf(10, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1),       "add_flf 56");
    assert_float(67.1, add_flf(11, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1, 11.1), "add_flf 67");

    assert_long_double(0.0,  add_ldf(0),                                                               "add_ldf 0");
    assert_long_double(1.1,  add_ldf(1,  1.1L),                                                        "add_ldf 1");
    assert_long_double(3.2,  add_ldf(2,  1.1L, 2.1L),                                                  "add_ldf 3");
    assert_long_double(6.3,  add_ldf(3,  1.1L, 2.1L, 3.1L),                                            "add_ldf 6");
    assert_long_double(10.4, add_ldf(4,  1.1L, 2.1L, 3.1L, 4.1L),                                      "add_ldf 10");
    assert_long_double(15.5, add_ldf(5,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L),                                "add_ldf 15");
    assert_long_double(21.6, add_ldf(6,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L),                          "add_ldf 21");
    assert_long_double(28.7, add_ldf(7,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L),                    "add_ldf 28");
    assert_long_double(36.8, add_ldf(8,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L),              "add_ldf 36");
    assert_long_double(45.9, add_ldf(9,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L, 9.1L),        "add_ldf 45");
    assert_long_double(56.0, add_ldf(10, 1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L, 9.1L, 10.1L), "add_ldf 56");

    // With a long double as a parameter
    assert_int(0,  add_inl(0),                                 "add_inl 0");
    assert_int(1,  add_inl(1,  1),                             "add_inl 1");
    assert_int(3,  add_inl(2,  1, 2),                          "add_inl 2");
    assert_int(6,  add_inl(3,  1, 2, 3),                       "add_inl 3");
    assert_int(10, add_inl(4,  1, 2, 3, 4),                    "add_inl 4");
    assert_int(15, add_inl(5,  1, 2, 3, 4, 5),                 "add_inl 5");
    assert_int(21, add_inl(6,  1, 2, 3, 4, 5, 6),              "add_inl 6");
    assert_int(28, add_inl(7,  1, 2, 3, 4, 5, 6, 7),           "add_inl 7");
    assert_int(36, add_inl(8,  1, 2, 3, 4, 5, 6, 7, 8),        "add_inl 8");
    assert_int(45, add_inl(9,  1, 2, 3, 4, 5, 6, 7, 8, 9),     "add_inl 9");
    assert_int(55, add_inl(10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10), "add_inl 10");

    assert_float(0.0,  add_fll(0),                                                           "add_flll 0");
    assert_float(1.1,  add_fll(1,  1.1),                                                     "add_flll 1");
    assert_float(3.2,  add_fll(2,  1.1, 2.1),                                                "add_flll 3");
    assert_float(6.3,  add_fll(3,  1.1, 2.1, 3.1),                                           "add_flll 6");
    assert_float(10.4, add_fll(4,  1.1, 2.1, 3.1, 4.1),                                      "add_flll 10");
    assert_float(15.5, add_fll(5,  1.1, 2.1, 3.1, 4.1, 5.1),                                 "add_flll 15");
    assert_float(21.6, add_fll(6,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1),                            "add_flll 21");
    assert_float(28.7, add_fll(7,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1),                       "add_flll 28");
    assert_float(36.8, add_fll(8,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1),                  "add_flll 36");
    assert_float(45.9, add_fll(9,  1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1),             "add_flll 45");
    assert_float(56.0, add_fll(10, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1),       "add_flll 56");
    assert_float(67.1, add_fll(11, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1, 11.1), "add_flll 67");

    assert_long_double(0.0,  add_ldl(0) ,                                                              "add_ldl 0");
    assert_long_double(1.1,  add_ldl(1,  1.1L),                                                        "add_ldl 1");
    assert_long_double(3.2,  add_ldl(2,  1.1L, 2.1L),                                                  "add_ldl 3");
    assert_long_double(6.3,  add_ldl(3,  1.1L, 2.1L, 3.1L),                                            "add_ldl 6");
    assert_long_double(10.4, add_ldl(4,  1.1L, 2.1L, 3.1L, 4.1L),                                      "add_ldl 10");
    assert_long_double(15.5, add_ldl(5,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L),                                "add_ldl 15");
    assert_long_double(21.6, add_ldl(6,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L),                          "add_ldl 21");
    assert_long_double(28.7, add_ldl(7,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L),                    "add_ldl 28");
    assert_long_double(36.8, add_ldl(8,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L),              "add_ldl 36");
    assert_long_double(45.9, add_ldl(9,  1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L, 9.1L),        "add_ldl 45");
    assert_long_double(56.0, add_ldl(10, 1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 7.1L, 8.1L, 9.1L, 10.1L), "add_ldl 56");
}

struct s { int i[5]; };

// Test returning a struct in memory from a vararg function. The return vector is
// passed in rdi.
struct s return_struct(int c, ...) {
    va_list ap; va_start(ap, c);
    int r = 0;
    for (int i = 0; i < c; i++) r += va_arg(ap, int);
    va_end(ap);

    struct s s;
    s.i[0] = r;
    return s;
}

void test_struct_return() {
    struct s s;
    s = return_struct(0                   ); assert_int(0,  s.i[0], "return struct 0");
    s = return_struct(1, 1                ); assert_int(1,  s.i[0], "return struct 1");
    s = return_struct(2, 1, 2             ); assert_int(3,  s.i[0], "return struct 2");
    s = return_struct(3, 1, 2, 3          ); assert_int(6,  s.i[0], "return struct 3");
    s = return_struct(4, 1, 2, 3, 4       ); assert_int(10, s.i[0], "return struct 4");
    s = return_struct(5, 1, 2, 3, 4, 5    ); assert_int(15, s.i[0], "return struct 5");
    s = return_struct(6, 1, 2, 3, 4, 5, 6 ); assert_int(21, s.i[0], "return struct 6");
}

int test_va_arg_sizes(int i, ...) {
    va_list ap;
    va_start(ap, i);

    assert_int(1, sizeof(va_arg(ap, char)),         "Varargs va_arg size 1");
    assert_int(2, sizeof(va_arg(ap, short)),        "Varargs va_arg size 2");
    assert_int(4, sizeof(va_arg(ap, int)),          "Varargs va_arg size 3");
    assert_int(3, sizeof(va_arg(ap, struct sca)),   "Varargs va_arg size 4");
    va_end(ap);
}

static int vadd_ints1(int count, va_list ap) {
    int result = 0;
    for (int i = 0; i < count; i++) result += va_arg(ap, int);
    return result;
}

int vadd_ints2(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int result = 0;
    result = va_arg(ap, int);

    // Pass a the ap that has moved beyond the first argument to vadd_ints
    result += vadd_ints1(count - 1, ap);
    va_end(ap);
    return result;
}

static void test_vadd_ints() {
    // Test two stage processing of varargs in separate functions
    assert_int(1,  vadd_ints2(1, 1),                      "Varargs passing va_list as an argument with ints 1");
    assert_int(3,  vadd_ints2(2, 1, 2),                   "Varargs passing va_list as an argument with ints 2");
    assert_int(6,  vadd_ints2(3, 1, 2, 3),                "Varargs passing va_list as an argument with ints 3");
    assert_int(10, vadd_ints2(4, 1, 2, 3, 4),             "Varargs passing va_list as an argument with ints 4");
    assert_int(15, vadd_ints2(5, 1, 2, 3, 4, 5),          "Varargs passing va_list as an argument with ints 5");
    assert_int(21, vadd_ints2(6, 1, 2, 3, 4, 5, 6),       "Varargs passing va_list as an argument with ints 6");
    assert_int(28, vadd_ints2(7, 1, 2, 3, 4, 5, 6, 7),    "Varargs passing va_list as an argument with ints 7");
    assert_int(36, vadd_ints2(8, 1, 2, 3, 4, 5, 6, 7, 8), "Varargs passing va_list as an argument with ints 8");
}

static float vadd_floats1(int count, va_list ap) {
    float result = 0;
    for (int i = 0; i < count; i++) result += va_arg(ap, double);
    return result;
}

float vadd_floats2(int count, ...) {
    va_list ap;
    va_start(ap, count);
    float result = 0;
    result = va_arg(ap, double);

    // Pass a the ap that has moved beyond the first argument to vadd_floats
    result += vadd_floats1(count - 1, ap);
    va_end(ap);
    return result;
}

static void test_vadd_floats() {
    // Test two stage processing of varargs in separate functions
    assert_float(1.1,  vadd_floats2(1, 1.1),                                    "Varargs passing va_list as an argument with floats 1");
    assert_float(3.1,  vadd_floats2(2, 1.1, 2.0),                               "Varargs passing va_list as an argument with floats 2");
    assert_float(6.1,  vadd_floats2(3, 1.1, 2.0, 3.0),                          "Varargs passing va_list as an argument with floats 3");
    assert_float(10.1, vadd_floats2(4, 1.1, 2.0, 3.0, 4.0),                     "Varargs passing va_list as an argument with floats 4");
    assert_float(15.1, vadd_floats2(5, 1.1, 2.0, 3.0, 4.0, 5.0),                "Varargs passing va_list as an argument with floats 5");
    assert_float(21.1, vadd_floats2(6, 1.1, 2.0, 3.0, 4.0, 5.0, 6.0),           "Varargs passing va_list as an argument with floats 6");
    assert_float(28.1, vadd_floats2(7, 1.1, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0),      "Varargs passing va_list as an argument with floats 7");
    assert_float(36.1, vadd_floats2(8, 1.1, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0), "Varargs passing va_list as an argument with floats 8");
}

static void va6ild(int foo, ...) {
    va_list ap;
    va_start(ap, foo);
    for (int i = 0; i < 6; i++) assert_int(i + 1, va_arg(ap, int), "Long double alignment");
    assert_long_double(7.1, va_arg(ap, long double), "Long double alignment 7");
    va_end(ap);
}

// Test pushing of 5 int varargs followed by a long double. This ensures that
// the alignment of the long double is correct
int test_long_double_alignment() {
    va6ild(0, 1, 2, 3, 4, 5, 6, 7.1L);
}

struct si4 { int i[4]; };
struct si5 { int i[5]; };
struct sf4 { float f[4]; };
struct sf5 { float f[5]; };

void foosi4(int count, ...) {
    va_list ap;
    va_start(ap, count);

    for (int i = 0; i < count; i++) {
        struct si4 si4 = va_arg(ap, struct si4);
        if (i % 2 == 0) {
            assert_int(1, si4.i[0], "foosi4 1");
            assert_int(2, si4.i[1], "foosi4 2");
            assert_int(3, si4.i[2], "foosi4 3");
            assert_int(4, si4.i[3], "foosi4 4");
        }
        else {
            assert_int(5,  si4.i[0], "foosi4 5");
            assert_int(6,  si4.i[1], "foosi4 6");
            assert_int(7,  si4.i[2], "foosi4 7");
            assert_int(8,  si4.i[3], "foosi4 8");
        }
    }
    va_end(ap);
}

void foosf4(int count, ...) {
    va_list ap;
    va_start(ap, count);

    for (int i = 0; i < count; i++) {
        struct sf4 sf4 = va_arg(ap, struct sf4);
        if (i % 2 == 0) {
            assert_float(1.1, sf4.f[0], "foosf4 1");
            assert_float(2.1, sf4.f[1], "foosf4 2");
            assert_float(3.1, sf4.f[2], "foosf4 3");
            assert_float(4.1, sf4.f[3], "foosf4 4");
        }
        else {
            assert_float(5.1,  sf4.f[0], "foosf4 5");
            assert_float(6.1,  sf4.f[1], "foosf4 6");
            assert_float(7.1,  sf4.f[2], "foosf4 7");
            assert_float(8.1,  sf4.f[3], "foosf4 8");
        }
    }
    va_end(ap);
}

void foosi5(float count, ...) {
    va_list ap;
    va_start(ap, count);

    for (int i = 0; i < count; i++) {
        struct si5 si5 = va_arg(ap, struct si5);

        if (i % 2 == 0) {
            assert_int(1, si5.i[0], "foosi5 1");
            assert_int(2, si5.i[1], "foosi5 2");
            assert_int(3, si5.i[2], "foosi5 3");
            assert_int(4, si5.i[3], "foosi5 4");
            assert_int(5, si5.i[4], "foosi5 5");
        }
        else {
            assert_int(6,  si5.i[0], "foosi5 6");
            assert_int(7,  si5.i[1], "foosi5 7");
            assert_int(8,  si5.i[2], "foosi5 8");
            assert_int(9,  si5.i[3], "foosi5 9");
            assert_int(10, si5.i[4], "foosi5 10");
        }
    }

    va_end(ap);
}

void foosf5(float count, ...) {
    va_list ap;
    va_start(ap, count);

    for (int i = 0; i < count; i++) {
        struct sf5 sf5 = va_arg(ap, struct sf5);

        if (i % 2 == 0) {
            assert_float(1.1, sf5.f[0], "foosf5 1.1");
            assert_float(2.1, sf5.f[1], "foosf5 2.1");
            assert_float(3.1, sf5.f[2], "foosf5 3.1");
            assert_float(4.1, sf5.f[3], "foosf5 4.1");
            assert_float(5.1, sf5.f[4], "foosf5 5.1");
        }
        else {
            assert_float(6.1,  sf5.f[0], "foosf5 6.1");
            assert_float(7.1,  sf5.f[1], "foosf5 7.1");
            assert_float(8.1,  sf5.f[2], "foosf5 8.1");
            assert_float(9.1,  sf5.f[3], "foosf5 9.1");
            assert_float(10.1, sf5.f[4], "foosf5 10.1");
        }
    }

    va_end(ap);
}

int test_structs() {
    struct si4 si41; si41.i[0] = 1; si41.i[1] = 2; si41.i[2] = 3; si41.i[3] = 4;
    struct si4 si42; si42.i[0] = 5; si42.i[1] = 6; si42.i[2] = 7; si42.i[3] = 8;
    foosi4(1, si41);                    // 3 int registers used
    foosi4(2, si41, si42);              // 5 int registers used
    foosi4(2, si41, si42, si41);        // 5 int registers used, the last arg is on the stack
    foosi4(4, si41, si42, si41, si42);  // 5 int registers used, the last two args are on the stack

    struct sf4 sf41; sf41.f[0] = 1.1; sf41.f[1] = 2.1; sf41.f[2] = 3.1; sf41.f[3] = 4.1;
    struct sf4 sf42; sf42.f[0] = 5.1; sf42.f[1] = 6.1; sf42.f[2] = 7.1; sf42.f[3] = 8.1;
    foosf4(1, sf41);                          // 3 sse registers used
    foosf4(2, sf41, sf42);                    // 5 sse registers used
    foosf4(3, sf41, sf42, sf41);              // 7 sse registers used
    foosf4(4, sf41, sf42, sf41, sf42);        // 7 sse registers used, the last arg is on the stack
    foosf4(5, sf41, sf42, sf41, sf42, sf41);  // 7 sse registers used, the last two args are on the stack

    // On the stack, since size > 16
    struct si5 si51; si51.i[0] = 1; si51.i[1] = 2; si51.i[2] = 3; si51.i[3] = 4; si51.i[4] = 5;
    struct si5 si52; si52.i[0] = 6; si52.i[1] = 7; si52.i[2] = 8; si52.i[3] = 9; si52.i[4] = 10;
    foosi5(1, si51);
    foosi5(2, si51, si52);
    foosi5(4, si51, si52, si51, si52);

    struct sf5 sf51; sf51.f[0] = 1.1; sf51.f[1] = 2.1; sf51.f[2] = 3.1; sf51.f[3] = 4.1; sf51.f[4] = 5.1;
    struct sf5 sf52; sf52.f[0] = 6.1; sf52.f[1] = 7.1; sf52.f[2] = 8.1; sf52.f[3] = 9.1; sf52.f[4] = 10.1;
    foosf5(1, sf51);
    foosf5(2, sf51, sf52);
    foosf5(4, sf51, sf52, sf51, sf52);
}

int test_va_mixed_types1(int foo, ...) {
    va_list ap;
    va_start(ap, foo);
    int result = 0;

    result = result * 10 + va_arg(ap, int);
    result = result * 10 + va_arg(ap, int);
    result = result * 10 + va_arg(ap, int);

    int *ia = va_arg(ap, int*);
    result = result * 10 + ia[0];
    result = result * 10 + ia[1];

    struct s {int i,j;};
    struct s sv = va_arg(ap, struct s);
    result = result * 10 + sv.i;
    result = result * 10 + sv.j;

    result = result * 10 + va_arg(ap, int);
    result = result * 10 + (int) va_arg(ap, long);

    va_end(ap);
    return result;
}

int test_va_mixed_types2(int foo, ...) {
    va_list ap;
    va_start(ap, foo);
    int result = 0;

    result = result * 10 + va_arg(ap, int);
    result = result * 10 + (int) va_arg(ap, long);
    result = result * 10 + va_arg(ap, int);

    va_end(ap);
    return result;
}

int test_va_mixed_types3(long ll, ...) {
    va_list ap;
    va_start(ap, ll);
    int result = ll;

    result = result * 10 + va_arg(ap, int);
    result = result * 10 + (int) va_arg(ap, long);
    result = result * 10 + va_arg(ap, int);

    va_end(ap);
    return result;
}

static void test_va_mixed_types() {
    char c1 = 1;
    short s1 = 2;
    int i1 = 3;
    int ia[2];
    ia[0] = 4;
    ia[1] = 5;
    struct {int i,j;} st1;
    st1.i = 6;
    st1.j = 7;
    int i2 = 8;
    long ll1 = 9;
    long ll2 = 1;
    assert_int(123456789,   test_va_mixed_types1(1, c1, s1, i1, ia, st1, i2, ll1),  "Varargs with mixed types 1");
    assert_int(398,         test_va_mixed_types2(1, i1, ll1, i2),                   "Varargs with mixed types 2");
    assert_int(9318,        test_va_mixed_types3(ll1, i1, ll2, i2),                 "Varargs with mixed types 3");
}

static void test_default_argument_promotions() {
    char buffer[100];

    sprintf(buffer, "%d", (char) 1);
    assert_string("1", buffer, "Default argument promotion in ... for char -> int");

    float f = 1.1;
    sprintf(buffer, "%f", (float) 1.1);
    assert_string("1.100000", buffer, "Default argument promotion in ... for float -> double");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_va_start_va_list_abi();
    test_additions();
    test_struct_return();

    test_va_arg_sizes(1, 2, 3, 4, 5, 6);
    test_vadd_ints();
    test_vadd_floats();
    test_long_double_alignment();
    test_structs();
    test_va_mixed_types();
    test_default_argument_promotions();

    finalize();
}
