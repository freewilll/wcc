#include "wcc.h"
#include "aarch64.h"

int char_is_unsigned_by_default = 1;
int total_function_stack_size_alignment = 16;

char is_32bit_to_aarch64_integer_register_size(int is_32bit) {
    return is_32bit ? 'w' : 'x';
}

char is_32bit_to_aarch64_floating_point_register_size(int is_32bit) {
    return is_32bit ? 's' : 'd';
}

char size_to_aarch64_size(int size) {
    switch (size) {
        case 1:  return 'w'; break;
        case 2:  return 'w'; break;
        case 3:  return 'w'; break;
        case 4:  return 'x'; break;
        default: panic("Unknown size %d", size);
    }
}

char *target_op_name(int operation) {
    switch (operation) {
        case AARCH64_OP_NULL:               return "(null)";
        case AARCH64_OP_STP:                return "stp";
        case AARCH64_OP_LDP:                return "ldp";
        case AARCH64_OP_MOV:                return "mov";
        case AARCH64_OP_MOV_INT_CST:        return "movcst";        // Implemented in codegen
        case AARCH64_OP_ADD:                return "add";
        case AARCH64_OP_ADC:                return "adc";
        case AARCH64_OP_SUB:                return "sub";
        case AARCH64_OP_SBC:                return "sbc";
        case AARCH64_OP_MUL:                return "mul";
        case AARCH64_OP_MUL128A:            return "mul128a";
        case AARCH64_OP_MUL128B:            return "mul128b";
        case AARCH64_OP_DIV:                return "div";
        case AARCH64_OP_ADD_LO12:           return "addlo12";
        case AARCH64_OP_BAND:               return "and";
        case AARCH64_OP_BOR:                return "or";
        case AARCH64_OP_XOR:                return "xor";
        case AARCH64_OP_BNOT:               return "bnot";
        case AARCH64_OP_LSL:                return "lsl";
        case AARCH64_OP_LSR:                return "lsr";
        case AARCH64_OP_ASR:                return "asr";
        case AARCH64_OP_CSET:               return "cset";
        case AARCH64_OP_CMP:                return "cmp";
        case AARCH64_OP_COND_B:             return "cb";
        case AARCH64_OP_B:                  return "b";
        case AARCH64_OP_CALL:               return "call";
        case AARCH64_OP_CALL_FROM_FUNC:     return "callf";         // Used in codegen
        case AARCH64_OP_PUSH_DOUBLE_WORD:   return "pushdw";        // Used in codegen
        case AARCH64_OP_POP_DOUBLE_WORD:    return "popdw";         // Used in codegen
        case AARCH64_OP_ALLOCATE_STACK:     return "allocst";       // Used in codegen
        case AARCH64_OP_DEALLOCATE_STACK:   return "deallocst";     // Used in codegen
        case AARCH64_OP_ADRP:               return "adrp";
        case AARCH64_OP_ADDRESS_OF:         return "address-of";

        default:                        panic("Unknown aarch64 operation %d", operation);
    }
}

void print_target_instruction(void *f, Tac *tac) {
    int o = tac->operation.id;
    switch (tac->operation.id) {
        case AARCH64_OP_CALL:
            fprintf(f, "%-12s", operation_string(o));
            print_value(f, tac->src1, 1);
            if (tac->dst) {
                printf(" -> ");
                print_value(f, tac->dst, 1);
            }
            break;

        case AARCH64_OP_CSET:
            fprintf(f, "%-12s\n", operation_string(o));
            break;

        case AARCH64_OP_BNOT:
        case AARCH64_OP_COND_B:
        case AARCH64_OP_B:
            fprintf(f, "%-12s", operation_string(o));
            print_value(f, tac->dst, 1);
            break;

        case AARCH64_OP_MOV:
        case AARCH64_OP_STP:
        case AARCH64_OP_LDP:
        case AARCH64_OP_MOV_INT_CST:
        case AARCH64_OP_ADD:
        case AARCH64_OP_ADC:
        case AARCH64_OP_SUB:
        case AARCH64_OP_SBC:
        case AARCH64_OP_MUL:
        case AARCH64_OP_MUL128A:
        case AARCH64_OP_MUL128B:
        case AARCH64_OP_DIV:
        case AARCH64_OP_BAND:
        case AARCH64_OP_BOR:
        case AARCH64_OP_XOR:
        case AARCH64_OP_LSL:
        case AARCH64_OP_LSR:
        case AARCH64_OP_ASR:
        case AARCH64_OP_ADRP:
        case AARCH64_OP_ADDRESS_OF:
        case AARCH64_OP_CMP:
            fprintf(f, "%-12s", operation_string(o));
            print_value(f, tac->dst, 1);
            fprintf(f, ", ");
            print_value(f, tac->src1,  1);
            break;

        default:
            panic("print_instruction(): Unknown operation: %d", tac->operation.id);
    }
}

