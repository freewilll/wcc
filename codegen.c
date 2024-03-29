#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "wcc.h"

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
static int elf_section;

typedef enum elf_section {
    SEC_NONE,
    SEC_TEXT,
    SEC_DATA,
} ElfSection;

typedef struct floating_point_literal {
    int type;
    float f;
    double d;
    long double ld;
} FloatingPointLiteral;

static FloatingPointLiteral *floating_point_literals; // Each floating point literal has an index in this array
static int floating_point_literal_count;              // Amount of floating point literals

StrMap *debug_strings;                      // Map of debug strings to identifiers
int debug_string_counter;                   // Counter to uniquely identify a debug string
int last_outputted_filename_id;             // Keep track of last printed .loc filename id
int last_outputted_filename_line_number;    // Keep track of last printed .loc line number

static List *allocated_strings;

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
    check_preg(preg, PC_INT | PC_SSE);
    char *names = "eax   ebx   ecx   edx   esi   edi   ebp   esp   r8d   r9d   r10d  r11d  r12d  r13d  r14d  r15d  xmm0  xmm1  xmm2  xmm3  xmm4  xmm5  xmm6  xmm7  xmm8  xmm9  xmm10 xmm11 xmm12 xmm13 xmm14 xmm15";
         if (preg < 10) sprintf(buffer, "%%%.3s", &names[preg * 6]);
    else if (preg < 26) sprintf(buffer, "%%%.4s", &names[preg * 6]);
    else                sprintf(buffer, "%%%.5s", &names[preg * 6]);
}

static void append_quad_register_name(char *buffer, int preg) {
    check_preg(preg, PC_INT | PC_SSE);
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
void make_stack_offsets(Function *function, char *function_name) {
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
        printf("Stack frame for %s:\n", function_name);
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

static int add_float_literal(long double value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].f = value;
    floating_point_literals[floating_point_literal_count].type = TYPE_FLOAT;
    return floating_point_literal_count++;
}

static int add_double_literal(long double value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].d = value;
    floating_point_literals[floating_point_literal_count].type = TYPE_DOUBLE;
    return floating_point_literal_count++;
}

static int add_long_double_literal(long double value) {
    check_floating_point_literal_max();
    floating_point_literals[floating_point_literal_count].ld = value;
    floating_point_literals[floating_point_literal_count].type = TYPE_LONG_DOUBLE;
    return floating_point_literal_count++;
}

