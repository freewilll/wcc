#include "wcc.h"
#include "aarch64.h"

// Corresponds with the offets of struct __va_list in internals.h
#define VA_LIST_STACK_OFFSET 0
#define VA_LIST_GR_TOP_OFFSET 8
#define VA_LIST_VR_TOP_OFFSET 16
#define VA_LIST_GR_OFFS_OFFSET 24
#define VA_LIST_VR_OFFS_OFFSET 28

#define FP_TYPE_TO_SIZE(type) (\
      type == TYPE_FLOAT ? 4 \
    : type == TYPE_DOUBLE ? 8 \
    : 16)

typedef struct  {
    int is_hfa;                 // Is the struct an HFA
    int fits_in_fp_registers;   // Does it fit into the available floating point registers in the CVA?
    int type;                   // TYPE_FLOAT, TYPE_DOUBLE or TYPE_LONG_DOUBLE;
    int count;                  // The amount of scalars. Must be < 4
} HfaDetails;

Set *allocate_return_value_live_ranges(void) {
    return new_set(LIVE_RANGE_PREG_V07 + 1);
}

// Check if the struct/union is a Homogeneous Floating-point Aggregate (HFA)
// and if it can fit into the available floating point registers.
// The max size is 4 * 16 bytes.
// The type is an HFA if:
// - The struct must only have one type of float, double or long doubles
// - Overlapping types due to unions have to be the same
//
static HfaDetails make_hfa_details(Type *type, int size, CallValueAllocation *cva) {
    HfaDetails h = {0}; // Defaults to not an HFA

    int seen_types[4] = {0};
    int seen_type = 0;

    // Break the struct up into individual scalars
    StructOrUnionScalars *scalars = wmalloc(sizeof(StructOrUnionScalars));
    scalars->scalars = wmalloc(sizeof(StructOrUnionScalar *) * MAX_STRUCT_OR_UNION_SCALARS);
    scalars->count = 0;
    flatten_type(type, scalars, 0);

    // Categorize the members by looking at what types are present

    for (int i = 0; i < scalars->count; i++) {
        StructOrUnionScalar *scalar = scalars->scalars[i];

        int position;

        if (scalar->type->type == TYPE_FLOAT)
            position = scalar->offset / 4;
        else if (scalar->type->type == TYPE_DOUBLE)
            position = scalar->offset / 8;
        else if (scalar->type->type == TYPE_LONG_DOUBLE)
            position = scalar->offset / 16;
        else
            goto done; // Any non-floating point type means it's not an HFA

        if (position >= 4) goto done; // An HFA can have no more than 4 FP types

        if (seen_type && seen_type != scalar->type->type) goto done; // There are mixed types
        seen_type = scalar->type->type;

        if (seen_types[position] && seen_types[position] != scalar->type->type) goto done; // Type mismatch at same offset
        seen_types[position] = scalar->type->type;
    }

    if (!seen_type) panic("Unexpectedly got 0 seen_type");

    // Count the amount of scalars
    int count = 0;
    for (int i = 0; i < 4; i++) if (seen_types[i]) count++;
    if (count > 4) goto done; // HFAs have max 4 scalars

    h.is_hfa = 1;
    h.type = seen_type;
    h.count = count;

    h.fits_in_fp_registers = (h.is_hfa && cva->single_fp_register_arg_count + count <= 8);

done:
    for (int i = 0; i < scalars->count; i++) wfree(scalars->scalars[i]);
    wfree(scalars->scalars);
    wfree(scalars);

    return h;
}

// For non-HFA struct/unions > 16 bytes, the caller puts a pointer
// to the return value struct in x8. Add a move and put it in function->return_value_pointer.
static int setup_return_for_struct_or_union(Function *function, Tac *ir) {
    CallValueAllocation *cva = function->type->function->return_value_cva;
    if (!cva) panic("In setup_return_for_struct_or_union() got an empty RV cva");
    if (CVA_CVL(cva, 0).locations[0].indirect_stack_offset == -1) return 0;

    setup_return_value_pointer(function, ir, LIVE_RANGE_PREG_R08);

    return 1;
}

int prepend_function_params(Function *function, Tac *ir) {
    if (function->type->target->type == TYPE_STRUCT_OR_UNION)
        setup_return_for_struct_or_union(function, ir);

    return 0;
}

// Create a return value CVA for functions returning a struct or union.
void init_target_call_value_allocaton(Type *function_type, CallValueAllocation *cva) {
    if (function_type->target->type == TYPE_STRUCT_OR_UNION)
        initialize_function_return_value_cva(function_type);
}

static int float_type_from_stru_size(int stru_size) {
    if (stru_size != 4 && stru_size != 8 && stru_size != 16)
        panic("Expected 4, 8 or 16 stru size in float_type_from_stru_size()");

    return
          stru_size == 4 ? TYPE_FLOAT
        : stru_size == 8 ? TYPE_DOUBLE
        : TYPE_LONG_DOUBLE;
}

// This implements the reverse of make_struct_or_union_to_abi_hfa_registers_move_instructions
// This is also used for moving struct/unions into function return value registers.
static int make_hfa_struct_or_union_move_from_registers_to_stack_instructions(
        Function *function, Tac *ir, CallValueLocation* cvl,
        int register_index, int stack_index, RegisterSet *register_set, int param_register_vreg) {

    if (debug_function_param_mapping)
        printf("Adding param move from int register %d size %d to stack_index %d, offset %d\n", register_index, cvl->stru_size, stack_index, cvl->stru_offset);

    if (!param_register_vreg) param_register_vreg = ++function->vreg_count;

    // Make param register value
    Value *param_register = new_value();
    param_register->vreg = param_register_vreg;
    int live_range_preg = register_set->fp_registers[register_index];
    param_register->live_range_preg = live_range_preg;

    int type = float_type_from_stru_size(cvl->stru_size);
    param_register->type = new_type(type);
    Value *dst = new_value_in_stack(type, stack_index, cvl->stru_offset);
    new_tac_before(ir, IR_MOVE, dst, param_register, 0, 0);

    return live_range_preg;
}

// Add instructions to move struct/union data from a param register to a struct on the stack
// live_ranges
int add_struct_or_union_param_move(Function *function, Tac *ir, Type *type, CallValueLocations *cvl, RegisterSet *register_set) {
    Value *v = allocate_stack_space_for_type(function, ir, type);

    for (int loc = 0; loc < cvl->count; loc++) {
        CallValueLocation *location = &(cvl->locations[loc]);

        if (location->int_register != -1)
            make_int_struct_or_union_move_from_register_to_stack_instructions(function, ir, location, location->int_register, v->stack.index, register_set, 0);
        else if (location->fp_register != -1)
            make_hfa_struct_or_union_move_from_registers_to_stack_instructions(function, ir, location, location->fp_register, v->stack.index, register_set, 0);
        else
            panic("Got unexpected stack offset in add_struct_or_union_param_move()");
    }

    return v->stack.index;
}

