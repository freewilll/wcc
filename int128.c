#include "assert.h"
#include "wcc.h"

// A 128-bit value that has been split into low and high values
typedef struct split_value {
    Value *low;
    Value *high;
} SplitValue;

#define VALUE_IS_INT128(value) (value && value->type && value->type->type == TYPE_INT128)
#define VALUE_IS_INT128_VREG(value) (value && value->vreg && value->type && value->type->type  == TYPE_INT128)
#define TAC_IS_INT128(tac) (VALUE_IS_INT128(tac->dst) || VALUE_IS_INT128(tac->src1) || VALUE_IS_INT128(tac->src2))

// Lots of things have not been implemented. Print the failing instruction and fail hard.
static void bail_on_unimplemented_instruction(Tac *tac, const char *message) {
    fprintf(stderr, "%s", message);
    print_instruction(stderr, tac, 0);
    panic("bailing");
}

// Allocate a new vreg
static int new_vreg(Function *function) {
    function->vreg_count++;
    if (function->vreg_count >= MAX_VREG_COUNT) panic_with_line_number("Exceeded max vreg count %d", MAX_VREG_COUNT);
    return function->vreg_count;
}

// Allocate a new vreg of type long
static Value *new_long_vreg(Function *function) {
    Value *value = new_value();
    value->type = new_type(TYPE_LONG);
    value->vreg = new_vreg(function);
    return value;
}

// Allocate a new vreg of type long or unsigned long using the signess from a reference value
static Value *new_long_vreg_from_value(Function *function, Value *reference_value) {
    Value *value = new_value();
    value->type = dup_type(reference_value->type);
    value->type->type = TYPE_LONG;
    value->vreg = new_vreg(function);
    return value;
}

// Allocate 2 vregs and create a new SplitVreg
static SplitVreg* new_split_vreg(Function *function) {
    SplitVreg *split_vreg = wmalloc(sizeof(SplitVreg));

    split_vreg->low = new_vreg(function);
    split_vreg->high = new_vreg(function);

    return split_vreg;
}

// Split one 128-bit vreg into two, if not already done and add to the map
static int map_one_vreg(Function *function, int vreg) {
    if (function->int128_register_mappings[vreg]) return 0;

    SplitVreg *split_vreg = new_split_vreg(function);
    function->int128_register_mappings[vreg] = split_vreg;
    if (debug_int128)
        printf("  int128 vreg %3d -> %3d / %3d\n", vreg, split_vreg->low, split_vreg->high);

    return 1;
}

// For each 128-bit regsiter of type INT_128, allocate two registers and add to the map
static int map_int128_registers(Function *function) {
    if (debug_int128) printf("Mapping vregs....\n");

    function->int128_register_mappings_count = function->vreg_count;
    // Allocate enough space in the worst case of all existing vregs needing a split.
    function->int128_register_mappings = wcalloc(function->int128_register_mappings_count + 1, sizeof(SplitVreg *));

    int count = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (VALUE_IS_INT128_VREG(tac->dst )) count += map_one_vreg(function, tac->dst->vreg);
        if (VALUE_IS_INT128_VREG(tac->src1)) count += map_one_vreg(function, tac->src1->vreg);
        if (VALUE_IS_INT128_VREG(tac->src2)) count += map_one_vreg(function, tac->src2->vreg);
    }

    return count;
}

// Split a 128-bit value into two 64-bit long values
static SplitValue split_value(Function *function, Value *value) {
    SplitValue split_value;

    if (value->vreg) {
        if (value->type->type != TYPE_INT128) panic("Expected an int128 register in split_value()");

        SplitVreg *split_vreg = function->int128_register_mappings[value->vreg];
        if (!split_vreg) panic("Expected int128 register in split_value()");

        split_value.low = dup_value(value);
        split_value.high = dup_value(value);
        split_value.low->vreg = split_vreg->low;
        split_value.high->vreg = split_vreg->high;
        split_value.low->type->type = TYPE_LONG;
        split_value.high->type->type = TYPE_LONG;

        return split_value;
    }

    else if (value->stack_index) {
        split_value.low = dup_value(value);
        split_value.high = dup_value(value);
        split_value.high->offset += 8;
        split_value.low->type->type = TYPE_LONG;
        split_value.high->type->type = TYPE_LONG;
    }

    else if (value->is_constant) {
        // Discard the upper bits, but sign extend if necessary
        long high_value = value->type->is_unsigned
            ? 0
            : value->int_value < 0 ? -1 : 0;
        split_value.low = new_integral_constant(TYPE_LONG, value->int_value);
        split_value.high = new_integral_constant(TYPE_LONG, high_value);

        if (value->type->is_unsigned) {
            split_value.low->type->is_unsigned = 1;
            split_value.high->type->is_unsigned = 1;
        }
    }

    else {
        fprintf(stderr, "split_value for non-vreg value:");
        print_value(stderr, value, 0);
        fprintf(stderr, "\n");
        panic("bailing");
    }

    return split_value;
}

