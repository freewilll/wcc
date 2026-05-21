#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "wcc.h"
#include "x86_64.h"

// DWARF constants taken from dwarf.h
#define DWARF_VERSION       4

#define DW_LANG_C89         0x0001

#define DW_TAG_compile_unit 0x11

#define DW_AT_name          0x03
#define DW_AT_stmt_list     0x10
#define DW_AT_low_pc        0x11
#define DW_AT_high_pc       0x12
#define DW_AT_language      0x13
#define DW_AT_comp_dir      0x1b
#define DW_AT_producer      0x25

#define DW_FORM_addr        0x01
#define DW_FORM_data8       0x07
#define DW_FORM_data1       0x0b
#define DW_FORM_strp        0x0e
#define DW_FORM_sec_offset  0x17

static int need_ru4_to_ld_symbol;
static int need_ld_to_ru4_symbol;

static int cur_stack_push_count; // Used in codegen to keep track of stack position

static void check_preg(int preg, int preg_class) {
    if (preg == -1) panic("Illegal attempt to output -1 preg");
    if (preg < 0 || preg >= 32) panic("Illegal preg %d", preg);
    if (preg_class == PC_INT && preg >= 16)
        panic("Illegal int preg %d", preg);
}

static void append_byte_register_name(char *buffer, int preg) {
    check_preg(preg, PC_INT);
    char *names = "al   bl   cl   dl   sil  dil  bpl  spl  r8b  r9b  r10b r11b r12b r13b r14b r15b";
         if (preg < 4)  sprintf(buffer, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) sprintf(buffer, "%%%.3s", &names[preg * 5]);
    else                sprintf(buffer, "%%%.4s", &names[preg * 5]);
}

static void append_word_register_name(char *buffer, int preg) {
    check_preg(preg, PC_INT);
    char *names = "ax   bx   cx   dx   si   di   bp   sp   r8w  r9w  r10w r11w r12w r13w r14w r15w";
         if (preg < 8)  sprintf(buffer, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) sprintf(buffer, "%%%.3s", &names[preg * 5]);
    else                sprintf(buffer, "%%%.4s", &names[preg * 5]);
}

static void append_long_register_name(char *buffer, int preg) {
    check_preg(preg, PC_INT | PC_FP);
    char *names = "eax   ebx   ecx   edx   esi   edi   ebp   esp   r8d   r9d   r10d  r11d  r12d  r13d  r14d  r15d  xmm0  xmm1  xmm2  xmm3  xmm4  xmm5  xmm6  xmm7  xmm8  xmm9  xmm10 xmm11 xmm12 xmm13 xmm14 xmm15";
         if (preg < 10) sprintf(buffer, "%%%.3s", &names[preg * 6]);
    else if (preg < 26) sprintf(buffer, "%%%.4s", &names[preg * 6]);
    else                sprintf(buffer, "%%%.5s", &names[preg * 6]);
}

static void append_quad_register_name(char *buffer, int preg) {
    check_preg(preg, PC_INT | PC_FP);
    char *names = "rax   rbx   rcx   rdx   rsi   rdi   rbp   rsp   r8    r9    r10   r11   r12   r13   r14   r15   xmm0  xmm1  xmm2  xmm3  xmm4  xmm5  xmm6  xmm7  xmm8  xmm9  xmm10 xmm11 xmm12 xmm13 xmm14 xmm15";
         if (preg == 8 || preg == 9) sprintf(buffer, "%%%.2s", &names[preg * 6]);
    else if (preg < 16)              sprintf(buffer, "%%%.3s", &names[preg * 6]);
    else if (preg < 26)              sprintf(buffer, "%%%.4s", &names[preg * 6]);
    else                             sprintf(buffer, "%%%.5s", &names[preg * 6]);
}

char *register_name(int preg) {
    char *buffer = wmalloc(16);
    buffer[0] = 0;
    append_quad_register_name(buffer, preg);
    return buffer;
}

void process_stack_offset(Value *value, int *stack_alignments, int *stack_sizes) {
    if (value && value->stack_index < 0) {
        // When in doubt, pick the biggest of size & alignment. There can be
        // mismatches during struct/union manipulation, where a bit of the stack
        // is loaded/saved into up 8 bytes.

        int value_alignment = get_type_alignment(value->type);
        if (value_alignment > stack_alignments[-value->stack_index]) stack_alignments[-value->stack_index] = value_alignment;
        int value_size = get_type_size(value->type);
        if (value_size > stack_sizes[-value->stack_index]) stack_sizes[-value->stack_index] = value_size;
    }
}

