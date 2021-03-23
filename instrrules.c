#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

enum {
    AUTO_NON_TERMINAL_START = 100,
};

int non_terminal_for_value(Value *v);

Rule *add_rule(int dst, int operation, int src1, int src2, int cost) {
    int i;
    Rule *r;

    if (instr_rule_count == MAX_RULE_COUNT) panic1d("Exceeded maximum number of rules %d", MAX_RULE_COUNT);

    r = &(instr_rules[instr_rule_count]);

    r->index          = instr_rule_count;
    r->operation      = operation;
    r->dst            = dst;
    r->src1           = src1;
    r->src2           = src2;
    r->cost           = cost;
    r->x86_operations = 0;

    if (cost == 0 && operation != 0) {
        print_rule(r, 0);
        printf("A zero cost rule cannot have an operation");
    }

    instr_rule_count++;
    return r;
}

int transform_rule_value(int v, int i) {
    if (v == REG || v == MEM || v == ADR || v == MDR)
        return v + i;
    else if (v == CST)
        return CSTL;
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
    result->template = operation->template ? strdup(operation->template) : 0;
    result->save_value_in_slot = operation->save_value_in_slot;
    result->load_value_from_slot = operation->load_value_from_slot;
    result->arg = operation->arg;
    result->next = 0;

    return result;
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

    if (!template) return 0; // Some magic operations have no templates but are implemented in codegen.

    x86_size = size_to_x86_size(size);
    src = template;
    result = malloc(128);
    memset(result, 0, 128);
    dst = result;

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
    // Expand all uses of REG, MEM, ADR into ADRB, ADRW, ADRL, ADRQ
    // e.g. REG, REG, CST is transformed into
    // REGB, REGB, CST
    // REGW, REGW, CST
    // REGL, REGL, CST
    // REGQ, REGQ, CST

    int operation, dst, src1, src2, cost, i;
    X86Operation *x86_operations, *x86_operation;
    Rule *new_rule;
    char *c;

    operation      = r->operation;
    dst            = r->dst;
    src1           = r->src1;
    src2           = r->src2;
    cost           = r->cost;
    x86_operations = r->x86_operations;

    // Only deal with outputs that go into registers, memory or address in register
    if (!(
            dst == REG || src1 == REG || src2 == REG ||
            dst == MEM || src1 == MEM || src2 == MEM ||
            dst == ADR || src1 == ADR || src2 == ADR ||
            dst == MDR || src1 == MDR || src2 == MDR)) {
        return;
    }

    instr_rule_count--; // Rewind next pointer so that the last rule is overwritten

    for (i = 1; i <= 4; i++) {
        new_rule = add_rule(transform_rule_value(dst, i), operation, transform_rule_value(src1, i), transform_rule_value(src2, i), cost);
        new_rule->x86_operations = dup_x86_operations(x86_operations);

        x86_operation = new_rule->x86_operations;
        while (x86_operation) {
            x86_operation->template = add_size_to_template(x86_operation->template, i);
            x86_operation = x86_operation->next;
        }
    }
}

// Add an X86Operation template to a rule's linked list
void add_x86_op_to_rule(Rule *r, X86Operation *x86op) {
    X86Operation *o;

    if (!r->x86_operations)
        r->x86_operations = x86op;
    else {
        o = r->x86_operations;
        while (o->next) o = o->next;
        o->next = x86op;
    }
}

// Add an x86 operation template to a rule
void add_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    X86Operation *x86op, *o;

    x86op = malloc(sizeof(X86Operation));
    x86op->operation = operation;

    x86op->dst = dst;
    x86op->v1 = v1;
    x86op->v2 = v2;

    x86op->template = template;
    x86op->save_value_in_slot = 0;
    x86op->load_value_from_slot = 0;
    x86op->arg = 0;

    x86op->next = 0;

    add_x86_op_to_rule(r, x86op);
}

// Add a save value operation to a rule
void add_save_value(Rule *r, int arg, int slot) {
    X86Operation *x86op, *o;

    x86op = malloc(sizeof(X86Operation));
    memset(x86op, 0, sizeof(X86Operation));
    x86op->save_value_in_slot = slot;
    x86op->arg = arg;
    add_x86_op_to_rule(r, x86op);
}

// Add a load value operation to a rule
void add_load_value(Rule *r, int arg, int slot) {
    X86Operation *x86op, *o;

    x86op = malloc(sizeof(X86Operation));
    memset(x86op, 0, sizeof(X86Operation));
    x86op->load_value_from_slot = slot;
    x86op->arg = arg;
    add_x86_op_to_rule(r, x86op);
}

void make_rule_hash(int i) {
    Rule *r;

    r = &(instr_rules[i]);

    r->hash =
        (r->dst              <<  0) +
        (r->src1             <<  8) +
        (r->src2             << 16) +
        (r->operation        << 24) +
        ((long) r->match_dst << 32);
}

void check_for_duplicate_rules() {
    int i, j, duplicates;

    for (i = 0; i < instr_rule_count; i++) make_rule_hash(i);

    duplicates = 0;
    for (i = 0; i < instr_rule_count; i++) {
        for (j = i + 1; j < instr_rule_count; j++) {
            if (instr_rules[i].hash == instr_rules[j].hash) {
                printf("Duplicate rules: %d and %d\n", i, j);
                printf("%-4d ", i);
                print_rule(&(instr_rules[i]), 1);
                printf("%-4d ", j);
                print_rule(&(instr_rules[j]), 1);
                duplicates++;
            }
        }
    }

    if (duplicates) {
        printf("There are %d duplicated rules\n", duplicates);
        exit(1);
    }
}

