#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static int value_ptr_target_x86_size(Value *v);

X86Operation *dup_x86_operation(X86Operation *operation) {
    X86Operation *result = wmalloc(sizeof(X86Operation));
    *result = *operation;
    result->template = operation->template ? wstrdup(operation->template) : 0;

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
        int first = 1;

        for (int i = 0; i < r->x86_operation_count; i++) {
            X86Operation *operation = &r->x86_operations[i];

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

            operation++;
        }
        printf("\n");
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

char *operation_string(int operation) {
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
        case IR_SUB:                 return "IR_SUB";
        case IR_RSUB:                return "IR_RSUB";
        case IR_MUL:                 return "IR_MUL";
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
        case IR_LT:                  return "IR_LT";
        case IR_GT:                  return "IR_GT";
        case IR_LE:                  return "IR_LE";
        case IR_GE:                  return "IR_GE";
        case IR_PHI_FUNCTION:        return "IR_PHI_FUNCTION";
        case X_MOV:                  return "mov";
        case X_ADD:                  return "add";
        case X_MUL:                  return "mul";
        case X_IDIV:                 return "idiv";
        case X_CQTO:                 return "cqto";
        case X_CMP:                  return "cmp";
        case X_COMIS:                return "comis";
        case X_TEST:                 return "test";
        case X_CMPZ:                 return "cmpz";
        case X_JMP:                  return "jmp";
        case X_JZ:                   return "jz";
        case X_JNZ:                  return "jnz";
        case X_JE:                   return "je";
        case X_JNE:                  return "jne";
        case X_JLT:                  return "jlt";
        case X_JGT:                  return "jgt";
        case X_JLE:                  return "jle";
        case X_JGE:                  return "jge";
        case X_JB:                   return "jb";
        case X_JA:                   return "ja";
        case X_JBE:                  return "jbe";
        case X_JAE:                  return "jae";
        case X_SETE:                 return "sete";
        case X_SETNE:                return "setne";
        case X_SETP:                 return "setp";
        case X_SETNP:                return "setnp";
        case X_SETLT:                return "setlt";
        case X_SETGT:                return "setgt";
        case X_SETLE:                return "setle";
        case X_SETGE:                return "setge";
        case X_MOVS:                 return "movs";
        case X_MOVZ:                 return "movz";
        case X_MOVC:                 return "movc";
        default:                     panic("Unknown x86 operation %d", operation);
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

    if (!v->type)
        panic("make_value_x86_size() got called with a value with no type");

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

int uncached_non_terminal_for_value(Value *v) {
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

    else
        panic("\n^ Bad value in non_terminal_for_value()");

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

void write_rule_coverage_file(void) {
    void *f = fopen(rule_coverage_file, "a");

    for (int i = 0; i <= rule_coverage->max_value; i++)
        if (rule_coverage->elements[i] == 1) fprintf(f, "%d\n", i);

    fclose(f);
}