// Allocate stack offsets for variables on the stack (stack_index < 0). Go backwards
// in alignment, allocating the ones with the largest alignment first, in order of
// stack_index.
void make_stack_offsets(Function *function) {
    int count = function->stack_register_count;

    if (!count) return; // Nothing is on the stack

    // Determine size & alignments for all variables on the stack
    int *stack_alignments = wcalloc((count + 1), sizeof(int));
    int *stack_sizes = wcalloc((count + 1), sizeof(int));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst)  process_stack_offset(tac->dst,  stack_alignments, stack_sizes);
        if (tac->src1) process_stack_offset(tac->src1, stack_alignments, stack_sizes);
        if (tac->src2) process_stack_offset(tac->src2, stack_alignments, stack_sizes);
    }

    // Determine stack offsets
    int *stack_offsets = wmalloc((count+ 1) * sizeof(int));
    int offset = 0;
    int total_size = 0;
    for (int size = 4; size >= 0; size--) {
        int wanted_alignment = 1 << size;
        for (int i = 1; i <= count; i++) {
            int alignment = stack_alignments[i];
            int object_size = stack_sizes[i];
            if (wanted_alignment != alignment) continue;
            offset += object_size;
            stack_offsets[i] = offset;
            total_size += object_size;
        }
    }

    if (debug_stack_frame_layout) {
        printf("Stack frame for %s:\n", function->identifier);
        for (int i = 1; i <= count; i++) {
            printf("Slot %d offset=%4d size=%4d alignment=%4d\n", i, stack_offsets[i], stack_sizes[i], stack_alignments[i]);
        }
        printf("\n");
    }

    // Align on 8 bytes, this is required by the function call stack alignment code.
    total_size = (total_size + 7) & ~7;
    function->stack_size = total_size;

    // Assign stack_offsets
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Special case for functions with a va_list. A src1 with stack_index OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX
        // needs to be reassigned with address where the pushed varargs start
        if (tac->src1 && tac->src1->stack_index == OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX) {
            tac->src1->stack_index = 2 + (function->fpa->size >> 3);
        }
        else
            if (tac->src1 && tac->src1->stack_index < 0) tac->src1->stack_offset = stack_offsets[-tac->src1->stack_index];

        if (tac ->dst && tac ->dst->stack_index < 0) tac ->dst->stack_offset = stack_offsets[-tac ->dst->stack_index];
        if (tac->src2 && tac->src2->stack_index < 0) tac->src2->stack_offset = stack_offsets[-tac->src2->stack_index];
    }

    wfree(stack_alignments);
    wfree(stack_sizes);
    wfree(stack_offsets);
}

// Get offset from the stack in bytes, from a stack_index for function args
// or stack offset for local params.
// The stack layout for a function with 8 parameters (function_pc=8)
// stack index  offset    what
// 7            +56       arg 9, e.g. an int
// 6            +48       arg 8, e.g. an int
// 4            +32       arg 7, e.g. a long double, aligned on 16 bytes
//                        alignment gap
// 2            +16       arg 6, e.g. an int
//              +8        return address
//              +0        Pushed BP
// -1           -8        first local variable / spilled register e.g. long
// -2           -12       second local variable / spilled register e.g. int
// -3           -16       second local variable / spilled register e.g. int
static int get_stack_offset(int function_pc, Value *v) {
    int stack_index = v->stack_index;

    if (stack_index >= 2)
        // Function parameter
        return 8 * stack_index;
    else if (stack_index < 0) {
        if (!v->stack_offset && !debug_instsel_tiling) panic("Unexpected zero stack offset");
        return -v->stack_offset;
    }
    else
        panic("Unexpected zero stack_index");
}

static void check_floating_point_literal_max(void) {
    if (floating_point_literal_count >= MAX_FLOATING_POINT_LITERALS) panic("Exceeded max floating point literals %d", MAX_FLOATING_POINT_LITERALS);
}

static int add_float_literal(Value *value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].f = value->fp_value;
    floating_point_literals[floating_point_literal_count].type = TYPE_FLOAT;
    return floating_point_literal_count++;
}

static int add_double_literal(Value *value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].d = value->fp_value;
    floating_point_literals[floating_point_literal_count].type = TYPE_DOUBLE;
    return floating_point_literal_count++;
}

static int add_long_double_literal(Value *value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].ld = value->fp_value;
    floating_point_literals[floating_point_literal_count].type = TYPE_LONG_DOUBLE;
    return floating_point_literal_count++;
}

