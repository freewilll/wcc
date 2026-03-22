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
__linux__
__GNUC__
__USER_LABEL_PREFIX__

// stdint macros
__INT8_TYPE__
__INT16_TYPE__
__INT32_TYPE__
__INT64_TYPE__
__UINT8_TYPE__
__UINT16_TYPE__
__UINT32_TYPE__
__UINT64_TYPE__
__INT_LEAST8_TYPE__
__INT_LEAST16_TYPE__
__INT_LEAST32_TYPE__
__INT_LEAST64_TYPE__
__UINT_LEAST8_TYPE__
__UINT_LEAST16_TYPE__
__UINT_LEAST32_TYPE__
__UINT_LEAST64_TYPE__
__INT_FAST8_TYPE__
__INT_FAST16_TYPE__
__INT_FAST32_TYPE__
__INT_FAST64_TYPE__
__UINT_FAST8_TYPE__
__UINT_FAST16_TYPE__
__UINT_FAST32_TYPE__
__UINT_FAST64_TYPE__
__INTPTR_TYPE__
__UINTPTR_TYPE__
__INTMAX_TYPE__
__UINTMAX_TYPE__
__INT8_MAX__
__UINT8_MAX__
__INT16_MAX__
__UINT16_MAX__
__INT32_MAX__
__UINT32_MAX__
__INT64_MAX__
__UINT64_MAX__

// Math macro replacements
__builtin_ceil
__builtin_floor
