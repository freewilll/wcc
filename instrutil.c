#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static int non_terminal_for_value(Value *v);
static int value_ptr_target_x86_size(Value *v);

Rule *add_rule(int dst, int operation, int src1, int src2, int cost) {
    if (instr_rule_count == MAX_RULE_COUNT) panic("Exceeded maximum number of rules %d", MAX_RULE_COUNT);

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
        switch(v) {
            case XCI: result = CI1 + size - 1; break;
            case XCU: result = CU1 + size - 1; break;
            case XRI: result = RI1 + size - 1; break;
            case XRU: result = RU1 + size - 1; break;
            case XMI: result = MI1 + size - 1; break;
            case XMU: result = MU1 + size - 1; break;
            case XRP: result = RP1 + size - 1; break;
            case XC:  result = (is_unsigned ? CU1 : CI1) + size - 1; break;
            case XR:  result = (is_unsigned ? RU1 : RI1) + size - 1; break;
            case XM:  result = (is_unsigned ? MU1 : MI1) + size - 1; break;
        }
    }
    else if (extend_sign) {
        switch (v) {
            case XC1: result = (is_unsigned ? CU1 : CI1); break;
            case XC2: result = (is_unsigned ? CU2 : CI2); break;
            case XC3: result = (is_unsigned ? CU3 : CI3); break;
            case XC4: result = (is_unsigned ? CU4 : CI4); break;
            case XR1: result = (is_unsigned ? RU1 : RI1); break;
            case XR2: result = (is_unsigned ? RU2 : RI2); break;
            case XR3: result = (is_unsigned ? RU3 : RI3); break;
            case XR4: result = (is_unsigned ? RU4 : RI4); break;
            case XM1: result = (is_unsigned ? MU1 : MI1); break;
            case XM2: result = (is_unsigned ? MU2 : MI2); break;
            case XM3: result = (is_unsigned ? MU3 : MI3); break;
            case XM4: result = (is_unsigned ? MU4 : MI4); break;
        }
    }

    return result;
}

X86Operation *dup_x86_operation(X86Operation *operation) {
    X86Operation *result = wmalloc(sizeof(X86Operation));
    *result = *operation;
    result->template = operation->template ? strdup(operation->template) : 0;
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
    switch (size) {
        case 1:  return 'b'; break;
        case 2:  return 'w'; break;
        case 3:  return 'l'; break;
        case 4:  return 'q'; break;
        default: panic("Unknown size %d", size);
    }
}

static char *add_size_to_template(char *template, int size) {
    if (!template) return 0; // Some magic operations have no templates but are implemented in codegen.

    char x86_size = size_to_x86_size(size);
    char *result = wcalloc(1, 128);
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
    X86Operation *x86op = wmalloc(sizeof(X86Operation));
    x86op->operation = operation;

    x86op->dst = dst;
    x86op->v1 = v1;
    x86op->v2 = v2;

    x86op->template = template;
    x86op->save_value_in_slot = 0;
    x86op->allocate_stack_index_in_slot = 0;
    x86op->allocate_register_in_slot = 0;
    x86op->allocate_label_in_slot = 0;
    x86op->allocated_type = 0;
    x86op->arg = 0;

    x86op->next = 0;

    add_x86_op_to_rule(r, x86op);

    return x86op;
}

// Add a save value operation to a rule
void add_save_value(Rule *r, int arg, int slot) {
    X86Operation *x86op = wcalloc(1, sizeof(X86Operation));
    x86op->save_value_in_slot = slot;
    x86op->arg = arg;
    add_x86_op_to_rule(r, x86op);
}

void add_allocate_stack_index_in_slot(Rule *r, int slot, int type) {
    X86Operation *x86op = wcalloc(1, sizeof(X86Operation));
    x86op->allocate_stack_index_in_slot = slot;
    x86op->allocated_type = type;
    add_x86_op_to_rule(r, x86op);
}

void add_allocate_register_in_slot(Rule *r, int slot, int type) {
    X86Operation *x86op = wcalloc(1, sizeof(X86Operation));
    x86op->allocate_register_in_slot = slot;
    x86op->allocated_type = type;
    add_x86_op_to_rule(r, x86op);
}

