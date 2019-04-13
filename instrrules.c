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

    instr_rule_count = 0;

    instr_rules = malloc(MAX_RULE_COUNT * sizeof(Rule));
    memset(instr_rules, 0, MAX_RULE_COUNT * sizeof(Rule));

    r = add_rule(REG, 0,                REG, 0, 0);
    r = add_rule(CST, 0,                CST, 0, 0);
    r = add_rule(0,   IR_RETURN,        CST, 0, 1);  add_op(r, X_RET, 0, CST1, 0,    "mov\t$%v1, %%rax");
    r = add_rule(0,   IR_RETURN,        REG, 0, 1);  add_op(r, X_RET, 0, SRC1, 0,    "mov\t%v1, %%rax" );
    r = add_rule(REG, IR_ASSIGN,        REG, 0, 1);  add_op(r, X_MOV, 0, SRC1, DST,  "mov\t%v1, %v2"   );
    r = add_rule(REG, IR_LOAD_CONSTANT, CST, 0, 1);  add_op(r, X_MOV, 0, SRC1, DST,  "mov\t$%v1, %v2"  );
    r = add_rule(REG, 0,                CST, 0, 1);  add_op(r, X_MOV, 0, SRC1, DST,  "mov\t$%v1, %v2"  );

    add_commutative_operation_rules("add",  IR_ADD, X_ADD, 10);
    add_commutative_operation_rules("imul", IR_MUL, X_MUL, 30);
}
