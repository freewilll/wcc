#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "../test-lib.h"

int check_stack_alignment();

int verbose;
int passes;
int failures;

struct csrs {
    int i;
};

struct ups {
    int i, j;
};

// Create an expression which undoubtedly will exhaust all registers, forcing
// the spilling code into action
void test_spilling_stress() {
    int i;
    i = 1;
    i = (i + 2) * (i + (i + 2) * (i + (i + 2) * (i + i + (i + 2) * (i + 3))));
    assert_int(390, i, "spilling stress");
}

int csr() {
    return 2;
}

// This test is a bit fragile. It does the job right now, but register
// candidates will likely change. Currently, foo() uses rbx, which is also
// used to store the lvalue for s->i. If rbx wasn't preserved, this code
// would segfault.
void test_callee_saved_registers() {
    struct csrs *s;

    s = malloc(sizeof(struct csrs));
    s->i = csr();
    assert_int(2, s->i, "callee saved registers");
}

int set_rax(int i) {
    return i;
}

// Variadic functions have the number of floating point arguments passed in al.
// Since floating point numbers isn't implemented, this should be set to zero.
// A bug was present where this wasn't being set in sprintf. This code below
// would segfault at the second sprintf.
void test_variadic_arg_bug() {
    char *s;

    s = malloc(10);

    set_rax(0);
    sprintf(s, "1\n");

    set_rax(1);
    sprintf(s, "2\n");
}

void test_backwards_jumps() {
    int i, j, k;
    int *r;

    r = malloc(sizeof(int) * 3);
    i = 0;
    j = 0;
    for (i = 0; i < 3; i++) {
        j += 10;
        r[i] = j;
        k = j + 1; // Trigger register reuse in the buggy case
    }

    assert_int(10, r[0], "backwards jumps liveness 1");
    assert_int(20, r[1], "backwards jumps liveness 2");
    assert_int(30, r[2], "backwards jumps liveness 3");
}

void test_first_declaration_in_if_in_for_liveness() {
    int i, j, k, l;

    i = 0;
    for (i = 0; i < 3; i++) {
        if (i == 0) j = 1; else k = j * 2;
        l = l + (l + (l + (l + (l + (l + (l + (l + (l + (l + (l + (l + (l + 1))))))))))));
    }

    assert_int(2, k, "liveness extension for conditional declaration inside loop");
}

void test_spilling_locals_to_stack_bug() {
    int i, j, k;
    int r1, r2, r3, r4, r5, r6, r7, r8, r9, r10;
    int q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;

    r1 = r2 = r3 = r4 = r5 = r6 = r7 = r8 = r9 = r10 = 1;
    assert_int(1, r1,  "Local stack spilling clobber bug 1");
    assert_int(1, r2,  "Local stack spilling clobber bug 2");
    assert_int(1, r3,  "Local stack spilling clobber bug 3");
    assert_int(1, r4,  "Local stack spilling clobber bug 4");
    assert_int(1, r5,  "Local stack spilling clobber bug 5");
    assert_int(1, r6,  "Local stack spilling clobber bug 6");
    assert_int(1, r7,  "Local stack spilling clobber bug 7");
    assert_int(1, r8,  "Local stack spilling clobber bug 8");
    assert_int(1, r9,  "Local stack spilling clobber bug 9");
    assert_int(1, r10, "Local stack spilling clobber bug 10");

    r10++; // This forces a spill of r10
}

int sa0()                                                               { assert_int(0, check_stack_alignment(), "SA 0"); }
int sa1(int i1)                                                         { assert_int(0, check_stack_alignment(), "SA 1"); }
int sa2(int i1, int i2)                                                 { assert_int(0, check_stack_alignment(), "SA 2"); }
int sa3(int i1, int i2, int i3)                                         { assert_int(0, check_stack_alignment(), "SA 3"); }
int sa4(int i1, int i2, int i3, int i4)                                 { assert_int(0, check_stack_alignment(), "SA 4"); }
int sa5(int i1, int i2, int i3, int i4, int i5)                         { assert_int(0, check_stack_alignment(), "SA 5"); }
int sa6(int i1, int i2, int i3, int i4, int i5, int i6)                 { assert_int(0, check_stack_alignment(), "SA 6"); }
int sa7(int i1, int i2, int i3, int i4, int i5, int i6, int i7)         { assert_int(0, check_stack_alignment(), "SA 7"); }
int sa8(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8) { assert_int(0, check_stack_alignment(), "SA 8"); }

int test_function_call_stack_alignment() {
    sa0();
    sa1(1);
    sa2(1, 1);
    sa3(1, 1, 1);
    sa4(1, 1, 1, 1);
    sa4(1, 1, 1, 1);
    sa5(1, 1, 1, 1, 1);
    sa6(1, 1, 1, 1, 1, 1);
    sa7(1, 1, 1, 1, 1, 1, 1);
    sa8(1, 1, 1, 1, 1, 1, 1, 1);
}

void test_local_var_stack_alignment0() {           assert_int(0, check_stack_alignment(), "Stack alignment 0"); }
void test_local_var_stack_alignment1() { int i;    assert_int(0, check_stack_alignment(), "Stack alignment 1"); }
void test_local_var_stack_alignment2() { int i, j; assert_int(0, check_stack_alignment(), "Stack alignment 2"); }

