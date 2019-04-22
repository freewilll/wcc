#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

Rule *add_rule(int non_terminal, int operation, int src1, int src2, int cost) {
    Rule *r;

    if (instr_rule_count == MAX_RULE_COUNT) panic1d("Exceeded maximum number of rules %d", MAX_RULE_COUNT);

    r = &(instr_rules[instr_rule_count]);

    r->index          = instr_rule_count;
    r->operation      = operation;
    r->non_terminal   = non_terminal;
    r->src1           = src1;
    r->src2           = src2;
    r->cost           = cost;
    r->x86_operations = 0;

    instr_rule_count++;

    return r;
}

int transform_rule_value(int v, int i) {
    if (v == REG || v == GLB || v == STK)
        return v + i;
    else
        return v;
}

X86Operation *dup_x86_operation(X86Operation *operation) {
    X86Operation *result;

    result = malloc(sizeof(X86Operation));
    result->operation = operation->operation;
    result->dst = operation->dst;
    result->v1 = operation->v1;
    result->v2 = operation->v2;
    result->template = strdup(operation->template);
    result->next = 0;
}

X86Operation *dup_x86_operations(X86Operation *operation) {
    X86Operation *result;
    X86Operation *o, *new_operation;

    result = 0;

    // Create new linked list with duplicates of the x86 operations
    while (operation) {
        new_operation = dup_x86_operation(operation);
        if (!result) {
            result = new_operation;
            o = result;
        }
        else {
            o->next = new_operation;
            o = new_operation;
        }
        operation = operation->next;
    }

    return result;
}

char size_to_x86_size(int size) {
         if (size == 1) return 'b';
    else if (size == 2) return 'w';
    else if (size == 3) return 'l';
    else if (size == 4) return 'q';
            else panic1d("Unknown size %d", size);
}

char *add_size_to_template(char *template, int size) {
    char *src, *dst, *c;
    char *result, x86_size;

    x86_size = size_to_x86_size(size);
    src = template;
    result = malloc(128);
    dst = result;

    // Add size to registers, converting e.g. %v1 -> %v1b if size=1
    // Registers that already have the size are untouched, so that
    // specific things like sign extensions work.
    c = template;
    while (*c) {
        if (c[0] == '%' && c[1] == 's') {
            *dst++ = x86_size;
            c++;
        }
        else if (c[0] == '%' && c[1] == 'v' && (c[3] != 'b' && c[3] != 'w' && c[3] != 'l' && c[3] != 'q')) {
            *dst++ = '%';
            *dst++ = 'v';
            *dst++ = c[2];
            *dst++ = x86_size;
            c += 2;
        }
        else
            *dst++ = *c;

        c++;
    }

    return result;
}

void fin_rule(Rule *r) {
    int operation, non_terminal, src1, src2, cost, i;
    X86Operation *x86_operations, *x86_operation;
    Rule *new_rule;
    char *c;

    operation      = r->operation;
    non_terminal   = r->non_terminal;
    src1           = r->src1;
    src2           = r->src2;
    cost           = r->cost;
    x86_operations = r->x86_operations;

    if (!(
            non_terminal == REG || src1 == REG || src2 == REG ||
            non_terminal == GLB || src1 == GLB || src2 == GLB ||
            non_terminal == STK || src1 == STK || src2 == STK)) {

        r->x86_operations = dup_x86_operations(r->x86_operations);
        return;
    }

    instr_rule_count--;

    for (i = 1; i <= 4; i++) {
        new_rule = add_rule(transform_rule_value(non_terminal, i), operation, transform_rule_value(src1, i), transform_rule_value(src2, i), cost);
        new_rule->x86_operations = dup_x86_operations(x86_operations);

        x86_operation = new_rule->x86_operations;
        while (x86_operation) {
            x86_operation->template = add_size_to_template(x86_operation->template, i);
            x86_operation = x86_operation->next;
        }
    }
}

void add_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    X86Operation *x86op, *o;

    x86op = malloc(sizeof(X86Operation));
    x86op->operation = operation;

    x86op->dst = dst;
    x86op->v1 = v1;
    x86op->v2 = v2;

    x86op->template = template;
    x86op->next = 0;

    if (!r->x86_operations)
        r->x86_operations = x86op;
    else {
        o = r->x86_operations;
        while (o->next) o = o->next;
        o->next = x86op;
    }
}

void print_rule(Rule *r) {
    int i, first;
    X86Operation *operation;

    printf("%d %-5d %-5d %-5d ", r->operation, r->non_terminal, r->src1, r->src2);
    operation = r->x86_operations;
    first = 1;
    while (operation) {
        if (!first) printf("                           ");
        first = 0;
        printf("%s\n", operation->template);
        operation = operation->next;
    }

    if (!r->x86_operations) printf("\n");
}