char *render_target_operation(Tac *tac, int function_pc, int expect_preg) {
    char *t = tac->target_template;

    if (!t) return 0;

    char *buffer = wcalloc(1, 128);
    char *result = buffer;

    while (*t && *t != ' ') *buffer++ = *t++;
    while (*t && *t == ' ') t++;

    if (*t) {
        int mnemonic_length = buffer - result;
        for (int i = 0; i < 12 - mnemonic_length; i++) *buffer++ = ' ';

             if (strlen(t) >= 8 && !memcmp(t, ".RU4TOLD", 8)) need_ru4_to_ld_symbol = 1;
        else if (strlen(t) >= 8 && !memcmp(t, ".LDTORU4", 8)) need_ld_to_ru4_symbol = 1;
    }

    while (*t) {
        if (*t == '%') {
            if (t[1] == '%') {
                *buffer++ = '%';
                t += 1;
            }
            else {
                Value *v;

                t++;

                if (t[0] != 'v') panic("Unknown placeholder in %s", tac->target_template);

                t++;

                     if (t[0] == '1') v = tac->src1;
                else if (t[0] == '2') v = tac->src2;
                else if (t[0] == 'd') v = tac->dst;
                else panic("Indecipherable placeholder \"%s\"", tac->target_template);

                int x86_size = 0;
                int is_offset = 0;
                int offset_is_required = 0;

                     if (t[1] == 'b') { t++; x86_size = 1; }
                else if (t[1] == 'w') { t++; x86_size = 2; }
                else if (t[1] == 'l') { t++; x86_size = 3; }
                else if (t[1] == 'q') { t++; x86_size = 4; }

                else if (t[1] == 'o') {
                    t++; x86_size = 4;
                    is_offset = 1;
                    if (t[1] == 'r') {
                        offset_is_required = 1;
                        t++;
                    }
                }

                int low = 0;
                int high = 0;
                int float_arg = 0;
                int double_arg = 0;
                int float_literal = 0;
                int double_literal = 0;
                int long_double_literal = 0;

                switch (t[1]) {
                    case 'L': t++; low  = 1; break;
                    case 'H': t++; high = 1; break;
                    case 'f': t++; float_arg = 1; break;
                    case 'd': t++; double_arg = 1; break;
                    case 'C': t++; long_double_literal = 1; break;
                    case 'F': t++; float_literal = 1; x86_size = 3; break;
                    case 'D': t++; double_literal = 1; x86_size = 4; break;
                }

                if (!v) panic("Unexpectedly got a null value while the template %s is expecting it", tac->target_template);

                if (is_offset) {
                    if (v->offset || offset_is_required) sprintf(buffer, "%d", v->offset);
                }
                else if (!expect_preg && v->vreg) {
                    if (!x86_size) panic("Missing size on register value \"%s\"", tac->target_template);
                    if (v->global_symbol) panic("Got global symbol in vreg");

                    *buffer++ = 'r';
                    sprintf(buffer, "%d", v->vreg);
                    while (*buffer) buffer++;
                    *buffer++ = size_to_x86_size(x86_size);
                }
                else if (expect_preg && v->preg != -1) {
                    if (!x86_size) panic("Missing size on register value \"%s\"", tac->target_template);

                         if (x86_size == 1) append_byte_register_name(buffer, v->preg);
                    else if (x86_size == 2) append_word_register_name(buffer, v->preg);
                    else if (x86_size == 3) append_long_register_name(buffer, v->preg);
                    else if (x86_size == 4) append_quad_register_name(buffer, v->preg);
                    else panic("Unknown register size %d", x86_size);
                }
                else if (v->is_constant) {
                    if (is_floating_point_type(v->type)) {
                        if (low)
                            sprintf(buffer, "$%ld", ((long *) &v->fp_value)[0]);
                        else if (high)
                            // The & is to be compatible with gcc
                            sprintf(buffer, "$%ld", ((long *) &v->fp_value)[1] & 0xffff);
                        else if (float_arg) {
                            float f = v->fp_value;
                            sprintf(buffer, "$%d", *((int *) &f));
                        }
                        else if (double_arg) {
                            double d = v->fp_value;
                            sprintf(buffer, "$%ld", *((long *) &d));
                        }
                        else if (float_literal)
                            sprintf(buffer, ".LFP%d(%%rip)", add_float_literal(v));
                        else if (double_literal)
                            sprintf(buffer, ".LFP%d(%%rip)", add_double_literal(v));
                        else if (long_double_literal)
                            sprintf(buffer, ".LFP%d(%%rip)", add_long_double_literal(v));
                        else
                            panic("Did not get L/H/C/F/D specifier for floating point constant");
                    }
                    else {
                        if (!x86_size) {
                            print_value(stdout, v, 0);
                            printf("\n");
                            panic("Did not get x86 size on template %s", tac->target_template);
                        }

                        if (x86_size == 1)
                            sprintf(buffer, "%ld", (long) (v->int_value & 0xff));
                        else if (x86_size == 2)
                            sprintf(buffer, "%ld", (long) (v->int_value & 0xffff));
                        else if (x86_size == 3)
                            sprintf(buffer, "%ld", (long) (v->int_value & 0xffffffff));
                        else
                            sprintf(buffer, "%ld", (long) v->int_value);
                    }
                }
                else if (v->is_string_literal)
                    sprintf(buffer, ".LS%d(%%rip)", v->string_literal_index);
                else if (v->global_symbol) {
                    if (v->type->type == TYPE_LONG_DOUBLE) {
                        if (low) {
                            if (v->offset)
                                sprintf(buffer, "%s+%d(%%rip)", v->global_symbol->global_identifier, v->offset);
                            else {
                                if (v->load_from_got)
                                    sprintf(buffer, "%s@GOTPCREL(%%rip)", v->global_symbol->global_identifier);
                                else
                                    sprintf(buffer, "%s(%%rip)", v->global_symbol->global_identifier);
                            }
                        }
                        else if (high)
                            sprintf(buffer, "%s+%d(%%rip)", v->global_symbol->global_identifier, v->offset + 8);
                        else
                            panic("Did not get L/H/C specifier for double long constant");
                    }
                    else {
                        if (v->offset)
                            sprintf(buffer, "%s+%d(%%rip)", v->global_symbol->global_identifier, v->offset);
                        else {
                            if (v->load_from_got)
                                sprintf(buffer, "%s@GOTPCREL(%%rip)", v->global_symbol->global_identifier);
                            else
                                sprintf(buffer, "%s(%%rip)", v->global_symbol->global_identifier);
                        }
                    }
                }
                else if (v->stack_index) {
                    int stack_offset = get_stack_offset(function_pc, v);
                    if (v->type->type == TYPE_LONG_DOUBLE) {
                        if (low)
                            sprintf(buffer, "%d(%%rbp)", stack_offset + v->offset);
                        else if (high)
                            sprintf(buffer, "%d(%%rbp)", stack_offset + v->offset + 8);
                        else
                            panic("Did not get L/H specifier for double long stack index");
                    }
                    else
                        sprintf(buffer, "%d(%%rbp)", stack_offset + v->offset);
                }
                else if (v->label)
                    sprintf(buffer, ".L%d", v->label);
                else {
                    print_value(stdout, v, 0);
                    printf("\n");
                    panic("Don't know how to render template value");
                }
            }
        }
        else
            *buffer++ = *t;

        while (*buffer) buffer++;
        t++;
    }

    return result;
}

