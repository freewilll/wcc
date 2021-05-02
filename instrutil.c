#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static int non_terminal_for_value(Value *v);
static int value_ptr_target_x86_size(Value *v);

Rule *add_rule(int dst, int operation, int src1, int src2, int cost) {
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
        print_rule(r, 0, 0);
        printf("A zero cost rule cannot have an operation");
    }

    instr_rule_count++;
    return r;
}

static int transform_rule_value(int v, int i) {
    if (v == REG || v == MEM || v == ADR || v == MDR)
        return v + i;
    else if (v == CST)
        return CSTL;
    else
        return v;
}

static X86Operation *dup_x86_operation(X86Operation *operation) {
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

static X86Operation *dup_x86_operations(X86Operation *operation) {
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

static char *add_size_to_template(char *template, int size) {
    char *dst, *c;
    char *result, x86_size;

    if (!template) return 0; // Some magic operations have no templates but are implemented in codegen.

    x86_size = size_to_x86_size(size);
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

// Expand all uses of REG, MEM, ADR into ADRB, ADRW, ADRL, ADRQ
// e.g. REG, REG, CST is transformed into
// REGB, REGB, CST
// REGW, REGW, CST
// REGL, REGL, CST
// REGQ, REGQ, CST
void fin_rule(Rule *r) {

    int operation, dst, src1, src2, cost, i;
    X86Operation *x86_operations, *x86_operation;
    Rule *new_rule;

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
static void add_x86_op_to_rule(Rule *r, X86Operation *x86op) {
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
X86Operation *add_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    X86Operation *x86op;

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

    return x86op;
}

// Add a save value operation to a rule
void add_save_value(Rule *r, int arg, int slot) {
    X86Operation *x86op;

    x86op = malloc(sizeof(X86Operation));
    memset(x86op, 0, sizeof(X86Operation));
    x86op->save_value_in_slot = slot;
    x86op->arg = arg;
    add_x86_op_to_rule(r, x86op);
}

// Add a load value operation to a rule
void add_load_value(Rule *r, int arg, int slot) {
    X86Operation *x86op;

    x86op = malloc(sizeof(X86Operation));
    memset(x86op, 0, sizeof(X86Operation));
    x86op->load_value_from_slot = slot;
    x86op->arg = arg;
    add_x86_op_to_rule(r, x86op);
}

static void make_rule_hash(int i) {
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
                print_rule(&(instr_rules[i]), 1, 0);
                printf("%-4d ", j);
                print_rule(&(instr_rules[j]), 1, 0);
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
            print_rule(r, 0, 0);
            bad_rules++;
        }
        if (r->src2 && make_x86_size_from_non_terminal(r->src2) > dst_size) {
            print_rule(r, 0, 0);
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

void print_rule(Rule *r, int print_operations, int indent) {
    int i, first;
    X86Operation *operation;

    printf("%-24s  %-5s%s  %-5s  %-5s  %2d    ",
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
            if (!first) {
                for (i = 0;i < indent; i++) printf(" ");
                printf("                                                        ");
            }
            first = 0;

            if (operation->save_value_in_slot)
                printf("special: save arg %d to slot %d\n", operation->arg, operation->save_value_in_slot);
            else if (operation->load_value_from_slot)
                printf("special: load arg %d from slot %d\n", operation->arg, operation->load_value_from_slot);
            else if (operation->template)
                printf("%s\n", operation->template);
            else
                printf("\n");

            operation = operation->next;
        }
    }
    else
        printf("\n");
}

void print_rules() {
    int i;

    for (i = 0; i < instr_rule_count; i++) {
        printf("%-5d ", i);
        print_rule(&(instr_rules[i]), 1, 6);
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

static int non_terminal_for_value(Value *v) {
    int adr_base, ptr_to_int, result;

    if (!v->x86_size) make_value_x86_size(v);
    if (v->non_terminal) return v->non_terminal;

    adr_base = v->vreg ? ADR : MDR;
    ptr_to_int = v->type >= TYPE_PTR + TYPE_CHAR && v->type <= TYPE_PTR + TYPE_LONG;

         if (v->is_string_literal)               result =  STL;
    else if (v->label)                           result =  LAB;
    else if (v->function_symbol)                 result =  FUN;
    else if (ptr_to_int)                         result =  adr_base + value_ptr_target_x86_size(v);
    else if (v->type >= TYPE_PTR)                result =  adr_base + 5; // ADRV or MDRV
    else if (v->is_lvalue_in_register)           result =  ADR + v->x86_size;
    else if (v->global_symbol || v->stack_index) result =  MEM + v->x86_size;
    else if (v->vreg)                            result =  REG + v->x86_size;
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

// Used to match root node, or of match_dst is true on a non-root node.
// This ensures the precision of dst is >= of the value.
// This is needed for a special case of indirects where there are rules that can
// indirect and sign extend at the same time.
int match_value_to_rule_dst(Value *v, int dst) {
    int vnt;

    vnt = non_terminal_for_value(v);

    if (vnt == dst) return 1;
    else if (vnt == REGB && dst== REGW) return 1;
    else if (vnt == REGB && dst== REGL) return 1;
    else if (vnt == REGB && dst== REGQ) return 1;
    else if (vnt == REGW && dst== REGL) return 1;
    else if (vnt == REGW && dst== REGQ) return 1;
    else if (vnt == REGL && dst== REGQ) return 1;
    else                                return 0;
}

static int value_ptr_target_x86_size(Value *v) {
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