char *target_non_terminal_string(int nt) {
    char *buf = wmalloc(6);

    switch (nt) {
        case CADDSUB: return "caddsub";
        default:
            wasprintf(&buf, "aarch64-nt%03d", nt);
            return buf;
    }
}

void print_physical_register_name_for_lr_reg_index(int preg_reg_index) {
    switch(preg_reg_index) {
        case LIVE_RANGE_PREG_R00: printf("r00"); break;
        case LIVE_RANGE_PREG_R01: printf("r01"); break;
        case LIVE_RANGE_PREG_R02: printf("r02"); break;
        case LIVE_RANGE_PREG_R03: printf("r03"); break;
        case LIVE_RANGE_PREG_R04: printf("r04"); break;
        case LIVE_RANGE_PREG_R05: printf("r05"); break;
        case LIVE_RANGE_PREG_R06: printf("r06"); break;
        case LIVE_RANGE_PREG_R07: printf("r07"); break;
        case LIVE_RANGE_PREG_R09: printf("r09"); break;
        case LIVE_RANGE_PREG_R10: printf("r10"); break;
        case LIVE_RANGE_PREG_R11: printf("r11"); break;
        case LIVE_RANGE_PREG_R12: printf("r12"); break;
        case LIVE_RANGE_PREG_R13: printf("r13"); break;
        case LIVE_RANGE_PREG_R19: printf("r19"); break;
        case LIVE_RANGE_PREG_R20: printf("r20"); break;
        case LIVE_RANGE_PREG_R21: printf("r21"); break;
        case LIVE_RANGE_PREG_R22: printf("r22"); break;
        case LIVE_RANGE_PREG_R23: printf("r23"); break;
        case LIVE_RANGE_PREG_R24: printf("r24"); break;
        case LIVE_RANGE_PREG_R25: printf("r25"); break;
        case LIVE_RANGE_PREG_R26: printf("r26"); break;
        case LIVE_RANGE_PREG_R27: printf("r27"); break;
        case LIVE_RANGE_PREG_R28: printf("r28"); break;
        case LIVE_RANGE_PREG_V00: printf("v00"); break;
        case LIVE_RANGE_PREG_V01: printf("v01"); break;
        case LIVE_RANGE_PREG_V02: printf("v02"); break;
        case LIVE_RANGE_PREG_V03: printf("v03"); break;
        case LIVE_RANGE_PREG_V04: printf("v04"); break;
        case LIVE_RANGE_PREG_V05: printf("v05"); break;
        case LIVE_RANGE_PREG_V06: printf("v06"); break;
        case LIVE_RANGE_PREG_V07: printf("v07"); break;
        case LIVE_RANGE_PREG_V08: printf("v08"); break;
        case LIVE_RANGE_PREG_V09: printf("v09"); break;
        case LIVE_RANGE_PREG_V10: printf("v10"); break;
        case LIVE_RANGE_PREG_V11: printf("v11"); break;
        case LIVE_RANGE_PREG_V12: printf("v12"); break;
        case LIVE_RANGE_PREG_V13: printf("v13"); break;
        case LIVE_RANGE_PREG_V18: printf("v18"); break;
        case LIVE_RANGE_PREG_V19: printf("v19"); break;
        case LIVE_RANGE_PREG_V20: printf("v20"); break;
        case LIVE_RANGE_PREG_V21: printf("v21"); break;
        case LIVE_RANGE_PREG_V22: printf("v22"); break;
        case LIVE_RANGE_PREG_V23: printf("v23"); break;
        case LIVE_RANGE_PREG_V24: printf("v24"); break;
        case LIVE_RANGE_PREG_V25: printf("v25"); break;
        case LIVE_RANGE_PREG_V26: printf("v26"); break;
        case LIVE_RANGE_PREG_V27: printf("v27"); break;
        case LIVE_RANGE_PREG_V28: printf("v28"); break;
        case LIVE_RANGE_PREG_V29: printf("v29"); break;
        case LIVE_RANGE_PREG_V30: printf("v30"); break;
        case LIVE_RANGE_PREG_V31: printf("v31"); break;
        default: printf("Unknown LR preg index %d", preg_reg_index);
    }
}

