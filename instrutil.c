#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static int non_terminal_for_value(Value *v);
static int value_ptr_target_x86_size(Value *v);

Rule *add_rule(int dst, int operation, int src1, int src2, int cost) {
    if (instr_rule_count == MAX_RULE_COUNT) panic1d("Exceeded maximum number of rules %d", MAX_RULE_COUNT);

    Rule *r = &(instr_rules[instr_rule_count]);

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

static int transform_rule_value(int extend_size, int extend_sign, int v, int size, int is_unsigned) {
    int result = v;

    if (extend_size) {
             if (v == XCI) result = CI1 + size - 1;
        else if (v == XCU) result = CU1 + size - 1;
        else if (v == XRI) result = RI1 + size - 1;
        else if (v == XRU) result = RU1 + size - 1;
        else if (v == XMI) result = MI1 + size - 1;
        else if (v == XMU) result = MU1 + size - 1;
        else if (v == XRP) result = RP1 + size - 1;
        else if (v == XC) result = (is_unsigned ? CU1 : CI1) + size - 1;
        else if (v == XR) result = (is_unsigned ? RU1 : RI1) + size - 1;
        else if (v == XM) result = (is_unsigned ? MU1 : MI1) + size - 1;
    }
    else if (extend_sign) {
             if (v == XC1) result = (is_unsigned ? CU1 : CI1);
        else if (v == XC2) result = (is_unsigned ? CU2 : CI2);
        else if (v == XC3) result = (is_unsigned ? CU3 : CI3);
        else if (v == XC4) result = (is_unsigned ? CU4 : CI4);
        else if (v == XR1) result = (is_unsigned ? RU1 : RI1);
        else if (v == XR2) result = (is_unsigned ? RU2 : RI2);
        else if (v == XR3) result = (is_unsigned ? RU3 : RI3);
        else if (v == XR4) result = (is_unsigned ? RU4 : RI4);
        else if (v == XM1) result = (is_unsigned ? MU1 : MI1);
        else if (v == XM2) result = (is_unsigned ? MU2 : MI2);
        else if (v == XM3) result = (is_unsigned ? MU3 : MI3);
        else if (v == XM4) result = (is_unsigned ? MU4 : MI4);
    }

    return result;
}

static X86Operation *dup_x86_operation(X86Operation *operation) {
    X86Operation *result = malloc(sizeof(X86Operation));
    result->operation = operation->operation;
    result->dst = operation->dst;
    result->v1 = operation->v1;
    result->v2 = operation->v2;
    result->template = operation->template ? strdup(operation->template) : 0;
    result->save_value_in_slot = operation->save_value_in_slot;
    result->allocate_stack_index_in_slot = operation->allocate_stack_index_in_slot;
    result->allocate_register_in_slot = operation->allocate_register_in_slot;
    result->allocated_type = operation->allocated_type;
    result->arg = operation->arg;
    result->next = 0;

    return result;
}

static X86Operation *dup_x86_operations(X86Operation *operation) {
    X86Operation *o;
    X86Operation *result = 0;

    // Create new linked list with duplicates of the x86 operations
    while (operation) {
        X86Operation *new_operation = dup_x86_operation(operation);
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
    if (!template) return 0; // Some magic operations have no templates but are implemented in codegen.

    char x86_size = size_to_x86_size(size);
    char *result = malloc(128);
    memset(result, 0, 128);
    char *dst = result;

    char *c = template;
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

// Create new rules by expanding type and/or sign in non terminals
// e.g.
// (RP, RI) => (RP1, RI1), (RP2, RI2), (RP3, RI3), (RP4, RI4)
//
// XC  => CI1, CI2, CI3, CI4, CU1, CU2, CU3, CU4
// XR  => RI1, RI2, RI3, RI4, RU1, RU2, RU3, RU4
// XM  => MI1, MI2, MI3, MI4, MU1, MU2, MU3, MU4
// XCI => CI1, CI2, CI3, CI4
// XRI => RI1, RI2, RI3, RI4
// XRU => RU1, RU2, RU3, RU4
// XRP => RP1, RP2, RP3, RP4
// XC1 => CI1, CU1, also XC2 => ... etc
// XR1 => RI1, RU1, also XR2 => ... etc
// XM1 => MI1, MU1, also XM2 => ... etc
void fin_rule(Rule *r) {
    int operation                = r->operation;
    int dst                      = r->dst;
    int src1                     = r->src1;
    int src2                     = r->src2;
    int cost                     = r->cost;
    X86Operation *x86_operations = r->x86_operations;

    int expand_size = dst & EXP_SIZE || src1 & EXP_SIZE || src2 & EXP_SIZE;
    int expand_sign = dst & EXP_SIGN || src1 & EXP_SIGN || src2 & EXP_SIGN;

    if (!expand_size && !expand_sign) return;

    instr_rule_count--; // Rewind next pointer so that the last rule is overwritten

    for (int size = 1; size <= (expand_size ? 4 : 1); size++) {
        for (int is_unsigned = 0; is_unsigned < (expand_sign ? 2 : 1); is_unsigned++) {
            Rule *new_rule = add_rule(
                transform_rule_value(expand_size, expand_sign, dst, size, is_unsigned),
                operation,
                transform_rule_value(expand_size, expand_sign, src1, size, is_unsigned),
                transform_rule_value(expand_size, expand_sign, src2, size, is_unsigned),
                cost
            );
            new_rule->x86_operations = dup_x86_operations(x86_operations);

            X86Operation *x86_operation = new_rule->x86_operations;
            while (x86_operation) {
                x86_operation->template = add_size_to_template(x86_operation->template, size);
                x86_operation = x86_operation->next;
            }
        }
    }
}

// Add an X86Operation template to a rule's linked list
static void add_x86_op_to_rule(Rule *r, X86Operation *x86op) {
    if (!r->x86_operations)
        r->x86_operations = x86op;
    else {
        X86Operation *o = r->x86_operations;
        while (o->next) o = o->next;
        o->next = x86op;
    }
}

// Add an x86 operation template to a rule
X86Operation *add_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    X86Operation *x86op = malloc(sizeof(X86Operation));
    x86op->operation = operation;

    x86op->dst = dst;
    x86op->v1 = v1;
    x86op->v2 = v2;

    x86op->template = template;
    x86op->save_value_in_slot = 0;
    x86op->allocate_stack_index_in_slot = 0;
    x86op->allocate_register_in_slot = 0;
    x86op->allocated_type = 0;
    x86op->arg = 0;

    x86op->next = 0;

    add_x86_op_to_rule(r, x86op);

    return x86op;
}

// Add a save value operation to a rule
void add_save_value(Rule *r, int arg, int slot) {
    X86Operation *x86op = malloc(sizeof(X86Operation));
    memset(x86op, 0, sizeof(X86Operation));
    x86op->save_value_in_slot = slot;
    x86op->arg = arg;
    add_x86_op_to_rule(r, x86op);
}

void add_allocate_stack_index_in_slot(Rule *r, int slot, int type) {
    X86Operation *x86op = malloc(sizeof(X86Operation));
    memset(x86op, 0, sizeof(X86Operation));
    x86op->allocate_stack_index_in_slot = slot;
    x86op->allocated_type = type;
    add_x86_op_to_rule(r, x86op);
}

void add_allocate_register_in_slot(Rule *r, int slot, int type) {
    X86Operation *x86op = malloc(sizeof(X86Operation));
    memset(x86op, 0, sizeof(X86Operation));
    x86op->allocate_register_in_slot = slot;
    x86op->allocated_type = type;
    add_x86_op_to_rule(r, x86op);
}

static void make_rule_hash(int i) {
    Rule *r = &(instr_rules[i]);

    r->hash =
        (r->dst       <<  0) +
        (r->src1      <<  8) +
        (r->src2      << 16) +
        (r->operation << 24);
}

void check_for_duplicate_rules() {
    for (int i = 0; i < instr_rule_count; i++) make_rule_hash(i);

    int duplicates = 0;
    for (int i = 0; i < instr_rule_count; i++) {
        for (int j = i + 1; j < instr_rule_count; j++) {
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
    int bad_rules = 0;
    for (int i = 0; i < instr_rule_count; i++) {
        Rule *r = &(instr_rules[i]);
        if (!r->dst) continue;

        int dst_size = make_x86_size_from_non_terminal(r->dst);

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
        printf("There are %d rules that decrease precision\n", bad_rules);
        exit(1);
    }
}

// Make a textual representation of a non terminal
char *non_terminal_string(int nt) {
    char *buf = malloc(6);

         if (!nt)         return "";
    else if (nt == CSTV1) return "cstv1";
    else if (nt == CSTV2) return "cstv2";
    else if (nt == CSTV3) return "cstv3";
    else if (nt == XC)    return "xc";
    else if (nt == XC1)   return "xc1";
    else if (nt == XC2)   return "xc2";
    else if (nt == XC3)   return "xc3";
    else if (nt == XC4)   return "xc4";
    else if (nt == XCI)   return "xci";
    else if (nt == CI1)   return "ci1";
    else if (nt == CI2)   return "ci2";
    else if (nt == CI3)   return "ci3";
    else if (nt == CI4)   return "ci4";
    else if (nt == XCU)   return "xcu";
    else if (nt == CU1)   return "cu1";
    else if (nt == CU2)   return "cu2";
    else if (nt == CU3)   return "cu3";
    else if (nt == CU4)   return "cu4";
    else if (nt == CLD)   return "cld";
    else if (nt == STL)   return "stl";
    else if (nt == LAB)   return "lab";
    else if (nt == FUN)   return "fun";
    else if (nt == XR)    return "xr";
    else if (nt == XR1)   return "xr1";
    else if (nt == XR2)   return "xr2";
    else if (nt == XR3)   return "xr3";
    else if (nt == XR4)   return "xr4";
    else if (nt == XRI)   return "xri";
    else if (nt == RI1)   return "ri1";
    else if (nt == RI2)   return "ri2";
    else if (nt == RI3)   return "ri3";
    else if (nt == RI4)   return "ri4";
    else if (nt == XRU)   return "xru";
    else if (nt == RU1)   return "ru1";
    else if (nt == RU2)   return "ru2";
    else if (nt == RU3)   return "ru3";
    else if (nt == RU4)   return "ru4";
    else if (nt == XM)    return "xm";
    else if (nt == XM1)   return "xm1";
    else if (nt == XM2)   return "xm2";
    else if (nt == XM3)   return "xm3";
    else if (nt == XM4)   return "xm4";
    else if (nt == MI1)   return "mi1";
    else if (nt == MI2)   return "mi2";
    else if (nt == MI3)   return "mi3";
    else if (nt == MI4)   return "mi4";
    else if (nt == MU1)   return "mu1";
    else if (nt == MU2)   return "mu2";
    else if (nt == MU3)   return "mu3";
    else if (nt == MU4)   return "mu4";
    else if (nt == RP1)   return "rp1";
    else if (nt == RP2)   return "rp2";
    else if (nt == RP3)   return "rp3";
    else if (nt == RP4)   return "rp4";
    else if (nt == RP5)   return "rp5";
    else if (nt == MLD5)  return "mld5";
    else if (nt == MRP5)  return "mrp5";
    else {
        asprintf(&buf, "nt%03d", nt);
        return buf;
    }
}

char *value_to_non_terminal_string(Value *v) {
    return non_terminal_string(non_terminal_for_value(v));
}

void print_rule(Rule *r, int print_operations, int indent) {
    printf("%-24s  %-5s  %-5s  %-5s  %2d    ",
        operation_string(r->operation),
        non_terminal_string(r->dst),
        non_terminal_string(r->src1),
        non_terminal_string(r->src2),
        r->cost
    );

    if (print_operations && r->x86_operations) {
        X86Operation *operation = r->x86_operations;
        int first = 1;
        while (operation) {
            if (!first) {
                for (int i = 0;i < indent; i++) printf(" ");
                printf("                                                     ");
            }
            first = 0;

            if (operation->save_value_in_slot)
                printf("special: save arg %d to slot %d\n", operation->arg, operation->save_value_in_slot);
            else if (operation->allocate_stack_index_in_slot)
                printf("special: allocate stack index of type %d to slot %d\n", operation->allocated_type, operation->allocate_stack_index_in_slot);
            else if (operation->allocate_register_in_slot)
                printf("special: allocate register of type %d to slot %d\n", operation->allocated_type, operation->allocate_register_in_slot);
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
    for (int i = 0; i < instr_rule_count; i++) {
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
        if (v->type->type >= TYPE_PTR)
            v->x86_size = 4;
        else if (v->type->type <= TYPE_LONG)
            v->x86_size = v->type->type - TYPE_CHAR + 1;
        else if (v->type->type == TYPE_FLOAT)
            v->x86_size = 3;
        else if (v->type->type == TYPE_DOUBLE)
            v->x86_size = 4;
        else if (v->type->type == TYPE_LONG_DOUBLE)
            v->x86_size = 5;
        else
            panic1d("Illegal type in make_value_x86_size() %d", v->type->type);
    }
}

static int non_terminal_for_value(Value *v) {
    int result;

    if (!v->x86_size) make_value_x86_size(v);
    if (v->non_terminal) return v->non_terminal;

    int is_local = !v->global_symbol && !v->stack_index;

         if (v->is_string_literal)                                           result =  STL;
    else if (v->label)                                                       result =  LAB;
    else if (v->function_symbol)                                             result =  FUN;
    else if (!is_local && v->type->type == TYPE_PTR + TYPE_LONG_DOUBLE)      result =  MRP5;
    else if (is_local && v->type->type == TYPE_PTR + TYPE_LONG_DOUBLE)       result =  RP5;
    else if (v->type->type >= TYPE_PTR && !v->type->is_unsigned && !v->vreg) result =  MI4;
    else if (v->type->type >= TYPE_PTR &&  v->type->is_unsigned && !v->vreg) result =  MU4;
    else if (v->type->type >= TYPE_PTR)                                      result =  RP1 + value_ptr_target_x86_size(v) -1;
    else if (v->is_lvalue_in_register)                                       result =  RP1 + v->x86_size - 1;
    else if (!is_local & v->type->type == TYPE_LONG_DOUBLE)                  result =  MLD5;
    else if (!is_local & !v->type->is_unsigned)                              result =  MI1 + v->x86_size - 1;
    else if (!is_local &  v->type->is_unsigned)                              result =  MU1 + v->x86_size - 1;
    else if (v->vreg && !v->type->is_unsigned)                               result =  RI1 + v->x86_size - 1;
    else if (v->vreg &&  v->type->is_unsigned)                               result =  RU1 + v->x86_size - 1;
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
        if (v->type->type == TYPE_LONG_DOUBLE)
            return src == CLD;
        else {
            // Integer constant
            if (v->type->type < TYPE_INT) panic1d("Unexpected constant type %d", v->type->type);

            // Match 1, 2 and 3
                 if (src == CSTV1 && v->int_value == 1) return 1;
            else if (src == CSTV2 && v->int_value == 2) return 1;
            else if (src == CSTV3 && v->int_value == 3) return 1;

            // Check match with type from the parser. This is necessary for evil casts, e.g.
            // (unsigned int) -1, which would otherwise become a CU4 and not match rules for CU3.
                 if (src >= CI3 && src <= CI4 && !v->type->is_unsigned && v->type->type == TYPE_INT)   return 1;
            else if (              src == CI4 && !v->type->is_unsigned)                                return 1;
                 if (src >= CU3 && src <= CU4 &&  v->type->is_unsigned && v->type->type == TYPE_INT)   return 1;
            else if (              src == CU4 &&  v->type->is_unsigned)                                return 1;

            // Determine constant non termimal by looking at the signdness and value
            else if (src >= CI1 && src <= CI4 && !v->type->is_unsigned && v->int_value >= -0x80        && v->int_value < 0x80       ) return 1;
            else if (src >= CI2 && src <= CI4 && !v->type->is_unsigned && v->int_value >= -0x8000      && v->int_value < 0x8000     ) return 1;
            else if (src >= CI3 && src <= CI4 && !v->type->is_unsigned && v->int_value >= -0x80000000l && v->int_value < 0x80000000l) return 1;
            else if (              src == CI4 && !v->type->is_unsigned)                                                               return 1;

            else if (src >= CU1 && src <= CU4 &&  v->type->is_unsigned && v->int_value >= 0 && v->int_value < 0x100      ) return 1;
            else if (src >= CU2 && src <= CU4 &&  v->type->is_unsigned && v->int_value >= 0 && v->int_value < 0x100000   ) return 1;
            else if (src >= CU3 && src <= CU4 &&  v->type->is_unsigned && v->int_value >= 0 && v->int_value < 0x100000000) return 1;
            else if (              src == CU4 &&  v->type->is_unsigned)                                                    return 1;

            else return 0;
        }
    }
    else
        return non_terminal_for_value(v) == src;
}

// Used to match the root node. It must be an exact match.
int match_value_to_rule_dst(Value *v, int dst) {
    return non_terminal_for_value(v) == dst;
}

// Match a value type to a non terminal rule type. This is necessary to ensure that
// non-root and non-leaf nodes have matching types while tree matching.
int match_value_type_to_rule_dst(Value *v, int dst) {
    int vnt = non_terminal_for_value(v);
    int is_ptr = v->type->type >= TYPE_PTR;

    int ptr_size;
    if (is_ptr) ptr_size = value_ptr_target_x86_size(v);

    if (dst == vnt) return 1;
    else if (dst >= AUTO_NON_TERMINAL_START) return 1;
    else if (dst == RI1  && v->type->type == TYPE_CHAR  && !v->type->is_unsigned) return 1;
    else if (dst == RI2  && v->type->type == TYPE_SHORT && !v->type->is_unsigned) return 1;
    else if (dst == RI3  && v->type->type == TYPE_INT   && !v->type->is_unsigned) return 1;
    else if (dst == RI4  && v->type->type == TYPE_LONG  && !v->type->is_unsigned) return 1;
    else if (dst == RU1  && v->type->type == TYPE_CHAR  &&  v->type->is_unsigned) return 1;
    else if (dst == RU2  && v->type->type == TYPE_SHORT &&  v->type->is_unsigned) return 1;
    else if (dst == RU3  && v->type->type == TYPE_INT   &&  v->type->is_unsigned) return 1;
    else if (dst == RU4  && v->type->type == TYPE_LONG  &&  v->type->is_unsigned) return 1;
    else if (dst == MI1  && v->type->type == TYPE_CHAR  && !v->type->is_unsigned) return 1;
    else if (dst == MI2  && v->type->type == TYPE_SHORT && !v->type->is_unsigned) return 1;
    else if (dst == MI3  && v->type->type == TYPE_INT   && !v->type->is_unsigned) return 1;
    else if (dst == MI4  && v->type->type == TYPE_LONG  && !v->type->is_unsigned) return 1;
    else if (dst == MU1  && v->type->type == TYPE_CHAR  &&  v->type->is_unsigned) return 1;
    else if (dst == MU2  && v->type->type == TYPE_SHORT &&  v->type->is_unsigned) return 1;
    else if (dst == MU3  && v->type->type == TYPE_INT   &&  v->type->is_unsigned) return 1;
    else if (dst == MU4  && v->type->type == TYPE_LONG  &&  v->type->is_unsigned) return 1;
    else if (dst == RP1  && is_ptr && ptr_size == 1)                              return 1;
    else if (dst == RP2  && is_ptr && ptr_size == 2)                              return 1;
    else if (dst == RP3  && is_ptr && ptr_size == 3)                              return 1;
    else if (dst == RP4  && is_ptr && ptr_size == 4)                              return 1;
    else if (dst == RP5  && is_ptr && ptr_size == 8)                              return 1;
    else return 0;
}

// Return how many bytes a dereferenced pointer takes up
static int value_ptr_target_x86_size(Value *v) {
    if (v->type->type < TYPE_PTR) panic("Expected pointer type");

    if (v->type->type >= TYPE_PTR + TYPE_CHAR && v->type->type <= TYPE_PTR + TYPE_LONG)
        return v->type->type - TYPE_PTR - TYPE_CHAR + 1;
    else if (v->type->type == TYPE_PTR + TYPE_LONG_DOUBLE)
        return 8;
    else
        return 4;
}

// Returns the width in bytes for a non terminal
int make_x86_size_from_non_terminal(int nt) {
         if (nt == CSTV1) return 1;
    else if (nt == CSTV2) return 1;
    else if (nt == CSTV3) return 1;
    else if (nt == CI1)   return 1;
    else if (nt == CI2)   return 2;
    else if (nt == CI3)   return 3;
    else if (nt == CI4)   return 4;
    else if (nt == CU1)   return 1;
    else if (nt == CU2)   return 2;
    else if (nt == CU3)   return 3;
    else if (nt == CU4)   return 4;
    else if (nt == CLD)   return 8;
    else if (nt == RP1 || nt == RP2 || nt == RP3 || nt == RP4 || nt == RP5 || nt == MRP5) return 4;
    else if (nt == RI1 || nt == RU1 || nt == MI1 || nt == MU1                           ) return 1;
    else if (nt == RI2 || nt == RU2 || nt == MI2 || nt == MU2                           ) return 2;
    else if (nt == RI3 || nt == RU3 || nt == MI3 || nt == MU3                           ) return 3;
    else if (nt == RI4 || nt == RU4 || nt == MI4 || nt == MU4                           ) return 4;
    else if (nt == MLD5) return 8;
    else if (nt == LAB) return -1;
    else if (nt == FUN) return -1;
    else if (nt == STL) return 4;
    else if (nt >= AUTO_NON_TERMINAL_START) return -1;
    else
        panic1s("Unable to determine size for %s", non_terminal_string(nt));
}

// Add an x86 instruction to the IR
Tac *add_x86_instruction(X86Operation *x86op, Value *dst, Value *v1, Value *v2) {
    if (v1) make_value_x86_size(v1);
    if (v2) make_value_x86_size(v2);

    Tac *tac = add_instruction(x86op->operation, dst, v1, v2);
    tac->x86_template = x86op->template;

    return tac;
}

void write_rule_coverage_file() {
    void *f = fopen(rule_coverage_file, "a");

    for (int i = 0; i <= rule_coverage->max_value; i++)
        if (rule_coverage->elements[i] == 1) fprintf(f, "%d\n", i);

    fclose(f);
}