static void output_x86_operation(Tac *tac, int function_pc) {
    char *buffer = render_target_operation(tac, function_pc, 1);
    if (buffer) {
        fprintf(output_file, "    %s\n", buffer);
        wfree(buffer);
    }
}

// Determine which registers are used in a function, push them onto the stack and return the list
static Tac *insert_push_callee_saved_registers(Tac *ir, Tac *tac, int *saved_registers) {
    while (tac) {
        if (tac->dst  && tac->dst ->preg != -1 && callee_saved_registers[tac->dst ->preg]) saved_registers[tac->dst ->preg] = 1;
        if (tac->src1 && tac->src1->preg != -1 && callee_saved_registers[tac->src1->preg]) saved_registers[tac->src1->preg] = 1;
        if (tac->src2 && tac->src2->preg != -1 && callee_saved_registers[tac->src2->preg]) saved_registers[tac->src2->preg] = 1;
        tac = tac->next;
    }

    for (int i = 0; i < physical_register_count; i++) {
        if (saved_registers[i]) {
            cur_stack_push_count++;
            ir = insert_target_instruction(ir, X86_OP_PUSH, new_preg_value(i), 0, 0, "push %vdq");
        }
    }

    return ir;
}

static Tac *insert_end_of_function(Tac *ir, int *saved_registers) {
    for (int i = physical_register_count - 1; i >= 0; i--)
        if (saved_registers[i])
            ir = insert_target_instruction(ir, X86_OP_POP, new_preg_value(i), 0, 0, "popq %vdq");

    ir = insert_target_instruction(ir, X86_OP_LEAVE, 0, 0, 0, "leaveq");
    return insert_target_instruction(ir, X86_OP_RET_FROM_FUNC, 0, 0, 0, "retq");
}

static Tac *add_sub_rsp(Tac *ir, int amount) {
    return insert_target_instruction(ir, X86_OP_SUB, new_preg_value(REG_RSP), new_integral_constant(TYPE_LONG, amount), 0, "subq $%v1q, %vdq");
}

static Tac *add_add_rsp(Tac *ir, int amount) {
    return insert_target_instruction(ir, X86_OP_ADD, new_preg_value(REG_RSP), new_integral_constant(TYPE_LONG, amount), 0, "addq $%v1q, %vdq");
}