void test_local_var_stack_alignment() {
    test_local_var_stack_alignment0();
    test_local_var_stack_alignment1();
    test_local_var_stack_alignment2();
}

void rssa1(int nt) {
    char *buf;

    buf = malloc(6);

    if (nt == 100) return;
    asprintf(&buf, "nt%03d", nt);
    assert_string("nt001", buf, "Return statement stack alignment 1");
}

void rssa2(int nt) {
    char *buf;

    buf = malloc(6);

    if (nt == 100) return;
    if (nt == 101) return;

    asprintf(&buf, "nt%03d", nt);
    assert_string("nt001", buf, "Return statement stack alignment 1");
}

void test_return_statement_stack_alignment() {
    // Test but where return statements would lead to an invalid push count
    rssa1(1);
    rssa2(1);
}

void test_ssa_label_merge_bug() {
    int i;

         if (0) i = 1;
    else if (1) i = 2;
    else if (0) i = 3;

    assert_int(2, i, "Label merge bug in three level if/else");
}

void tsmab(int *i) {}

void test_ssa_memory_alocation_bug() {
    int a, b;
    int i;

    i = 0;
    tsmab(&i);
}

int test_ssa_continue_with_statements_afterwards() {
    // A silly example, but a bug was leading to incorrect liveness, leading to
    // in correct register allocations.
    int i;

    i = 0;
    while (i++ < 3) {
        continue;
        i = 1;
    }
}

int test_ssa_break_with_statements_afterwards() {
    // A silly example, but a bug was leading to incorrect liveness, leading to
    // in correct register allocations.
    int i;

    i = 0;
    while (i++ < 3) {
        continue;
        i = 1;
    }
}

void test_ssa_arithmetic_optimizations() {
    int i;

    i = 10;

    assert_int( 0, i * 0, "Arithetic optimization i * 0");
    assert_int(10, i * 1, "Arithetic optimization i * 1");
    assert_int(20, i * 2, "Arithetic optimization i * 2");
    assert_int(30, i * 3, "Arithetic optimization i * 3");
    assert_int(40, i * 4, "Arithetic optimization i * 4");
    assert_int(50, i * 5, "Arithetic optimization i * 5");
    assert_int(60, i * 6, "Arithetic optimization i * 6");
    assert_int(70, i * 7, "Arithetic optimization i * 7");
    assert_int(80, i * 8, "Arithetic optimization i * 8");
}

void test_bad_or_and_stack_consumption() {
    long ip;
    ip = 0;
    while ((1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1) && ip < 2) {
        ip += 1;
    }
}

void test_double_deref_assign_with_cast() {
    // This test is a bit bonkers.
    // stack contains longs, but they are really pointers to longs.
    long i, a, *sp, *stack;

    stack = malloc(32);
    sp = stack;
    i = 10;
    a = 20;

    // Assign a pointer to a long (i) to sp
    *sp = &i;

    // With explicit parentheses:
    // (*((long *) *sp))++ = a;
    //
    // 1. Dereference sp (which is a long, but is equal to &i)
    // 2. Cast it to a pointer to long (long *)
    // 3. Assign to the long*, which is &i, thus modifying the value of i
    // 4. Increment sp
    *(long *) *sp++ = a;

    assert_int(20, i, "double deref assign with a cast");
}

char *ffrptcicj() { return (char *) 256; }

void test_function_returning_pointer_to_char_in_conditional_jump() {
    char *c;
    int r;

    c = ffrptcicj();
    r = c ? 1 : 0;
    assert_int(1, r, "Comparison with *char from function call in conditional jump");
}

void test_unary_precedence() {
    struct ups *s;
    s = malloc(sizeof(struct ups));
    s->i = 0;
    assert_int( 1, !s[0].i,                       "unary precedence !");
    assert_int(-1, ~s[0].i,                       "unary precedence ~");
    assert_int( 0, -s[0].i,                       "unary precedence -");
    assert_int( 4, (long) &s[0].j - (long) &s[0], "unary precedence");
}

void trr(int i) {
    i--;

    assert_int(1, i, "Register reuse in inc + function calls");
    assert_int(1, i, "Register reuse in inc + function calls");
}

void test_register_reuse_in_function_calls() {
    trr(2);
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_spilling_stress();
    test_callee_saved_registers();
    test_variadic_arg_bug();
    test_backwards_jumps();
    test_first_declaration_in_if_in_for_liveness();
    test_spilling_locals_to_stack_bug();
    test_local_var_stack_alignment();
    test_function_call_stack_alignment();
    test_return_statement_stack_alignment();
    test_ssa_label_merge_bug();
    test_ssa_memory_alocation_bug();
    test_ssa_continue_with_statements_afterwards();
    test_ssa_break_with_statements_afterwards();
    test_ssa_arithmetic_optimizations();
    test_bad_or_and_stack_consumption();
    test_double_deref_assign_with_cast();
    test_function_returning_pointer_to_char_in_conditional_jump();
    test_unary_precedence();
    test_register_reuse_in_function_calls();

    finalize();
}