// Split a 128-bit instruction into two identical instructions,
// the first with the low bits and the second with the high bits.
static Tac *split_instruction(Function *function, Tac *tac) {
    SplitValue tmp_dst, tmp_src1, tmp_src2;

    SplitValue *split_dst =  tac->dst  ? (tmp_dst  = split_value(function, tac->dst),  &tmp_dst)  : NULL;
    SplitValue *split_src1 = tac->src1 ? (tmp_src1 = split_value(function, tac->src1), &tmp_src1) : NULL;
    SplitValue *split_src2 = tac->src2 ? (tmp_src2 = split_value(function, tac->src2), &tmp_src2) : NULL;

    assign_values_to_instruction(tac,
        split_dst  ? split_dst->low  : NULL,
        split_src1 ? split_src1->low : NULL,
        split_src2 ? split_src2->low : NULL);

    Tac *tac2 = new_instruction_with_values(tac->operation.id,
        split_dst  ? split_dst->high  : NULL,
        split_src1 ? split_src1->high : NULL,
        split_src2 ? split_src2->high : NULL);

    insert_tac_after(tac, tac2);

    return tac2;
}

// Convert an non-128 bit integer in a register to an int128 in a register
static Tac *transform_convert_int_to_int128(Function *function, Tac *tac) {
    Tac *top_tac = tac;

    SplitValue split_dst = split_value(function, tac->dst);

    // Upgrade the value size to a long if necessary
    Value *long_value;
    if (tac->src1->type->type < TYPE_LONG) {
        long_value = new_long_vreg_from_value(function, tac->src1);
        long_value->type->is_unsigned = tac->src1->type->is_unsigned;
        tac = new_tac_after(tac, IR_MOVE, long_value, tac->src1, NULL);
    }
    else {
        long_value = top_tac->src1;
    }

    // Copy the low bits
    tac = new_tac_after(tac, IR_MOVE, split_dst.low, long_value, NULL);

    // Set the high bits to all zeroes or all ones
    if (top_tac->src1->type->is_unsigned) {
        // Set the high bits to zero
        tac = new_tac_after(tac, IR_MOVE, split_dst.high, new_unsigned_integral_constant(TYPE_LONG, 0), NULL);
    } else {
        // Sign extend the high bits by copying the low bits over
        // and then doing an arithmetic shift right of 31 bits.
        Value *sign_bits = new_long_vreg(function);
        tac = new_tac_after(tac, IR_MOVE, sign_bits, long_value, NULL);
        tac = new_tac_after(tac, IR_ASHR, split_dst.high, sign_bits, new_integral_constant(TYPE_INT, 63));
    }

    make_instruction_a_nop(top_tac);

    return tac;
}

// Convert an int128 in a register to a non-128 bit integer in a register
static Tac *transform_convert_int128_to_int(Function *function, Tac *tac) {
    SplitVreg *src1_vregs = function->int128_register_mappings[tac->src1->vreg];
    assert(src1_vregs);

    // Discard the high bits
    tac->src1 = dup_value(tac->src1);
    tac->src1->vreg = src1_vregs->low;
    tac->src1->type->type = TYPE_LONG;

    return tac;
}