// Add prologue, epilogue, stack alignment pushes/pops, function calls and main() return result
void add_final_instructions(Function *function) {
    int stack_size;             // Size of the stack containing local variables and spilled registers
    int *saved_registers;       // Callee saved registers
    int added_end_of_function;  // To ensure a double epilogue isn't emitted

    Tac *ir = function->ir;

    cur_stack_push_count = 2; // Program counter and rbp

    // Add function prologue
    ir = insert_target_instruction(ir, X86_OP_PUSH, new_preg_value(REG_RBP), 0, 0, "push %vdq");
    ir = insert_target_instruction(ir, X86_OP_MOV, new_preg_value(REG_RBP), new_preg_value(REG_RSP), 0, "mov %v1q, %vdq");

    // Allocate stack space for local variables and spilled registers
    stack_size = function->stack_size;
    if (stack_size > 0) {
        ir = add_sub_rsp(ir, stack_size);
        cur_stack_push_count += stack_size / 8;
    }

    saved_registers = wcalloc(sizeof(int), physical_register_count);

    ir = insert_push_callee_saved_registers(ir, function->ir, saved_registers);

    while (ir) {
        added_end_of_function = 0;

        switch (ir->operation.id) {
            case IR_NOP:
                break;

            case IR_START_LOOP:
            case IR_END_LOOP:
                ir->operation.id = IR_NOP;
                break;

            case IR_START_CALL: {
                if (ir->operation.id == IR_START_CALL) {
                    ir->operation.id = IR_NOP;

                    int alignment_pushes = 0;
                    if (ir->src1->function_call.function_call_arg_stack_padding >= 8)
                        alignment_pushes++;

                    // Align the stack. This is matched with an adjustment when the function call ends
                    int need_aligned_call_push = ((cur_stack_push_count + ir->src1->function_call.function_call_arg_push_count) % 2 == 1);
                    if (need_aligned_call_push) {
                        ir->src1->function_call.function_call_arg_push_count++;
                        alignment_pushes++;
                    }

                    // Special case of an alignment push to align the whole stack structure
                    // combined with padding at the end of the stack. Eliminate both alignments
                    // to save 16 bytes to stack space.
                    if (alignment_pushes == 2) {
                        ir->src1->function_call.function_call_arg_push_count -= 2;
                        alignment_pushes = 0;
                    }

                    if (alignment_pushes) {
                        cur_stack_push_count += alignment_pushes;
                        ir = add_sub_rsp(ir, alignment_pushes * 8);
                    }
                }

                break;
            }

            case IR_END_CALL: {
                ir->operation.id = IR_NOP;

                // Adjust the stack for any args that are on in stack
                int function_call_arg_push_count = ir->src1->function_call.function_call_arg_push_count;
                if (function_call_arg_push_count > 0) {
                    cur_stack_push_count -= function_call_arg_push_count;
                    ir = add_add_rsp(ir, function_call_arg_push_count * 8);
                }

                break;
            }

            case X86_OP_ARG:
                cur_stack_push_count++;
                break;

            case IR_ARG_STACK_PADDING:
                // This alignment push is needed for structures that are aligned
                // on 16-bytes and are preceded in memory by something that left the stack
                // aligned on 8-bytes.
                cur_stack_push_count++;
                ir = add_sub_rsp(ir, 8);

                break;

            case X86_OP_ALLOCATE_STACK:
                cur_stack_push_count += ir->src1->int_value / 8;

                break;

            case X86_OP_CALL: {
                ir->operation.id = IR_NOP;

                Tac *orig_ir = ir;

                // A function can be either a direct function or a function pointer
                Type *function_type = ir->src1->type->type == TYPE_FUNCTION ? ir->src1->type : ir->src1->type->target;
                if (function_type->function->is_variadic) {
                    char *buffer;
                    wasprintf(&buffer, "movb $%d, %%vdb", ir->src1->function_call.function_call_fp_register_arg_count);
                    append_to_list(allocated_strings, buffer);
                    ir = insert_target_instruction(ir, X86_OP_MOV, new_preg_value(REG_RAX), 0, 0, buffer);
                }

                Tac *tac = new_instruction(X86_OP_CALL_FROM_FUNC);

                if (!orig_ir->src1->function_call.function_symbol) {
                    wasprintf(&(tac->target_template), "callq *%%v1q");
                    append_to_list(allocated_strings, tac->target_template);
                    tac->src1 = orig_ir->src1;
                }
                else {
                    // If a function has been defined locally, call it directly, otherwise use the PLT
                    if (orig_ir->src1->function_call.function_symbol->function && orig_ir->src1->function_call.function_symbol->function->is_defined) {
                         wasprintf(&(tac->target_template), "callq %s", orig_ir->src1->function_call.function_symbol->global_identifier);
                         append_to_list(allocated_strings, tac->target_template);
                     }
                    else {
                         wasprintf(&(tac->target_template), "callq %s@PLT", orig_ir->src1->function_call.function_symbol->global_identifier);
                         append_to_list(allocated_strings, tac->target_template);
                     }
                 }

                ir = insert_tac_after(ir, tac);
                break;
            }

            case IR_RETURN:
                ir = insert_end_of_function(ir, saved_registers);
                added_end_of_function = 1;
        }

        ir = ir->next;
    }

    ir = function->ir;
    while (ir->next) ir = ir->next;

    // Special case for main, return 0 if no return statement is present
    if (function_is_main(function))
        ir = insert_target_instruction(ir, X86_OP_MOV, new_preg_value(REG_RAX), 0, 0, "movq $0, %vdq");

    if (!added_end_of_function)
        insert_end_of_function(ir, saved_registers);

    wfree(saved_registers);
}