void add_allocate_label_in_slot(Rule *r, int slot) {
    X86Operation *x86op = wcalloc(1, sizeof(X86Operation));
    x86op->allocate_label_in_slot = slot;
    add_x86_op_to_rule(r, x86op);
}

static void make_rule_hash(int i) {
    Rule *r = &(instr_rules[i]);

    r->hash =
        ((long) r->dst       <<  0) +
        ((long) r->src1      <<  9) +
        ((long) r->src2      << 18) +
        ((long) r->operation << 27);
}

void make_rules_by_operation(void) {
    instr_rules_by_operation = new_longmap();

    for (int i = 0; i < instr_rule_count; i++) {
        Rule *r = &(instr_rules[i]);
        List *list = longmap_get(instr_rules_by_operation, r->operation);
        if (!list) {
            list = new_list(128);
            longmap_put(instr_rules_by_operation, r->operation, list);
        }
        append_to_list(list, r);
    }
}

void check_for_duplicate_rules(void) {
    LongMap *map = new_longmap();

    int duplicates = 0;
    for (int i = 0; i < instr_rule_count; i++) {
        make_rule_hash(i);
        Rule *other_rule = longmap_get(map, instr_rules[i].hash);
        if (other_rule) {
            printf("Duplicate rules: %d\n", i);
            print_rule(&(instr_rules[i]), 1, 0);
            print_rule(other_rule, 1, 0);
            printf("\n");
            duplicates++;
        }
        longmap_put(map, instr_rules[i].hash, &(instr_rules[i]));
    }

    if (duplicates) {
        printf("There are %d duplicated rules\n", duplicates);
        exit(1);
    }
}