static Tac *transform_move(Function *function, Tac *tac) {
    if (tac->dst->vreg && tac->src1->is_constant) {
        // Assign an int or long constant to an int128 in a register
        return split_instruction(function, tac);
    }

    else if (tac->dst->stack_index && tac->src1->is_constant) {
        // Assign an int or long constant to an int128 in the stack
        return split_instruction(function, tac);
    }

    else if (VALUE_IS_INT128_VREG(tac->dst) && tac->src1->vreg && is_non_128_bit_integer_type(tac->src1->type)) {
        // Convert an non-128 bit integer in a register to an int128 in a register
        return transform_convert_int_to_int128(function, tac);
    }

    // vreg = vreg
    else if (tac->dst->vreg && tac->src1->vreg) {
        if (is_non_128_bit_integer_type(tac->dst->type)) {
            // Convert an int128 in a register to a non-128 bit integer in a register
            return transform_convert_int128_to_int(function, tac);
        }

        else if (tac->dst->type->type != TYPE_INT128 || tac->src1->type->type != TYPE_INT128)
            bail_on_unimplemented_instruction(tac, "int128 type conversion not implemented for:");

        // Move a 128-bit vreg to a 128-bit vreg
        return split_instruction(function, tac);
    }

    // vreg = stack
    else if (tac->dst->vreg && tac->src1->stack_index) {
        if (is_non_128_bit_integer_type(tac->dst->type))
            panic("int128: Should not get here, a stack int128 is loaded into a register first");

        else if (tac->dst->type->type != TYPE_INT128 || tac->src1->type->type != TYPE_INT128)
            bail_on_unimplemented_instruction(tac, "int128 type conversion not implemented for:");

        // Move a 128-bit value in the stack to a 128-bit vreg
        return split_instruction(function, tac);
    }

    // stack = vreg
    else if (tac->dst->stack_index && tac->src1->vreg) {
        if (is_non_128_bit_integer_type(tac->dst->type))
            panic("int128: Should not get here, a stack int128 is loaded into split registers first");

        else if (tac->dst->type->type != TYPE_INT128 || tac->src1->type->type != TYPE_INT128)
            bail_on_unimplemented_instruction(tac, "int128 type conversion not implemented for:");

        // Move a 128-bit value in a vreg to a 128-bit value in the stack
        return split_instruction(function, tac);
    }

    bail_on_unimplemented_instruction(tac, "int128 IR_MOVE not implemented for:");

    return tac;
}

// An address of operation for an int128 is simply a case of pretending it's a long.
static Tac *transform_address_of(Function *function, Tac *tac) {
    tac->dst = dup_value(tac->dst);
    tac->dst->type->target->type = TYPE_LONG;
    tac->src1 = dup_value(tac->src1);
    tac->src1->type->type = TYPE_LONG;

    return tac;
}

// An indirect is done with two indirect instructions, one for the low value and one for the high one.
static Tac *transform_indirect(Function *function, Tac *tac) {
    SplitValue split_dst = split_value(function, tac->dst);

    Value *src1_low = tac->src1;
    Value *src1_high = dup_value(src1_low);
    src1_high->offset += 8;

    // Nuke the current TAC for convenience
    make_instruction_a_nop(tac);

    tac = new_tac_after(tac, IR_INDIRECT, split_dst.low, src1_low, 0);
    tac = new_tac_after(tac, IR_INDIRECT, split_dst.high, src1_high, 0);

    return tac;
}