// Merge any consecutive add/sub stack operations that aren't involved in jmp instructions.
void merge_rsp_func_call_add_subs(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (
                // First operation is add/sub n, %rsp
                (tac->operation.id == X86_OP_ADD || tac->operation.id == X86_OP_SUB) && tac->dst && tac->dst->preg == REG_RSP &&
                // Second operation is add/sub n, %rsp
                tac->next && (tac->next->operation.id == X86_OP_ADD || tac->next->operation.id == X86_OP_SUB) && tac->next->dst && tac->next->dst->preg == REG_RSP &&
                // No labels are involved
                !tac->label && !tac->next->label) {

            int value = tac->operation.id == X86_OP_ADD ? tac->src1->int_value : -tac->src1->int_value;
            value += tac->next->operation.id == X86_OP_ADD ? tac->next->src1->int_value : -tac->next->src1->int_value;
            if (!value) {
                tac = delete_instruction(tac);
                tac = delete_instruction(tac);
            } else {
                tac = delete_instruction(tac);
                tac->operation.id = value < 0 ? X86_OP_SUB : X86_OP_ADD;
                tac->target_template = value < 0 ? "subq $%v1q, %vdq" : "addq $%v1q, %vdq";
                tac->src1->int_value = value > 0 ? value : -value;
            }
        }
    }
}

void optimize_final_instructions(Function *function) {
    merge_rsp_func_call_add_subs(function);
}

// Add a ".loc" line with an integer identifying the filename and the line number.
// The debug_strings map has the mapping from filename to id.
static void output_debug_loc(Tac *tac) {
    if (opt_debug_symbols && tac->origin && tac->origin->filename) {
        int id = (long) strmap_get(debug_strings, tac->origin->filename);
        if (!id) {
            id = ++debug_string_counter;
            strmap_put(debug_strings, wstrdup(tac->origin->filename), (void *) (long) id);
            fprintf(output_file, "    .file       %d \"%s\"\n", id, tac->origin->filename);
        }

        if (id != last_outputted_filename_id || tac->origin->line_number != last_outputted_filename_line_number) {
            fprintf(output_file, "    .loc        %d %d\n", id, tac->origin->line_number);
            last_outputted_filename_id = id;
            last_outputted_filename_line_number = tac->origin->line_number;
        }
    }
}

// Output code from the IR of a function
static void output_function_body_code(Symbol *symbol) {
    int function_pc = symbol->function->type->function->param_count;

    for (Tac *tac = symbol->function->ir; tac; tac = tac->next) {
        if (tac->label) fprintf(output_file, ".L%d:\n", tac->label);
        if (tac->operation.id != IR_NOP) {
            output_debug_loc(tac);
            output_x86_operation(tac, function_pc);
        }
    }
}

// Output data for a defined object symbol
void output_defined_object_symbol(Symbol *symbol) {
    if (!symbol->initializers) panic("Expected initializers for a symbol with definition status defined");

    int size = get_type_size(symbol->type);
    if (symbol->linkage == LINKAGE_EXTERNAL)
        fprintf(output_file, "    .globl   %s\n", symbol->global_identifier);

    if (elf_section != SEC_DATA) { fprintf(output_file, "    .data\n"); elf_section = SEC_DATA; }

    fprintf(output_file, "    .align   %d\n", get_type_alignment(symbol->type));
    fprintf(output_file, "    .type    %s, @object\n", symbol->global_identifier);
    fprintf(output_file, "    .size    %s, %d\n", symbol->global_identifier, size);
    fprintf(output_file, "%s:\n", symbol->global_identifier);

    for (int i = 0; i < symbol->initializers->length; i++) {
        Initializer *in = (Initializer *) symbol->initializers->elements[i];

        if (in->is_address_of || in->symbol) {
            if (in->address_of_offset)
                fprintf(output_file,"    .quad    %s + %d\n", in->symbol->global_identifier, in->address_of_offset);
            else
                fprintf(output_file,"    .quad    %s\n", in->symbol->global_identifier);
            size -= 8;
        }
        else if (in->is_string_literal) {
            if (in->address_of_offset)
                fprintf(output_file,"    .quad    .LS%d + %d\n", in->string_literal_index, in->address_of_offset);
            else
                fprintf(output_file,"    .quad    .LS%d\n", in->string_literal_index);
            size -= 8;
        }
        else {
            if (!in->data) {
                if (in->size < 0)
                    panic("Got negative .zero %d for the intializer for %s", in->size, symbol->identifier);
                fprintf(output_file,"    .zero    %d\n", in->size);
            }
            else if (in->size == 1) fprintf(output_file, "    .byte    %d\n",  *((char *)  in->data));
            else if (in->size == 2) fprintf(output_file, "    .word    %d\n",  *((short *) in->data));
            else if (in->size == 4) fprintf(output_file, "    .long    %d\n",  *((int *)   in->data));
            else if (in->size == 8) fprintf(output_file, "    .quad    %ld\n", *((long *)  in->data));
            else if (in->is_int128) {
                // int128
                fprintf(output_file, "    .quad    %ld\n", ((long *) in->data)[0]);
                fprintf(output_file, "    .quad    %ld\n", ((long *) in->data)[1]);
            }
            else if (in->size == 16) {
                // Long double
                fprintf(output_file, "    .long   %d\n", (((int *) in->data))[0]);
                fprintf(output_file, "    .long   %d\n", (((int *) in->data))[1]);
                fprintf(output_file, "    .long   %d\n", (((int *) in->data))[2] & 0xffff);
                fprintf(output_file, "    .long   0\n");
            }
            else panic("Unknown initializer size=%d data=%p\n", in->size, in->data);
            size -= in->size;
        }
    }

    // Add padding for structs that have padding at the end
    if (size < 0)
        panic("Got negative .zero padding %d for final padding", size);

    if (size) fprintf(output_file,"    .zero    %d\n", size);
}