// return_value_pointer contains the vreg moves from the x8 register move at the top of the function
// and has a pointer to where the return value has to be copied to.
static void add_function_return_moves_for_indirect_struct_or_union_to_abi_register(Function *function, Tac *ir, CallValueLocations *cvl) {
    ir->operation.id = IR_NOP;

    // Convert src1 to be a pointer to void
    Value *src1 = dup_value(ir->src1);
    src1->type = make_pointer_to_void();
    Value *dst = dup_value(function->return_value_pointer);
    dst->is_lvalue = 1;

    int size = get_type_size(ir->src1->type);
    ir = add_memory_copy(function, ir, dst, src1, size);

    ir = new_tac_after(ir, IR_RETURN, 0, 0, 0);
}

// For functions with variadic arguments, move registers into the register save area.
// The register save area has been allocated on the stack by the parser with the
// value set in function->register_save_area.
void add_function_vararg_param_moves(Function *function, CallValueAllocation *cva, Tac *ir) {
    // Add moves for ints registers to register save area
    for (int i = cva->single_int_register_arg_count; i < 8; i++) {
        Value *dst = dup_value(function->register_save_area);
        dst->type = new_type(TYPE_LONG);
        dst->offset = i * 8;
        ir = add_arg_move_to_dst(function, ir, dst, int_arg_registers[i]);
    }

    // Add moves for FP registers to register save area
    for (int i = cva->single_fp_register_arg_count; i < 8; i++) {
        Value *dst = dup_value(function->register_save_area);
        dst->type = new_type(TYPE_LONG_DOUBLE);
        dst->offset = 64 + i * 16;
        ir = add_arg_move_to_dst(function, ir, dst, fp_arg_registers[i]);
    }
}

// Add code for a va_start() call. The va_list struct, present in src1, has to be
// populated.
void process_function_va_start(Function *function, Tac *ir) {
    Value *va_list = ir->src1;
    ir->operation.id = IR_NOP;
    ir->src1 = 0;

    CallValueAllocation *cva = function->cva;
    int stack_offset = cva->size;
    stack_offset = (stack_offset + 7) & ~7;

    // Set the "stack" member to the stack pointer after the last named arg on the stack, rounded up to 8 bytes
    Value *address = make_pointer_to_void_in_vreg_value(function);;
    Value *va_stack_start = new_value();
    va_stack_start->type = make_pointer_to_void();
    va_stack_start->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
    ir = new_tac_after(ir, IR_ADDRESS_OF, address, va_stack_start, NULL);
    Value *va_list_stack = dup_value(va_list);
    va_list_stack->type = address->type;
    va_list_stack->offset = VA_LIST_STACK_OFFSET;
    ir = new_tac_after(ir, IR_MOVE, va_list_stack, address, 0);

    // Set the "gr_top" member to the stack pointer after the last integer register
    address = make_pointer_to_void_in_vreg_value(function);
    Value *register_save_area_gr_top = dup_value(function->register_save_area);
    register_save_area_gr_top->type = address->type;
    register_save_area_gr_top->offset = 64;
    ir = new_tac_after(ir, IR_ADDRESS_OF, address, register_save_area_gr_top, NULL);
    Value *va_list_gr_top = dup_value(va_list);
    va_list_gr_top->type = address->type;
    va_list_gr_top->offset = VA_LIST_GR_TOP_OFFSET;
    ir = new_tac_after(ir, IR_MOVE, va_list_gr_top, address, 0);

    // Set the "vr_top" member to the stack pointer after the last floating point register
    address = make_pointer_to_void_in_vreg_value(function);
    Value *register_save_area_vr_top = dup_value(function->register_save_area);
    register_save_area_vr_top->type = address->type;
    register_save_area_vr_top->offset = 64 + 128;
    ir = new_tac_after(ir, IR_ADDRESS_OF, address, register_save_area_vr_top, NULL);
    Value *va_list_vr_top = dup_value(va_list);
    va_list_vr_top->type = address->type;
    va_list_vr_top->offset = VA_LIST_VR_TOP_OFFSET;
    ir = new_tac_after(ir, IR_MOVE, va_list_vr_top, address, 0);

    // Set the "gr_offs" member to to 0 – ((8 – named_gr) * 8)
    Value *gr_offs_value = new_value();
    gr_offs_value->type = new_type(TYPE_INT);
    gr_offs_value->is_constant = 1;
    gr_offs_value->int_value = 0 - ((8 -function->cva->single_int_register_arg_count) * 8);
    Value *va_list_gr_offs = dup_value(va_list);
    va_list_gr_offs->type = new_type(TYPE_INT);
    va_list_gr_offs->offset = VA_LIST_GR_OFFS_OFFSET;
    ir = new_tac_after(ir, IR_MOVE, va_list_gr_offs, gr_offs_value, 0);

    // Set the "vr_offs" member to to 0 – ((8 – named_vr) * 16)
    Value *vr_offs_value = new_value();
    vr_offs_value->type = new_type(TYPE_INT);
    vr_offs_value->is_constant = 1;
    vr_offs_value->int_value = 0 - ((8 -function->cva->single_fp_register_arg_count) * 16);
    Value *va_list_vr_offs = dup_value(va_list);
    va_list_vr_offs->type = new_type(TYPE_INT);
    va_list_vr_offs->offset = VA_LIST_VR_OFFS_OFFSET;
    ir = new_tac_after(ir, IR_MOVE, va_list_vr_offs, vr_offs_value, 0);
}

// // Read gr_offs or vr_off into a register
static Tac *read_va_list_offs(Function *function, Tac *ir, Value *va_list, int is_int_register, Value **output) {
    // Read gr_offs or vr_off into offs register
    Type *int_offs_type = new_type(TYPE_INT);
    int offs_offset = is_int_register ? VA_LIST_GR_OFFS_OFFSET : VA_LIST_VR_OFFS_OFFSET;
    Value *int_offs;
    ir = read_from_va_list(function, ir, va_list, int_offs_type, int_offs_type, offs_offset, &int_offs);

    // Convert offs from an int to a long
    *output = dup_value(int_offs);
    (*output)->type = new_type(TYPE_LONG);
    (*output)->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_MOVE, (*output), int_offs, 0);
}

// Compare offs to zero with a comparison operator, and if it passes, jump to the stack fetching code
static Tac *cmp_va_offs_to_zero_and_jump(Function *function, Tac *ir, Value *offs, Value *lfetch_from_stack, int cmp_instruction) {
    Value *offs_ge_result = new_value();
    offs_ge_result->type = new_type(TYPE_INT);
    offs_ge_result->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, cmp_instruction, offs_ge_result, offs, new_integral_constant(TYPE_LONG, 0));
    ir = new_tac_after(ir, IR_JNZ, 0, offs_ge_result, lfetch_from_stack);

    return ir;
}