void print_rules() {
    int i;

    for (i = 0; i < instr_rule_count; i++) {
        printf("%-5d ", i);
        print_rule(&(instr_rules[i]));
    }
}

void make_value_x86_size(Value *v) {
    if (v->x86_size) return;

    if (v->is_string_literal)
        v->x86_size = 4;
    else if (v->vreg || v->global_symbol || v->stack_index) {
        if (v->type >= TYPE_PTR)
            v->x86_size = 4;
        else
            v->x86_size = v->type - TYPE_CHAR + 1;
    }
}

void add_x86_instruction(X86Operation *x86op, Value *dst, Value *v1, Value *v2) {
    Tac *tac;

    if (v1) make_value_x86_size(v1);
    if (v2) make_value_x86_size(v2);

    if (DEBUG_SIGN_EXTENSION) printf("  adding instruction for operation %d: %s\n", x86op->operation, x86op->template);
    tac = add_instruction(x86op->operation, dst, v1, v2);
    tac->x86_template = x86op->template;
}

void add_comparison_conditional_jmp_rules(int *ntc, int src1, int src2, char *template) {
    Rule *r;

    (*ntc)++;
    r = add_rule(*ntc, IR_EQ,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template   ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je .l%v1" ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne .l%v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_NE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template   ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne .l%v1"); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je .l%v1" ); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_LT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template   ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl .l%v1" ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge .l%v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_GT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template   ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg .l%v1" ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle .l%v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_LE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template   ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle .l%v1"); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg .l%v1" ); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_GE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template   ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge .l%v1"); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl .l%v1" ); fin_rule(r);
}

void add_comparison_assignment_rule(int src1, int src2, char *cmp_template, int operation, int set_operation, char *set_template) {
    int i;
    Rule *r;

    for (i = 1; i <= 4; i++) {
        r = add_rule(REG + i, operation, src1, src2, 12 + (i > 1));

        add_op(r, X_CMP,         0,   SRC1, SRC2, cmp_template);
        add_op(r, set_operation, DST, 0,    0,    set_template);

             if (i == 2) add_op(r, X_MOVZBW, DST, DST, 0, "movzbw %v1b, %v1w");
        else if (i == 3) add_op(r, X_MOVZBL, DST, DST, 0, "movzbl %v1b, %v1l");
        else if (i == 4) add_op(r, X_MOVZBQ, DST, DST, 0, "movzbq %v1b, %v1q");
        fin_rule(r);
    }
}

void add_comparison_assignment_rules(int src1, int src2, char *template) {
    Rule *r;

    add_comparison_assignment_rule(src1, src2, template, IR_EQ, X_SETE,  "sete %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_NE, X_SETNE, "setne %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_LT, X_SETLT, "setl %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_GT, X_SETGT, "setg %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_LE, X_SETLE, "setle %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_GE, X_SETGE, "setge %vdb");
}

void add_commutative_operation_rules(char *x86_operand, int operation, int x86_operation, int cost) {
    char *op_rr, *op_cr, *op_rs, *op_rg;
    Rule *r;

    asprintf(&op_rr, "%s %%v1, %%v2",          x86_operand);  // Perform operation on two registers
    asprintf(&op_cr, "%s $%%v1, %%v2",         x86_operand);  // Perform operation on a constant and a register
    asprintf(&op_rs, "%s %%v1(%%%%rbp), %%v2", x86_operand);  // Perform operation on a register and a stack local
    asprintf(&op_rg, "%s %%v1(%%%%rip), %%v2", x86_operand);  // Perform operation on a register and a global

    r = add_rule(REG, operation, REG, REG, cost);     add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_rr           );
                                                      fin_rule(r);
    r = add_rule(REG, operation, CST, REG, cost);     add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_cr           );
                                                      fin_rule(r);
    r = add_rule(REG, operation, REG, CST, cost);     add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s %v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_cr           );
                                                      fin_rule(r);
    r = add_rule(REG, operation, REG, GLB, cost + 1); add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s %v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_rg           );
                                                      fin_rule(r);
    r = add_rule(REG, operation, GLB, REG, cost + 1); add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_rg           );
                                                      fin_rule(r);
    r = add_rule(REG, operation, REG, STK, cost + 1); add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s %v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_rs           );
                                                      fin_rule(r);
    r = add_rule(REG, operation, STK, REG, cost + 1); add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_rs           );
                                                      fin_rule(r);
}