// Output a debug_info section with only a DW_TAG_compile_unit with a corresponding
// debug_abbrev section. debug_str contains debug strings. This is enough information
// to have the source filename, directory, function names and line numbers.
void output_debug_sections(char *input_filename) {
    char *cwd = wmalloc(1024);
    if (!getcwd(cwd, 1024)) panic("Unable to get cwd");

    // Output debug_info section
    fprintf(output_file, "    .section .debug_info,\"\",@progbits\n\n");

    fprintf(output_file, ".Ldebug_info0:\n");
    fprintf(output_file, "    .long   .Ldebug_info_end - .Ldebug_info_start\n"); // Size

    fprintf(output_file, ".Ldebug_info_start:\n");
    fprintf(output_file, "    .value  %d\n", DWARF_VERSION);
    fprintf(output_file, "    .long   .debug_abbrev\n");      // Pointer to debug_abbrev section
    fprintf(output_file, "    .byte   0x8\n");                // Pointer size

    // Output DW_TAG_compile_unit
    fprintf(output_file, "    .uleb128 0x1\n");                               // DW_TAG_compile_unit
    fprintf(output_file, "    .long   .Ldebug_info.producer\n");              // DW_AT_producer
    fprintf(output_file, "    .byte   %d\n", DW_LANG_C89);                    // DW_AT_language
    fprintf(output_file, "    .long   .Ldebug_info.filename\n");              // DW_AT_name
    fprintf(output_file, "    .long   .debug_info.cwd\n");                    // DW_AT_comp_dir
    fprintf(output_file, "    .quad   .Lall.code.start\n");                   // DW_AT_low_pc (start of code)
    fprintf(output_file, "    .quad   .Lall.code.end-.Lall.code.start \n");   // DW_AT_high_pc (size of code)
    fprintf(output_file, "    .long   .Lline_table_start\n");                 // DW_AT_stmt_list (pointer to line number table)
    fprintf(output_file, ".Ldebug_info_end:\n\n");

    // Output debug_abbrev section. All lines with DW are pairs of a key & data type
    fprintf(output_file, "    .section    .debug_abbrev,\"\",@progbits\n");

    fprintf(output_file, "    .uleb128 0x1\n");                       // type number 1
    fprintf(output_file, "    .uleb128 %d\n", DW_TAG_compile_unit);   // tag: DW_TAG_compile_unit
    fprintf(output_file, "    .byte   0\n");                          // has children 0
    fprintf(output_file, "    .uleb128 %d\n", DW_AT_producer);        // DW_AT_producer / DW_FORM_strp
    fprintf(output_file, "    .uleb128 %d\n", DW_FORM_strp);
    fprintf(output_file, "    .uleb128 %d\n", DW_AT_language);        // DW_AT_language / DW_FORM_data1
    fprintf(output_file, "    .uleb128 %d\n", DW_FORM_data1);
    fprintf(output_file, "    .uleb128 %d\n", DW_AT_name);            // DW_AT_name / DW_FORM_strp
    fprintf(output_file, "    .uleb128 %d\n", DW_FORM_strp);
    fprintf(output_file, "    .uleb128 %d\n", DW_AT_comp_dir);        // DW_AT_comp_dir / DW_FORM_strp
    fprintf(output_file, "    .uleb128 %d\n", DW_FORM_strp);
    fprintf(output_file, "    .uleb128 %d\n", DW_AT_low_pc);          // DW_AT_low_pc / DW_FORM_addr
    fprintf(output_file, "    .uleb128 %d\n", DW_FORM_addr);
    fprintf(output_file, "    .uleb128 %d\n", DW_AT_high_pc);         // DW_AT_high_pc / DW_FORM_data8
    fprintf(output_file, "    .uleb128 %d\n", DW_FORM_data8);
    fprintf(output_file, "    .uleb128 %d\n", DW_AT_stmt_list);       // DW_AT_stmt_list / DW_FORM_sec_offset
    fprintf(output_file, "    .uleb128 %d\n", DW_FORM_sec_offset);
    fprintf(output_file, "    .byte   0\n");                          // End
    fprintf(output_file, "    .byte   0\n");
    fprintf(output_file, "    .byte   0\n");

    // Output debug_str section
    fprintf(output_file, "\n    .section    .debug_str,\"MS\",@progbits,1\n");
    fprintf(output_file, ".Ldebug_info.producer:\n");
    fprintf(output_file, "    .string  \"wcc\"\n");
    fprintf(output_file, ".Ldebug_info.filename:\n");
    fprintf(output_file, "    .string  \"%s\"\n", input_filename);
    fprintf(output_file, ".debug_info.cwd:\n");
    fprintf(output_file, "    .string  \"%s\"\n", cwd);

    // Output debug_line section. There's nothing much here, the assembler populates
    // this with information from the .loc lines.
    fprintf(output_file, "\n    .section    .debug_line,\"\",@progbits\n");
    fprintf(output_file, "\n.Lline_table_start:\n");

    wfree(cwd);
}