char *render_x86_operation(Tac *tac, int function_pc, int expect_preg) {
    char *t = tac->x86_template;

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

                if (t[0] != 'v') panic("Unknown placeholder in %s", tac->x86_template);

                t++;

                     if (t[0] == '1') v = tac->src1;
                else if (t[0] == '2') v = tac->src2;
                else if (t[0] == 'd') v = tac->dst;
                else panic("Indecipherable placeholder \"%s\"", tac->x86_template);

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

                if (!v) panic("Unexpectedly got a null value while the template %s is expecting it", tac->x86_template);

                if (is_offset) {
                    if (v->offset || offset_is_required) sprintf(buffer, "%d", v->offset);
                }
                else if (!expect_preg && v->vreg) {
                    if (!x86_size) panic("Missing size on register value \"%s\"", tac->x86_template);
                    if (v->global_symbol) panic("Got global symbol in vreg");

                    *buffer++ = 'r';
                    sprintf(buffer, "%d", v->vreg);
                    while (*buffer) buffer++;
                    *buffer++ = size_to_x86_size(x86_size);
                }
                else if (expect_preg && v->preg != -1) {
                    if (!x86_size) panic("Missing size on register value \"%s\"", tac->x86_template);

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
                            sprintf(buffer, ".FPL%d(%%rip)", add_float_literal(v->fp_value));
                        else if (double_literal)
                            sprintf(buffer, ".FPL%d(%%rip)", add_double_literal(v->fp_value));
                        else if (long_double_literal)
                            sprintf(buffer, ".FPL%d(%%rip)", add_long_double_literal(v->fp_value));
                        else
                            panic("Did not get L/H/C/F/D specifier for floating point constant");
                    }
                    else {
                        if (!x86_size) {
                            print_value(stdout, v, 0);
                            printf("\n");
                            panic("Did not get x86 size on template %s", tac->x86_template);
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
                    sprintf(buffer, ".SL%d(%%rip)", v->string_literal_index);
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
    char *buffer = render_x86_operation(tac, function_pc, 1);
    if (buffer) {
        fprintf(f, "    %s\n", buffer);
        wfree(buffer);
    }
}

// Add an instruction after ir and return ir of the new instruction
static Tac *insert_x86_instruction(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, char *x86_template) {
    Tac *tac = new_instruction(operation);
    tac->operation = operation;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->x86_template = x86_template;

    return insert_instruction_after(ir, tac);
}

static Value *new_preg_value(int preg) {
    Value *v = new_value();
    v->preg = preg;
    return v;
}

// Determine which registers are used in a function, push them onto the stack and return the list
static Tac *insert_push_callee_saved_registers(Tac *ir, Tac *tac, int *saved_registers) {
    while (tac) {
        if (tac->dst  && tac->dst ->preg != -1 && callee_saved_registers[tac->dst ->preg]) saved_registers[tac->dst ->preg] = 1;
        if (tac->src1 && tac->src1->preg != -1 && callee_saved_registers[tac->src1->preg]) saved_registers[tac->src1->preg] = 1;
        if (tac->src2 && tac->src2->preg != -1 && callee_saved_registers[tac->src2->preg]) saved_registers[tac->src2->preg] = 1;
        tac = tac->next;
    }

    for (int i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (saved_registers[i]) {
            cur_stack_push_count++;
            ir = insert_x86_instruction(ir, X_PUSH, new_preg_value(i), 0, 0, "push %vdq");
        }
    }

    return ir;
}

static Tac *insert_end_of_function(Tac *ir, int *saved_registers) {
    for (int i = PHYSICAL_REGISTER_COUNT - 1; i >= 0; i--)
        if (saved_registers[i])
            ir = insert_x86_instruction(ir, X_POP, new_preg_value(i), 0, 0, "popq %vdq");

    ir = insert_x86_instruction(ir, X_LEAVE, 0, 0, 0, "leaveq");
    return insert_x86_instruction(ir, X_RET_FROM_FUNC, 0, 0, 0, "retq");
}

static Tac *add_sub_rsp(Tac *ir, int amount) {
    return insert_x86_instruction(ir, X_SUB, new_preg_value(REG_RSP), new_integral_constant(TYPE_LONG, amount), 0, "subq $%v1q, %vdq");
}

static Tac *add_add_rsp(Tac *ir, int amount) {
    return insert_x86_instruction(ir, X_ADD, new_preg_value(REG_RSP), new_integral_constant(TYPE_LONG, amount), 0, "addq $%v1q, %vdq");
}

// Add prologue, epilogue, stack alignment pushes/pops, function calls and main() return result
void add_final_x86_instructions(Function *function, char *function_name) {
    int stack_size;             // Size of the stack containing local variables and spilled registers
    int *saved_registers;       // Callee saved registers
    int added_end_of_function;  // To ensure a double epilogue isn't emitted

    Tac *ir = function->ir;

    cur_stack_push_count = 2; // Program counter and rbp

    // Add function prologue
    ir = insert_x86_instruction(ir, X_PUSH, new_preg_value(REG_RBP), 0, 0, "push %vdq");
    ir = insert_x86_instruction(ir, X_MOV, new_preg_value(REG_RBP), new_preg_value(REG_RSP), 0, "mov %v1q, %vdq");

    // Allocate stack space for local variables and spilled registers
    stack_size = function->stack_size;
    if (stack_size > 0) {
        ir = add_sub_rsp(ir, stack_size);
        cur_stack_push_count += stack_size / 8;
    }

    saved_registers = wcalloc(sizeof(int), PHYSICAL_REGISTER_COUNT);

    ir = insert_push_callee_saved_registers(ir, function->ir, saved_registers);

    while (ir) {
        added_end_of_function = 0;

        switch (ir->operation) {
            case IR_NOP:
                break;

            case IR_START_LOOP:
            case IR_END_LOOP:
                ir->operation = IR_NOP;
                break;

            case IR_START_CALL: {
                if (ir->operation == IR_START_CALL) {
                    ir->operation = IR_NOP;

                    int alignment_pushes = 0;
                    if (ir->src1->function_call_arg_stack_padding >= 8)
                        alignment_pushes++;

                    // Align the stack. This is matched with an adjustment when the function call ends
                    int need_aligned_call_push = ((cur_stack_push_count + ir->src1->function_call_arg_push_count) % 2 == 1);
                    if (need_aligned_call_push) {
                        ir->src1->function_call_arg_push_count++;
                        alignment_pushes++;
                    }

                    // Special case of an alignment push to align the whole stack structure
                    // combined with padding at the end of the stack. Eliminate both alignments
                    // to save 16 bytes to stack space.
                    if (alignment_pushes == 2) {
                        ir->src1->function_call_arg_push_count -= 2;
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
                ir->operation = IR_NOP;

                // Adjust the stack for any args that are on in stack
                int function_call_arg_push_count = ir->src1->function_call_arg_push_count;
                if (function_call_arg_push_count > 0) {
                    cur_stack_push_count -= function_call_arg_push_count;
                    ir = add_add_rsp(ir, function_call_arg_push_count * 8);
                }

                break;
            }

            case X_ARG:
                cur_stack_push_count++;
                break;

            case IR_ARG_STACK_PADDING:
                // This alignment push is needed for structures that are aligned
                // on 16-bytes and are preceded in memory by something that left the stack
                // aligned on 8-bytes.
                cur_stack_push_count++;
                ir = add_sub_rsp(ir, 8);

                break;

            case X_ALLOCATE_STACK:
                cur_stack_push_count += ir->src1->int_value / 8;

                break;

            case X_CALL: {
                ir->operation = IR_NOP;

                Tac *orig_ir = ir;

                // A function can be either a direct function or a function pointer
                Type *function_type = ir->src1->type->type == TYPE_FUNCTION ? ir->src1->type : ir->src1->type->target;
                if (function_type->function->is_variadic) {
                    char *buffer;
                    wasprintf(&buffer, "movb $%d, %%vdb", ir->src1->function_call_sse_register_arg_count);
                    append_to_list(allocated_strings, buffer);
                    ir = insert_x86_instruction(ir, X_MOV, new_preg_value(REG_RAX), 0, 0, buffer);
                }

                Tac *tac = new_instruction(X_CALL_FROM_FUNC);

                if (!orig_ir->src1->function_symbol) {
                    wasprintf(&(tac->x86_template), "callq *%%v1q");
                    append_to_list(allocated_strings, tac->x86_template);
                    tac->src1 = orig_ir->src1;
                }
                else {
                    // If a function has been defined locally, call it directly, otherwise use the PLT
                    if (orig_ir->src1->function_symbol->function && orig_ir->src1->function_symbol->function->is_defined) {
                         wasprintf(&(tac->x86_template), "callq %s", orig_ir->src1->function_symbol->global_identifier);
                         append_to_list(allocated_strings, tac->x86_template);
                     }
                    else {
                         wasprintf(&(tac->x86_template), "callq %s@PLT", orig_ir->src1->function_symbol->global_identifier);
                         append_to_list(allocated_strings, tac->x86_template);
                     }
                 }

                ir = insert_instruction_after(ir, tac);
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
    if (!strcmp(function_name, "main"))
        ir = insert_x86_instruction(ir, X_MOV, new_preg_value(REG_RAX), 0, 0, "movq $0, %vdq");

    if (!added_end_of_function) {
        insert_end_of_function(ir, saved_registers);
    }

    wfree(saved_registers);
}

// Remove all possible IR_NOP instructions
void remove_nops(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation != IR_NOP) continue;
        if (!tac->next) panic("Unexpected NOP as last instruction");
        if (!tac->prev) continue;
        if (tac->next->label) continue;

        delete_instruction(tac);
    }
}

// Merge any consecutive add/sub stack operations that aren't involved in jmp instructions.
void merge_rsp_func_call_add_subs(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (
                // First operation is add/sub n, %rsp
                (tac->operation == X_ADD || tac->operation == X_SUB) && tac->dst && tac->dst->preg == REG_RSP &&
                // Second operation is add/sub n, %rsp
                tac->next && (tac->next->operation == X_ADD || tac->next->operation == X_SUB) && tac->next->dst && tac->next->dst->preg == REG_RSP &&
                // No labels are involved
                !tac->label && !tac->next->label) {

            int value = tac->operation == X_ADD ? tac->src1->int_value : -tac->src1->int_value;
            value += tac->next->operation == X_ADD ? tac->next->src1->int_value : -tac->next->src1->int_value;
            if (!value) {
                tac = delete_instruction(tac);
                tac = delete_instruction(tac);
            } else {
                tac = delete_instruction(tac);
                tac->operation = value < 0 ? X_SUB : X_ADD;
                tac->x86_template = value < 0 ? "subq $%v1q, %vdq" : "addq $%v1q, %vdq";
                tac->src1->int_value = value > 0 ? value : -value;
            }
        }
    }
}

int fprintf_octal_char(void *f, char c) {
    if (c == 0)
        return fprintf(f, "\\0");

    int count = 0;
    count += fprintf(f, "\\");
    for (int i = 2; i >= 0; i--) {
        int value = ((unsigned char) c >> (i * 3)) & 7;
        count += fprintf(f, "%d", value);
    }
    return count;
}

int fprintf_escaped_char(void *f, char c) {
         if (c == '"' ) return fprintf(f, "\\\"");
    else if (c == '\\') return fprintf(f, "\\\\");
    else if (c == '\b') return fprintf(f, "\\b");
    else if (c == '\f') return fprintf(f, "\\f");
    else if (c == '\n') return fprintf(f, "\\n");
    else if (c == '\r') return fprintf(f, "\\r");
    else if (c == '\t') return fprintf(f, "\\t");
    else if (c < 32 || c >= 128) return fprintf_octal_char(f, c);
    else return fprintf(f, "%c", c);
}

int fprintf_escaped_string_literal(void *f, StringLiteral* sl, int for_assembly) {
    char *data = sl->data;
    int c = 0;

    if (for_assembly)
        c += fprintf(f, "    .string \"");
    else
        c += fprintf(f, "\"");

    int data_count = sl->is_wide_char ? sl->size * 4 : sl->size;
    for (int i = 0; i < data_count; i++) {
        if (for_assembly && data[i] == 0 && i != data_count - 1) {
            // Terminate the string & start a new one
            c += fprintf(f, "\"\n");
            c += fprintf(f, "    .string \"");

        } else if (!for_assembly || (for_assembly && data[i]))
            c += fprintf_escaped_char(f, data[i]);
    }
    c += fprintf(f, "\"");

    if (for_assembly) c += fprintf(f, "\n");

    return c;
}

// Add a ".loc" line with an integer identifying the filename and the line number.
// The debug_strings map has the mapping from filename to id.
static void output_debug_loc(Tac *tac) {
    if (opt_debug_symbols && tac->origin && tac->origin->filename) {
        int id = (long) strmap_get(debug_strings, tac->origin->filename);
        if (!id) {
            id = ++debug_string_counter;
            strmap_put(debug_strings, wstrdup(tac->origin->filename), (void *) (long) id);
            fprintf(f, "    .file       %d \"%s\"\n", id, tac->origin->filename);
        }

        if (id != last_outputted_filename_id || tac->origin->line_number != last_outputted_filename_line_number) {
            fprintf(f, "    .loc        %d %d\n", id, tac->origin->line_number);
            last_outputted_filename_id = id;
            last_outputted_filename_line_number = tac->origin->line_number;
        }
    }
}

// Output code from the IR of a function
static void output_function_body_code(Symbol *symbol) {
    int function_pc = symbol->function->type->function->param_count;

    for (Tac *tac = symbol->function->ir; tac; tac = tac->next) {
        if (tac->label) fprintf(f, ".L%d:\n", tac->label);
        if (tac->operation != IR_NOP) {
            output_debug_loc(tac);
            output_x86_operation(tac, function_pc);
        }
    }
}

static void output_symbol(Symbol *symbol) {
    if (symbol->linkage == LINKAGE_INTERNAL && !symbol->initializers) {
        if (elf_section != SEC_TEXT) { fprintf(f, "    .text\n"); elf_section = SEC_TEXT; }
        fprintf(f, "    .local  %s\n", symbol->global_identifier);
    }

    if ((symbol->linkage == LINKAGE_INTERNAL || symbol->linkage == LINKAGE_IMPLICIT_EXTERNAL) && !symbol->initializers) {
        if (elf_section != SEC_TEXT) { fprintf(f, "    .text\n"); elf_section = SEC_TEXT; }
        fprintf(f, "    .comm   %s,%d,%d\n",
            symbol->global_identifier,
            get_type_size(symbol->type),
            get_type_alignment(symbol->type));
    }

    else if (symbol->initializers) {
        int size = get_type_size(symbol->type);
        if (symbol->linkage == LINKAGE_IMPLICIT_EXTERNAL)
            fprintf(f, "    .globl   %s\n", symbol->global_identifier);

        if (elf_section != SEC_DATA) { fprintf(f, "    .data\n"); elf_section = SEC_DATA; }

        fprintf(f, "    .align   %d\n", get_type_alignment(symbol->type));
        fprintf(f, "    .type    %s, @object\n", symbol->global_identifier);
        fprintf(f, "    .size    %s, %d\n", symbol->global_identifier, size);
        fprintf(f, "%s:\n", symbol->global_identifier);

        for (int i = 0; i < symbol->initializers->length; i++) {

            Initializer *in = (Initializer *) symbol->initializers->elements[i];

            if (in->is_address_of || in->symbol) {
                if (in->address_of_offset)
                    fprintf(f,"    .quad    %s + %d\n", in->symbol->global_identifier, in->address_of_offset);
                else
                    fprintf(f,"    .quad    %s\n", in->symbol->global_identifier);
                size -= 8;
            }
            else if (in->is_string_literal) {
                if (in->address_of_offset)
                    fprintf(f,"    .quad    .SL%d + %d\n", in->string_literal_index, in->address_of_offset);
                else
                    fprintf(f,"    .quad    .SL%d\n", in->string_literal_index);
                size -= 8;
            }
            else {
                if (!in->data) fprintf(f,"    .zero    %d\n", in->size);
                else if (in->size == 1) fprintf(f,"    .byte    %d\n", *((char *) in->data));
                else if (in->size == 2) fprintf(f,"    .word    %d\n", *((short *) in->data));
                else if (in->size == 4) fprintf(f,"    .long    %d\n", *((int *) in->data));
                else if (in->size == 8) fprintf(f,"    .quad    %ld\n", *((long *) in->data));
                else if (in->size == 16) {
                    fprintf(f, "    .long   %d\n", (((int *) in->data))[0]);
                    fprintf(f, "    .long   %d\n", (((int *) in->data))[1]);
                    fprintf(f, "    .long   %d\n", (((int *) in->data))[2] & 0xffff);
                    fprintf(f, "    .long   0\n");
                }
                else panic("Unknown initializer size=%d data=%p\n", in->size, in->data);
                size -= in->size;
            }
        }

        // Add padding for structs that have padding at the end
        if (size) fprintf(f,"    .zero    %d\n", size);
    }
}

// Output a debug_info section with only a DW_TAG_compile_unit with a corresponding
// debug_abbrev section. debug_str contains debug strings. This is enough information
// to have the source filename, directory, function names and line numbers.
void output_debug_sections(char *input_filename) {
    char *cwd = wmalloc(1024);
    if (!getcwd(cwd, 1024)) panic("Unable to get cwd");

    // Output debug_info section
    fprintf(f, "    .section .debug_info,\"\",@progbits\n\n");

    fprintf(f, ".Ldebug_info0:\n");
    fprintf(f, "    .long   .Ldebug_info_end - .Ldebug_info_start\n"); // Size

    fprintf(f, ".Ldebug_info_start:\n");
    fprintf(f, "    .value  %d\n", DWARF_VERSION);
    fprintf(f, "    .long   .debug_abbrev\n");      // Pointer to debug_abbrev section
    fprintf(f, "    .byte   0x8\n");                // Pointer size

    // Output DW_TAG_compile_unit
    fprintf(f, "    .uleb128 0x1\n");                               // DW_TAG_compile_unit
    fprintf(f, "    .long   .Ldebug_info.producer\n");              // DW_AT_producer
    fprintf(f, "    .byte   %d\n", DW_LANG_C89);                    // DW_AT_language
    fprintf(f, "    .long   .Ldebug_info.filename\n");              // DW_AT_name
    fprintf(f, "    .long   .debug_info.cwd\n");                    // DW_AT_comp_dir
    fprintf(f, "    .quad   .Lall.code.start\n");                   // DW_AT_low_pc (start of code)
    fprintf(f, "    .quad   .Lall.code.end-.Lall.code.start \n");   // DW_AT_high_pc (size of code)
    fprintf(f, "    .long   .Lline_table_start\n");                 // DW_AT_stmt_list (pointer to line number table)
    fprintf(f, ".Ldebug_info_end:\n\n");

    // Output debug_abbrev section. All lines with DW are pairs of a key & data type
    fprintf(f, "    .section    .debug_abbrev,\"\",@progbits\n");

    fprintf(f, "    .uleb128 0x1\n");                       // type number 1
    fprintf(f, "    .uleb128 %d\n", DW_TAG_compile_unit);   // tag: DW_TAG_compile_unit
    fprintf(f, "    .byte   0\n");                          // has children 0
    fprintf(f, "    .uleb128 %d\n", DW_AT_producer);        // DW_AT_producer / DW_FORM_strp
    fprintf(f, "    .uleb128 %d\n", DW_FORM_strp);
    fprintf(f, "    .uleb128 %d\n", DW_AT_language);        // DW_AT_language / DW_FORM_data1
    fprintf(f, "    .uleb128 %d\n", DW_FORM_data1);
    fprintf(f, "    .uleb128 %d\n", DW_AT_name);            // DW_AT_name / DW_FORM_strp
    fprintf(f, "    .uleb128 %d\n", DW_FORM_strp);
    fprintf(f, "    .uleb128 %d\n", DW_AT_comp_dir);        // DW_AT_comp_dir / DW_FORM_strp
    fprintf(f, "    .uleb128 %d\n", DW_FORM_strp);
    fprintf(f, "    .uleb128 %d\n", DW_AT_low_pc);          // DW_AT_low_pc / DW_FORM_addr
    fprintf(f, "    .uleb128 %d\n", DW_FORM_addr);
    fprintf(f, "    .uleb128 %d\n", DW_AT_high_pc);         // DW_AT_high_pc / DW_FORM_data8
    fprintf(f, "    .uleb128 %d\n", DW_FORM_data8);
    fprintf(f, "    .uleb128 %d\n", DW_AT_stmt_list);       // DW_AT_stmt_list / DW_FORM_sec_offset
    fprintf(f, "    .uleb128 %d\n", DW_FORM_sec_offset);
    fprintf(f, "    .byte   0\n");                          // End
    fprintf(f, "    .byte   0\n");
    fprintf(f, "    .byte   0\n");

    // Output debug_str section
    fprintf(f, "\n    .section    .debug_str,\"MS\",@progbits,1\n");
    fprintf(f, ".Ldebug_info.producer:\n");
    fprintf(f, "    .string  \"wcc\"\n");
    fprintf(f, ".Ldebug_info.filename:\n");
    fprintf(f, "    .string  \"%s\"\n", input_filename);
    fprintf(f, ".debug_info.cwd:\n");
    fprintf(f, "    .string  \"%s\"\n", cwd);

    // Output debug_line section. There's nothing much here, the assembler populates
    // this with information from the .loc lines.
    fprintf(f, "\n    .section    .debug_line,\"\",@progbits\n");
    fprintf(f, "\n.Lline_table_start:\n");

    wfree(cwd);
}

// Output code for the translation unit
void output_code(char *input_filename, char *output_filename) {
    if (!strcmp(output_filename, "-"))
        f = stdout;
    else {
        // Open output file for writing
        f = fopen(output_filename, "w");
        if (f == 0) {
            perror(output_filename);
            exit(1);
        }
    }

    fprintf(f, "    .file   \"%s\"\n", input_filename);

    // Output symbols
    elf_section = SEC_NONE;
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (!symbol->scope->parent && symbol->type->type != TYPE_FUNCTION && symbol->type->type != TYPE_TYPEDEF && !symbol->is_enum_value)
            output_symbol(symbol);
    }

    // Output static local symbols
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            Function *function = symbol->function;
            for (int j = 0; j < function->static_symbols->length; j++)
                output_symbol(function->static_symbols->elements[j]);
        }
        symbol++;
    }

    // Output string literals
    if (string_literal_count > 0) {
        fprintf(f, "\n    .section .rodata\n\n");
        fprintf(f, ".Ltext0:\n\n");
        for (int i = 0; i < string_literal_count; i++) {
            StringLiteral *sl = &(string_literals[i]);
            if (sl->is_wide_char) fprintf(f, "    .align   4\n");
            fprintf(f, ".SL%d:\n", i);
            fprintf_escaped_string_literal(f, sl, 1);
        }
        fprintf(f, "\n");
    }

    // Output code
    fprintf(f, "    .text\n");

    // Output symbols for all functions that are defined and have external linkage
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined &&
                (symbol->linkage == LINKAGE_IMPLICIT_EXTERNAL || symbol->linkage == LINKAGE_EXPLICIT_EXTERNAL)) {

            fprintf(f, "    .globl  %s\n", symbol->identifier);
            fprintf(f, "    .type   %s, @function\n", symbol->global_identifier);
        }
    }

    fprintf(f, "\n");

    label_count = 0; // Used in label renumbering

    string_literal_count = 0;

    // Output functions code
    need_ru4_to_ld_symbol = 0;
    need_ld_to_ru4_symbol = 0;
    fprintf(f, ".Lall.code.start:\n");
    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            fprintf(f, "%s:\n", symbol->identifier);
            fprintf(f, ".L%s.start:\n", symbol->identifier);
            output_function_body_code(symbol);
            fprintf(f, "    .size       %s, .-%s\n", symbol->global_identifier, symbol->global_identifier);
            fprintf(f, ".L%s.end:\n", symbol->identifier);
            fprintf(f, "\n");
        }
    }
    fprintf(f, ".Lall.code.end:\n\n");

    // Output floating point literals
    if (floating_point_literal_count > 0) {
        for (int i = 0; i < floating_point_literal_count; i++) {
            // The zero and & is to be compatible with gcc
            fprintf(f, ".FPL%d:\n", i);

            if (floating_point_literals[i].type == TYPE_FLOAT) {
                float fl = floating_point_literals[i].f;
                fprintf(f, "    .long   %d\n", *((int *) &fl));
            }
            else if (floating_point_literals[i].type == TYPE_DOUBLE) {
                double d = floating_point_literals[i].d;
                fprintf(f, "    .long   %d\n", *((int *) &d));
                fprintf(f, "    .long   %d\n", *((int *) &d + 1));
            }
            else {
                long double ld = floating_point_literals[i].ld;
                fprintf(f, "    .long   %d\n", ((int *) &ld)[0]);
                fprintf(f, "    .long   %d\n", ((int *) &ld)[1]);
                fprintf(f, "    .long   %d\n", ((int *) &ld)[2] & 0xffff);
                fprintf(f, "    .long   0\n");
            }
        }
    }

    if (need_ru4_to_ld_symbol) {
        fprintf(f, ".RU4TOLD:\n");
        fprintf(f, "    .long   0\n");
        fprintf(f, "    .long   1602224128 # 0x5f800000\n");
        fprintf(f, "\n");
    }

    if (need_ld_to_ru4_symbol) {
        fprintf(f, ".LDTORU4:\n");
        fprintf(f, "     .long   1593835520 # 9223372036854775808\n");
    }

    if (opt_debug_symbols) output_debug_sections(input_filename);

    fclose(f);
}

void init_codegen(void) {
    floating_point_literals = wmalloc(sizeof(FloatingPointLiteral) * MAX_FLOATING_POINT_LITERALS);
    floating_point_literal_count = 0;

    debug_strings = new_strmap();
    debug_string_counter = 0;

    last_outputted_filename_id = 0;
    last_outputted_filename_line_number = 0;

    allocated_strings = new_list(128);
}

void free_codegen(void) {
    wfree(floating_point_literals);

    strmap_foreach(debug_strings, it) wfree(strmap_iterator_key(&it));
    free_strmap(debug_strings);

    for (int i = 0; i < allocated_strings->length; i++) wfree(allocated_strings->elements[i]);
    free_list(allocated_strings);
}