// Check if offs >= 0, and if so jump to the stack fetching code
static Tac *check_va_offs_ge_zero(Function *function, Tac *ir, Value *offs, Value *lfetch_from_stack) {
    return cmp_va_offs_to_zero_and_jump(function, ir, offs, lfetch_from_stack, IR_GE);
}

// Check if offs > 0, and if so jump to the stack fetching code
static Tac *check_va_offs_gt_zero(Function *function, Tac *ir, Value *offs, Value *lfetch_from_stack) {
    return cmp_va_offs_to_zero_and_jump(function, ir, offs, lfetch_from_stack, IR_GT);
}

// Add a constant value to src and store in dst
static Tac *add_to_va_offs(Function *function, Tac *ir, Value *src, Value **dst, int delta) {
    *dst = dup_value(src);
    (*dst)->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_ADD, *dst, src, new_integral_constant(TYPE_INT, delta));

    return ir;
}

static Tac *write_offs_to_va_list(Function *function, Tac *ir, Value *va_list, Value *offs, int is_int) {
    Value *new_offs = dup_value(offs);
    Value *int_new_offs = dup_value(new_offs);
    int_new_offs->type = new_type(TYPE_INT);
    int_new_offs->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_MOVE, int_new_offs, new_offs, 0);
    int offs_offset = is_int ? VA_LIST_GR_OFFS_OFFSET : VA_LIST_VR_OFFS_OFFSET;
    ir = write_to_va_list(function, ir, va_list, int_new_offs, offs_offset);

    return ir;
}

// Make a pointer to the type with address top + offs and put in param_address
static Tac *make_address_of_va_register_save_area(Function *function, Tac *ir, Value *va_list, Value *offs, int is_int, Type *dst_type, Value **param_address, Value *ldone) {
    // make pointer to gr_top/vr_top + offs
    // Read gr_top or vr_top
    Type *top_type = make_pointer(dst_type);
    int top_offset = is_int ? VA_LIST_GR_TOP_OFFSET : VA_LIST_VR_TOP_OFFSET;
    Value *top;
    ir = read_from_va_list(function, ir, va_list, top_type, top_type, top_offset, &top);

    // param_address = top + offs
    *param_address = new_value();
    (*param_address)->type = make_pointer(dst_type);
    (*param_address)->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_ADD, *param_address, top, offs);

    return ir;
}

// Create a new value from value which is aligned up 16 bytes.
// This requires a conversion to int and then back to void *, since the IR_BAND
// operation has to happen on an integral type.
static Tac *align_value_to_16_bytes(Function *function, Tac *ir, Value *src, Value **dst) {
    // Convert the src to a long
    Value *tmp = dup_value(src);
    tmp->vreg = ++function->vreg_count;
    tmp->type = new_type(TYPE_LONG);
    ir = new_tac_after(ir, IR_MOVE, tmp, src, NULL);

    // Add 15
    Value *tmp2 = dup_value(tmp);
    tmp2->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_ADD, tmp2, tmp, new_integral_constant(TYPE_LONG, 15));

    // Binary and ~15
    Value *tmp3 = dup_value(tmp2);
    tmp2->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_BAND, tmp3, tmp2, new_integral_constant(TYPE_LONG, ~15));

    // Convert the result back to the same type as src
    *dst = dup_value(src);
    (*dst)->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_MOVE, *dst, tmp3, NULL);

    return ir;
}

// Copy memory. src is a pointer to the stack. dst is a non-pointer where the memory has to be copied to.
static Tac *copy_va_memory(Function *function, Tac *ir, Value *src, Value *dst, int size) {
    // The memory copy function needs lvalues for dst and src
    Value *memory_copy_src = dup_value(src);
    memory_copy_src->is_lvalue = 1;

    // Make a pointer for the destination
    Value *memory_copy_dst = new_value();
    memory_copy_dst->type = make_pointer_to_void();
    memory_copy_dst->vreg = ++function->vreg_count;
    memory_copy_dst->is_lvalue = 1;

    ir = new_tac_after(ir, IR_ADDRESS_OF, memory_copy_dst, dst, NULL);
    ir = add_memory_copy(function, ir, memory_copy_dst, memory_copy_src, size);

    return ir;
}

// Advance the stack pointer by increment and return a value with the pointer to the stack in stack
static Tac *prepare_read_from_va_stack(Function *function, Tac *ir, Type *type, Value *va_list, Value *dst, Value *lfetch_from_stack, Value **stack) {
    if (lfetch_from_stack) {
        ir = new_tac_after(ir, IR_NOP, 0, 0, 0);
        ir->label = lfetch_from_stack->label;
    }

    // Read stack pointer from va_list
    Type *stack_type = make_pointer(dst->type);
    int stack_offset = VA_LIST_STACK_OFFSET;
    ir = read_from_va_list(function, ir, va_list, stack_type, stack_type, stack_offset, stack);

    // Align the stack up to 16 bytes if the alignment needs this
    if (get_type_alignment(type) == 16)
        ir = align_value_to_16_bytes(function, ir, *stack, stack);

    // new_stack = stack + 8 or 16
    Value *new_stack = dup_value(*stack);
    new_stack->vreg = ++function->vreg_count;

    int increment = (get_type_size(type) + 7) & ~7;

    ir = new_tac_after(ir, IR_ADD, new_stack, *stack, new_integral_constant(TYPE_LONG, increment));
    ir = write_to_va_list(function, ir, va_list, new_stack, stack_offset);

    return ir;
}