// Generic function to make an IR with left or right bitshifts, arithmetic and binary.
static Tac *transform_bitshift(Function *function, Tac *tac, int operation) {
    int is_left = operation == IR_BSHL;

    int opposite_operation;
    switch (operation) {
        case IR_BSHL:
            opposite_operation = IR_BSHR;
            break;

        case IR_ASHR:
        case IR_BSHR:
            opposite_operation = IR_BSHL;
            break;

        default:
            panic("Unhandled bitshift operation");
    }

    // Handle a bit shift from a constant or register by a constant amount
    if (VALUE_IS_INT128_VREG(tac->dst) && (tac->src1->is_constant || VALUE_IS_INT128_VREG(tac->src1)) && tac->src2->is_constant) {
        SplitValue split_dst = split_value(function, tac->dst);
        SplitValue split_src1 = split_value(function, tac->src1);

        Value *start_dst_value  = is_left ? split_dst.low   : split_dst.high;
        Value *end_dst_value    = is_left ? split_dst.high  : split_dst.low;
        Value *start_src1_value = is_left ? split_src1.low  : split_src1.high;
        Value *end_src1_value   = is_left ? split_src1.high : split_src1.low;

        if (tac->src2->int_value == 0) {
            // A zero shift is equivalent to a straight copy
            make_instruction_a_nop(tac);

            tac = new_tac_after(tac, IR_MOVE, split_dst.low, split_src1.low, NULL);
            tac = new_tac_after(tac, IR_MOVE, split_dst.high, split_src1.high, NULL);

            return tac;
        }

        if (tac->src2->int_value < 64) {
            // Shift by less than 64 bits. This needs doing in a couple of steps,
            // since there are some bits that travel from low to/from high.

            Tac *top_tac = tac;

            int count = top_tac->src2->int_value;

            Value *temp = new_long_vreg_from_value(function, top_tac->src1);
            Value *transition_bits = new_long_vreg_from_value(function, top_tac->src1);
            Value *shifted = new_long_vreg_from_value(function, top_tac->src1);

            make_instruction_a_nop(top_tac);

            // Make a copy of the low bits
            tac = new_tac_after(tac, IR_MOVE, temp, start_src1_value, NULL);

            // Shift the transitional bits to the left, so they are ready to be orred onto the vacated high bits
            tac = new_tac_after(tac, opposite_operation, transition_bits, temp, new_integral_constant(TYPE_INT, 64 - count));

            // Shift the low bits to the left
            tac = new_tac_after(tac, operation, start_dst_value, start_src1_value, new_integral_constant(TYPE_INT, count));

            // Shift the high bits to the left into a temporary high vreg
            tac = new_tac_after(tac, operation, shifted, end_src1_value, new_integral_constant(TYPE_INT, count));

            // Or the transitional bits onto the temporary high vreg into the dst
            tac = new_tac_after(tac, IR_BOR, end_dst_value, shifted, transition_bits);

            return tac;
        }

        if (tac->src2->int_value == 64) {
            // Move the start byte over to end byte

            tac->operation.id = IR_MOVE;
            tac->dst = end_dst_value;
            tac->src1 = start_src1_value;
            tac->src2 = NULL;

            return new_tac_after(tac, IR_MOVE, start_dst_value, new_integral_constant(TYPE_LONG, 0), NULL);
        }

        if (tac->src2->int_value >= 64) {
            // Shift left by more than 64 bits. The entire low byte is obliterated.
            // Shift the low bytes by 64 bits less.

            tac->dst = end_dst_value;
            tac->src1 = start_src1_value;
            tac->src2->int_value -= 64;

            return new_tac_after(tac, IR_MOVE, start_dst_value, new_integral_constant(TYPE_LONG, 0), NULL);
        }
    }

    bail_on_unimplemented_instruction(tac, "int128 bit shift not implemented for:");

    return tac;
}

// Transform and, or and xor
static Tac *transform_binary_operation(Function *function, Tac *tac) {
    if (VALUE_IS_INT128_VREG(tac->dst) && VALUE_IS_INT128_VREG(tac->src1) && VALUE_IS_INT128(tac->src2) && tac->src2->is_constant) {
        return split_instruction(function, tac);
    }
    else if (VALUE_IS_INT128_VREG(tac->dst) && VALUE_IS_INT128_VREG(tac->src1) && VALUE_IS_INT128_VREG(tac->src2)) {
        return split_instruction(function, tac);
    }

    bail_on_unimplemented_instruction(tac, "int128 IR_BOR not implemented for:");

    return tac;
}

// Transform addition and subtraction
static Tac *transform_add_and_sub(Function *function, Tac *tac, int first_op, int second_op) {
    SplitValue split_dst = split_value(function, tac->dst);
    SplitValue split_src1 = split_value(function, tac->src1);
    SplitValue split_src2 = split_value(function, tac->src2);

    make_instruction_a_nop(tac);

    tac = new_tac_after(tac, first_op, split_dst.low, split_src1.low, split_src2.low);
    tac = new_tac_after(tac, second_op, split_dst.high, split_src1.high, split_src2.high);

    return tac;
}