void check_rules_dont_decrease_precision() {
    int i, dst_size, bad_rules;
    Rule *r;

    bad_rules = 0;
    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);
        if (!r->dst) continue;
        if (r->match_dst) continue;

        dst_size = make_x86_size_from_non_terminal(r->dst);

        if (r->src1 && make_x86_size_from_non_terminal(r->src1) > dst_size) {
            print_rule(r, 0);
            bad_rules++;
        }
        if (r->src2 && make_x86_size_from_non_terminal(r->src2) > dst_size) {
            print_rule(r, 0);
            bad_rules++;
        }
    }

    if (bad_rules) {
        printf("There are %d rules with match_dst=0 that decrease precision\n", bad_rules);
        exit(1);
    }
}

char *non_terminal_string(int nt) {
    // Make a textual representation of a non terminal

    char *buf;

    buf = malloc(6);

         if (!nt)        return "     ";
    else if (nt == CST)  return "cst  ";
    else if (nt == STL)  return "stl  ";
    else if (nt == LAB)  return "lab  ";
    else if (nt == FUN)  return "fun  ";
    else if (nt == CSTL) return "cst:l";
    else if (nt == CSTQ) return "cst:q";
    else if (nt == CST1) return "cst:1";
    else if (nt == CST2) return "cst:2";
    else if (nt == CST3) return "cst:3";
    else if (nt == REG)  return "reg  ";
    else if (nt == REGB) return "reg:b";
    else if (nt == REGW) return "reg:w";
    else if (nt == REGL) return "reg:l";
    else if (nt == REGQ) return "reg:q";
    else if (nt == MEMB) return "mem:b";
    else if (nt == MEMW) return "mem:w";
    else if (nt == MEML) return "mem:l";
    else if (nt == MEMQ) return "mem:q";
    else if (nt == ADRB) return "adr:b";
    else if (nt == ADRW) return "adr:w";
    else if (nt == ADRL) return "adr:l";
    else if (nt == ADRQ) return "adr:q";
    else if (nt == ADRV) return "adr:v";
    else if (nt == MDRB) return "mdr:b";
    else if (nt == MDRW) return "mdr:w";
    else if (nt == MDRL) return "mdr:l";
    else if (nt == MDRQ) return "mdr:q";
    else if (nt == MDRV) return "mdr:v";
    else {
        asprintf(&buf, "nt%03d", nt);
        return buf;
    }
}

char *value_to_non_terminal_string(Value *v) {
    return non_terminal_string(non_terminal_for_value(v));
}

int print_rule(Rule *r, int print_operations) {
    int i, first, chars_printed;
    X86Operation *operation;

    chars_printed = 0;

    chars_printed += printf("%-24s  %-5s%s  %-5s  %-5s  %2d    ",
        operation_string(r->operation),
        non_terminal_string(r->dst),
        r->match_dst ? "(d)" : "   ",
        non_terminal_string(r->src1),
        non_terminal_string(r->src2),
        r->cost
    );

    if (print_operations && r->x86_operations) {
        operation = r->x86_operations;
        first = 1;
        while (operation) {
            if (!first) printf("                                                              ");
            first = 0;

            if (operation->save_value_in_slot)
                chars_printed += printf("special: save arg %d to slot %d\n", operation->arg, operation->save_value_in_slot);
            else if (operation->load_value_from_slot)
                chars_printed += printf("special: load arg %d from slot %d\n", operation->arg, operation->load_value_from_slot);
            else if (operation->template)
                chars_printed += printf("%s\n", operation->template);
            else
                printf("\n");

            operation = operation->next;
        }
    }
    else
        printf("\n");

    return chars_printed;
}

void print_rules() {
    int i;

    for (i = 0; i < instr_rule_count; i++) {
        printf("%-5d ", i);
        print_rule(&(instr_rules[i]), 1);
    }
}

void make_value_x86_size(Value *v) {
    // Determine how many bytes a value takes up, if not already done, and write it to
    // v->86_size

    if (v->x86_size) return;
    if (v->label || v->function_symbol) return;

    if (!v->type) {
        print_value(stdout, v, 0);
        printf("\n");
        panic("make_value_x86_size() got called with a value with no type");
    }

    if (v->is_string_literal)
        v->x86_size = 4;
    else if (v->vreg || v->global_symbol || v->stack_index) {
        if (v->type >= TYPE_PTR)
            v->x86_size = 4;
        else if (v->type <= TYPE_LONG)
            v->x86_size = v->type - TYPE_CHAR + 1;
        else
            panic1d("Illegal type in make_value_x86_size() %d", v->type);
    }
}

int non_terminal_for_value(Value *v) {
    int adr_base;
    int result;

    if (!v->x86_size) make_value_x86_size(v);
    if (v->non_terminal) return v->non_terminal;

    adr_base = v->vreg ? ADR : MDR;

         if (v->is_string_literal)                                  result =  STL;
    else if (v->label)                                              result =  LAB;
    else if (v->function_symbol)                                    result =  FUN;
    else if (v->type == TYPE_PTR + TYPE_VOID)                       result =  adr_base + 5; // *void ADRV or MDRQ
    else if (v->type >= TYPE_PTR + TYPE_PTR)                        result =  adr_base + 4; // **... ADRL or MDRL
    else if (v->type >= TYPE_PTR + TYPE_STRUCT)                     result =  adr_base + 5; // *void ADRV or MDRQ
    else if (v->type >= TYPE_PTR)                                   result =  adr_base + value_ptr_target_x86_size(v);
    else if (v->is_lvalue_in_register)                              result =  ADR + v->x86_size;
    else if (v->global_symbol || v->local_index || v->stack_index)  result =  MEM + v->x86_size;
    else if (v->vreg)                                               result =  REG + v->x86_size;
    else {
        print_value(stdout, v, 0);
        panic("Bad value in non_terminal_for_value()");
    }

    v->non_terminal = result;

    return result;
}

