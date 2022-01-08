#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

typedef int altint, intarr[3], *intptr, *arrptrint[3];
typedef struct {int a;} s;
typedef struct {int a, b;} args_t;

static int altint_func(altint i)   { return i;     }
static int intarr_func(intarr ia)  { return ia[0]; }
static int intptr_func(intptr ip)  { return *ip;   }
static int sfunc(s v)              { return v.a;   }
static int psfunc(s *v)            { return v->a;  }

// Note: wcc will also allow (struct {int a, b;} args), but gcc won't accept
// that, stating a type mismatch when the function is called.
static int process_anonymous_struct(args_t args) {
    return args.a + args.b;
}

void *function_returning_void() {}

static void test_typedef_functions() {
    altint i;
    i = 1;
    assert_int(1, altint_func(i), "Function with int typedef parameter");

    intptr ip = &i;
    assert_int(1, intptr_func(ip), "Function with intptr typedef parameter");

    intarr ia;
    ia[0] = 1;
    assert_int(1, intarr_func(ia), "Function with intarr typedef parameter");

    s sv1;
    sv1.a = 1;
    assert_int(1, sfunc(sv1), "Function with typedef struct parameter");

    s *psv = &sv1;
    assert_int(1, psv->a, "Dereference pointer to typedef struct");
    assert_int(1, sfunc(*psv), "Function with typedef struct parameter");
    assert_int(1, psfunc(psv), "Function with pointer to typedef struct parameter");

    args_t x;
    x.a = 1;
    x.b = 2;
    assert_int(3, process_anonymous_struct(x), "Anonymous structs in function params");

    // Test typedef of pointer to a function.
    typedef void (*f_t)();
    f_t f = function_returning_void;
    f();
}

static void test_arrays_of_typedefs() {
    altint ia2[3];
    ia2[0] = 1;
    assert_int(1, ia2[0], "Array of typedef");

    altint i = 1;
    arrptrint api;
    api[0] = &i;
    assert_int(1, *api[0], "Typedef of array of pointer to int");

    arrptrint aapi[3];
    aapi[0][0] = &i;
    assert_int(1, *aapi[0][0], "Array of typedef of array of pointer to int");

    typedef int ia23_t[2][3];
    ia23_t ia3;
    assert_int(24, sizeof(ia3), "ia3[2][3]");
}

static void test_typedef_scopes() {
    assert_int(4, sizeof(altint), "Typedefs scopes 1");
    typedef char altint;
    assert_int(1, sizeof(altint), "Typedefs scopes 2");
    {
        typedef short altint;
        assert_int(2, sizeof(altint), "Typedefs scopes 3");
    }
    assert_int(1, sizeof(altint), "Typedefs scopes 4");
}

static void test_typedef_combinations() {
    typedef int *pi_t;
    typedef pi_t api_t[3];
    api_t api;
    assert_int(24, sizeof(api), "Typedef combinations 1");

    {
        // local redefinition
        typedef int *pi_t;
        typedef pi_t api_t[4];
        api_t api;
        assert_int(32, sizeof(api), "Typedef combinations 2");
    }

    typedef int ai_t[3];
    typedef ai_t *pai_t;
    pai_t pai;
    assert_int(8, sizeof(pai), "Typedef combinations 3");

    typedef int i3_t[3];
    typedef int i3_t[3]; // A redefinition should be allowed
    assert_int(12, sizeof(i3_t), "Typedef combinations 4");

    typedef i3_t i4_t;
    assert_int(12, sizeof(i4_t), "Typedef combinations 5");
    {
        // A local redefinition should be allowed. This also tests the
        // lexing/parsing code that should identify i3_t as a typedef, but i4_t
        // as an identifier, used to redeclare typedef i4_t.
        typedef i3_t *i4_t;
        assert_int(8, sizeof(i4_t), "Typedef combinations 6");
    }
}

static void test_typedef_structs() {
    // Typedef in a struct
    typedef int aint;
    struct s2 {aint i;};
    struct s2 v2;
    v2.i = 1;
    assert_int(1, v2.i, "Typedef in a struct");

    typedef struct s3 {aint i;} S3;
    S3 v3;
    v3.i = 1;
    assert_int(1, v3.i, "Typedef in a typedef struct");
}

static void test_typedef_tags() {
    // Test typedef tag having same identifier as a struct tag
    typedef struct s4 {int i;} s4;
    struct s4 sv2;
    sv2.i = 1;
    assert_int(1, sv2.i, "Same typedef & struct tag 1");

    s4 sv3;
    sv3.i = 1;
    assert_int(1, sv3.i, "Same typedef & struct tag 2");

    // Test typedef tag having same identifier as a union tag
    typedef union u1 {int i;} u1;
    union u1 uv1;
    uv1.i = 1;
    assert_int(1, uv1.i, "Same typedef & union tag 1");

    u1 uv2;
    uv2.i = 1;
    assert_int(1, uv2.i, "Same typedef & union tag 2");

    // Test enum tag having same identifier as a struct tag
    typedef enum e {E} e;
    enum e ev1;
    ev1 = E;
    assert_int(E, ev1, "Same typedef & enum tag 1");

    e ev2;
    ev2 = E;
    assert_int(E, ev2, "Same typedef & enum tag 2");
}

static int test_typedef_forward_declaration() {
    // Test a typedef of an incompelte struct
    typedef struct s S;
    struct s {int i;};
    S sv;
    sv.i = 1;
    assert_int(1, sv.i, "Forward declaration of struct typedef");
}

// Not much can be tested here, but this verifies that this kind of function
// definition can at least be dealt with without error.
static void do_nothing_with_an_anonymous_struct(struct {int a;} v) {}

typedef int foo;

int test_typedef_redeclared_as_variable(void) {
    foo foo;
    foo = 1;
    assert_int(1, foo, "Typedef redeclared as variable");
    {
        foo = 2;
        assert_int(2, foo, "Typedef redeclared as variable");
    }
    assert_int(2, foo, "Typedef redeclared as variable");
}

// C89 3.5.4.3
// In a parameter declaration, a single typedef name in parentheses is taken to be an
// abstract declarator that specifies a function with a single parameter, not as
// redundant parentheses around the identifier for a declarator.
typedef unsigned char u8;
int fu8(u8); // function with a u8 as a parmeter;
int fu8(u8 i) { return  i + 1; }

int test_typedef_as_single_function_parameter() {
    assert_int(2, fu8(1), "int fu8(u8);");
}

typedef int (fri)();

int return_int() { return 1; }

fri *function_that_returns_int = return_int;

static void test_typedef_of_function_returning_int() {
    assert_int(1, function_that_returns_int(), "typedef int (fri)()");
}

int main(int argc, char **argv) {
    parse_args(argc, argv, &verbose);

    test_typedef_functions();
    test_arrays_of_typedefs();
    test_typedef_scopes();
    test_typedef_combinations();
    test_typedef_structs();
    test_typedef_tags();
    test_typedef_forward_declaration();
    test_typedef_redeclared_as_variable();
    test_typedef_as_single_function_parameter();
    test_typedef_of_function_returning_int();

    finalize();
}