// Load an integer or floating point vararg into dst.
// First, check the offs pointer to see if the data is in the register save area.
// Otherwise, it's on the stack.
static Tac *add_function_va_arg_scalar_read(Function *function, Tac *ir, Type *type, Value *va_list, int is_int, Value *dst) {
    int alignment = get_type_alignment(type);

    // Make jump labels
    Value *lfetch_from_stack = new_label_dst();
    Value *ldone = new_label_dst();

    // Read gr_offs or vr_off into offs register
    Value *offs;
    ir = read_va_list_offs(function, ir, va_list, is_int, &offs);

    // Check if offs >= 0, and if so jump to the stack fetching code
    ir = check_va_offs_ge_zero(function, ir, offs, lfetch_from_stack);

    if (is_int && alignment == 16)
        printf("Types with 16 byte alignment in a vararg are not implemented\n");

    // Increase offs by 8 for integers and 16 for FP values
    Value *new_offs;
    ir = add_to_va_offs(function, ir, offs, &new_offs, is_int ? 8 : 16);

    // Write new_offs to va_list
    ir = write_offs_to_va_list(function, ir, va_list, new_offs, is_int);

    // Check if offs > 0 and and jump to stack code if the arg isn't in the register save area
    ir = check_va_offs_gt_zero(function, ir, new_offs, lfetch_from_stack);

    // Read from the register save area at relative offset offs from the top pointer and jump to ldone
    Value *param_address;
    ir = make_address_of_va_register_save_area(function, ir, va_list, offs, is_int, dst->type, &param_address, ldone);
    ir = new_tac_after(ir, IR_INDIRECT, dst, param_address, 0);
    ir = new_tac_after(ir, IR_JMP, 0, ldone, 0);

    // Read from stack
    Value *stack;
    ir = prepare_read_from_va_stack(function, ir, type, va_list, dst, lfetch_from_stack, &stack);

    // Finally, load the param into dst and fall through to the done label
    ir = new_tac_after(ir, IR_INDIRECT, dst, stack, 0);

    // Add the done label
    ir = new_tac_after(ir, IR_NOP, 0, 0, 0);
    ir->label = ldone->label;

    return ir;
}

// Read an HFA struct/union from the HFA area into dst.
static Tac *add_function_va_arg_hfa_struct_or_union_read(Function *function, Tac *ir, Value *va_list, Type *type, int member_type_type, int count, Value *dst) {
    // Make jump labels
    Value *lfetch_from_stack = new_label_dst();
    Value *ldone = new_label_dst();

    // Read vr_offs into offs register
    Value *offs;
    ir = read_va_list_offs(function, ir, va_list, 0, &offs);

    // Check if offs >= 0, and if so jump to the stack fetching code
    ir = check_va_offs_ge_zero(function, ir, offs, lfetch_from_stack);

    // Increase offs by 16 * count
    Value *new_offs;
    ir = add_to_va_offs(function, ir, offs, &new_offs, 16 * count);

    // Write new_offs to va_list
    ir = write_offs_to_va_list(function, ir, va_list, new_offs, 0);

    // Check if offs > 0 and and jump to stack code if the arg isn't in the register save area
    ir = check_va_offs_gt_zero(function, ir, new_offs, lfetch_from_stack);

    // Prepare the base address in the register save area
    Type *member_type = new_type(member_type_type);
    Value *base_address;
    ir = make_address_of_va_register_save_area(function, ir, va_list, offs, 0, member_type, &base_address, ldone);

    // Loop over all FP scalars and add move instructions.
    // This needs doing one scalar at a time, since the register save area has
    // 16 bytes for each floating point value, so the layout is different
    // from the struct.
    for (int i = 0; i < count; i++) {
        Value *temp_vreg = new_value();
        temp_vreg->type = member_type;
        temp_vreg->vreg = ++function->vreg_count;

        Value *member = dup_value(dst);
        member->type = member_type;
        member->offset = i *  FP_TYPE_TO_SIZE(member_type_type);

        Value *base_address_with_offset = dup_value(base_address);
        base_address_with_offset->offset = i * 16;

        ir = new_tac_after(ir, IR_INDIRECT, temp_vreg, base_address_with_offset, 0);
        ir = new_tac_after(ir, IR_MOVE, member, temp_vreg, 0);
    }

    // All done, jump past the stack code
    ir = new_tac_after(ir, IR_JMP, 0, ldone, 0);

    // Read from stack with a memory copy
    Value *src;
    ir = prepare_read_from_va_stack(function, ir, type, va_list, dst, lfetch_from_stack, &src);
    ir = copy_va_memory(function, ir, src, dst, get_type_size(type));

    // Add the done label
    ir = new_tac_after(ir, IR_NOP, 0, 0, 0);
    ir->label = ldone->label;
}

// Copy a small (<= 16 bytes) struct from the register save area or stack.
static Tac *add_function_va_arg_small_struct_or_union_read(Function *function, Tac *ir, Value *va_list, Type *type, Value *dst) {
    int alignment = get_type_alignment(type);

    // Make jump labels
    Value *lfetch_from_stack = new_label_dst();
    Value *ldone = new_label_dst();

    // Read vr_offs into offs register
    Value *offs;
    ir = read_va_list_offs(function, ir, va_list, 1, &offs);

    // Check if offs >= 0, and if so jump to the stack fetching code
    ir = check_va_offs_ge_zero(function, ir, offs, lfetch_from_stack);

    int size = get_type_size(type);
    int rounded_up_size = (size + 7) & ~7;

    // The registers save area is also aligned to 16-byte boundaries for structs that have 16-byte alignment.
    if (alignment == 16)
        ir = align_value_to_16_bytes(function, ir, offs, &offs);

    // Increase offs by rounded_up_size
    Value *new_offs;
    ir = add_to_va_offs(function, ir, offs, &new_offs, rounded_up_size);

    // Write new_offs to va_list
    ir = write_offs_to_va_list(function, ir, va_list, new_offs, 1);

    // Check if offs > 0 and and jump to stack code if the arg isn't in the register save area
    ir = check_va_offs_gt_zero(function, ir, new_offs, lfetch_from_stack);

    // Read from register save area with a memory copy
    Value *src;
    ir = make_address_of_va_register_save_area(function, ir, va_list, offs, 1, make_pointer_to_void(), &src, ldone);
    ir = copy_va_memory(function, ir, src, dst, size);

    // All done, jump past the stack code
    ir = new_tac_after(ir, IR_JMP, 0, ldone, 0);

    // Read from stack with a memory copy
    ir = prepare_read_from_va_stack(function, ir, type, va_list, dst, lfetch_from_stack, &src);
    ir = copy_va_memory(function, ir, src, dst, size);

    // Add the done label
    ir = new_tac_after(ir, IR_NOP, 0, 0, 0);
    ir->label = ldone->label;
}

// Copy a big (> 16 bytes) struct from the register save area or stack.
// A pointer to the struct is either in the register save area or stack in place of the argument.
static Tac *add_function_va_arg_big_struct_or_union_read(Function *function, Tac *ir, Value *va_list, Type *type, Value *dst) {
    // Read the pointer to the struct by treating it as a regular integer argument
    Value *src = new_value();
    src->type = make_pointer_to_void();
    src->vreg = ++function->vreg_count;
    ir = add_function_va_arg_scalar_read(function, ir, type, va_list, 1, src);

    // Read from the pointer with a memory copy
    ir = copy_va_memory(function, ir, src, dst, get_type_size(type));
}

