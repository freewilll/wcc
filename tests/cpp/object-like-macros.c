#define FOO foo bar
FOO
#undef FOO
FOO
#define FOO  foo2 bar2
FOO
#undef FOO with more tokens
FOO

#define INT1 1
INT1

#define INT2 2
INT2

#define STR1 "string"
STR1

#define STR2 string2
string2

#define CHR1 'c'
'c'

#define CHR2 'c' + 1
CHR2

#define LOTS_OF_TOKENS1 "lots" 'o' f tokens
LOTS_OF_TOKENS1

#define SELF1 SELF1
SELF1

#define x y
#define y x
x
y

#define COMBO INT1 INT2 CHR1 CHR2
COMBO

#define COMBO2 COMBO SELF1
COMBO2

#define EMPTY
a EMPTY b
a  EMPTY  b
a EMPTY EMPTY b
a  EMPTY EMPTY b
a  EMPTY  EMPTY b
a  EMPTY  EMPTY  b
EMPTY a
EMPTY EMPTY a
EMPTY EMPTY  a

A # in the middle of a line

// Check an object like macro can be defined with a replacement list staring with a (
#define OBJ (stuff in parentheses)
OBJ
OBJ()
OBJ(a)

// Builtin macros
__STDC__
__FILE__
__LINE__
__x86_64__
__LP64__

// From c-testsuite 00201.c
#define CAT(a,b) a##b
#define AB(x) CAT(x,y)
CAT(A,B)(x)