int get_preg_class_for_scalar_type(Type *type) {
    return (type->type >= TYPE_FLOAT && type->type <= TYPE_LONG_DOUBLE) ? PC_FP : PC_INT;
}

// Returns 1 if a 32-bit or 64-bit value can be encoded as an aarch64 logical immediate.
//
// See:
// https://developer.arm.com/documentation/ddi0487/mb/-Part-C-The-AArch64-Instruction-Set/-Chapter-C3-A64-Instruction-Set-Overview/-C3-5-Data-processing---immediate/-C3-5-3-Logical--immediate-
//
// The Logical (immediate) instructions accept a bitmask immediate value that is a 32-bit pattern or a 64-bit pattern
// viewed as a vector of identical elements of size e = 2, 4, 8, 16, 32 or, 64 bits.
// Each element contains the same sub-pattern, that is a single run of 1 to (e - 1) nonzero bits
// from bit 0 followed by zero bits, then rotated by 0 to (e - 1) bits.
// This mechanism can generate 5334 unique 64-bit patterns as 2667 pairs of pattern and their bitwise inverse.
int is_logical_immediate(unsigned long l, int is_32bit) {
    // All ones or all zeroes cannot be encoded
    if (!l || (!is_32bit && l == -1) || (is_32bit && l == 0xffffffff))
        return 0;

    int repeat_size = is_32bit ? 32 : 64;

    while (repeat_size > 2) {
        repeat_size /= 2;
        unsigned long mask = (1UL << repeat_size) - 1;

        if ((l & mask) != ((l >> repeat_size) & mask)) {
            repeat_size *= 2;
            break;
        }
    }

    unsigned long repeat_value = l;
    if (repeat_size < 64) {
        unsigned long mask = (1UL << repeat_size) - 1;
        repeat_value = l & mask;
    }

    int zeroes_right = __builtin_ctzll(repeat_value);
    int zeroes_left = __builtin_clzll(repeat_value);

    int ok;

    unsigned long right_shifted_repeat_value = repeat_value >> zeroes_right;
    int bit_count = 64 - zeroes_left - zeroes_right;
    if (!bit_count) panic("Unexpected zero bit count");

    // The shifted right binary string must consist entirely bit_count ones,
    // e.g. 000111, 0001, 0011111, but not 0101.
    unsigned long expected_value = (1UL << bit_count) - 1;
    ok = right_shifted_repeat_value == expected_value;

    // The repeat pattern may be something like 110...01
    // Check if there is a middle chunk of consecutive zeroes
    // This can be done by inverting everything and using the same test as above.
    if (!ok && (repeat_value & 1) && (repeat_value & (1UL << (repeat_size - 1)))) {
        unsigned long mask = (1UL << repeat_size) - 1;
        repeat_value = l | ~mask;
        repeat_value = ~repeat_value;

        zeroes_right = __builtin_ctzll(repeat_value);
        zeroes_left = __builtin_clzll(repeat_value);

        unsigned long right_shifted_repeat_value = repeat_value >> zeroes_right;
        int bit_count = 64 - zeroes_left - zeroes_right;
        if (!bit_count) panic("Unexpected zero bit count");

        unsigned long expected_value = (1UL << bit_count) - 1;
        ok = right_shifted_repeat_value == expected_value;
    }

    return ok;
}

// Check if an offset can be encoded as [r, offset] in a ldr or str instruction
int is_ldr_str_immediate_offset(int size, int offset) {
    if (size < 1 || size > 4)
        panic("Unknown size in is_ldr_str_immediate_offset: %d", size);

    if (size == 1                      && offset <= 4095 ) return 1;
    if (size == 2 && (offset & 1) == 0 && offset <= 8190 ) return 1;
    if (size == 3 && (offset & 3) == 0 && offset <= 16380) return 1;
    if (size == 4 && (offset & 7) == 0 && offset <= 32760) return 1;

    return 0;
}