// Load a integer or floating point vararg from the register save are or stack into dst
static Tac *add_function_va_arg_struct_or_union_read(Function *function, Tac *ir, Type *type, Value *va_list, int is_int, Value *dst) {
    int size = get_type_size(type);

    CallValueAllocation *cva = init_call_value_allocaton("vararg struct");
    add_type_to_cva(cva, type);
    CallValueLocations *cvl = cva->locations->elements[0];

    // Determine if the struct is an HFA. The largest possible HFA struct has 4 long doubles,
    // so only check structs smaller than that size.
    HfaDetails hfad = {0};
    if (size <= 4 * 16) hfad = make_hfa_details(type, size, cva);

    if (hfad.is_hfa) {
        add_function_va_arg_hfa_struct_or_union_read(function, ir, va_list, type, hfad.type, hfad.count, dst);
    }
    else if (cvl->locations[0].indirect_stack_offset == -1) {
        add_function_va_arg_small_struct_or_union_read(function, ir, va_list, type, dst);
    }
    else {
        add_function_va_arg_big_struct_or_union_read(function, ir, va_list, type, dst);
    }
}

// Generate code to read a vararg from either the register save area or the stack
// Following the pseudocode in section 14.4 in aapcs64.pdf
void process_function_va_arg(Function *function, Tac *ir) {
    Value *va_list = ir->src1;
    Value *dst = ir->dst;
    Type *type = dst->type;

    // Nuke the IR_VA_ARG tac
    ir->operation.id = IR_NOP;
    ir->src1 = 0;
    ir->dst = 0;

    int is_single_int_register = type_fits_in_single_int_register(type);
    int is_single_fp_register = is_floating_point_type(type);

    if (is_single_int_register || is_single_fp_register) {
        ir = add_function_va_arg_scalar_read(function, ir, type, va_list, is_single_int_register, dst);
    }

    else if (type->type == TYPE_INT128) {
        panic("va_arg isn't implemented for __int128");
    }

    else if (type->type == TYPE_STRUCT_OR_UNION) {
        ir = add_function_va_arg_struct_or_union_read(function, ir, type, va_list, is_single_int_register, dst);
    }

    else {
        printf("Unhandled aarch64 VA type ");
        print_type(stdout, type);
        printf("\n");
        panic("Bailing");
    }
}

// Using the state of already allocated registers & stack entries in cva, determine the location for a non-composite type and set it in cvl.
void add_type_to_cvl(CallValueAllocation *cva, CallValueLocation *cvl, Type *type, int force_stack) {
    init_cvl(cvl);

    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

    int is_single_int_register = type_fits_in_single_int_register(type);
    int is_single_fp_register = is_floating_point_type(type);
    int in_stack =
        force_stack ||
        (is_single_int_register && cva->single_int_register_arg_count >= 8) || (is_single_fp_register && cva->single_fp_register_arg_count >= 8);

    int alignment = get_type_alignment(type);
    if (alignment < 8) alignment = 8;

    if (!in_stack && is_single_int_register)
        cvl->int_register = cva->single_int_register_arg_count < 8 ? cva->single_int_register_arg_count : -1;

    else if (!in_stack && is_single_fp_register)
        cvl->fp_register = cva->single_fp_register_arg_count < 8 ? cva->single_fp_register_arg_count : -1;

    else {
        // It's on the stack
        add_type_to_cvl_in_stack(cva, cvl, type, alignment);

        if (debug_call_value_allocation)
            printf("  arg %2d with alignment %2d     offset 0x%04x with padding 0x%04x\n", cva->locations->length, alignment, cvl->stack_offset, cvl->stack_padding);
    }

    if (debug_call_value_allocation && !in_stack && (is_single_int_register || is_single_fp_register)) {
        if (cvl->int_register != -1)
            printf("  arg %2d with alignment %2d     int reg %5d\n", cva->locations->length, alignment, cvl->int_register);
        else
            printf("  arg %2d with alignment %2d     fp  reg %5d\n", cva->locations->length, alignment, cvl->fp_register);
    }

    cva->single_int_register_arg_count += is_single_int_register;
    if (!in_stack && is_single_fp_register) cva->single_fp_register_arg_count++;
}

// Allocate stack space on the "indirect" stack
// which is folded into the end of the regular
// args stack after all args have been processed.
// See convert_target_arg_move_to_indirect_stack_instructions()
// and move_indirect_stack_args().
// A pointer to the allocated stack is used in
// place of the argument.
static void add_struct_or_union_call_value_location_for_big_struct(CallValueAllocation *cva, Type *type) {
    int alignment = get_type_alignment(type);
    if (alignment < 8) alignment = 8;

    CallValueLocations *cvls = allocate_call_value_locations(2);

    CallValueLocation *cvl_indirect_stack = &cvls->locations[0];
    CallValueLocation *cvl_indirect_arg = &cvls->locations[1];

    init_cvl(cvl_indirect_stack);
    init_cvl(cvl_indirect_arg);

    add_type_to_cvl_in_indirect_stack(cva, cvl_indirect_stack, type, alignment);

    if (debug_call_value_allocation) {
        printf("  arg %2d with alignment %2d     indirect offset 0x%04x\n", cva->locations->length, alignment, cvl_indirect_stack->indirect_stack_offset);
    }

    add_type_to_cvl(cva, cvl_indirect_arg, make_pointer_to_void(), 0);
    append_to_list(cva->locations, cvls);
}

// Each member in an HFA struct, which consists of entirely floats, doubles or long doubles, gets its own register.
static void add_struct_or_union_call_value_location_for_hfa_struct_in_registers(CallValueAllocation *cva, int type, int count) {
    int member_size = FP_TYPE_TO_SIZE(type);

    CallValueLocations *cvl = allocate_call_value_locations(count);

    for (int i = 0; i < count; i++) {
        cvl->locations[i].fp_register = cva->single_fp_register_arg_count + i;
        cvl->locations[i].stru_size = member_size;
        cvl->locations[i].stru_offset = i * member_size;
        add_type_to_cvl(cva, &(cvl->locations[i]), new_type(type), 0);
    }

    append_to_list(cva->locations, cvl);
}

// The struct goes either into int registers, of if there aren't enough,
// on the stack.
static void add_struct_or_union_call_value_location_for_non_hfa_struct(CallValueAllocation *cva, Type *type) {
    int size = get_type_size(type);
    int alignment = get_type_alignment(type);

    // aapcs64 C.10
    // If the argument has an alignment of 16 then the NGRN is rounded up to the next even number.
    if (alignment == 16)
        cva->single_int_register_arg_count = (cva->single_int_register_arg_count + 1) & ~1;

    // aapcs64 C.12
    int needed_int_registers = (size + 7) / 8;

    if (cva->single_int_register_arg_count + needed_int_registers <= 8) {
        // The struct fits in integer registers

        CallValueLocations *cvl = allocate_call_value_locations(needed_int_registers);

        for (int i = 0; i < needed_int_registers; i++) {
            cvl->locations[i].stru_size = size > 8 ? 8 : size;
            size -= 8;
            cvl->locations[i].stru_offset = i * 8;
            add_type_to_cvl(cva, &(cvl->locations[i]), new_type(TYPE_LONG), 0);
        }

        append_to_list(cva->locations, cvl);
    }
    else {
        // The struct goes into the stack
        cva->single_int_register_arg_count = 8;
        add_single_call_value_location(cva, type);
    }
}