// Transform multiplication
static Tac *transform_mul(Function *function, Tac *tac) {
    if (VALUE_IS_INT128_VREG(tac->dst) && VALUE_IS_INT128_VREG(tac->src1) && VALUE_IS_INT128_VREG(tac->src2)) {
        // Multiply src1 with src2
        // Notation: src1_hi:src1_lo src2_hi:src2_lo
        // p00 = src1_lo * src2_lo
        // p01 = src1_hi * src2_lo
        // p10 = src1_lo * src2_hi
        //
        // result_lo = p00_lo
        // result_hi = p00_hi + p01_lo + p10_lo

        SplitValue split_dst = split_value(function, tac->dst);
        SplitValue split_src1 = split_value(function, tac->src1);
        SplitValue split_src2 = split_value(function, tac->src2);

        // Nuke the current TAC for convenience
        make_instruction_a_nop(tac);

        Value *p01 = new_long_vreg_from_value(function, split_src1.low);
        Value *p10 = new_long_vreg_from_value(function, split_src1.low);
        Value *p00_low = new_long_vreg_from_value(function, split_src1.low);
        Value *p00_high = new_long_vreg_from_value(function, split_src1.low);

        // p01 = src1_hi * src2_lo
        tac = new_tac_after(tac, IR_MUL, p01, split_src1.high, split_src2.low);

        // p10 = src1_lo * src2_hi
        tac = new_tac_after(tac, IR_MUL, p10, split_src1.low, split_src2.high);

        // p00 = src1_lo * src2_lo
        tac = new_tac_after(tac, IR_MUL128A, p00_low, split_src1.low, split_src2.low);
        tac = new_tac_after(tac, IR_MUL128B, p00_high, split_src1.low, split_src2.low);

        // result_lo = p00_lo
        tac = new_tac_after(tac, IR_MOVE, split_dst.low, p00_low, NULL);

        // result_hi = p00_hi + p01_lo + p10_lo
        Value *result_hi = new_long_vreg_from_value(function, split_src1.low);
        tac = new_tac_after(tac, IR_ADD, result_hi, p00_high, p01);
        tac = new_tac_after(tac, IR_ADD, split_dst.high, result_hi, p10);

        return tac;
    }

    bail_on_unimplemented_instruction(tac, "int128 IR_MUL not implemented for:");

    return tac;
}

// Transform a IR_EQ or IR_NE into a sequence of xor, xor, or, then a comparison with zero
static Tac *transform_eq_ne(Function *function, Tac *tac, int operation) {
    SplitValue split_src1 = split_value(function, tac->src1);
    SplitValue split_src2 = split_value(function, tac->src2);

    Value *dst = tac->dst;

    // Nuke the current TAC for convenience
    make_instruction_a_nop(tac);

    Value *tmp_high = new_long_vreg_from_value(function, split_src1.low);
    Value *tmp_low = new_long_vreg_from_value(function, split_src1.low);
    Value *tmp_combination = new_long_vreg_from_value(function, split_src1.low);

    Value *zero = new_integral_constant(TYPE_LONG, 0);
    zero->type->is_unsigned = split_src1.low->type->is_unsigned;

    tac = new_tac_after(tac, IR_XOR, tmp_high, split_src1.high, split_src2.high); // xor high, high
    tac = new_tac_after(tac, IR_XOR, tmp_low, split_src1.low, split_src2.low);    // xor low, low
    tac = new_tac_after(tac, IR_BOR, tmp_combination, tmp_low, tmp_high);         // or the results of the xors
    tac = new_tac_after(tac, operation, dst, tmp_combination, zero);              // Do the comparison on the result of the xors

    return tac;
}

