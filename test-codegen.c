#include <stdio.h>

#include "wc4.h"

int failures;
char *input_filename, *output_filename;
int label;

Function *new_function() {
    Function *function;

    function = malloc(sizeof(Function));
    memset(function, 0, sizeof(Function));

    return function;
}

void i(int operation, Value *dst, Value *src1, Value *src2) {
    add_instruction(operation, dst, src1, src2);
}

void li(int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    tac = add_instruction(operation, dst, src1, src2);
    tac->label = label;
}

Value *p(int preg) {
    Value *v;

    v = new_value();
    v->type = TYPE_INT;
    v->preg = preg;

    return v;
}

Value *cpu() {
    Value *v;

    v = new_value();
    v->type = TYPE_INT;
    v->is_in_cpu_flags = 1;

    return v;
}

Value *l(int label) {
    Value *v;

    v = new_value();
    v->label = label;

    return v;
}

Value *c(int value) {
    return new_constant(TYPE_INT, value);
}

void add_test(int test_number, int op, Value *dst, Value *src1, Value *src2, int ip0, int ip1, int ip2, int op0, int op1, int op2) {
    int first_label;

    if (ir_start == 0)
        first_label = 0;
    else
        first_label = label;

    li(first_label, IR_LOAD_CONSTANT, p(0), c(ip0), 0);
    i(              IR_LOAD_CONSTANT, p(1), c(ip1), 0);
    i(              IR_LOAD_CONSTANT, p(2), c(ip2), 0);

    i(op, dst, src1, src2);

    // Note: EQ and NE are reversed due to bonkers constant handling in comparisons
    i(IR_NE, cpu(), c(op0), p(0));
    i(IR_JZ, 0, cpu(), l(label + 1));
    i(IR_LOAD_CONSTANT, p(0), c(test_number), 0);
    i(IR_RETURN, 0, 0, 0);

    li(label + 1, IR_NE, cpu(), c(op1), p(1));
    i(IR_JZ, 0, cpu(), l(label + 2));
    i(IR_LOAD_CONSTANT, p(0), c(test_number), 0);
    i(IR_RETURN, 0, 0, 0);

    li(label + 2, IR_NE, cpu(), c(op2), p(2));
    i(IR_JZ, 0, cpu(), l(label + 3));
    i(IR_LOAD_CONSTANT, p(0), c(test_number), 0);
    i(IR_RETURN, 0, 0, 0);

    label += 3;
}

void run_added_tests() {
    Symbol *s;
    int size, result;
    char *assembly, *command;

    li(label, IR_LOAD_CONSTANT, p(0), c(0), 0);
    i(IR_RETURN, 0, 0, 0);

    s = malloc(sizeof(Symbol));
    memset(s, 0, sizeof(Symbol));
    s->function = new_function();
    s->identifier = "main";
    s->function->ir = ir_start;

    f = stdout;
    f  = fopen(input_filename, "w");
    if (f == 0) {
        perror(input_filename);
        exit(1);
    }

    fprintf(f, "\t.globl\tmain\n");
    fprintf(f, "\t.text\n");
    fprintf(f, "main:\n");

    cur_stack_push_count = 0;
    output_function_body_code(s);
    fclose(f);

    asprintf(&command, "gcc %s -o %s", input_filename, output_filename);
    result = system(command);
    if (result != 0) {
        perror("while calling gcc");
        exit(1);
    }

    // printf("output_filename=%s\n", output_filename);
    result = system(output_filename) >> 8;
    if (result != 0) {
        f  = fopen(input_filename, "r");
        if (f == 0) {
            perror(input_filename);
            exit(1);
        }
        assembly = malloc(102400);
        size = fread(assembly, 1, 102400, f);
        if (size < 0) {
            printf("Unable to read input file\n");
            exit(1);
        }
        fclose(f);

        printf("%s", assembly);

        printf("\nFailed test %d\n", result);

        exit(1);
    }
}