static void add_struct_or_union_call_value_location(CallValueAllocation *cva, Type *type) {
    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

    int size = get_type_size(type);

    // Determine if the struct is an HFA. The largest possible HFA struct has 4 long doubles,
    // so only check structs smaller than that size.
    HfaDetails hfad = {0};
    if (size <= 4 * 16) hfad = make_hfa_details(type, size, cva);

    // aapcs64 C.2
    // If it's an HFA and there are enough FP registers left, use them
    if (hfad.is_hfa && cva->single_fp_register_arg_count + hfad.count <= 8) {
        add_struct_or_union_call_value_location_for_hfa_struct_in_registers(cva, hfad.type, hfad.count);
    }

    // aapcs64 C.3
    // Set all FP registers to allocated, round size up to 8 bytes,
    // and allocate stack space
    else if (hfad.is_hfa) {
        cva->single_fp_register_arg_count = 8;
        add_single_call_value_location(cva, type);
    }

    else if (size > 16 && !hfad.fits_in_fp_registers) {
        add_struct_or_union_call_value_location_for_big_struct(cva, type);
    }

    else {
        add_struct_or_union_call_value_location_for_non_hfa_struct(cva, type);
    }
}

// Add a type to a call value allocation
void add_type_to_cva(CallValueAllocation *cva, Type *type) {
    if (type->type == TYPE_INT128) {
        // aapcs64 C.10
        // If the argument has an alignment of 16 then the NGRN is rounded up to the next even number.
        cva->single_int_register_arg_count = (cva->single_int_register_arg_count + 1) & ~1;
        add_int128_call_value_locations(cva, type);
    }

    else if (type->type == TYPE_STRUCT_OR_UNION) {
        add_struct_or_union_call_value_location(cva, type);
    }

    else {
        // Create a single location for the arg
        add_single_call_value_location(cva, type);
    }
}

// In aarch64, there are no instructions which originally move a param to a register that are
// rewritten to move them to the stack. See add_param_move_to_register_tac().
// Therefore, aarch64 has no original stack indexes.
int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));
    return result;
} // TODO aarch64

// Move floats/doubles from ABI encoded HFA in registers to the struct members in the stack
static int make_struct_or_union_to_abi_hfa_registers_move_instructions(Function *function, Tac *ir, Value *arg, int preg_class, int register_index,
    CallValueLocation *cvl, RegisterSet *register_set) {

    if (debug_function_arg_mapping) printf("Adding arg move from struct to SSE register_index=%d register size=%d\n", register_index, cvl->stru_size);

    if (cvl->stru_size != 4 && cvl->stru_size != 8 && cvl->stru_size != 16)
        panic("Expected 4, 8 or 16 stru size in make_struct_or_union_to_abi_hfa_registers_move_instructions()");

    int type = float_type_from_stru_size(cvl->stru_size);
    Value *temp = load_struct_scalar_into_new_vreg(function, ir, arg, cvl, new_type(type));
    return add_arg_move_to_register(function, ir, new_type(type), temp, preg_class, register_index, register_set);
}

// Load an ABI register from a part of a struct or union
int make_struct_or_union_to_abi_move_instructions(
    Function *function, Tac *ir, Value *arg, int preg_class, int register_index,
    CallValueLocation *cvl, RegisterSet *register_set) {

    if (debug_function_arg_mapping)
        printf("Adding arg move from struct preg_class=%d register_index=%d register size=%d\n", preg_class, register_index, cvl->stru_size);

    if (preg_class == PC_INT) {
        return make_struct_or_union_to_abi_int_registers_move_instructions(function, ir, arg, preg_class, register_index, cvl, register_set);
    }
    else {
        return make_struct_or_union_to_abi_hfa_registers_move_instructions(function, ir, arg, preg_class, register_index, cvl, register_set);
    }
}

// Move struct/union data from registers or memory to the destination of a function call
static void add_function_call_result_moves_for_struct_or_union(Function *function, Tac *ir) {
    if (ir->dst->stack.index == 0) panic("Expected a stack index for a function call returning a struct/union");

    int stack_index = ir->dst->stack.index;
    Type *function_type = ir->src1->type;
    Value *function_value = ir->src1;

    CallValueAllocation *cva = function_type->function->return_value_cva;
    if (!cva) panic("In add_function_call_result_moves_for_struct_or_union() got an empty RV cva");
    CallValueLocations *cvl = cva->locations->elements[0];

    if (cvl->locations[0].stack_offset == -1 && cvl->locations[0].indirect_stack_offset == -1) {
        // Move registers to a struct/union on the stack

        ir->dst = NULL;
        ir = ir->next;

        // Allocate result vregs and create a live range or them
        int *live_range_pregs = allocate_vregs_for_cvl(function, ir, cvl);

        for (int loc = 0; loc < cvl->count; loc++) {
            CallValueLocation *location = &(cvl->locations[loc]);

            int live_range_preg;
            int param_register_vreg = live_range_pregs[loc];

            if (location->int_register != -1)
                live_range_preg = make_int_struct_or_union_move_from_register_to_stack_instructions(
                    function, ir, location, location->int_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            else if (location->fp_register != -1) {
                live_range_preg = make_hfa_struct_or_union_move_from_registers_to_stack_instructions(
                    function, ir, location, location->fp_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            }
            else
                panic("Got unexpected stack offset in add_function_call_result_moves_for_struct_or_union");

            add_to_set(function_value->return_value_live_ranges, live_range_preg);
        }

        wfree(live_range_pregs);
    }
    else if (cvl->locations[0].indirect_stack_offset != -1) {
        // Put the address of the struct/union in the x8 register.
        // The caller copies the result over.

        // Take address of dst struct/union on the stack
        Value *address_value = new_value();
        address_value->vreg = ++function->vreg_count;
        address_value->type = make_pointer_to_void();
        new_tac_before(ir, IR_ADDRESS_OF, address_value, ir->dst, 0, 1);

        // Move struct/union target address into x8
        Value *x8_value = new_value();
        x8_value->vreg = ++function->vreg_count;
        x8_value->type = make_pointer_to_void();
        x8_value->live_range_preg = LIVE_RANGE_PREG_R08;
        new_tac_before(ir, IR_MOVE, x8_value, address_value, 0, 1);

        // Keep x8 alive, since args may be inserted between the above move and the call.
        new_tac_before(ir, IR_FUNCTION_CALL_REG, 0, x8_value, 0, 1);

        add_to_set(function_value->return_value_live_ranges, x8_value->live_range_preg);
    }
    else {
        panic("Unhandled result move for struct/union");
    }
}

// Move the result value of a function call to a vreg
// Convert v1 = IR_CALL... to:
// 1. v2 = IR_CALL...
// 2. v1 = v2
// Live range coalescing prevents the v1-v2 live range from getting merged with
// a special check for IR_CALL.
static void add_function_call_result_moves(Function *function) {
    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id != IR_CALL || !ir->dst) continue;

        if (ir->dst && ir->dst->type->type == TYPE_INT128)
            add_function_call_result_moves_for_int128(function, ir, LIVE_RANGE_PREG_R00, LIVE_RANGE_PREG_R01);

        else if (ir->dst && ir->dst->type->type == TYPE_STRUCT_OR_UNION) {
            add_function_call_result_moves_for_struct_or_union(function, ir);
        }

        else {
            // Add move for integer, pointer, or FP
            int is_fp = is_floating_point_type(ir->dst->type);
            int live_range_preg = is_fp ? LIVE_RANGE_PREG_V00 : LIVE_RANGE_PREG_R00;
            add_function_result_moves_for_scalar(function, ir, live_range_preg);
        }
    }
}