// Used to match a value to a leaf node (operation = 0) src rule
// There are a couple of possible matches for a constant.
int match_value_to_rule_src(Value *v, int src) {
    if (v->is_constant) {
             if (src == CST1 && v->value == 1) return 1;
        else if (src == CST2 && v->value == 2) return 1;
        else if (src == CST3 && v->value == 3) return 1;
        else if (v->value >= -2147483648 && v->value <= 2147483647)
            return src == CSTL;
        else
            return src == CSTQ;
    }
    else
        return non_terminal_for_value(v) == src;
}

// Can parent (src1, src2) and child (dst) non terminals be joined?
// If they are an exact match, yes.
// If the parent wants a REG of a lower precision, that's also allowed,
// since it's the parent that gets to choose the precision, and the child has to
// offer anything that satisfies the parent.
// Upgrades aren't allowed since they require sign extension, which is done
// by specific rules.
int rules_match(int parent, int child) {
         if (parent == child)                return 1;
    else if (parent == REGB && child== REGW) return 1;
    else if (parent == REGB && child== REGL) return 1;
    else if (parent == REGB && child== REGQ) return 1;
    else if (parent == REGW && child== REGL) return 1;
    else if (parent == REGW && child== REGQ) return 1;
    else if (parent == REGL && child== REGQ) return 1;
    else                                     return 0;
}

// Used to match root node, or of match_dst is true on a non-root node
int match_value_to_rule_dst(Value *v, int dst) {
    return rules_match(non_terminal_for_value(v), dst);
}

int value_ptr_target_x86_size(Value *v) {
    // Return how many bytes a dereferenced pointer takes up

    if (v->type < TYPE_PTR) panic("Expected pointer type");

    if (v->type - TYPE_PTR <= TYPE_LONG)
        return v->type - TYPE_PTR - TYPE_CHAR + 1;
    else
        return 4;
}

int make_x86_size_from_non_terminal(int nt) {
    // Returns the width in bytes for a non terminal

         if (nt == CSTL) return 3;
    else if (nt == CSTQ) return 4;
    else if (nt == CST1) return 1;
    else if (nt == CST2) return 1;
    else if (nt == CST3) return 1;
    else if (nt == ADRB || nt == ADRW || nt == ADRL || nt == ADRQ || nt == ADRV) return 4;
    else if (nt == MDRB || nt == MDRW || nt == MDRL || nt == MDRQ || nt == MDRV) return 4;
    else if (nt == REGB || nt == MEMB) return 1;
    else if (nt == REGW || nt == MEMW) return 2;
    else if (nt == REGL || nt == MEML) return 3;
    else if (nt == REGQ || nt == MEMQ) return 4;
    else if (nt == LAB) return -1;
    else if (nt == FUN) return -1;
    else if (nt == STL) return 4;
    else if (nt >= AUTO_NON_TERMINAL_START) return -1;
    else
        panic1s("Unable to determine size for %s", non_terminal_string(nt));
}

Tac *add_x86_instruction(X86Operation *x86op, Value *dst, Value *v1, Value *v2) {
    // Add an x86 instruction to the IR

    Tac *tac;

    if (v1) make_value_x86_size(v1);
    if (v2) make_value_x86_size(v2);

    tac = add_instruction(x86op->operation, dst, v1, v2);
    tac->x86_template = x86op->template;

    return tac;
}

void add_cast_rules() {
    Rule *r;
    int i, j;

    // Casting from/to any integer types non increasing precision
    for (i = REGB; i <= REGQ; i++) {
        for (j = REGB; j <= REGQ; j++) {
            if (i <= j) {
                r = add_rule(i, IR_MOVE, j, 0, 1);
                r->match_dst = 1;
                     if (i == REGB) add_op(r, X_MOV, DST, SRC1, 0 , "movb %v1b, %vdb");
                else if (i == REGW) add_op(r, X_MOV, DST, SRC1, 0 , "movw %v1w, %vdw");
                else if (i == REGL) add_op(r, X_MOV, DST, SRC1, 0 , "movl %v1l, %vdl");
                else if (i == REGQ) add_op(r, X_MOV, DST, SRC1, 0 , "movq %v1q, %vdq");
            }

            fin_rule(r);
        }
    }

    // Casting from/to any integer types with decreasing precision
    // Similar to generic register register sign extend rules
    r = add_rule(REGW, IR_MOVE, REGB, 0, 1); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b, %vdw"); fin_rule(r);
    r = add_rule(REGL, IR_MOVE, REGB, 0, 1); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b, %vdl"); fin_rule(r);
    r = add_rule(REGQ, IR_MOVE, REGB, 0, 1); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b, %vdq"); fin_rule(r);
    r = add_rule(REGL, IR_MOVE, REGW, 0, 1); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w, %vdl"); fin_rule(r);
    r = add_rule(REGQ, IR_MOVE, REGW, 0, 1); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w, %vdq"); fin_rule(r);
    r = add_rule(REGQ, IR_MOVE, REGL, 0, 1); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l, %vdq"); fin_rule(r);

    // Crazy shit for some code that uses a (long *) to store pointers to longs.
    // The sane code should have been using (long **)
    // long *sp; (*((long *) *sp))++ = a;
    r = add_rule(ADRQ, IR_MOVE, REGQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0 , "movq %v1q, %vdq");
    r->match_dst = 1;
}

Rule *add_store_to_pointer(int src1, int src2, char *template) {
    Rule *r;

    r = add_rule(src1, IR_MOVE_TO_REG_LVALUE, src1, src2, 3);
    add_op(r, X_MOV,        DST, SRC1, 0,    "movq %v1q, %vdq");
    add_op(r, X_MOV_TO_IND, 0,   DST,  SRC2, template);
    return r;
}