// Take a mov to register instruction with an immediate constant and convert it up into
// 1-3 separate mov* instructions.
Tac *process_integer_constant_move_to_register(Tac *tac) {
    static const int MOVZ = 0; // Set zeroes
    static const int MOVN = 1; // Set ones and negate
    static const int MOVK = 2; // Keep other half words

    // Define all possible templates at compile time to avoid memory allocations
    static char *templates[3][4][2] = {
        "movz %vdw, %v1w" ,        "movz %vdx, %v1x" ,
        "movz %vdw, %v1w, lsl 16", "movz %vdx, %v1x, lsl 16",
        "movz %vdw, %v1w, lsl 32", "movz %vdx, %v1x, lsl 32",
        "movz %vdw, %v1w, lsl 48", "movz %vdx, %v1x, lsl 48",

        "movn %vdw, %v1w" ,        "movn %vdx, %v1x" ,
        "movn %vdw, %v1w, lsl 16", "movn %vdx, %v1x, lsl 16",
        "movn %vdw, %v1w, lsl 32", "movn %vdx, %v1x, lsl 32",
        "movn %vdw, %v1w, lsl 48", "movn %vdx, %v1x, lsl 48",

        "movk %vdw, %v1w" ,        "movk %vdx, %v1x" ,
        "movk %vdw, %v1w, lsl 16", "movk %vdx, %v1x, lsl 16",
        "movk %vdw, %v1w, lsl 32", "movk %vdx, %v1x, lsl 32",
        "movk %vdw, %v1w, lsl 48", "movk %vdx, %v1x, lsl 48",
    };

    Value *dst = tac->dst;
    Value *src1 = dup_value(tac->src1);

    if (!dst->vreg && !dst->preg) panic("Expected a vreg or preg for dst in process_integer_constant_move_to_register()");
    if (!src1->is_constant) panic("Expected a constant for src1 in process_integer_constant_move_to_register()");

    long constant_value = src1->int_value;
    int size_offset = dst->target_size > 3;

    int c[4] = {
        constant_value         & 0xffff,
        (constant_value >> 16) & 0xffff,
        (constant_value >> 32) & 0xffff,
        (constant_value >> 48) & 0xffff,
    };

    int zeroes = (c[0] == 0) + (c[1] == 0);
    int ones = (c[0] == 0xffff) + (c[1] == 0xffff);

    if (dst->target_size > 3) {
        zeroes += (c[2] == 0) + (c[3] == 0);
        ones += (c[2] == 0xffff) + (c[3] == 0xffff);
    }

    tac->operation.id = IR_NOP;
    tac->dst = 0;
    tac->src1 = 0;
    tac->src2 = 0;
    tac->target_template = NULL;

    int base_operation;
    int base_value;
    int negate = 0;

    if (zeroes >= ones) {
        base_operation = MOVZ;
        base_value = 0;
        negate = 0;
    }
    else {
        base_operation = MOVN;
        base_value = 0xffff;
        negate = 1;
    }

    int half_words = dst->target_size > 3 ? 4 : 2;

    int initted = 0;
    int emissions = 0;

    for (int i = 0; i < half_words; i++) {
        if (c[i] == base_value) continue;

        tac = new_tac_after(tac, AARCH64_OP_MOV, dst, dup_value(src1), NULL);
        tac->src1->int_value = c[i];
        if (!initted) {
            if (negate)
                tac->src1->int_value = (~c[i]) & 0xffff;
            else
                tac->src1->int_value = c[i];

            tac->target_template = templates[base_operation][i][size_offset];
            initted = 1;
        }
        else {
            tac->target_template = templates[MOVK][i][size_offset];
            tac->src1->int_value = c[i];
        }

        emissions++;
    }

    if (!emissions) {
        tac = new_tac_after(tac, AARCH64_OP_MOV, dst, src1, NULL);
        tac->src1->int_value = 0;
        tac->target_template = templates[base_operation][0][size_offset];
    }

    return tac;
}

// Check if the constant in src2 be used in an add/sub instruction,
// and if not, insert a register load using a preg scratch register
Tac *insert_constant_load_for_add_sub_using_preg(Tac *tac, int preg) {
    if IS_ADD_SUB_IMMEDIATE(tac->src2->int_value) return tac;

    Value *dst = new_value();
    dst->type = new_type(TYPE_LONG);
    dst->target_size = 4;
    dst->preg = preg;

    Value *v = dup_value(tac->src2);
    tac->src2 = dst;

    tac = new_tac_before(tac, AARCH64_OP_MOV, dst, v, 0, 0);
    tac->target_template = "mov %vdx, %v1x";
    tac = process_integer_constant_move_to_register(tac);

    return tac->next;
}

