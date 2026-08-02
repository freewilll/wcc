#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

const int non_terminal_sizes[] = {
    0,
#define RULE_NON_TERMINAL_ITEM(non_terminal, size) size,
    RULE_NON_TERMINAL_LIST(RULE_NON_TERMINAL_ITEM)
#undef RULE_NON_TERMINAL_ITEM
};

const int non_terminals_count = sizeof(non_terminal_sizes) / sizeof(int);

char *non_terminal_strings[] = {
    NULL,
#define RULE_NON_TERMINAL_ITEM(non_terminal, size) # non_terminal,
    RULE_NON_TERMINAL_LIST(RULE_NON_TERMINAL_ITEM)
#undef RULE_NON_TERMINAL_ITEM
};

// Add a rule to instr_rules
Rule *add_rule(int dst, int operation, int src1, int src2, int cost) {
    if (instr_rule_count == MAX_RULE_COUNT) panic("Exceeded maximum number of rules %d", MAX_RULE_COUNT);

    Rule *r = &(instr_rules[instr_rule_count]);

    r->index             = instr_rule_count;
    r->operation         = operation;
    r->dst               = dst;
    r->src1              = src1;
    r->src2              = src2;
    r->cost              = cost;
    r->target_operations = 0;

    if (cost == 0 && operation != 0) {
        print_rule(r, 0, 0);
        printf("A zero cost rule cannot have an operation");
    }

    instr_rule_count++;
    return r;
}


TargetOperation *dup_target_operation(TargetOperation *operation) {
    TargetOperation *result = wmalloc(sizeof(TargetOperation));
    *result = *operation;
    result->template = operation->template ? wstrdup(operation->template) : 0;

    return result;
}