// Move struct or union of size <= 16 into ABI registers
static void add_function_return_moves_for_struct_or_union(Function *function, Tac *ir) {
    // Determine registers
    CallValueAllocation *cva = init_call_value_allocaton(function->identifier);
    add_type_to_cva(cva, ir->src1->type);
    CallValueLocations *cvl = cva->locations->elements[0];

    if (cvl->locations[0].indirect_stack_offset != -1) {
        // Return a pointer toe the struct
        add_function_return_moves_for_indirect_struct_or_union_to_abi_register(function, ir, cvl);
    }
    else {
        // Move the data into registers
        add_function_return_moves_for_struct_or_union_to_abi_register(function, ir);
    }
}


static void add_function_return_moves(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation.id == IR_RETURN && !ir->src1) || ir->operation.id != IR_RETURN) continue;

        else if (ir->src1->type->type == TYPE_INT128)
            add_function_return_moves_for_int128(function, ir, LIVE_RANGE_PREG_R00, LIVE_RANGE_PREG_R01);

        else if (ir->src1->type->type == TYPE_STRUCT_OR_UNION)
            add_function_return_moves_for_struct_or_union(function, ir);

        else {
            int is_fp = is_floating_point_type(function->type->target);
            int live_range_preg = is_fp ? LIVE_RANGE_PREG_V00 : LIVE_RANGE_PREG_R00;
            add_function_return_moves_for_scalar(function, ir, live_range_preg);
        }
    }
}

// Make move instrcutions to the stack from a src
static void make_move_instructions_to_stack(Function *function, Tac *tac, int stack_offset, Value *src_low, Value *src_high) {
    Value *dst_low = new_value();
    dst_low->type = dup_type(src_low->type);
    dst_low->type->type = TYPE_LONG;
    dst_low->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
    dst_low->stack.area = SA_FUNCTION_ARGS;

    Value *dst_high = dup_value(dst_low);
    dst_high->stack.index++;

    tac->operation.id = IR_MOVE;
    tac->dst = dst_low;
    tac->src1 = src_low;
    tac->src2 = 0;

    new_tac_after(tac, IR_MOVE, dst_high, src_high, NULL);
}

// Add instructions to copy an int128 from a constant to the stack
static void add_function_call_constant_arg_move_for_int128_to_stack(Function *function, Tac *tac, int stack_offset) {
    if (debug_function_arg_mapping)
        printf("Adding copy from constant for int128\n");

    Value *src2_low = dup_value(tac->src2);
    src2_low->type->type = TYPE_LONG; // The low value is already in int_value

    Value *src2_high = dup_value(tac->src2);
    src2_high->type->type =TYPE_LONG;
    src2_high->int_value = src2_high->int128_value >> 64;

    make_move_instructions_to_stack(function, tac, stack_offset, src2_low, src2_high);
}

// Add instructions to copy an int128 from a register/stack to the stack
static void add_function_call_non_constant_arg_move_for_int128_to_stack(Function *function, Tac *tac, int stack_offset) {
    SplitVreg *split_vreg = function->int128_register_mappings[tac->src2->vreg];
    if (!split_vreg) panic("NULL pointer when fetching split vreg for int128 for vreg %d", tac->src2->vreg);

    if (debug_function_arg_mapping)
        printf("Adding copy from vregs for int128 vreg=%d, split %d / %d\n", tac->src2->vreg, split_vreg->low, split_vreg->high);

    Value *src2_low = dup_value(tac->src2);
    src2_low->type->type =TYPE_LONG;
    src2_low->vreg = split_vreg->low;

    Value *src2_high = dup_value(tac->src2);
    src2_high->type->type =TYPE_LONG;
    src2_high->vreg = split_vreg->high;

    make_move_instructions_to_stack(function, tac, stack_offset, src2_low, src2_high);
}

// Add instructions to copy an int128 from a constant/register/stack to the stack
static void add_function_call_arg_move_for_int128_to_stack(Function *function, Tac *tac) {
    if (tac->src2->stack.index)
        panic("Got unexpected stack index %d in add_function_call_arg_move_for_int128_to_stack", tac->src2->stack.index);

    Value *arg = tac->src1;
    CallValueLocations *cvl = arg->function_call.function_call_arg_locations;
    if (cvl->count != 1) panic("Unexpected int128 to stack move with locations->count != 1");
    int stack_offset = cvl->locations[0].stack_offset;

    if (tac->src2->is_constant)
        add_function_call_constant_arg_move_for_int128_to_stack(function, tac, stack_offset);
    else
        add_function_call_non_constant_arg_move_for_int128_to_stack(function, tac, stack_offset);
}

// Move a single register to the arg stack
static void add_single_register_arg_move_to_stack(Function *function, Tac *tac) {
    Value *arg = tac->src1;
    CallValueLocations *cvl = arg->function_call.function_call_arg_locations;
    if (cvl->count != 1) panic("Unexpected int128 to stack move with locations->count != 1");
    int stack_offset = cvl->locations[0].stack_offset;

    Value *dst = new_value();
    dst->type = dup_type(tac->src2->type);
    dst->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
    dst->stack.area = SA_FUNCTION_ARGS;

    tac->operation.id = IR_MOVE;
    tac->dst = dst;
    tac->src1 = tac->src2;
    tac->src2 = 0;
}