// Prepend an instruction that loads the address of tac->dst into a pointer
static Tac *load_dst_address_into_pointer(Function *function, Tac *tac) {
    Value *new_dst = new_value();
    new_dst->type = make_pointer(tac->dst->type);
    new_dst->vreg = ++function->vreg_count;
    return new_tac_before(tac, IR_ADDRESS_OF, new_dst, tac->dst, 0, 1);
}

// A pointer to a global symbol has been loaded into a register with AARCH64_OP_ADRP instruction.
// Add an add instruction for the offset
static Tac *insert_load_global_instructions(Tac *tac) {
    Value *offset_value = new_integral_constant(TYPE_LONG, tac->src1->offset);
    offset_value->target_size = 4;
    tac->src1->offset = 0;
    tac = new_tac_after(tac, AARCH64_OP_ADD, tac->dst, tac->dst, offset_value);
    tac->target_template = "add %vdx, %v1x, %v2x";
    tac = insert_constant_load_for_add_sub_using_preg(tac, REG_R16);

    return tac;
}

// A AARCH64_OP_STR instruction with a global in src1 needs to be modified so that the.
// global is replaced with a register that has the address of the global.
static void insert_store_global_instructions(Function *function, Tac *tac) {
    int offset = tac->src1->offset;
    tac->src1->offset = 0;

    Value *r17 = new_value();
    r17->type = new_type(TYPE_LONG);
    r17->target_size = 4;
    r17->preg = REG_R17;

    Tac *tmp = new_tac_before(tac, AARCH64_OP_ADRP, r17, tac->src1, 0, 1);
    tmp->target_template = "adrp %vdx, %v1";

    tac->src1 = r17;

    if (offset) {
        if (is_ldr_str_immediate_offset(tac->src2->target_size, offset)) {
            // str reg, [x17, offset]
            tac->src1 = dup_value(tac->src1);
            tac->src1->offset = offset;
        }
        else {
            // movz  x16, 5000
            // add   x17, x17, x16
            // strb  reg, [x17]
            Value *offset_value = new_integral_constant(TYPE_LONG, offset);
            Tac *tmp = new_tac_before(tac, AARCH64_OP_ADD, r17, r17, offset_value, 1);
            tmp->target_template = "add %vdx, %vdx, %v2x";
            insert_constant_load_for_add_sub_using_preg(tmp, REG_R16); // Use R16 to potentially hold the offset in the addition
        }
    }
}

// At this point, the total function stack size is known and stack offsets have been updated.
// Split instructions with r, [sp, offset] with large offsets so that the offset is loaded separately.
// TODO aarch64: deal with stack offsets for pushed vars in a function call
static void insert_offset_instructions_for_ldr_str_stack_access(Tac *tac) {
    int size = tac->src1->target_size;
    int offset = tac->src1->stack_offset;

    if (offset < 0) panic("Got a negative stack_index: %d", offset);

    if (is_ldr_str_immediate_offset(size, offset)) return;

    // Make a value for the sp register
    Value *sp = new_value();
    sp->type = new_type(TYPE_LONG);
    sp->target_size = 4;
    sp->preg = REG_SP;

    // Make a value for the r17 register
    Value *r17 = new_value();
    r17->type = new_type(TYPE_LONG);
    r17->target_size = 4;
    r17->preg = REG_R17;

    // Make a value for the offset
    Value *offset_value = new_integral_constant(TYPE_LONG, offset);
    offset_value->target_size = 4;

    // Add mov x17, offset and encode the constant if necessary
    Tac *pre_tac = new_tac_before(tac, AARCH64_OP_MOV, r17, offset_value, 0, 1);
    pre_tac->target_template = "mov %vdx, %v1x";
    process_integer_constant_move_to_register(pre_tac);

    // Add add x17, sp, x17
    pre_tac = new_tac_before(tac, AARCH64_OP_ADD, r17, sp, r17, 1);
    pre_tac->target_template = "add %vdx, %v1x, %v2x";

    // Replace [sp + offset] with [r17]
    tac->src1->stack_offset = 0;
    tac->src1->offset = 0;
    tac->src1->preg = REG_R17;
}

void add_load_memory_instructions(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Split ldr r, [sp + offset] with large offsets so that the offset is loaded separately
        if (tac->operation.id == AARCH64_OP_LDR && tac->src1->stack_offset) {
            insert_offset_instructions_for_ldr_str_stack_access(tac);
        }

        // Add an add of the offset if a pointer to global has been loaded into a register
        if (tac->operation.id == AARCH64_OP_ADRP && tac->src1->global_symbol && tac->src1->offset) {
            tac = insert_load_global_instructions(tac);
        }
    }
}