void init_rules_by_operation(void) {
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

void free_rules_by_operation(void) {
    longmap_foreach(instr_rules_by_operation, it)
        free_list(longmap_get(instr_rules_by_operation, longmap_iterator_key(&it)));

    free_longmap(instr_rules_by_operation);
}

// Make a textual representation of a non terminal
char *non_terminal_string(int nt) {
    if (nt >= TARGET_NON_TERMINAL_START && nt < TARGET_NON_TERMINAL_END)
        return target_non_terminal_string(nt);
    else if (nt < non_terminals_count)
        return non_terminal_strings[nt];
    else {
        char *buf;
        wasprintf(&buf, "nt%03d", nt);
        return buf;
    }
}

char *value_to_non_terminal_string(Value *v) {
    return non_terminal_string(non_terminal_for_value(v));
}

void print_rule(Rule *r, int print_operations, int indent) {
    fprintf(stderr, "%-24s  %-5s  %-5s  %-5s  %2d    ",
        operation_string(r->operation),
        non_terminal_string(r->dst),
        non_terminal_string(r->src1),
        non_terminal_string(r->src2),
        r->cost
    );

    if (print_operations && r->target_operations) {
        int first = 1;

        for (int i = 0; i < r->target_operation_count; i++) {
            TargetOperation *operation = &r->target_operations[i];

            if (!first) {
                for (int i = 0;i < indent; i++) printf(" ");
                printf("                                                     ");
            }
            first = 0;

            if (operation->save_value_in_slot)
                fprintf(stderr, "special: save arg %d to slot %d\n", operation->arg, operation->save_value_in_slot);
            else if (operation->allocate_stack_index_in_slot)
                fprintf(stderr, "special: allocate stack index of type %d to slot %d\n", operation->allocated_type, operation->allocate_stack_index_in_slot);
            else if (operation->allocate_register_in_slot)
                fprintf(stderr, "special: allocate register of type %d to slot %d\n", operation->allocated_type, operation->allocate_register_in_slot);
            else if (operation->allocate_label_in_slot)
                fprintf(stderr, "special: allocate label to slot %d\n", operation->allocate_label_in_slot);
            else if (operation->template)
                fprintf(stderr, "%s\n", operation->template);
            else
                fprintf(stderr, "\n");

            operation++;
        }
        fprintf(stderr, "\n");
    }
    else
        fprintf(stderr, "\n");
}

void print_rules(void) {
    for (int i = 0; i < instr_rule_count; i++) {
        fprintf(stderr, "%-5d ", i);
        print_rule(&(instr_rules[i]), 1, 6);
    }
}

char *operation_string(int operation) {
    if (operation >= TARGET_OPS_START) {
        return target_op_name(operation);
    }

    switch (operation) {
        case 0:                      return "";
        case IR_MOVE:                return "IR_MOVE";
        case IR_MOVE_PREG_CLASS:     return "IR_MOVE_PREG_CLASS";
        case IR_MOVE_STACK_PTR:      return "IR_MOVE_STACK_PTR";
        case IR_ADDRESS_OF:          return "IR_ADDRESS_OF";
        case IR_INDIRECT:            return "IR_INDIRECT";
        case IR_DECL_LOCAL_COMP_OBJ: return "IR_DECL_LOCAL_COMP_OBJ";
        case IR_LOAD_BIT_FIELD:      return "IR_LOAD_BIT_FIELD";
        case IR_LOAD_FROM_GOT:       return "IR_LOAD_FROM_GOT";
        case IR_ADDRESS_OF_FROM_GOT: return "IR_ADDRESS_OF_FROM_GOT";
        case IR_SAVE_BIT_FIELD:      return "IR_SAVE_BIT_FIELD";
        case IR_START_CALL:          return "IR_START_CALL";
        case IR_ARG:                 return "IR_ARG";
        case IR_ARG_STACK_PADDING:   return "IR_ARG_STACK_PADDING";
        case IR_CALL:                return "IR_CALL";
        case IR_CALL_ARG_REG:        return "IR_CALL_ARG_REG";
        case IR_END_CALL:            return "IR_END_CALL";
        case IR_VA_START:            return "IR_VA_START";
        case IR_VA_ARG:              return "IR_VA_ARG";
        case IR_RETURN:              return "IR_RETURN";
        case IR_ZERO:                return "IR_ZERO";
        case IR_LOAD_LONG_DOUBLE:    return "IR_LOAD_LONG_DOUBLE";
        case IR_START_LOOP:          return "IR_START_LOOP";
        case IR_END_LOOP:            return "IR_END_LOOP";
        case IR_ALLOCATE_STACK:      return "IR_ALLOCATE_STACK";
        case IR_MOVE_TO_PTR:         return "IR_MOVE_TO_PTR";
        case IR_NOP:                 return "IR_NOP";
        case IR_JMP:                 return "IR_JMP";
        case IR_JZ:                  return "IR_JZ";
        case IR_JNZ:                 return "IR_JNZ";
        case IR_ADD:                 return "IR_ADD";
        case IR_ADDC:                return "IR_ADDC";
        case IR_SUB:                 return "IR_SUB";
        case IR_SUBC:                return "IR_SUBC";
        case IR_MUL:                 return "IR_MUL";
        case IR_MUL128A:             return "IR_MUL128A";
        case IR_MUL128B:             return "IR_MUL128B";
        case IR_DIV:                 return "IR_DIV";
        case IR_MOD:                 return "IR_MOD";
        case IR_EQ:                  return "IR_EQ";
        case IR_NE:                  return "IR_NE";
        case IR_BNOT:                return "IR_BNOT";
        case IR_BOR:                 return "IR_BOR";
        case IR_BAND:                return "IR_BAND";
        case IR_XOR:                 return "IR_XOR";
        case IR_BSHL:                return "IR_BSHL";
        case IR_BSHR:                return "IR_BSHR";
        case IR_ASHR:                return "IR_ASHR";
        case IR_BIT_SCAN_FWD:        return "IR_BIT_SCAN_FWD";
        case IR_BIT_SCAN_REV:        return "IR_BIT_SCAN_REV";
        case IR_LT:                  return "IR_LT";
        case IR_GT:                  return "IR_GT";
        case IR_LE:                  return "IR_LE";
        case IR_GE:                  return "IR_GE";
        case IR_PHI_FUNCTION:        return "IR_PHI_FUNCTION";
        default:                     panic("Unknown operation %d", operation);
    }
}

void make_value_target_size(Value *v) {
    // Determine how many bytes a value takes up, if not already done, and write it to
    // v->target_size

    if (v->target_size) return;
    if (v->label) return;
    if (v->type->type == TYPE_STRUCT_OR_UNION) return;
    if (v->type->type == TYPE_ARRAY) return;
    if (v->type->type == TYPE_FUNCTION || v->function_call.function_symbol) return;

    if (!v->type)
        panic("make_value_target_size() got called with a value with no type");

    if (v->is_string_literal)
        v->target_size = 4;
    else if (v->vreg || v->global_symbol || v->stack_index) {
        if (v->type->type == TYPE_PTR)
            v->target_size = 4;
        else if (v->type->type <= TYPE_INT128)
            v->target_size = v->type->type - TYPE_CHAR + 1;
        else if (v->type->type == TYPE_FLOAT)
            v->target_size = 3;
        else if (v->type->type == TYPE_DOUBLE)
            v->target_size = 4;
        else if (v->type->type == TYPE_LONG_DOUBLE)
            v->target_size = 5;
        else
            panic("Illegal type in make_value_target_size() %d", v->type->type);
    }
}

// Match a value type to a non terminal rule type. This is necessary to ensure that
// non-root nodes have matching types while tree matching.
int match_value_type_to_rule_dst(Value *v, int dst) {
    int vnt = non_terminal_for_value(v);
    int is_ptr = v->type->type == TYPE_PTR;

    int ptr_size;
    if (is_ptr) ptr_size = value_ptr_target_target_size(v);
    int is_ptr_to_function = is_pointer_to_function_type(v->type);

    if (dst == vnt) return 1;
    else if (dst >= AUTO_NON_TERMINAL_START) return 1;
    else if (dst >= TARGET_NON_TERMINAL_START && dst < TARGET_NON_TERMINAL_END) return 1;

    else if (dst == CI1  && v->type->type == TYPE_CHAR  && !v->type->is_unsigned) return 1;
    else if (dst == CI2  && v->type->type == TYPE_SHORT && !v->type->is_unsigned) return 1;
    else if (dst == CI3  && v->type->type == TYPE_INT   && !v->type->is_unsigned) return 1;
    else if (dst == CI4  && v->type->type == TYPE_LONG  && !v->type->is_unsigned) return 1;
    else if (dst == CU1  && v->type->type == TYPE_CHAR  &&  v->type->is_unsigned) return 1;
    else if (dst == CU2  && v->type->type == TYPE_SHORT &&  v->type->is_unsigned) return 1;
    else if (dst == CU3  && v->type->type == TYPE_INT   &&  v->type->is_unsigned) return 1;
    else if (dst == CU4  && v->type->type == TYPE_LONG  &&  v->type->is_unsigned) return 1;
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
    else if (dst == MSI1 && v->type->type == TYPE_CHAR  && !v->type->is_unsigned) return 1;
    else if (dst == MSI2 && v->type->type == TYPE_SHORT && !v->type->is_unsigned) return 1;
    else if (dst == MSI3 && v->type->type == TYPE_INT   && !v->type->is_unsigned) return 1;
    else if (dst == MSI4 && v->type->type == TYPE_LONG  && !v->type->is_unsigned) return 1;
    else if (dst == MSU1 && v->type->type == TYPE_CHAR  &&  v->type->is_unsigned) return 1;
    else if (dst == MSU2 && v->type->type == TYPE_SHORT &&  v->type->is_unsigned) return 1;
    else if (dst == MSU3 && v->type->type == TYPE_INT   &&  v->type->is_unsigned) return 1;
    else if (dst == MSU4 && v->type->type == TYPE_LONG  &&  v->type->is_unsigned) return 1;
    else if (dst == MGI1 && v->type->type == TYPE_CHAR  && !v->type->is_unsigned) return 1;
    else if (dst == MGI2 && v->type->type == TYPE_SHORT && !v->type->is_unsigned) return 1;
    else if (dst == MGI3 && v->type->type == TYPE_INT   && !v->type->is_unsigned) return 1;
    else if (dst == MGI4 && v->type->type == TYPE_LONG  && !v->type->is_unsigned) return 1;
    else if (dst == MGU1 && v->type->type == TYPE_CHAR  &&  v->type->is_unsigned) return 1;
    else if (dst == MGU2 && v->type->type == TYPE_SHORT &&  v->type->is_unsigned) return 1;
    else if (dst == MGU3 && v->type->type == TYPE_INT   &&  v->type->is_unsigned) return 1;
    else if (dst == MGU4 && v->type->type == TYPE_LONG  &&  v->type->is_unsigned) return 1;
    else if (dst == RO3  && v->type->type == TYPE_FLOAT)                          return 1;
    else if (dst == RO4  && v->type->type == TYPE_DOUBLE)                         return 1;
    else if (dst == MO3  && v->type->type == TYPE_FLOAT)                          return 1;
    else if (dst == MO4  && v->type->type == TYPE_DOUBLE)                         return 1;
    else if (dst == RP1  && is_ptr && ptr_size == 1)                              return 1;
    else if (dst == RP2  && is_ptr && ptr_size == 2)                              return 1;
    else if (dst == RP3  && is_ptr && ptr_size == 3)                              return 1;
    else if (dst == RP4  && is_ptr && ptr_size == 4)                              return 1;
    else if (dst == RP5  && is_ptr && ptr_size == 8)                              return 1;
    else if (dst == RPF  && is_ptr_to_function)                                   return 1;
    else return 0;
}

// Return how many bytes a dereferenced pointer takes up
int value_ptr_target_target_size(Value *v) {
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
int make_target_size_from_non_terminal(int nt) {
    if (nt < non_terminals_count)
        return non_terminal_sizes[nt];
    else if (nt >= AUTO_NON_TERMINAL_START)
        return -1;
    else if (nt >= TARGET_NON_TERMINAL_START && nt < TARGET_NON_TERMINAL_END)
        return -1;
    else
        panic("Unable to determine size for %s", non_terminal_string(nt));
}

// Add an TargetOperation template to a rule's linked list, making a copy
TargetOperation *add_target_op_to_rule(Rule *r, TargetOperation *target_op) {
    if (!r->target_operation_count)
        r->target_operations = wmalloc(MAX_TARGET_OPS_PER_ROLE * sizeof(TargetOperation));

    if (r->target_operation_count == MAX_TARGET_OPS_PER_ROLE) panic("Exceeded MAX_TARGET_OPS_PER_ROLE");

    int index = r->target_operation_count++;
    r->target_operations[index] = *target_op;
    return &r->target_operations[index];
}

static void dup_target_operations(TargetOperation *target_operations, int target_operation_count, Rule *dst) {
    dst->target_operation_count = target_operation_count;
    if (!target_operation_count) return;

    dst->target_operations = wmalloc(MAX_TARGET_OPS_PER_ROLE * sizeof(TargetOperation));

    for (int i = 0; i < target_operation_count; i++)
        dst->target_operations[i] = *dup_target_operation(&target_operations[i]);

    return;
}

// Given a wildcard operation, make an operation for it
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
// XMS => MSI1, MSU1, similar to XM but for memory in the stack
// XMG => MGI1, MGU1, similar to XM but for memory in globals
static int transform_rule_value(int extend_size, int extend_sign, int v, int size, int is_unsigned) {
    int result = v;

    if (extend_size) {
        switch(v) {
            case XCI:  result = CI1 + size - 1; break;
            case XCU:  result = CU1 + size - 1; break;
            case XRI:  result = RI1 + size - 1; break;
            case XRU:  result = RU1 + size - 1; break;
            case XMI:  result = MI1 + size - 1; break;
            case XMU:  result = MU1 + size - 1; break;
            case XRP:  result = RP1 + size - 1; break;
            case XC:   result = (is_unsigned ? CU1  : CI1)  + size - 1; break;
            case XR:   result = (is_unsigned ? RU1  : RI1)  + size - 1; break;
            case XM:   result = (is_unsigned ? MU1  : MI1)  + size - 1; break;
            case XMS:  result = (is_unsigned ? MSU1 : MSI1) + size - 1; break;
            case XMG:  result = (is_unsigned ? MGU1 : MGI1) + size - 1; break;
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

// Create new rules by expanding type and/or sign in non terminals
// e.g.
// (RP, RI) => (RP1, RI1), (RP2, RI2), (RP3, RI3), (RP4, RI4)
void fin_rule(Rule *r) {
    int operation                = r->operation;
    int dst                      = r->dst;
    int src1                     = r->src1;
    int src2                     = r->src2;
    int cost                     = r->cost;
    int target_operation_count   = r->target_operation_count;

    TargetOperation *target_operations = r->target_operations;

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

            dup_target_operations(target_operations, target_operation_count, new_rule);

            for (int i = 0; i < new_rule->target_operation_count; i++) {
                TargetOperation *x86_operation = &new_rule->target_operations[i];
                x86_operation->template = add_size_to_template(x86_operation->template, size);
            }
        }
    }
}

// Add a save value operation to a rule
void add_save_value(Rule *r, int arg, int slot) {
    TargetOperation *target_op = wcalloc(1, sizeof(TargetOperation));
    target_op->save_value_in_slot = slot;
    target_op->arg = arg;
    add_target_op_to_rule(r, target_op);
}

void add_allocate_stack_index_in_slot(Rule *r, int slot, int type) {
    TargetOperation *target_op = wcalloc(1, sizeof(TargetOperation));
    target_op->allocate_stack_index_in_slot = slot;
    target_op->allocated_type = type;
    add_target_op_to_rule(r, target_op);
}

void add_allocate_register_in_slot(Rule *r, int slot, int type) {
    TargetOperation *target_op = wcalloc(1, sizeof(TargetOperation));
    target_op->allocate_register_in_slot = slot;
    target_op->allocated_type = type;
    add_target_op_to_rule(r, target_op);
}

void add_allocate_label_in_slot(Rule *r, int slot) {
    TargetOperation *target_op = wcalloc(1, sizeof(TargetOperation));
    target_op->allocate_label_in_slot = slot;
    add_target_op_to_rule(r, target_op);
}

void write_rule_coverage_file(void) {
    void *f = fopen(rule_coverage_file, "a");

    for (int i = 0; i <= rule_coverage->max_value; i++)
        if (rule_coverage->elements[i] == 1) fprintf(f, "%d\n", i);

    fclose(f);
}

