// Check keywords work fine as pp-tokens too.
#define TOKEN_TEST1 define
#define TOKEN_TEST2 undef
#define TOKEN_TEST3 if
#define TOKEN_TEST4 ifdef
#define TOKEN_TEST5 ifdnef
#define TOKEN_TEST6 elif
#define TOKEN_TEST7 else
#define TOKEN_TEST8 endif

// Not defined
#if FOO
foo
#endif

// Defined & zero
#define FOO 0
#if FOO
foo
#endif

// Defined & non-zero
#undef FOO
#define FOO 1
#if FOO
foo

#endif

// Arithmetic with undef BAR
#if !FOO
notfoo
#endif

// Arithmetic with undef BAR
#if FOO + BAR
foo
#endif

// Arithmetic with defined BAR
#define BAR 1
#if FOO + BAR
foo
#endif

#undef BAR
#define BAR 0
#if FOO * BAR
foo
#endif

#undef BAR
#define BAR 1 + 'a'
#if BAR
foo
#endif

// Ensure all integer constants are interpreted as longs
#if 1073741824 * 4 == 4294967296
longs are ok
#endif

// Nesting with true in the middle
#if FOO
true
#if 0
x
#endif
#endif

// Nesting with false in the middle
#if FOO
true
#if 1
x
#endif
#endif

// if/else with FOO 0
#if FOO
foo
#else
bar
#endif

// if/else with FOO 1
#undef FOO
#define FOO 1
#if FOO
foo
#else
bar
#endif

// A couple of combinations
#if 1
foo
#elif 1
bar
#else
baz
#endif

#if 0
foo
#elif 1
bar
#else
baz
#endif

#if 1
foo
#elif 0
bar
#else
baz
#endif

#if 0
foo
#elif 0
bar
#else
baz
#endif

// Some nesting tests
#if 0
    #if 1
    foo
    #elif 1
    foo
    #else
    foo
        #if 1
        foo
        #elif 1
        foo
        #else
        foo
        #endif
    #endif
#elif 0
bar
    #if 1
    foo
    #endif
#else
baz
    #if 0
    foo
    #endif
#endif

// Check # directives in a skipped section have no effect
#if 0
    #define OBJ 1
    #define FUNC(x) x
    #undef FOO
#endif
OBJ
FUNC(1)
FOO

#undef FOO
#ifdef FOO
foo
#endif

#undef FOO
#ifndef FOO
foo
#endif

#define FOO
#ifdef FOO
foo
#endif

#ifndef FOO
foo
#endif

#undef FOO
#define FOO(x) a
#ifdef FOO
def
#endif

// Use of defined and defined(...)
#undef UNDEF
#define DEF
#if defined DEF
def
#endif

#if defined UNDEF
def
#endif

#if defined(DEF)
def
#endif

#if defined (DEF)
def
#endif

#if defined( DEF)
def
#endif

#if defined(DEF )
def
#endif

#if defined ( DEF )
def
#endif

#if !defined UNDEF
!defined undef
#endif

// defined in the middle of an expression
#if defined(UNDEF) || FOO
foo
#endif

#if defined UNDEF || FOO
foo
#endif

#if FOO && defined(DEF)
foo
#endif

// Ensure the error directive is skipped
#if 0
#error "error"
#endif

// Ensure the include directive is skipped
#if 0
#include "foo"
#endif

#define INC1(x) # x
#include INC1(include.h)

#define INC2 "include.h"
#include INC2