void add_composite_pointer_rules(int *ntc) {
    int i, ntc1, ntc2;
    char *template;
    Rule *r;

    // Composite loads from pointer for IR_BSHL + IR_ADD
    for (i = 2; i <= 4; i++) { // short, int and long
        ntc1 = ++*ntc;
        r = add_rule(ntc1, IR_BSHL, REGQ, CST1 + i - 2, 1); add_save_value(r, 1, 2); // Save index register to slot 2
             if (i == 2) template = "movw  (%v1q,%v2q,2), %vdw";
        else if (i == 3) template = "movl  (%v1q,%v2q,4), %vdl";
        else if (i == 4) template = "movq  (%v1q,%v2q,8), %vdq";

        ntc2 = ++*ntc;
        r = add_rule(ntc2, IR_ADD, ADR + i, ntc1, 10); add_save_value(r, 1, 1); // Save address register to slot 1
        r = add_rule(REG + i, IR_INDIRECT, ntc2, 0, 2);
        r->match_dst = 1;
        add_load_value(r, 1, 1); // Load address register from slot 1
        add_load_value(r, 2, 2); // Load index register from slot 2
        add_op(r, X_MOV_FROM_SCALED_IND, DST, SRC1, 0, template);
    }

    // Composite load from pointer for a pointer to a struct
    ntc1 = ++*ntc;
    ntc2 = ++*ntc;
    r = add_rule(ntc1, IR_BSHL, REGQ, CST3, 1); add_save_value(r, 1, 2); // Save index register to slot 2
    r = add_rule(ntc2, IR_ADD, ADRQ, ntc1, 10); add_save_value(r, 1, 1); // Save address register to slot 1
    r = add_rule(ADRV, IR_INDIRECT, ntc2, 0, 2); r->match_dst = 1;
    r->match_dst = 1;
    add_load_value(r, 1, 1); // Load address register from slot 1
    add_load_value(r, 2, 2); // Load index register from slot 2
    add_op(r, X_MOV_FROM_SCALED_IND, DST, SRC1, 0, "movq  (%v1q,%v2q,8), %vdq");

    // Composite lea from pointer for IR_BSHL
    for (i = 2; i <= 4; i++) {
        ntc1 = ++*ntc;
        r = add_rule(ntc1, IR_BSHL, REGQ, CST1 + i - 2, 1); add_save_value(r, 1, 2); // Save index register to slot 2
             if (i == 2) template = "lea   (%v1q,%v2q,2), %vdq";
        else if (i == 3) template = "lea   (%v1q,%v2q,4), %vdq";
        else if (i == 4) template = "lea   (%v1q,%v2q,8), %vdq";

        ntc2 = ++*ntc;
        r = add_rule(ntc2, IR_ADD, ADRV, ntc1, 10); add_save_value(r, 1, 1); // Save address register to slot 1
        r = add_rule(ADRV, IR_ADDRESS_OF, ntc2, 0, 2);
        add_load_value(r, 1, 1); // Load address register from slot 1
        add_load_value(r, 2, 2); // Load index register from slot 2
        add_op(r, X_LEA_FROM_SCALED_IND, DST, SRC1, 0, template);
    }
}