// Add instructions to copy a struct to the stack.
static void add_function_call_arg_move_for_struct_or_union_to_stack(Function *function, Tac *tac) {
    int size = get_type_size(tac->src2->type);
    int rounded_up_size = (size + 7) & ~7;

    Value *arg = tac->src1;
    CallValueLocations *cvl = arg->function_call.function_call_arg_locations;
    if (cvl->count != 1) panic("Unexpected struct to stack move with locations->count != 1");
    int stack_offset = cvl->locations[0].stack_offset;

    Value *dst = new_value();
    dst->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
    dst->stack.area = SA_FUNCTION_ARGS;

    dst->type = make_pointer_to_void();
    dst->is_lvalue = 1;

    // Convert src to be a pointer to void
    Value *src = dup_value(tac->src2);
    src->type = make_pointer_to_void();

    if (debug_function_arg_mapping)
        printf("Adding memory copy for struct/union SI=%d rounded-up-size=%d\n", src->stack.index, rounded_up_size);

    add_memory_copy(function, tac, dst, src, size);

    make_instruction_a_nop(tac);
}

// Arguments in function calls that go to the stack are placed in a reserved stack
// SA_FUNCTION_ARGS area. Codegen needs this to distinguish params on the stack
// from args on the stack.
void convert_target_arg_move_to_stack_instructions(Function *function, Tac *tac) {
    if (tac->src2->type->type == TYPE_INT128) {
        // Add memory copies for int128
        add_function_call_arg_move_for_int128_to_stack(function, tac);
    }
    else if (tac->src2->type->type == TYPE_STRUCT_OR_UNION) {
        // Add memory copies for struct and unions
        add_function_call_arg_move_for_struct_or_union_to_stack(function, tac);
    }
    else {
        // Move a single register to the arg stack
        add_single_register_arg_move_to_stack(function, tac);
    }
}

// The aarch64 ABI says that structs and unions larger than 16 bytes (except HFA cases)
// are copied to a stack area allocated by the caller. The argument is replaced with
// a pointer to the allocated memory.
// Returns a value for an arg preg value if the pointer is in an arg register,
// otherwise NULL if the arg is on the stack.
Value *convert_target_arg_move_to_indirect_stack_instructions(Function *function, Tac *tac, Tac *arg_ir) {
    Value *arg = arg_ir->src1;
    Value *src = arg_ir->src2;

    CallValueLocations *cvls = arg->function_call.function_call_arg_locations;
    if (cvls->count != 2) panic("Unexpected struct/union to indirect stack move with locations->count != 2");

    CallValueLocation *cvl_indirect_stack = &cvls->locations[0];
    CallValueLocation *cvl_indirect_register = &cvls->locations[1];

    // Add memory copy of the struct to the indirect stack
    int stack_offset = cvl_indirect_stack->indirect_stack_offset;

    int size = get_type_size(src->type);

    Value *dst = new_value();
    dst->type = dup_type(src->type);
    dst->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1.
    dst->stack.area = SA_FUNCTION_ARGS;

    add_memory_copy(function, arg_ir, dst, src, size);

    // Put the address of the stack in the arg register
    Value *address = make_pointer_to_void_in_vreg_value(function);
    dst = dup_value(dst);
    dst->type = make_pointer_to_void();
    new_tac_before(tac, IR_ADDRESS_OF, address, dst, NULL, 0);

    if (cvl_indirect_register->int_register != -1) {
        add_arg_move_to_register(function, tac, address->type, address, PC_INT, cvl_indirect_register->int_register, &arg_register_set);
        return tac->prev->dst;
    }
    else {
        Value *dst = new_value();
        dst->type = make_pointer_to_void();
        dst->stack.index = cvl_indirect_register->stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
        dst->stack.area = SA_FUNCTION_ARGS;

        new_tac_before(tac, IR_MOVE, dst, address, NULL, 0);
        return NULL;
    }
}

// Moves allocated indirect stack space, done in
// convert_target_arg_move_to_indirect_stack_instructions()
// to the end of the arg stack and increase the arg stack.
// The arg stack consists of the regular args that are pushed on the stack
// followed by the large structs.
// See the comment above process_stack_offset()
// for an illustration of the full stack layout.
void move_indirect_stack_args(CallValueAllocation *cva) {
    int total_indirect_block_size = 0;
    for (int i = 0; i < cva->locations->length; i++) {
        CallValueLocations *cvls = cva->locations->elements[i];
        for (int j = 0; j < cvls->count; j++) {
            CallValueLocation *cvl = &cvls->locations[j];
            if (cvls->locations[j].indirect_stack_offset == -1) continue;
            total_indirect_block_size = cvl->indirect_stack_offset + cvl->stru_size;
        }
    }

    // Round indirect block size up to 16 bytes
    total_indirect_block_size = (total_indirect_block_size + 15) & ~15;

    // Round stack args area up to 16 bytes
    cva->size = (cva->size + 15) & ~15;

    // How much to move the indirect stack offset up into the regular arg stack area
    int stack_offset_delta = cva->size;
    cva->size += total_indirect_block_size;

    for (int i = 0; i < cva->locations->length; i++) {
        CallValueLocations *cvls = cva->locations->elements[i];
        for (int j = 0; j < cvls->count; j++) {
            CallValueLocation *cvl = &cvls->locations[j];
            if (cvls->locations[j].indirect_stack_offset == -1) continue;
            cvl->indirect_stack_offset += stack_offset_delta;
        }
    }
}

void process_target_functions(Function *function) {
    // Callee
    initialize_function_return_value_cva(function->type);
    add_function_param_moves(function);
    add_function_return_moves(function);
    process_function_varargs(function);

    // Caller
    process_function_call_arg_allocations(function);
    reverse_function_call_args_order(function);
    add_function_call_result_moves(function);
    add_function_call_arg_moves(function);
}

void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac) {
    // Clobber all temporaries
    for (int i = 0; i < clobbered_registers_in_function_call_count; i++)
        clobber_livenow(ig, vreg_count, livenow, tac, clobbered_registers_in_function_call[i]);

    // Integer arguments are clobbered, except for r0 and r1 which may be used for results.
    for (int i = 2; i < 8; i++)
        clobber_livenow(ig, vreg_count, livenow, tac, int_arg_registers[i]);

    // Unless the function returns something in r00, clobber r00
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_R00))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_R00);

    // Unless the function returns something in r01, clobber r01
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_R01))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_R01);

    // FP arguments are clobbered, except for v0 and v1 which may be used for results.
    for (int i = 2; i < 8; i++)
        clobber_livenow(ig, vreg_count, livenow, tac, fp_arg_registers[i]);

    // Unless the function returns something in v00, clobber v00
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_V00))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_V00);

    // Unless the function returns something in v01, clobber v01
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_V01))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_V01);
}