static Tac *transform_lt_gt_le_ge(Function *function, Tac *tac, int operation) {
    SplitValue split_src1 = split_value(function, tac->src1);
    SplitValue split_src2 = split_value(function, tac->src2);

    Value *unsigned_split_src1_low = dup_value(split_src1.low);
    Value *unsigned_split_src2_low = dup_value(split_src2.low);

    unsigned_split_src1_low->type->is_unsigned = 1;
    unsigned_split_src2_low->type->is_unsigned = 1;

    Value *dst = tac->dst;

    // Nuke the current TAC for convenience
    make_instruction_a_nop(tac);

    Value *zero = new_integral_constant(TYPE_INT, 0);
    Value *one = new_integral_constant(TYPE_INT, 1);

    Value *tmp_int = new_value();
    tmp_int->type = new_type(TYPE_INT);
    tmp_int->vreg = new_vreg(function);

    Value *lfalse = new_label_dst();
    Value *ldone = new_label_dst();

    // Narrow the operation down to a non-equality comparison
    int lt_gt_operation;

    switch(operation) {
        case IR_LT: lt_gt_operation = IR_LT; break;
        case IR_LE: lt_gt_operation = IR_LT; break;
        case IR_GT: lt_gt_operation = IR_GT; break;
        case IR_GE: lt_gt_operation = IR_GT; break;
        default:
            panic("Unknown comparison operation: %d", operation);
    }

    // Default the result to one
    tac = new_tac_after(tac, IR_MOVE, dst, one, 0);

    // Compare the high vreg using a signed/unsigned comparison based on the original value
    tac = new_tac_after(tac, lt_gt_operation, tmp_int, split_src1.high, split_src2.high);
    tac = new_tac_after(tac, IR_JNZ, 0, tmp_int, ldone);

    tac = new_tac_after(tac, IR_EQ, tmp_int, split_src1.high, split_src2.high);
    tac = new_tac_after(tac, IR_JZ, 0, tmp_int, lfalse);

    // Compare the low vreg using an unsigned operation
    tac = new_tac_after(tac, operation, tmp_int, unsigned_split_src1_low, unsigned_split_src2_low);
    tac = new_tac_after(tac, IR_JNZ, 0, tmp_int, ldone);

    // Set the result to zero
    tac = new_tac_after(tac, IR_MOVE, dst, zero, 0);
    tac->label = lfalse->label;

    tac = new_tac_after(tac, IR_NOP, 0, 0, 0);
    tac->label = ldone->label;

    return tac;
}

// Allocate two new vregs for each 128-bit vreg and alter the instructions so that no
// int128 types remain. Transform all instructions so that all traces of 128-bit integers are gone.
void transform_int128_instructions(Function *function) {
    if (debug_int128) {
        printf("Before int128 instruction transformations:\n");
        print_ir(function, 0);
    }

    // Allocate split registers
    map_int128_registers(function);

    // Transform operations
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (!TAC_IS_INT128(tac)) continue;

        switch (tac->operation.id) {
            case IR_MOVE:
                tac = transform_move(function, tac);
                break;
            case IR_ADDRESS_OF:
                tac = transform_address_of(function, tac);
                break;
            case IR_INDIRECT:
                tac = transform_indirect(function, tac);
                break;
            case IR_ARG:
                break; // Already handled
            case IR_ASHR:
                tac = transform_bitshift(function, tac, IR_ASHR);
                break;
            case IR_BSHR:
                tac = transform_bitshift(function, tac, IR_BSHR);
                break;
            case IR_BSHL:
                tac = transform_bitshift(function, tac, IR_BSHL);
                break;
            case IR_BOR:
            case IR_BAND:
            case IR_XOR:
                tac = transform_binary_operation(function, tac);
                break;
            case IR_ADD:
                tac = transform_add_and_sub(function, tac, IR_ADD, IR_ADDC);
                break;
            case IR_SUB:
                tac = transform_add_and_sub(function, tac, IR_SUB, IR_SUBC);
                break;
            case IR_MUL:
                tac = transform_mul(function, tac);
                break;
            case IR_EQ:
                tac = transform_eq_ne(function, tac, IR_EQ);
                break;
            case IR_NE:
                tac = transform_eq_ne(function, tac, IR_NE);
                break;
            case IR_LT: tac = transform_lt_gt_le_ge(function, tac, IR_LT); break;
            case IR_GT: tac = transform_lt_gt_le_ge(function, tac, IR_GT); break;
            case IR_LE: tac = transform_lt_gt_le_ge(function, tac, IR_LE); break;
            case IR_GE: tac = transform_lt_gt_le_ge(function, tac, IR_GE); break;
            default:
                fprintf(stderr, "Unimplemented int128 IR operation %s in %s\n", operation_string(tac->operation.id), function->identifier);
                bail_on_unimplemented_instruction(tac, "for:");
        }
    }
    if (debug_int128) {
        printf("After int128 instruction transformations:\n");
        print_ir(function, 0);
    }
}