// Output code for the translation unit
void output_code(char *input_filename, char *output_filename) {
    open_output_file(input_filename, output_filename);
    output_object_symbols();

    // Output string literals
    if (string_literal_count > 0) {
        fprintf(output_file, "\n    .section .rodata\n\n");
        fprintf(output_file, ".Ltext0:\n\n");
        for (int i = 0; i < string_literal_count; i++) {
            StringLiteral *sl = &(string_literals[i]);
            if (sl->is_wide_char) fprintf(output_file, "    .align   4\n");
            fprintf(output_file, ".LS%d:\n", i);
            fprintf_escaped_string_literal(output_file, sl, 1);
        }
        fprintf(output_file, "\n");
    }

    // Output code
    fprintf(output_file, "    .text\n");

    // Output symbols for all functions that are defined and have external linkage
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            if (symbol->linkage == LINKAGE_EXTERNAL)
                fprintf(output_file, "    .globl  %s\n", symbol->identifier);
            fprintf(output_file, "    .type   %s, @function\n", symbol->global_identifier);
        }
    }

    fprintf(output_file, "\n");

    label_count = 0; // Used in label renumbering

    string_literal_count = 0;

    // Output functions code
    need_ru4_to_ld_symbol = 0;
    need_ld_to_ru4_symbol = 0;
    fprintf(output_file, ".Lall.code.start:\n");
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            fprintf(output_file, "%s:\n", symbol->identifier);
            fprintf(output_file, ".L%s.start:\n", symbol->identifier);
            output_function_body_code(symbol);
            fprintf(output_file, "    .size       %s, .-%s\n", symbol->global_identifier, symbol->global_identifier);
            fprintf(output_file, ".L%s.end:\n", symbol->identifier);
            fprintf(output_file, "\n");
        }
    }
    fprintf(output_file, ".Lall.code.end:\n\n");

    // Output floating point literals
    if (floating_point_literal_count > 0) {
        for (int i = 0; i < floating_point_literal_count; i++) {
            // The zero and & is to be compatible with gcc
            fprintf(output_file, ".LFP%d:\n", i);

            if (floating_point_literals[i].type == TYPE_FLOAT) {
                float fl = floating_point_literals[i].f;
                fprintf(output_file, "    .long   %d\n", *((int *) &fl));
            }
            else if (floating_point_literals[i].type == TYPE_DOUBLE) {
                double d = floating_point_literals[i].d;
                fprintf(output_file, "    .long   %d\n", *((int *) &d));
                fprintf(output_file, "    .long   %d\n", *((int *) &d + 1));
            }
            else {
                long double ld = floating_point_literals[i].ld;
                fprintf(output_file, "    .long   %d\n", ((int *) &ld)[0]);
                fprintf(output_file, "    .long   %d\n", ((int *) &ld)[1]);
                fprintf(output_file, "    .long   %d\n", ((int *) &ld)[2] & 0xffff);
                fprintf(output_file, "    .long   0\n");
            }
        }
    }

    if (need_ru4_to_ld_symbol) {
        fprintf(output_file, ".RU4TOLD:\n");
        fprintf(output_file, "    .long   0\n");
        fprintf(output_file, "    .long   1602224128 # 0x5f800000\n");
        fprintf(output_file, "\n");
    }

    if (need_ld_to_ru4_symbol) {
        fprintf(output_file, ".LDTORU4:\n");
        fprintf(output_file, "     .long   1593835520 # 9223372036854775808\n");
    }

    if (opt_debug_symbols) output_debug_sections(input_filename);

    fclose(output_file);
}

