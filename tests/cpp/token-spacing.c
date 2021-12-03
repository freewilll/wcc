// Test leading spaces are handled correctly when combined with macro substitution
#define p +
#define q +
    p q

// From https://gcc.gnu.org/onlinedocs/cppinternals/Token-Spacing.html#Token-Spacing

#define PLUS +
#define EMPTY
#define f(x) =x=
+PLUS -EMPTY- PLUS+ f(=)

#define add(x, y, z) x + y +z;
sum = add (1,2, 3);

#define foo bar
#define bar baz
[foo]

#define foo2 bar2
#define bar2 EMPTY baz
#define EMPTY
[foo2] EMPTY;

#define empty_func(x) EMPTY x
empty_func(foo)
 empty_func(foo)

#define func(x) x
 func(foo)

// From http://www.chiark.greenend.org.uk/doc/cpp-4.3-doc/cppinternals.html
#define foo3() bar3
foo3
baz3