void init_instruction_selection_rules() {
    Rule *r;
    int ntc;  // Non terminal counter
    char *cmp_rr, *cmp_rc, *cmp_rg, *cmp_gr, *cmp_gc, *cmp_rs, *cmp_sr, *cmp_sc;

    instr_rule_count = 0;

    instr_rules = malloc(MAX_RULE_COUNT * sizeof(Rule));
    memset(instr_rules, 0, MAX_RULE_COUNT * sizeof(Rule));

    // Identity rules
    r = add_rule(REG, 0, REG, 0,    0); fin_rule(r);
    r = add_rule(CST, 0, CST, 0,    0); fin_rule(r);
    r = add_rule(STL, 0, STL, 0,    0); fin_rule(r);
    r = add_rule(GLB, 0, GLB, 0,    0); fin_rule(r);
    r = add_rule(STK, 0, STK, 0,    0); fin_rule(r);
    r = add_rule(LAB, 0, LAB, 0,    0); fin_rule(r);

    // Register -> register sign extension
    r = add_rule(REGW, 0, REGB, 0, 1); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b, %vdw"); fin_rule(r);
    r = add_rule(REGL, 0, REGB, 0, 1); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b, %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, REGB, 0, 1); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b, %vdq"); fin_rule(r);
    r = add_rule(REGL, 0, REGW, 0, 1); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w, %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, REGW, 0, 1); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w, %vdq"); fin_rule(r);
    r = add_rule(REGQ, 0, REGL, 0, 1); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l, %vdq"); fin_rule(r);

    // Global -> register sign extension
    r = add_rule(REGW, 0, GLBB, 0, 2); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b(%%rip), %vdw"); fin_rule(r);
    r = add_rule(REGL, 0, GLBB, 0, 2); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b(%%rip), %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, GLBB, 0, 2); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b(%%rip), %vdq"); fin_rule(r);
    r = add_rule(REGL, 0, GLBW, 0, 2); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w(%%rip), %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, GLBW, 0, 2); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w(%%rip), %vdq"); fin_rule(r);
    r = add_rule(REGQ, 0, GLBL, 0, 2); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l(%%rip), %vdq"); fin_rule(r);

    // Local -> register sign extension
    r = add_rule(REGW, 0, STKB, 0, 2); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b(%%rbp), %vdw"); fin_rule(r);
    r = add_rule(REGL, 0, STKB, 0, 2); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b(%%rbp), %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, STKB, 0, 2); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b(%%rbp), %vdq"); fin_rule(r);
    r = add_rule(REGL, 0, STKW, 0, 2); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w(%%rbp), %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, STKW, 0, 2); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w(%%rbp), %vdq"); fin_rule(r);
    r = add_rule(REGQ, 0, STKL, 0, 2); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l(%%rbp), %vdq"); fin_rule(r);

    r = add_rule(0,   IR_RETURN,        CST, 0,    1); add_op(r, X_RET,  0,   SRC1, 0,    "mov $%v1q, %%rax"        ); fin_rule(r); // Return constant
    r = add_rule(0,   IR_RETURN,        REG, 0,    1); add_op(r, X_RET,  0,   SRC1, 0,    "mov %v1q, %%rax"         ); fin_rule(r); // Return register

    r = add_rule(REG, IR_ASSIGN,        REG, 0,    1); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1, %vd"          ); fin_rule(r); // Register to register copy
    r = add_rule(GLB, IR_ASSIGN,        CST, 0,    2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s $%v1, %vd(%%rip)"  ); fin_rule(r); // Store constant in global
    r = add_rule(GLB, IR_ASSIGN,        REG, 0,    2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1, %vd(%%rip)"   ); fin_rule(r); // Store register in global
    r = add_rule(STK, IR_ASSIGN,        REG, 0,    2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1, %vd(%%rbp)"   ); fin_rule(r); // Store register in stack local
    r = add_rule(STK, IR_ASSIGN,        CST, 0,    2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s $%v1, %vd(%%rbp)"  ); fin_rule(r); // Store constant in stack local
    r = add_rule(REG, IR_LOAD_CONSTANT, CST, 0,    1); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s $%v1, %vd"         ); fin_rule(r); // Process standalone r1 = constant

    r = add_rule(REG, IR_ASSIGN,        GLB, 0,    2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1(%%rip), %vd"   ); fin_rule(r);  // Load standalone global into register
    r = add_rule(REG, 0,                GLB, 0,    2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1(%%rip), %vd"   ); fin_rule(r);  // Load temp global into register

    r = add_rule(REG, 0,                STK, 0,    2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1(%%rbp), %vd"   ); fin_rule(r);  // Load temp stack local into register
    r = add_rule(REG, 0,                CST, 0,    1); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s $%v1, %vd"         ); fin_rule(r);  // Load constant into register
    r = add_rule(REG, IR_ASSIGN,        CST, 0,    1); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s $%v1, %vd"         ); fin_rule(r);  // Load standalone constant into register
    r = add_rule(REG, 0,                STL, 0,    1); add_op(r, X_LEA,  DST, SRC1, 0,    "leaq .SL%v1(%%rip), %vd" ); fin_rule(r);  // Load string literal into register
    r = add_rule(0,   IR_ARG,           CST, CST,  2); add_op(r, X_ARG,  0,   SRC1, SRC2, "pushq $%v2q"             ); fin_rule(r);  // Use constant as function arg
    r = add_rule(0,   IR_ARG,           CST, REGQ, 2); add_op(r, X_ARG,  0,   SRC1, SRC2, "pushq %v2q"              ); fin_rule(r);  // Use register as function arg
    r = add_rule(0,   IR_JMP,           LAB, 0,    1); add_op(r, X_JMP,  0,   SRC1, 0,    "jmp .l%v1"               ); fin_rule(r);  // JMP

    r = add_rule(0,   IR_JZ,            REG, LAB,  1); add_op(r, X_CMPZ, 0,   SRC1, 0,    "cmp $0, %v1"             ); // JZ with register
                                                       add_op(r, X_JZ,   0,   SRC2, 0,    "jz .l%v1"                );
                                                       fin_rule(r);
    r = add_rule(0,   IR_JZ,            GLB, LAB,  1); add_op(r, X_CMPZ, 0,   SRC1, 0,    "cmp $0, %v1(%%rip)"      ); // JZ with global
                                                       add_op(r, X_JZ,   0,   SRC2, 0,    "jz .l%v1"                );
                                                       fin_rule(r);
    r = add_rule(0,   IR_JNZ,           REG, LAB,  1); add_op(r, X_CMPZ, 0,   SRC1, 0,    "cmp $0, %v1"             ); // JNZ with register
                                                       add_op(r, X_JNZ,  0,   SRC2, 0,    "jnz .l%v1"               );
                                                       fin_rule(r);
    r = add_rule(0,   IR_JNZ,           GLB, LAB,  1); add_op(r, X_CMPZ, 0,   SRC1, 0,    "cmp $0, %v1(%%rip)"      ); // JNZ with global
                                                       add_op(r, X_JNZ,  0,   SRC2, 0,    "jnz .l%v1"               );
                                                       fin_rule(r);

    // All pairwise combinations of (CST, REG, GLB, STL) that have associated x86 instructions
    cmp_rr = "cmp%s %v2, %v1";
    cmp_rc = "cmp%s $%v2, %v1";
    cmp_rg = "cmp%s %v2(%%rip), %v1";
    cmp_gr = "cmp%s %v2, %v1(%%rip)";
    cmp_gc = "cmp%s $%v2, %v1(%%rip)";
    cmp_rs = "cmp%s %v2(%%rbp), %v1";
    cmp_sr = "cmp%s %v2, %v1(%%rbp)";
    cmp_sc = "cmp%s $%v2, %v1(%%rbp)";

    // Comparision + conditional jump
    ntc = 2000;
    add_comparison_conditional_jmp_rules(&ntc, REG, REG, cmp_rr);
    add_comparison_conditional_jmp_rules(&ntc, REG, CST, cmp_rc);
    add_comparison_conditional_jmp_rules(&ntc, REG, GLB, cmp_rg);
    add_comparison_conditional_jmp_rules(&ntc, GLB, REG, cmp_gr);
    add_comparison_conditional_jmp_rules(&ntc, GLB, CST, cmp_gc);
    add_comparison_conditional_jmp_rules(&ntc, REG, STK, cmp_rs);
    add_comparison_conditional_jmp_rules(&ntc, STK, REG, cmp_sr);
    add_comparison_conditional_jmp_rules(&ntc, STK, CST, cmp_sc);

    add_comparison_assignment_rules(REG, REG, cmp_rr);
    add_comparison_assignment_rules(REG, CST, cmp_rc);
    add_comparison_assignment_rules(REG, GLB, cmp_rg);
    add_comparison_assignment_rules(GLB, REG, cmp_gr);
    add_comparison_assignment_rules(GLB, CST, cmp_gc);
    add_comparison_assignment_rules(REG, STK, cmp_rs);
    add_comparison_assignment_rules(STK, REG, cmp_sr);
    add_comparison_assignment_rules(STK, CST, cmp_sc);

    add_commutative_operation_rules("add%s",  IR_ADD, X_ADD, 10);
    add_commutative_operation_rules("imul%s", IR_MUL, X_MUL, 30);
}