void test_2_operand_commutative_operations() {
    ir_start = 0;
    label = 0;

    add_test( 1, IR_ADD, p(0), c(1), p(0), 1, 2, 9, 2, 2, 9);
    add_test( 2, IR_ADD, p(0), c(1), p(1), 1, 2, 9, 3, 2, 9);
    add_test( 3, IR_ADD, p(0), p(0), p(1), 1, 2, 9, 3, 2, 9);
    add_test( 4, IR_ADD, p(0), p(1), p(0), 1, 2, 9, 3, 2, 9);
    add_test( 5, IR_ADD, p(2), p(0), p(1), 1, 2, 9, 1, 2, 3);
    add_test( 6, IR_ADD, p(0), p(0), p(0), 1, 2, 9, 2, 2, 9);
    add_test( 7, IR_ADD, p(0), p(1), p(1), 1, 2, 9, 4, 2, 9);
    add_test( 8, IR_ADD, p(1), p(0), p(0), 1, 3, 9, 1, 2, 9);

    add_test(11, IR_MUL, p(0), c(4), p(0), 2, 3, 7, 8, 3, 7);
    add_test(12, IR_MUL, p(0), c(4), p(1), 3, 2, 7, 8, 2, 7);
    add_test(13, IR_MUL, p(0), p(0), p(1), 2, 3, 4, 6, 3, 4);
    add_test(14, IR_MUL, p(0), p(1), p(0), 2, 3, 4, 6, 3, 4);
    add_test(15, IR_MUL, p(2), p(0), p(1), 2, 3, 7, 2, 3, 6);
    add_test(16, IR_MUL, p(0), p(0), p(0), 2, 1, 3, 4, 1, 3);
    add_test(17, IR_MUL, p(0), p(1), p(1), 2, 3, 1, 9, 3, 1);
    add_test(18, IR_MUL, p(1), p(0), p(0), 2, 3, 5, 2, 4, 5);

    add_test(21, IR_BOR, p(0), c(8), p(0), 1, 2, 4, 9,  2, 4);
    add_test(22, IR_BOR, p(0), c(8), p(1), 1, 2, 4, 10, 2, 4);
    add_test(23, IR_BOR, p(0), p(0), p(1), 1, 2, 4, 3,  2, 4);
    add_test(24, IR_BOR, p(0), p(1), p(0), 1, 2, 4, 3,  2, 4);
    add_test(25, IR_BOR, p(2), p(0), p(1), 1, 2, 4, 1,  2, 3);
    add_test(26, IR_BOR, p(0), p(0), p(0), 1, 2, 4, 1,  2, 4);
    add_test(27, IR_BOR, p(0), p(1), p(1), 1, 2, 4, 2,  2, 4);
    add_test(28, IR_BOR, p(1), p(0), p(0), 1, 3, 4, 1,  1, 4);

    // These are identical to the BOR tests, except ever operand is ~0xf
    add_test(31, IR_BAND, p(0), c(7), p(0), 15-1, 15-2, 15-4, 15-9,  15-2, 15-4);
    add_test(32, IR_BAND, p(0), c(7), p(1), 15-1, 15-2, 15-4, 15-10, 15-2, 15-4);
    add_test(33, IR_BAND, p(0), p(0), p(1), 15-1, 15-2, 15-4, 15-3,  15-2, 15-4);
    add_test(34, IR_BAND, p(0), p(1), p(0), 15-1, 15-2, 15-4, 15-3,  15-2, 15-4);
    add_test(35, IR_BAND, p(2), p(0), p(1), 15-1, 15-2, 15-4, 15-1,  15-2, 15-3);
    add_test(36, IR_BAND, p(0), p(0), p(0), 15-1, 15-2, 15-4, 15-1,  15-2, 15-4);
    add_test(37, IR_BAND, p(0), p(1), p(1), 15-1, 15-2, 15-4, 15-2,  15-2, 15-4);
    add_test(38, IR_BAND, p(1), p(0), p(0), 15-1, 15-2, 15-4, 15-1,  15-1, 15-4);

    add_test(41, IR_XOR, p(0), c(1), p(0), 1, 2, 4, 0, 2, 4);
    add_test(42, IR_XOR, p(0), c(1), p(1), 1, 2, 4, 3, 2, 4);
    add_test(43, IR_XOR, p(0), p(0), p(1), 3, 2, 8, 1, 2, 8);
    add_test(44, IR_XOR, p(0), p(1), p(0), 3, 2, 8, 1, 2, 8);
    add_test(45, IR_XOR, p(2), p(0), p(1), 3, 2, 4, 3, 2, 1);
    add_test(46, IR_XOR, p(0), p(0), p(0), 1, 2, 4, 0, 2, 4);
    add_test(47, IR_XOR, p(0), p(1), p(1), 1, 2, 4, 0, 2, 4);
    add_test(48, IR_XOR, p(1), p(0), p(0), 1, 3, 4, 1, 0, 4);
}

void test_rsub() {
    add_test(51, IR_RSUB, p(0), c(1), p(0), 6, 2, 4, 5, 2, 4);
    add_test(52, IR_RSUB, p(0), c(1), p(1), 1, 7, 5, 6, 7, 5);
    add_test(53, IR_RSUB, p(1), p(0), p(1), 2, 3, 9, 2, 1, 9);
    add_test(54, IR_RSUB, p(2), p(0), p(1), 2, 3, 4, 2, 3, 1);
    add_test(55, IR_RSUB, p(1), p(0), p(0), 1, 2, 3, 1, 0, 3);
    add_test(56, IR_RSUB, p(2), p(0), p(0), 1, 2, 3, 1, 2, 0);
}

void test_bnot() {
    add_test(63, IR_BNOT, p(0), p(0), 0,  1, 2, 0, -2, 2, 0);
    add_test(63, IR_BNOT, p(0), p(1), 0,  1, 2, 0, -3, 2, 0);
}

void run_tests() {
    test_2_operand_commutative_operations();    // +, *, |, &, ^
    test_rsub();
    test_bnot();

    run_added_tests();
}

int main() {
    int fd;

    failures = 0;
    init_callee_saved_registers();

    input_filename = strdup("/tmp/XXXXXX.s");
    fd = mkstemps(input_filename, 2);
    if (fd == -1) {
        perror("in make_temp_filename");
        exit(1);
    }
    close(fd);

    output_filename = strdup("/tmp/XXXXXX");
    fd = mkstemps(output_filename, 0);
    if (fd == -1) {
        perror("in make_temp_filename");
        exit(1);
    }
    close(fd);

    run_tests();

    if (failures)
        printf("Codegen tests had %d failures\n", failures);
    else
        printf("All codegen tests passed\n");

    exit(failures > 0);
}
