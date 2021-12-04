// Backslash-newlines in directives

#define FOO1 something\
backslashed
FOO1

#define FOO2 something\
 backslashed
FOO2

#define FOO3 something \
backslashed
FOO3

#define FOO4 something \
 backslashed
FOO4

#define FOO5 something \
  backslashed
FOO5

#define FOO6 something \
 backslashed \
 again
FOO6

#define FOO7 something\
backslashed \
again
FOO7

#define FOO8 \
something \
backslashed \
again
FOO8

#\
define\
 FOO9\
 1
FOO9

#\
define \
FOO10 \
1
FOO10

#\
undef FOO8
FOO8

// Backslash-newlines in non-directives
something\
backslashed

something \
backslashed

something\
 backslashed

something \
 backslashed

something \
  backslashed

something  \
  backslashed

something  \
   backslashed

something \

backslashed

something \

 backslashed

something2\
\
backslashed

something2\
 \
backslashed

something2\
\
 backslashed

something2\
 \
 backslashed

a\
b\
c\
d

"foo\\bar"

"something\
backslashed"

"something \
backslashed"

"something \
 backslashed"
