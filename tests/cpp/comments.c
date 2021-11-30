// Single line comments

#define foo y // a comment
foo

#define foo2 z// a comment
foo2

#undef NULL /* a comment */
#undef NULL // a comment


foo // a comment

foo // a comment with as BSNL \
x
y

// Multiline comments

/*
    Goodbye world
*/

foo/* x */bar
foo /* x */bar
foo/* x */ bar
foo /* x */ bar
foo  /* x */  bar
foo 	/* x */ 	bar

foo/* x
*/bar
foo /* x
*/bar
foo/* x
*/ bar
foo /* x
*/ bar

foo/* x
 */bar
foo /* x
 */bar
foo/* x
 */ bar
foo /* x
 */ bar

foo/* x \
 */bar
foo /* x \
 */bar
foo/* x \
 */ bar
foo /* x \
 */ bar
foo /* x *\
/ bar

// Check backslash-newlines are working ok after the above
foo\
bar

// Check backslash-newlines are working ok after the above
foo\
bar

This line is to check line numbers are still correct after the last BSNL

#define foobar1 foo/* foo */bar
foobar1
#define foobar2 foo /* foo */bar
foobar2
#define foobar3 foo/* foo */ bar
foobar3
#define foobar4 foo /* foo */ bar
foobar4

#define foobar4 foo/* foo
*/bar
foobar4
#define foobar5 foo /* foo
*/bar
foobar5
#define foobar6 foo/* foo
*/ bar
foobar6
#define foobar7 foo /* foo
*/ bar
foobar7

foo/* foo
    // single line comment
*/bar

/*
    Test handling of # token
    #
*/

#define mlcfoo1 bar1 /* */
mlcfoo1

#define mlcfoo2 bar2 /*
*/
mlcfoo2

a /* */ /* */ b

// The convoluted example from https://gcc.gnu.org/onlinedocs/cpp/Initial-processing.html#Initial-processing
// This "code" is masochistically intentionally unreadable.
/\
*
*/ # /*
*/ defi\
ne FO\
O 10\
20
FOO