void add_store_memory_instructions(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Split str r, [sp + offset] with large offsets so that the offset is loaded separately
        if (tac->operation.id == AARCH64_OP_STR && tac->src1->stack_offset) {
            insert_offset_instructions_for_ldr_str_stack_access(tac);
        }

        if (tac->operation.id == AARCH64_OP_STR && tac->src1->global_symbol) {
            insert_store_global_instructions(function, tac);
        }
    }
}

// Add code to get the address of variable on the stack,
// with the pseudo operation AARCH64_OP_ADDRESS_OF.
void add_address_of_instructions(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation.id != AARCH64_OP_ADDRESS_OF) continue;

        // Make a value for the sp register
        Value *sp = new_value();
        sp->type = new_type(TYPE_LONG);
        sp->target_size = 4;
        sp->preg = REG_SP;

        // Make a value for the offset
        int offset = tac->src1->stack_offset;

        if (offset) {
            Value *offset_value = new_integral_constant(TYPE_LONG, offset);

            tac->operation.id = AARCH64_OP_ADD;
            tac->src1 = sp;
            tac->src2 = offset_value;
            tac->target_template = "add %vdx, %v1x, %v2x";
            tac = insert_constant_load_for_add_sub_using_preg(tac, REG_R16);
        }
        else {
            tac->operation.id = AARCH64_OP_MOV;
            tac->src1 = sp;
            tac->target_template = "mov %vdx, %v1x";
        }
    }
}

// For each AARCH64_OP_ADRP instruction, append a AARCH64_OP_ADD_LO12 following it using the same register
void expand_adrp_instructions(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation.id == AARCH64_OP_ADRP)
            insert_target_instruction(tac, AARCH64_OP_ADD_LO12, tac->dst, tac->dst, tac->src1, "add %vdx, %v1x, :lo12:%v2");
    }
}

// Adjust AARCH64_OP_LDR instructions that looks like lrd* [preg, offset] with unencodable offset.
//
// Replace the instduction with::
// mov x16, offset   # Constant encoding omitted for brevity
// ldr* foo, [preg, x16]
void expand_indirect_offsets(Function *function) {
    char *templates[] = {"ldrb %vdw, [%v1x, %v2x]", "ldrh %vdw, [%v1x, %v2x]", "ldr %vdw, [%v1x, %v2x]", "ldr %vdx, [%v1x, %v2x]"};

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation.id == AARCH64_OP_LDR && tac->src1->preg != REG_SP && !is_ldr_str_immediate_offset(tac->dst->target_size, tac->src1->offset)) {
            int size = tac->dst->target_size;

            if (size < 1 || size > 4)
                panic("Unknown size in is_ldr_str_immediate_offset: %d", size);

            Value *r16 = new_value();
            r16->type = new_type(TYPE_LONG);
            r16->target_size = 4;
            r16->preg = REG_R16;

            Value *offset_value = new_integral_constant(TYPE_LONG, tac->src1->offset);
            offset_value->target_size = 4;

            Tac *tac2 = new_tac_before(tac, AARCH64_OP_MOV, r16, offset_value, 0, 0);
            tac2->target_template = "mov %vdx, %v1x";
            process_integer_constant_move_to_register(tac2);

            Value *src1_copy = dup_value(tac->src1);
            src1_copy->offset = 0;

            tac->src1 = src1_copy;
            tac->src2 = r16;
            tac->target_template = templates[size - 1];
        }
    }
}

static void remove_self_register_copies(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->dst && tac->dst->preg != -1 && tac->src1 && tac->src1->preg != -1 && tac->dst->preg == tac->src1->preg)
            if (tac->operation.id == AARCH64_OP_MOV && !tac->operation.is_convert_move) tac->operation.id = IR_NOP;
}


void perform_peephole_optimization(Function *function) {
    // remove_stack_self_moves(function); // TODO aarch64
    remove_self_register_copies(function);
}

// This removes instructions that copy a register to itself by replacing them with noops.
void remove_vreg_self_moves(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation.id == AARCH64_OP_MOV && tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            tac->operation.id = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->src2 = 0;
            tac->target_template = 0;
        }
    }
}