void add_pointer_rules(int *ntc) {
    int i, j;
    Rule *r;

    // Loads & stores
    r = add_rule(ADRV, IR_MOVE, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRV, IR_MOVE, ADRB, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, ADRW, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, ADRL, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, ADRQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, REGQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1; // For register (* void) v = (long) l;
    r = add_rule(MDRV, IR_MOVE, REGQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1; // For memory (* void) v = (long) l;

    for (i = ADRB; i <= ADRQ; i++) {
        for (j = ADRB; j <= ADRQ; j++) {
            r = add_rule(i,  IR_MOVE, j, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
            r->match_dst = 1;
        }
    }

    for (i = ADRB; i <= ADRQ; i++) {
        r = add_rule(i,  IR_MOVE, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
        r->match_dst = 1;
    }

    for (i = MDRB; i <= MDRQ; i++) {
        r = add_rule(i,  IR_MOVE, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
        r->match_dst = 1;
    }

    r = add_rule(MDR,  IR_MOVE,       ADR,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(MDRV, IR_MOVE,       ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADR,  IR_ADDRESS_OF, ADR,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r); // & for *&*&*pi
    r = add_rule(ADRV, IR_ADDRESS_OF, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(REGQ, IR_MOVE,       ADR,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(ADRB, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRW, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRL, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRQ, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE,       MDRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRB, 0,             MDRB, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRW, 0,             MDRW, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRL, 0,             MDRL, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRQ, 0,             MDRQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");

    // Address loads
    r = add_rule(ADRB, 0,             STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADRB, IR_MOVE,       STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADRB, IR_ADDRESS_OF, MEMQ, 0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); // For **ppi = 1
    r = add_rule(ADRW, IR_ADDRESS_OF, MEMQ, 0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADRL, IR_ADDRESS_OF, MEMQ, 0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADR,  IR_ADDRESS_OF, MEM,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); fin_rule(r);
    r = add_rule(ADRQ, IR_ADDRESS_OF, MDR,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); fin_rule(r);

    // Loads from pointer
    r = add_rule(REGB, IR_INDIRECT, ADRB, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movb   (%v1q), %vdb"); r->match_dst = 1;
    r = add_rule(REGW, IR_INDIRECT, ADRW, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movw   (%v1q), %vdw"); r->match_dst = 1;
    r = add_rule(REGL, IR_INDIRECT, ADRL, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movl   (%v1q), %vdl"); r->match_dst = 1;
    r = add_rule(REGQ, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;

    r = add_rule(REGQ, IR_INDIRECT, ADRB, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movsbq (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(REGQ, IR_INDIRECT, ADRW, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movswq (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(REGQ, IR_INDIRECT, ADRL, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movslq (%v1q), %vdq"); r->match_dst = 1;

    r = add_rule(ADRB, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRW, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRL, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRQ, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;

    r = add_rule(REGL, IR_INDIRECT, ADRB, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movsbl (%v1q), %vdl"); r->match_dst = 1;
    r = add_rule(REGL, IR_INDIRECT, ADRW, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movswl (%v1q), %vdl"); r->match_dst = 1;
    r = add_rule(REGW, IR_INDIRECT, ADRL, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movslw (%v1q), %vdl"); r->match_dst = 1;

    add_composite_pointer_rules(ntc);

    // Stores of a pointer to a pointer
    add_store_to_pointer(ADRQ, ADRB, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRW, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRL, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRQ, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRV, "movq %v2q, (%v1q)");

    // Stores to pointer from registers
    r = add_store_to_pointer(ADRB, REGB, "movb %v2b, (%v1q)");
    r = add_store_to_pointer(ADRB, REGW, "movb %v2b, (%v1q)"); r->match_dst = 1;
    r = add_store_to_pointer(ADRB, REGL, "movb %v2b, (%v1q)"); r->match_dst = 1;
    r = add_store_to_pointer(ADRB, REGQ, "movb %v2b, (%v1q)"); r->match_dst = 1;
    r = add_store_to_pointer(ADRW, REGW, "movw %v2w, (%v1q)");
    r = add_store_to_pointer(ADRW, REGL, "movw %v2w, (%v1q)"); r->match_dst = 1;
    r = add_store_to_pointer(ADRW, REGQ, "movw %v2w, (%v1q)"); r->match_dst = 1;
    r = add_store_to_pointer(ADRL, REGL, "movl %v2l, (%v1q)");
    r = add_store_to_pointer(ADRL, REGQ, "movl %v2l, (%v1q)"); r->match_dst = 1;
    r = add_store_to_pointer(ADRQ, REGQ, "movq %v2q, (%v1q)");

    // Stores of 32 bit constants in a pointer.
    add_store_to_pointer(ADRB, CSTL, "movb $%v2b, (%v1q)");
    add_store_to_pointer(ADRW, CSTL, "movw $%v2w, (%v1q)");
    add_store_to_pointer(ADRL, CSTL, "movl $%v2l, (%v1q)");
    add_store_to_pointer(ADRQ, CSTL, "movq $%v2q, (%v1q)");
    add_store_to_pointer(ADRV, CSTL, "movq $%v2q, (%v1q)");
}

void add_conditional_zero_jump_rule(int operation, int src1, int src2, int cost, int x86_cmp_operation, char *comparison, char *conditional_jmp, int do_fin_rule) {
    int x86_jmp_operation;
    Rule *r;

    x86_jmp_operation = operation == IR_JZ ? X_JZ : X_JNZ;

    r = add_rule(0, operation, src1, src2, cost);
    add_op(r, x86_cmp_operation, 0, SRC1, 0, comparison);
    add_op(r, x86_jmp_operation, 0, SRC2, 0, conditional_jmp);
    if (do_fin_rule) fin_rule(r);
}

void add_comparison_conditional_jmp_rules(int *ntc, int src1, int src2, char *template) {
    Rule *r;

    (*ntc)++;
    r = add_rule(*ntc, IR_EQ,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je %v1" ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne %v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_NE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne %v1"); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je %v1" ); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_LT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl %v1" ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge %v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_GT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg %v1" ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle %v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_LE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle %v1"); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg %v1" ); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_GE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge %v1"); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl %v1" ); fin_rule(r);
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
    int i;
    char *op_vv, *op_cv;
    Rule *r;

    asprintf(&op_vv, "%s %%v1, %%v2",  x86_operand); // Perform operation on two registers or memory
    asprintf(&op_cv, "%s $%%v1, %%v2", x86_operand); // Perform operation on a constant and a register

    r = add_rule(REG, operation, REG, REG, cost);     add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, CST, REG, cost);     add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_cv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, REG, CST, cost);     add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_cv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, REG, MEM, cost + 1); add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, MEM, REG, cost + 1); add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, CST, MEM, cost + 1); add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s $%v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, MEM, CST, cost + 1); add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s $%v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_vv            );
                                                      fin_rule(r);


    if (operation == IR_ADD) {
        r = add_rule(ADR, operation, REGQ, ADR, cost);    add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq %v1q, %v2q");
                                                          fin_rule(r);
        r = add_rule(ADR, operation, ADR, REGQ, cost);    add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq %v1q, %v2q");
                                                          fin_rule(r);

        r = add_rule(ADR, operation, CSTL, ADR, cost);    add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq $%v1q, %v2q");
                                                          fin_rule(r);
        r = add_rule(ADR, operation, ADR, CSTL, cost);    add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");
                                                          fin_rule(r);

        r = add_rule(ADRV, operation, REGQ, ADRV, cost);  add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq %v1q, %v2q");
        r = add_rule(ADRV, operation, ADRV, REGQ, cost);  add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq %v1q, %v2q");

        r = add_rule(ADRV, operation, CSTL, ADRV, cost);  add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq $%v1q, %v2q");
        r = add_rule(ADRV, operation, ADRV, CSTL, cost);  add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");

        r = add_rule(REGQ, operation, ADR, CSTL, cost);   add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq" );
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");
                                                          fin_rule(r);
        r = add_rule(REGQ, operation, ADRV, CSTL, cost);  add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq" );
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");

        for (i = ADRB; i <= ADRQ; i++) {
            r = add_rule(i, operation, ADRV, CSTL, cost);
            add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
            add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");
            r->match_dst = 1;
          }
    }
}

void add_sub_rule(int dst, int src1, int src2, int cost, char *mov_template, char *sub_template) {
    Rule *r;

    r = add_rule(dst, IR_SUB, src1, src2, cost);
    add_op(r, X_MOV, DST, SRC1, 0,   mov_template);
    add_op(r, X_SUB, DST, SRC2, DST, sub_template);
    fin_rule(r);
}

void add_sub_rules() {
    int i, j;
    Rule *r;

    add_sub_rule(REG, REG, REG,  11, "mov%s %v1, %vd",  "sub%s %v1, %vd");
    add_sub_rule(REG, CST, REG,  11, "mov%s $%v1, %vd", "sub%s %v1, %vd");
    add_sub_rule(REG, REG, CST,  11, "mov%s %v1, %vd",  "sub%s $%v1, %vd");
    add_sub_rule(REG, ADRB, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, ADRW, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, ADRL, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, ADRQ, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(ADR, ADR, CST,  10, "movq %v1q, %vdq", "subq $%v1q, %vdq");
    add_sub_rule(ADR, ADR, REGQ, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, REG, MEM,  11, "mov%s %v1, %vd",  "sub%s %v1, %vd");
    add_sub_rule(REG, MEM, REG,  11, "mov%s %v1, %vd",  "sub%s %v1, %vd");
    add_sub_rule(REG, CST, MEM,  11, "mov%s $%v1, %vd", "sub%s %v1, %vd");
    add_sub_rule(REG, MEM, CST,  11, "mov%s %v1, %vd",  "sub%s $%v1, %vd");

    for (i = ADRB; i <= ADRV; i++) {
        for (j = ADRB; j <= ADRV; j++) {
            add_sub_rule(REGQ, i, j,  11, "movq %v1q, %vdq", "subq %v1q, %vdq");
        }
    }

    add_sub_rule(ADRV, ADRV, CSTL,  11, "movq %v1q, %vdq",  "subq $%v1q, %vdq");
}

void add_div_rule(int dst, int src1, int src2, int cost, char *t1, char *t2, char *t3, char *t4, char *tdiv, char *tmod) {
    Rule *r;

    r = add_rule(dst, IR_DIV, src1,  src2,  cost); add_op(r, X_MOV,  0,    SRC1, 0,    t1);
                                                   add_op(r, X_CLTD, 0,    0,    0,    t2);
                                                   add_op(r, X_MOV,  DST,  SRC2, 0,    t3);
                                                   add_op(r, X_IDIV, DST,  SRC1, SRC2, t4);
                                                   add_op(r, X_MOV,  DST,  0,    0,    tdiv);
                                                   fin_rule(r);
    r = add_rule(dst, IR_MOD, src1,  src2,  cost); add_op(r, X_MOV,  0,    SRC1, 0,    t1);
                                                   add_op(r, X_CLTD, 0,    0,    0,    t2);
                                                   add_op(r, X_MOV,  DST,  SRC2, 0,    t3);
                                                   add_op(r, X_IDIV, DST,  SRC1, SRC2, t4);
                                                   add_op(r, X_MOV,  DST,  0,    0,    tmod);
                                                   fin_rule(r);
}

void add_div_rules() {
    Rule *r;

    add_div_rule(REGL, REGL, REGL, 40, "movl %v1l, %%eax", "cltd", "movl %v1l, %vdl", "idivl %vdl", "movl %%eax, %vdl", "movl %%edx, %vdl");
    add_div_rule(REGQ, REGQ, REGQ, 50, "movq %v1q, %%rax", "cqto", "movq %v1q, %vdq", "idivq %vdq", "movq %%rax, %vdq", "movq %%rdx, %vdq");
}

void add_bnot_rules() {
    Rule *r;

    r = add_rule(REG, IR_BNOT, REG, 0, 3); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd");
                                           add_op(r, X_BNOT, DST, DST,  0, "not%s %vd");
                                           fin_rule(r);
    r = add_rule(REG, IR_BNOT, MEM, 0, 3); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd");
                                           add_op(r, X_BNOT, DST, DST,  0, "not%s %vd");
                                           fin_rule(r);
}

void add_binary_shift_rule(int src2, char *template) {
    Rule *r;

    r = add_rule(REG, IR_BSHL, REG, src2, 4); add_op(r, X_MOV, DST, SRC2, 0, template);
                                              add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                              add_op(r, X_SHL, DST, SRC2, 0, "shl%s %%cl, %vd");
                                              fin_rule(r);
    r = add_rule(REG, IR_BSHR, REG, src2, 4); add_op(r, X_MOV, DST, SRC2, 0, template);
                                              add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                              add_op(r, X_SAR, DST, SRC2, 0, "sar%s %%cl, %vd");
                                              fin_rule(r);
}

void add_binary_shift_rules() {
    Rule *r;

    r = add_rule(REG, IR_BSHL, REG, CST, 3); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SHL, DST, SRC2, 0, "shl%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(REG, IR_BSHR, REG, CST, 3); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SAR, DST, SRC2, 0, "sar%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(REG, IR_BSHL, MEM, CST, 4); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SHL, DST, SRC2, 0, "shl%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(REG, IR_BSHR, MEM, CST, 4); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SAR, DST, SRC2, 0, "sar%s $%v1, %vd");
                                             fin_rule(r);

    add_binary_shift_rule(REGB, "movb %v1b, %%cl");
    add_binary_shift_rule(REGW, "movw %v1w, %%cx");
    add_binary_shift_rule(REGL, "movl %v1l, %%ecx");
    add_binary_shift_rule(REGQ, "movq %v1q, %%rcx");
}

void init_instruction_selection_rules() {
    int i, j;
    Rule *r;
    int ntc; // Non terminal counter
    char *cmp_rr, *cmp_rc, *cmp_rm, *cmp_mr, *cmp_mc;
    char *cmpq_rr, *cmpq_rc, *cmpq_rm, *cmpq_mr, *cmpq_mc;

    instr_rule_count = 0;
    disable_merge_constants = 0;

    instr_rules = malloc(MAX_RULE_COUNT * sizeof(Rule));
    memset(instr_rules, 0, MAX_RULE_COUNT * sizeof(Rule));

    ntc = AUTO_NON_TERMINAL_START;

    // Identity rules, for matching leaf nodes in the instruction tree
    r = add_rule(REG,  0, REG,  0, 0); fin_rule(r);
    r = add_rule(CSTL, 0, CSTL, 0, 0);
    r = add_rule(CSTQ, 0, CSTQ, 0, 0);
    r = add_rule(CST1, 0, CST1, 0, 0);
    r = add_rule(CST2, 0, CST2, 0, 0);
    r = add_rule(CST3, 0, CST3, 0, 0);
    r = add_rule(MEM,  0, MEM,  0, 0); fin_rule(r);
    r = add_rule(ADR,  0, ADR,  0, 0); fin_rule(r);
    r = add_rule(ADRV, 0, ADRV, 0, 0);
    r = add_rule(MDRV, 0, MDRV, 0, 0);
    r = add_rule(MDR,  0, MDR,  0, 0); fin_rule(r);
    r = add_rule(REGQ, 0, ADRQ, 0, 0);
    r = add_rule(REGQ, 0, ADRV, 0, 0);
    r = add_rule(LAB,  0, LAB,  0, 0); fin_rule(r);
    r = add_rule(FUN,  0, FUN,  0, 0); fin_rule(r);

    // This rule is needed for memory-memory moves
    r = add_rule(ADRV, 0, MDRV, 0, 2); add_op(r, X_MOV, DST, SRC1, 0 , "movq %v1q, %vdq");

    add_cast_rules();

    // Register -> register sign extension for precision increases
    r = add_rule(REGW, 0, REGB, 0, 1); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b, %vdw");
    r = add_rule(REGL, 0, REGB, 0, 1); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b, %vdl");
    r = add_rule(REGQ, 0, REGB, 0, 1); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b, %vdq");
    r = add_rule(REGL, 0, REGW, 0, 1); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w, %vdl");
    r = add_rule(REGQ, 0, REGW, 0, 1); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w, %vdq");
    r = add_rule(REGQ, 0, REGL, 0, 1); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l, %vdq");

    // Memory -> register sign extension
    r = add_rule(REGW, 0, MEMB, 0, 2); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b, %vdw"); fin_rule(r);
    r = add_rule(REGL, 0, MEMB, 0, 2); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b, %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, MEMB, 0, 2); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b, %vdq"); fin_rule(r);
    r = add_rule(REGL, 0, MEMW, 0, 2); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w, %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, MEMW, 0, 2); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w, %vdq"); fin_rule(r);
    r = add_rule(REGQ, 0, MEML, 0, 2); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l, %vdq"); fin_rule(r);

    r = add_rule(0, IR_RETURN, 0,    0, 1); add_op(r, X_RET,  0,   0, 0, 0                      ); fin_rule(r); // Return void
    r = add_rule(0, IR_RETURN, CSTL, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "mov $%v1q, %%rax"  ); fin_rule(r); // Return constant
    r = add_rule(0, IR_RETURN, CSTQ, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "mov $%v1q, %%rax"  ); fin_rule(r); // Return constant
    r = add_rule(0, IR_RETURN, REGB, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movsbq %v1b, %%rax"); fin_rule(r); // Return register byte
    r = add_rule(0, IR_RETURN, REGW, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movswq %v1w, %%rax"); fin_rule(r); // Return register word
    r = add_rule(0, IR_RETURN, REGL, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movslq %v1l, %%rax"); fin_rule(r); // Return register long
    r = add_rule(0, IR_RETURN, REGQ, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  ); fin_rule(r); // Return register quad
    r = add_rule(0, IR_RETURN, MEMB, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movsbq %v1b, %%rax"); fin_rule(r); // Return memory byte
    r = add_rule(0, IR_RETURN, MEMW, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movswq %v1w, %%rax"); fin_rule(r); // Return memory word
    r = add_rule(0, IR_RETURN, MEML, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movslq %v1l, %%rax"); fin_rule(r); // Return memory long
    r = add_rule(0, IR_RETURN, MEMQ, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  ); fin_rule(r); // Return memory quad
    r = add_rule(0, IR_RETURN, ADR,  0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  ); fin_rule(r); // Return a pointer
    r = add_rule(0, IR_RETURN, ADRV, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  );              // Return a *void

    r = add_rule(0, IR_ARG, CSTL, CSTL, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq $%v2q"); fin_rule(r); // Use constant as function arg, must not be a quad
    r = add_rule(0, IR_ARG, CSTL, REGQ, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); fin_rule(r); // Use register as function arg, must be a quad

    r = add_rule(0, IR_ARG, CSTL, ADRB, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); fin_rule(r); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRW, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); fin_rule(r); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRL, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); fin_rule(r); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRQ, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); fin_rule(r); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRV, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); fin_rule(r); // Use pointer in register as function arg

    r = add_rule(REG,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r); // Function call with a return value
    r = add_rule(ADR,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r); // Function call with a return value
    r = add_rule(ADRV, IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0);              // Function call with a *void return value

    r = add_rule(REG, IR_MOVE, REG, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1, %vd"  ); fin_rule(r); // Register to register copy
    r = add_rule(MEM, IR_MOVE, CST, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s $%v1, %vd" ); fin_rule(r); // Store constant in memory
    r = add_rule(MEM, IR_MOVE, REG, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1, %vd"  ); fin_rule(r); // Store register in memory

    // Constant loads into a register
    r = add_rule(REGL, 0,       CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movl $%v1l, %vdl");
    r = add_rule(REGQ, 0,       CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(REGQ, 0,       CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(REGL, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movl $%v1l, %vdl");
    r = add_rule(REGQ, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(REGQ, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRB, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRB, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRW, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRW, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRL, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRL, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRQ, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRQ, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRV, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRV, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");

    r = add_rule(REG,  0,       MEM,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r); // Load temp memory into register
    r = add_rule(REG,  IR_MOVE, MEM,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r); // Load standalone memory into register

    add_pointer_rules(&ntc);

    r = add_rule(0, IR_JMP, LAB, 0,1);  add_op(r, X_JMP, 0, SRC1, 0, "jmp %v1"); fin_rule(r);  // JMP

    add_conditional_zero_jump_rule(IR_JZ,  REG,  LAB,  3,  X_TEST, "test%s %v1, %v1",  "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  ADR,  LAB,  3,  X_TEST, "testq %v1q, %v1q", "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  ADRV, LAB,  3,  X_TEST, "testq %v1q, %v1q", "jz %v1q",  0);
    add_conditional_zero_jump_rule(IR_JZ,  MEM,  LAB,  11, X_CMPZ, "cmp $0, %v1",      "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JNZ, REG,  LAB,  3,  X_TEST, "test%s %v1, %v1",  "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, ADR,  LAB,  3,  X_TEST, "testq %v1q, %v1q", "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, ADRV, LAB,  3,  X_TEST, "testq %v1q, %v1q", "jnz %v1q", 0);
    add_conditional_zero_jump_rule(IR_JNZ, MEM,  LAB,  11, X_CMPZ, "cmp $0, %v1",      "jnz %v1",  1);

    // All pairwise combinations of (CST, REG, MEM) that have associated x86 instructions
    cmp_rr = "cmp%s %v2, %v1";  cmpq_rr = "cmpq %v2q, %v1q";
    cmp_rc = "cmp%s $%v2, %v1"; cmpq_rc = "cmpq $%v2q, %v1q";
    cmp_rm = "cmp%s %v2, %v1";  cmpq_rm = "cmpq %v2q, %v1q";
    cmp_mr = "cmp%s %v2, %v1";  cmpq_mr = "cmpq %v2q, %v1q";
    cmp_mc = "cmp%s $%v2, %v1"; cmpq_mc = "cmpq $%v2q, %v1q";

    // Comparision + conditional jump
    add_comparison_conditional_jmp_rules(&ntc, REG, REG, cmp_rr);
    add_comparison_conditional_jmp_rules(&ntc, REG, CST, cmp_rc);
    add_comparison_conditional_jmp_rules(&ntc, REG, MEM, cmp_rm);
    add_comparison_conditional_jmp_rules(&ntc, MEM, REG, cmp_mr);
    add_comparison_conditional_jmp_rules(&ntc, MEM, CST, cmp_mc);

    // Comparision + conditional jump for pointers.
    add_comparison_conditional_jmp_rules(&ntc, REG, ADR, cmpq_rr);
    add_comparison_conditional_jmp_rules(&ntc, ADR, ADR, cmpq_rr);
    add_comparison_conditional_jmp_rules(&ntc, ADR, REG, cmpq_rr);
    add_comparison_conditional_jmp_rules(&ntc, ADR, CST, cmpq_rc);
    add_comparison_conditional_jmp_rules(&ntc, ADR, MEM, cmpq_rm);
    add_comparison_conditional_jmp_rules(&ntc, MEM, ADR, cmpq_mr);

    add_comparison_conditional_jmp_rules(&ntc, ADRV, CSTL, "cmpq $%v2l, %v1q");
    add_comparison_conditional_jmp_rules(&ntc, CSTL, ADRV, "cmpq $%v1l, %v2q");

    // Comparision + conditional assignment
    add_comparison_assignment_rules(REG, REG, cmp_rr);
    add_comparison_assignment_rules(REG, CST, cmp_rc);
    add_comparison_assignment_rules(REG, MEM, cmp_rm);
    add_comparison_assignment_rules(MEM, REG, cmp_mr);
    add_comparison_assignment_rules(MEM, CST, cmp_mc);

    add_comparison_assignment_rules(ADR,  CSTL, "cmpq $%v2, %v1q");
    add_comparison_assignment_rules(ADRV, CSTL, "cmpq $%v2, %v1q");
    add_comparison_assignment_rules(ADR,  ADRV, "cmpq %v2q, %v1q");
    add_comparison_assignment_rules(ADRV, ADR,  "cmpq %v2q, %v1q");
    add_comparison_assignment_rules(ADRV, ADRV, "cmpq %v2q, %v1q");

    add_commutative_operation_rules("add%s",  IR_ADD,  X_ADD,  10);
    add_commutative_operation_rules("imul%s", IR_MUL,  X_MUL,  30);
    add_commutative_operation_rules("or%s",   IR_BOR,  X_BOR,  3);
    add_commutative_operation_rules("and%s",  IR_BAND, X_BAND, 3);
    add_commutative_operation_rules("xor%s",  IR_XOR,  X_XOR,  3);

    add_sub_rules();
    add_div_rules();
    add_bnot_rules();
    add_binary_shift_rules();

    check_for_duplicate_rules();
}
