#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

Rule *add_rule(int non_terminal, int operation, int src1, int src2, int cost) {
    Rule *r;

    if (instr_rule_count == MAX_RULE_COUNT) panic("Exceeded maximum number of rules");

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

void add_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    X86Operation *x86op, *o;

    x86op = malloc(sizeof(X86Operation));
    x86op->operation = operation;
    x86op->dst = dst ? dst : v2; // By default, the second operand is the dst
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
    printf("operation=%d nt=%d src1=%d src2=%d\n", r->operation, r->non_terminal, r->src1, r->src2);
}

void add_comparison_conditional_jmp_rules(int *cjn, int src1, int src2, char *template) {
    Rule *r;

    (*cjn)++;
    r = add_rule(*cjn, IR_EQ,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template    );
    r = add_rule(0,    IR_JZ,  *cjn, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je\t.l%v1" );
    r = add_rule(0,    IR_JNZ, *cjn, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne\t.l%v1");

    (*cjn)++;
    r = add_rule(*cjn, IR_NE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template    );
    r = add_rule(0,    IR_JZ,  *cjn, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne\t.l%v1");
    r = add_rule(0,    IR_JNZ, *cjn, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je\t.l%v1" );

    (*cjn)++;
    r = add_rule(*cjn, IR_LT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template    );
    r = add_rule(0,    IR_JZ,  *cjn, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl\t.l%v1" );
    r = add_rule(0,    IR_JNZ, *cjn, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge\t.l%v1");

    (*cjn)++;
    r = add_rule(*cjn, IR_GT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template    );
    r = add_rule(0,    IR_JZ,  *cjn, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg\t.l%v1" );
    r = add_rule(0,    IR_JNZ, *cjn, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle\t.l%v1");

    (*cjn)++;
    r = add_rule(*cjn, IR_LE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template    );
    r = add_rule(0,    IR_JZ,  *cjn, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle\t.l%v1");
    r = add_rule(0,    IR_JNZ, *cjn, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg\t.l%v1" );

    (*cjn)++;
    r = add_rule(*cjn, IR_GE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template    );
    r = add_rule(0,    IR_JZ,  *cjn, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge\t.l%v1");
    r = add_rule(0,    IR_JNZ, *cjn, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl\t.l%v1" );
}

void add_commutative_operation_rules(char *x86_operand, int operation, int x86_operation, int cost) {
    char *op_rr, *op_cr;
    Rule *r;

    asprintf(&op_rr, "%s\t%%v1, %%v2", x86_operand);  // Perform operation on two registers
    asprintf(&op_cr, "%s\t$%%v1, %%v2", x86_operand); // Perform operation on a constant and a register

    r = add_rule(REG, operation, REG, REG, cost);  add_op(r, X_MOV,         0, SRC2, DST,  "mov\t%v1, %v2");
                                                   add_op(r, x86_operation, 0, SRC1, DST,  op_rr          );
    r = add_rule(REG, operation, CST, REG, cost);  add_op(r, X_MOV,         0, SRC2, DST,  "mov\t%v1, %v2");
                                                   add_op(r, x86_operation, 0, SRC1, DST,  op_cr          );
    r = add_rule(REG, operation, REG, CST, cost);  add_op(r, X_MOV,         0, SRC1, DST,  "mov\t%v1, %v2");
                                                   add_op(r, x86_operation, 0, SRC2, DST,  op_cr          );
}

void init_instruction_selection_rules() {
    Rule *r;
    int cjn;  // Comparison + jump non terminal

    instr_rule_count = 0;

    instr_rules = malloc(MAX_RULE_COUNT * sizeof(Rule));
    memset(instr_rules, 0, MAX_RULE_COUNT * sizeof(Rule));

    // Identity rules
    r = add_rule(REG, 0,                REG, 0,   0);
    r = add_rule(CST, 0,                CST, 0,   0);
    r = add_rule(STL, 0,                STL, 0,   0);
    r = add_rule(GLB, 0,                GLB, 0,   0);
    r = add_rule(LAB, 0,                LAB, 0,   0);

    r = add_rule(0,   IR_RETURN,        CST, 0,   1);  add_op(r, X_RET,     0, SRC1, 0,    "mov\t$%v1, %%rax"        ); // Return constant
    r = add_rule(0,   IR_RETURN,        REG, 0,   1);  add_op(r, X_RET,     0, SRC1, 0,    "mov\t%v1, %%rax"         ); // Return register
    r = add_rule(REG, IR_ASSIGN,        REG, 0,   1);  add_op(r, X_MOV,     0, SRC1, DST,  "mov\t%v1, %v2"           ); // Register to register copy
    r = add_rule(GLB, IR_ASSIGN,        CST, 0,   2);  add_op(r, X_MOV,     0, SRC1, DST,  "movq\t$%v1, %v2(%%rip)"  ); // Store constant in global
    r = add_rule(GLB, IR_ASSIGN,        REG, 0,   2);  add_op(r, X_MOV,     0, SRC1, DST,  "mov\t%v1, %v2(%%rip)"    ); // Store register in global
    r = add_rule(REG, IR_LOAD_CONSTANT, CST, 0,   1);  add_op(r, X_MOV,     0, SRC1, DST,  "mov\t$%v1, %v2"          ); // Process standalone r1 = constant

    r = add_rule(REG, IR_ASSIGN,        GLB, 0,   2);  add_op(r, X_MOV,     0, SRC1, DST,  "mov\t%v1(%%rip), %v2"    ); // Load standalone global into register
    r = add_rule(REG, 0,                GLB, 0,   2);  add_op(r, X_MOV,     0, SRC1, DST,  "mov\t%v1(%%rip), %v2"    ); // Load temp global into register

    r = add_rule(REG, 0,                CST, 0,   1);  add_op(r, X_MOV,     0, SRC1, DST,  "mov\t$%v1, %v2"          ); // Load constant into register
    r = add_rule(REG, 0,                STL, 0,   1);  add_op(r, X_LEA,     0, SRC1, DST,  "leaq\t.SL%v1(%%rip), %v2"); // Load string literal into register
    r = add_rule(0,   IR_ARG,           CST, CST, 2);  add_op(r, X_ARG,     0, SRC1, SRC2, "pushq\t$%v2"             ); // Use constant as function arg
    r = add_rule(0,   IR_ARG,           CST, REG, 2);  add_op(r, X_ARG,     0, SRC1, SRC2, "pushq\t%v2"              ); // Use register as function arg

    r = add_rule(0,   IR_JMP,           LAB, 0,   1);  add_op(r, X_JMP,     0, SRC1, 0,    "jmp\t.l%v1"              ); // JMP
    r = add_rule(0,   IR_JZ,            REG, LAB, 1);  add_op(r, X_CMPZ,    0, SRC1, 0,    "cmpq\t$0, %v1"           ); // JZ with register
                                                       add_op(r, X_JZ,      0, SRC2, 0,    "jz\t.l%v1"               );
    r = add_rule(0,   IR_JZ,            GLB, LAB, 1);  add_op(r, X_CMPZ,    0, SRC1, 0,    "cmpq\t$0, %v1(%%rip)"    ); // JZ with global
                                                       add_op(r, X_JZ,      0, SRC2, 0,    "jz\t.l%v1"               );
    r = add_rule(0,   IR_JNZ,           REG, LAB, 1);  add_op(r, X_CMPZ,    0, SRC1, 0,    "cmpq\t$0, %v1"           ); // JNZ with register
                                                       add_op(r, X_JNZ,     0, SRC2, 0,    "jnz\t.l%v1"              );
    r = add_rule(0,   IR_JNZ,           GLB, LAB, 1);  add_op(r, X_CMPZ,    0, SRC1, 0,    "cmpq\t$0, %v1(%%rip)"    ); // JNZ with global
                                                       add_op(r, X_JNZ,     0, SRC2, 0,    "jnz\t.l%v1"              );

    // Comparision + conditional jump
    cjn = 2000;
    add_comparison_conditional_jmp_rules(&cjn, REG, REG, "cmp\t%v2, %v1");
    add_comparison_conditional_jmp_rules(&cjn, REG, CST, "cmp\t$%v2, %v1");
    add_comparison_conditional_jmp_rules(&cjn, CST, REG, "cmp\t$%v2, $%v1");
    add_comparison_conditional_jmp_rules(&cjn, REG, GLB, "cmp\t%v2(%%rip), %v1");
    add_comparison_conditional_jmp_rules(&cjn, GLB, REG, "cmp\t%v2, %v1(%%rip)");
    add_comparison_conditional_jmp_rules(&cjn, GLB, CST, "cmp\t$%v2, %v1(%%rip)");
    add_comparison_conditional_jmp_rules(&cjn, CST, GLB, "cmp\t%v2(%%rip), $%v1");

    add_commutative_operation_rules("add",  IR_ADD, X_ADD, 10);
    add_commutative_operation_rules("imul", IR_MUL, X_MUL, 30);
}