// Make a textual representation of a non terminal
char *non_terminal_string(int nt) {
    char *buf = wmalloc(6);

    switch (nt) {
        case 0:     return "";
        case CSTV1: return "cstv1";
        case CSTV2: return "cstv2";
        case CSTV3: return "cstv3";
        case XC:    return "xc";
        case XC1:   return "xc1";
        case XC2:   return "xc2";
        case XC3:   return "xc3";
        case XC4:   return "xc4";
        case XCI:   return "xci";
        case CI1:   return "ci1";
        case CI2:   return "ci2";
        case CI3:   return "ci3";
        case CI4:   return "ci4";
        case XCU:   return "xcu";
        case CU1:   return "cu1";
        case CU2:   return "cu2";
        case CU3:   return "cu3";
        case CU4:   return "cu4";
        case CLD:   return "cld";
        case CS3:   return "cs3";
        case CS4:   return "cs4";
        case STL:   return "stl";
        case LAB:   return "lab";
        case FUN:   return "fun";
        case XR:    return "xr";
        case XR1:   return "xr1";
        case XR2:   return "xr2";
        case XR3:   return "xr3";
        case XR4:   return "xr4";
        case XRI:   return "xri";
        case RI1:   return "ri1";
        case RI2:   return "ri2";
        case RI3:   return "ri3";
        case RI4:   return "ri4";
        case XRU:   return "xru";
        case RU1:   return "ru1";
        case RU2:   return "ru2";
        case RU3:   return "ru3";
        case RU4:   return "ru4";
        case XM:    return "xm";
        case XM1:   return "xm1";
        case XM2:   return "xm2";
        case XM3:   return "xm3";
        case XM4:   return "xm4";
        case MI1:   return "mi1";
        case MI2:   return "mi2";
        case MI3:   return "mi3";
        case MI4:   return "mi4";
        case MU1:   return "mu1";
        case MU2:   return "mu2";
        case MU3:   return "mu3";
        case MU4:   return "mu4";
        case RP1:   return "rp1";
        case RP2:   return "rp2";
        case RP3:   return "rp3";
        case RP4:   return "rp4";
        case RP5:   return "rp5";
        case RPF:   return "rpf";
        case MPF:   return "mpf";
        case RS3:   return "rs3";
        case RS4:   return "rs4";
        case MLD5:  return "mld5";
        case MS3:   return "ms3";
        case MS4:   return "ms4";
        case MPV:   return "mpv";
        case MSA:   return "msa";
        default:
            wasprintf(&buf, "nt%03d", nt);
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
            else if (operation->allocate_label_in_slot)
                printf("special: allocate label to slot %d\n", operation->allocate_label_in_slot);
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

void print_rules(void) {
    for (int i = 0; i < instr_rule_count; i++) {
        printf("%-5d ", i);
        print_rule(&(instr_rules[i]), 1, 6);
    }
}

void make_value_x86_size(Value *v) {
    // Determine how many bytes a value takes up, if not already done, and write it to
    // v->86_size

    if (v->x86_size) return;
    if (v->label) return;
    if (v->type->type == TYPE_STRUCT_OR_UNION) return;
    if (v->type->type == TYPE_ARRAY) return;
    if (v->type->type == TYPE_FUNCTION || v->function_symbol) return;

    if (!v->type) {
        print_value(stdout, v, 0);
        printf("\n");
        panic("make_value_x86_size() got called with a value with no type");
    }

    if (v->is_string_literal)
        v->x86_size = 4;
    else if (v->vreg || v->global_symbol || v->stack_index) {
        if (v->type->type == TYPE_PTR)
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
            panic("Illegal type in make_value_x86_size() %d", v->type->type);
    }
}

static int non_terminal_for_value(Value *v) {
    int result;

    if (!v->x86_size) make_value_x86_size(v);
    if (v->non_terminal) return v->non_terminal;

    int is_local = !v->global_symbol && !v->stack_index;
    int is_pointer = v->type && v->type->type == TYPE_PTR;

         if (v->is_string_literal)                                            result =  STL;
    else if (v->label)                                                        result =  LAB;
    else if (v->type->type == TYPE_FUNCTION)                                  result =  FUN;
    else if (is_local  && is_pointer_to_function_type(v->type))               result =  RPF;
    else if (!is_local && is_pointer_to_function_type(v->type))               result =  MPF;
    else if (v->type->type == TYPE_STRUCT_OR_UNION)                           result =  MSA;
    else if (v->type->type == TYPE_ARRAY)                                     result =  MSA;

    // Pointers
    else if (is_local  && is_pointer && v->type->target->type == TYPE_FLOAT)       result =  RP3;
    else if (is_local  && is_pointer && v->type->target->type == TYPE_DOUBLE)      result =  RP4;
    else if (is_local  && is_pointer && v->type->target->type == TYPE_LONG_DOUBLE) result =  RP5;

    else if (!is_local && is_pointer)                                         result =  MPV;
    else if (is_local  && is_pointer)                                         result =  RP1 + value_ptr_target_x86_size(v) - 1;

    // Lvalue in register
    else if (v->is_lvalue_in_register)                                        result =  RP1 + v->x86_size - 1;

    // Floats, doubles & long doubles
    else if (!is_local && v->type->type == TYPE_FLOAT)                        result =  MS3;
    else if (is_local  && v->type->type == TYPE_FLOAT)                        result =  RS3;
    else if (!is_local && v->type->type == TYPE_DOUBLE)                       result =  MS4;
    else if (is_local  && v->type->type == TYPE_DOUBLE)                       result =  RS4;
    else if (!is_local && v->type->type == TYPE_LONG_DOUBLE)                  result =  MLD5;

    // Integers
    else if (!is_local && !v->type->is_unsigned)                              result =  MI1 + v->x86_size - 1;
    else if (is_local  && !v->type->is_unsigned)                              result =  RI1 + v->x86_size - 1;
    else if (!is_local && v->type->is_unsigned)                               result =  MU1 + v->x86_size - 1;
    else if (is_local  &&  v->type->is_unsigned)                              result =  RU1 + v->x86_size - 1;

    else {
        print_value(stdout, v, 0);
        panic("\n^ Bad value in non_terminal_for_value()");
    }

    v->non_terminal = result;

    return result;
}

// Used to match a value to a leaf node (operation = 0) src rule
// There are a couple of possible matches for a constant.
int match_value_to_rule_src(Value *v, int src) {
    if (v->is_constant) {
        int vtt = v->type->type;

        if (vtt == TYPE_LONG_DOUBLE)
            return src == CLD;
        else if (vtt == TYPE_FLOAT)
            return src == CS3;
        else if (vtt == TYPE_DOUBLE)
            return src == CS4;
        else {
            // Integer constant

            // Match 1, 2 and 3
                 if (src == CSTV1 && v->int_value == 1) return 1;
            else if (src == CSTV2 && v->int_value == 2) return 1;
            else if (src == CSTV3 && v->int_value == 3) return 1;

            // Check match with type from the parser. This is necessary for evil casts, e.g.
            // (unsigned int) -1, which would otherwise become a CU4 and not match rules for CU3.
                 if (src >= CI3 && src <= CI4 && !v->type->is_unsigned && vtt != TYPE_LONG)  return 1;
            else if (              src == CI4 && !v->type->is_unsigned)                      return 1;
                 if (src >= CU3 && src <= CU4 &&  v->type->is_unsigned && vtt != TYPE_LONG)  return 1;
            else if (              src == CU4 &&  v->type->is_unsigned)                      return 1;

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
// non-root nodes have matching types while tree matching.
int match_value_type_to_rule_dst(Value *v, int dst) {
    int vnt = non_terminal_for_value(v);
    int is_ptr = v->type->type == TYPE_PTR;

    int ptr_size;
    if (is_ptr) ptr_size = value_ptr_target_x86_size(v);
    int is_ptr_to_function = is_pointer_to_function_type(v->type);

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
    else if (dst == RS3  && v->type->type == TYPE_FLOAT)                          return 1;
    else if (dst == RS4  && v->type->type == TYPE_DOUBLE)                         return 1;
    else if (dst == MS3  && v->type->type == TYPE_FLOAT)                          return 1;
    else if (dst == MS4  && v->type->type == TYPE_DOUBLE)                         return 1;
    else if (dst == RP1  && is_ptr && ptr_size == 1)                              return 1;
    else if (dst == RP2  && is_ptr && ptr_size == 2)                              return 1;
    else if (dst == RP3  && is_ptr && ptr_size == 3)                              return 1;
    else if (dst == RP4  && is_ptr && ptr_size == 4)                              return 1;
    else if (dst == RP5  && is_ptr && ptr_size == 8)                              return 1;
    else if (dst == RPF  && is_ptr_to_function)                                   return 1;
    else return 0;
}

// Return how many bytes a dereferenced pointer takes up
static int value_ptr_target_x86_size(Value *v) {
    if (v->type->type != TYPE_PTR) panic("Expected pointer type");

    int target_type = v->type->target->type;

    if (target_type >= TYPE_CHAR && target_type <= TYPE_LONG)
        return target_type - TYPE_CHAR + 1;
    else if (target_type == TYPE_FLOAT)
        return 3;
    else if (target_type == TYPE_DOUBLE)
        return 4;
    else if (target_type == TYPE_LONG_DOUBLE)
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
    else if (nt == CS3)   return 3;
    else if (nt == CS4)   return 4;
    else if (nt == RP1 || nt == RP2 || nt == RP3 || nt == RP4 || nt == RP5 || nt == MPV ) return 4;
    else if (nt == RI1 || nt == RU1 || nt == MI1 || nt == MU1                           ) return 1;
    else if (nt == RI2 || nt == RU2 || nt == MI2 || nt == MU2                           ) return 2;
    else if (nt == RI3 || nt == RU3 || nt == RS3 || nt == MI3 || nt == MU3 || nt == MS3 ) return 3;
    else if (nt == RI4 || nt == RU4 || nt == RS4 || nt == MI4 || nt == MU4 || nt == MS4 ) return 4;
    else if (nt == RPF) return 4;
    else if (nt == MPF) return 4;
    else if (nt == MLD5) return 8;
    else if (nt == LAB) return -1;
    else if (nt == FUN) return -1;
    else if (nt == MSA) return -1;
    else if (nt == STL) return 4;
    else if (nt >= AUTO_NON_TERMINAL_START) return -1;
    else
        panic("Unable to determine size for %s", non_terminal_string(nt));
}

// Add an x86 instruction to the IR
Tac *add_x86_instruction(X86Operation *x86op, Value *dst, Value *v1, Value *v2) {
    if (v1) make_value_x86_size(v1);
    if (v2) make_value_x86_size(v2);

    Tac *tac = add_instruction(x86op->operation, dst, v1, v2);
    tac->x86_template = x86op->template;

    return tac;
}

void write_rule_coverage_file(void) {
    void *f = fopen(rule_coverage_file, "a");

    for (int i = 0; i <= rule_coverage->max_value; i++)
        if (rule_coverage->elements[i] == 1) fprintf(f, "%d\n", i);

    fclose(f);
}
