#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

static void test_comma_in_function(int i, int j, int k) {
    assert_int(2, i, "test_comma_in_function from C89 spec i");
    assert_int(5, j, "test_comma_in_function from C89 spec j");
    assert_int(4, k, "test_comma_in_function from C89 spec k");
}

static void test_comma_operator() {
    int i = 1;
    int j = 2;
    int k = 3;

    assert_int(k, (j, k), "(j, k) as function call argument");

    i++, j--;
    assert_int(2, i, "i++, j-- for i");
    assert_int(1, j, "i++, j-- for j");

    j = 10; k = 1;
    for (i = 1; i < 10; i++, j--) k++;

    assert_int(10, i, "i++, j-- in for loop i");
    assert_int(1,  j, "i++, j-- in for loop j");
    assert_int(10, k, "i++, j-- in for loop k");

    // From C89 spec
    int t; int a = 2; int c = 4; test_comma_in_function(a, (t=3, t+2), c);
}

// From https://en.wikipedia.org/wiki/Comma_operator
void example1() {
    // Commas act as separators in this line, not as an operator.
    // Results: a=1, b=2, c=3, i=0
    int a=1, b=2, c=3, i=0;

    assert_int(1, a, "commas example 1 a");
    assert_int(2, b, "commas example 1 b");
    assert_int(3, c, "commas example 1 c");
    assert_int(0, i, "commas example 1 i");
}

void example2() {
    //  Assigns value of b into i.
    //  Commas act as separators in the first line and as an operator in the second line.
    //  Results: a=1, b=2, c=3, i=2
    int a=1, b=2, c=3;
    int i = (a, b);

    assert_int(1, a, "commas example 2 a");
    assert_int(2, b, "commas example 2 b");
    assert_int(3, c, "commas example 2 c");
    assert_int(2, i, "commas example 2 i");
}

void example3() {
    // Assigns value of a into i.
    // Equivalent to: int i = a; int b;
    // Commas act as separators in both lines.
    // The braces on the second line avoid variable redeclaration in the same block,
    // which would cause a compilation error.
    // The second b declared is given no initial value.
    // Results: a=1, b=2, c=3, i=1
    int a=1, b=2, c=3;
    {
        int i = a, b;

        assert_int(1, a, "example 3 a");
        assert_int(3, c, "example 3 c");
        assert_int(1, i, "example 3 i");
    }

    assert_int(1, a, "commas example 3 a");
    assert_int(2, b, "commas example 3 b");
    assert_int(3, c, "commas example 3 c");
}

void example4() {
    // Increases value of a by 2, then assigns value of resulting operation a + b into i.
    // Commas act as separators in the first line and as an operator in the second line.
    // Results: a=3, b=2, c=3, i=5
    int a=1, b=2, c=3;
    int i = (a += 2, a + b);

    assert_int(3, a, "commas example 4 a");
    assert_int(2, b, "commas example 4 b");
    assert_int(3, c, "commas example 4 c");
    assert_int(5, i, "commas example 4 i");
}

void example5() {
    // Increases value of a by 2, then stores value of a to i, and discards unused
    // values of resulting operation a + b.
    // Equivalent to: (i = (a += 2)), a + b;
    // Commas act as separators in the first line and as an operator in the third line.
    // Results: a=3, b=2, c=3, i=3
    int a=1, b=2, c=3;
    int i;
    i = a += 2, a + b;

    assert_int(3, a, "commas example 5 a");
    assert_int(2, b, "commas example 5 b");
    assert_int(3, c, "commas example 5 c");
    assert_int(3, i, "commas example 5 i");
}

void example6() {
    // Assigns value of a into i.
    // Commas act as separators in both lines.
    // The braces on the second line avoid variable redeclaration in the same block,
    // which would cause a compilation error.
    // The second b and c declared are given no initial value.
    // Results: a=1, b=2, c=3, i=1
    int a=1, b=2, c=3;
    {
        int i = a, b, c;

        assert_int(1, a, "commas example 6 a");
        assert_int(1, i, "commas example 6 i");
    }

    assert_int(1, a, "commas example 6 a");
    assert_int(2, b, "commas example 6 b");
    assert_int(3, c, "commas example 6 c");
}

int example7() {
    // Commas act as separators in the first line and as an operator in the second line.
    // Assigns value of c into i, discarding the unused a and b values.
    // Results: a=1, b=2, c=3, i=3
    int a=1, b=2, c=3;
    int i = (a, b, c);

    // Returns 6, not 4, since comma operator sequence points following the keyword
    // return are considered a single expression evaluating to rvalue of final
    // subexpression c=6.
    // Commas act as operators in this line.
    return a=4, b=5, c=6;
}

int example8() {
    // Returns 3, not 1, for same reason as previous example.
    // Commas act as operators in this line.
    return 1, 2, 3;
}

int example9() {
    // Returns 3, not 1, still for same reason as above. This example works as it does
    // because return is a keyword, not a function call. Even though compilers will
    // allow for the construct return(value), the parentheses are only relative to "value"
    // and have no special effect on the return keyword.
    // Return simply gets an expression and here the expression is "(1), 2, 3".
    // Commas act as operators in this line.
    return(1), 2, 3;
}

int test_comma_in_addition() {
    assert_int(3, 1 + (1, 2), "1 + (1, 2)");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    test_comma_operator();

    // From https://en.wikipedia.org/wiki/Comma_operator
    example1();
    example2();
    example3();
    example4();
    example5();
    example6();
    assert_int(6, example7(), "commas example 7");
    assert_int(3, example8(), "commas example 8");
    assert_int(3, example9(), "commas example 9");
    test_comma_in_addition();

    finalize();
}
