#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "wcc.h"

int need_ru4_to_ld_symbol;
int need_ld_to_ru4_symbol;

static void check_preg(int preg, int preg_class) {
    if (preg == -1) panic("Illegal attempt to output -1 preg");
    if (preg < 0 || preg >= 32) panic1d("Illegal preg %d", preg);
    if (preg_class == PC_INT && preg >= 16)
        panic1d("Illegal int preg %d", preg);
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
    char *buffer = malloc(16);
    buffer[0] = 0;
    append_quad_register_name(buffer, preg);
    return buffer;
}

// Allocate stack offsets for variables on the stack (stack_index < 0). Go backwards
// in alignment, allocating the ones with the largest alignment first, in order of
// stack_index.
void make_stack_offsets(Function *function, char *function_name) {
    int count = function->stack_register_count;

    if (!count) return; // Nothing is on the stack

    // Determine size & alignments for all variables on the stack
    int *stack_alignments = malloc((count+ 1) * sizeof(int));
    memset(stack_alignments, 0, (count+ 1) * sizeof(int));

    int *stack_sizes = malloc((count+ 1) * sizeof(int));
    memset(stack_sizes, 0, (count+ 1) * sizeof(int));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst && tac->dst->stack_index < 0) {
            if (!stack_alignments[-tac->dst->stack_index]) {
                stack_alignments[-tac->dst->stack_index] = get_type_alignment(tac->dst->type);
                stack_sizes[-tac->dst->stack_index] = get_type_size(tac->dst->type);
            }
        }
        if (tac->src1 && tac->src1->stack_index < 0) {
            if (!stack_alignments[-tac->src1->stack_index]) {
                stack_alignments[-tac->src1->stack_index] = get_type_alignment(tac->src1->type);
                stack_sizes[-tac->src1->stack_index] = get_type_size(tac->src1->type);
            }
        }
        if (tac->src2 && tac->src2->stack_index < 0) {
            if (!stack_alignments[-tac->src2->stack_index]) {
                stack_alignments[-tac->src2->stack_index] = get_type_alignment(tac->src2->type);
                stack_sizes[-tac->src2->stack_index] = get_type_size(tac->src2->type);
            }
        }
    }

    // Determine stack offsets
    int *stack_offsets = malloc((count+ 1) * sizeof(int));
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

    // Align on 8 bytes, this is required by the function call stack alignment code.
    total_size = (total_size + 7) & ~0x7;
    function->stack_size = total_size;

    // Assign stack_offsets
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac ->dst && tac ->dst->stack_index < 0) tac ->dst->stack_offset = stack_offsets[-tac ->dst->stack_index];
        if (tac->src1 && tac->src1->stack_index < 0) tac->src1->stack_offset = stack_offsets[-tac->src1->stack_index];
        if (tac->src2 && tac->src2->stack_index < 0) tac->src2->stack_offset = stack_offsets[-tac->src2->stack_index];
    }
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
    else if (stack_index < 0)
        return -v->stack_offset;
    else
        printf("Unexpected stack_index %d\n", stack_index);
}

static void check_floating_point_literal_max() {
    if (floating_point_literal_count >= MAX_FLOATING_POINT_LITERALS) panic1d("Exceeded max floaing point literals %d", MAX_FLOATING_POINT_LITERALS);
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

    char *buffer = malloc(128);
    memset(buffer, 0, 128);
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

                if (t[0] != 'v') panic1s("Unknown placeholder in %s", tac->x86_template);

                t++;

                     if (t[0] == '1') v = tac->src1;
                else if (t[0] == '2') v = tac->src2;
                else if (t[0] == 'd') v = tac->dst;
                else panic1s("Indecipherable placeholder \"%s\"", tac->x86_template);

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

                     if (t[1] == 'L') { t++; low  = 1; }
                else if (t[1] == 'H') { t++; high = 1; }
                else if (t[1] == 'f') { t++; float_arg = 1; }
                else if (t[1] == 'd') { t++; double_arg = 1; }
                else if (t[1] == 'C') { t++; long_double_literal = 1; }
                else if (t[1] == 'F') { t++; float_literal = 1; x86_size = 3; }
                else if (t[1] == 'D') { t++; double_literal = 1; x86_size = 4; }

                if (!v) panic1s("Unexpectedly got a null value while the template %s is expecting it", tac->x86_template);

                if (is_offset) {
                    if (v->offset || offset_is_required) sprintf(buffer, "%d", v->offset);
                }
                else if (!expect_preg && v->vreg) {
                    if (!x86_size) panic1s("Missing size on register value \"%s\"", tac->x86_template);
                    if (v->global_symbol) panic("Got global symbol in vreg");

                    *buffer++ = 'r';
                    sprintf(buffer, "%d", v->vreg);
                    while (*buffer) buffer++;
                    *buffer++ = size_to_x86_size(x86_size);
                }
                else if (expect_preg && v->preg != -1) {
                    if (!x86_size) panic1s("Missing size on register value \"%s\"", tac->x86_template);

                         if (x86_size == 1) append_byte_register_name(buffer, v->preg);
                    else if (x86_size == 2) append_word_register_name(buffer, v->preg);
                    else if (x86_size == 3) append_long_register_name(buffer, v->preg);
                    else if (x86_size == 4) append_quad_register_name(buffer, v->preg);
                    else panic1d("Unknown register size %d", x86_size);
                }
                else if (v->is_constant) {
                    if (is_floating_point_type(v->type)) {
                        if (low)
                            sprintf(buffer, "$%ld", *((long *) &v->fp_value));
                        else if (high)
                            // The & is to be compatible with gcc
                            sprintf(buffer, "$%ld", (*((long *) &v->fp_value + 1) & 0xffff));
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
                    else
                        sprintf(buffer, "%ld", v->int_value);
                }
                else if (v->is_string_literal)
                    sprintf(buffer, ".SL%d(%%rip)", v->string_literal_index);
                else if (v->global_symbol) {
                    if (v->type->type == TYPE_LONG_DOUBLE) {
                        if (low) {
                            if (v->offset)
                                sprintf(buffer, "%s+%d(%%rip)", v->global_symbol->identifier, v->offset);
                            else
                                sprintf(buffer, "%s(%%rip)", v->global_symbol->identifier);
                        }
                        else if (high)
                            sprintf(buffer, "%s+%d(%%rip)", v->global_symbol->identifier, v->offset + 8);
                        else
                            panic("Did not get L/H/C specifier for double long constant");
                    }
                    else {
                        if (v->offset)
                            sprintf(buffer, "%s+%d(%%rip)", v->global_symbol->identifier, v->offset);
                        else
                            sprintf(buffer, "%s(%%rip)", v->global_symbol->identifier);
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
        free(buffer);
    }
}

// Add an instruction after ir and return ir of the new instruction
static Tac *insert_x86_instruction(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, char *x86_template) {
    Tac *tac = malloc(sizeof(Tac));
    memset(tac, 0, sizeof(Tac));
    tac->operation = operation;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->x86_template = x86_template;

    return insert_instruction_after(ir, tac);
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
    int stack_size;       // Size of the stack containing local variables and spilled registers
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

    saved_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(saved_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    ir = insert_push_callee_saved_registers(ir, function->ir, saved_registers);

    while (ir) {
        added_end_of_function = 0;

        if (ir->operation == IR_NOP);
        if (ir->operation == IR_START_LOOP || ir->operation == IR_END_LOOP) ir->operation = IR_NOP;

        else if (ir->operation == IR_START_CALL) {
            ir->operation = IR_NOP;

            int alignment_pushes = 0;
            if (ir->src1->function_call_arg_stack_padding) alignment_pushes++;

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

        else if (ir->operation == IR_END_CALL) {
            ir->operation = IR_NOP;

            // Adjust the stack for any args that are on in stack
            int function_call_arg_push_count = ir->src1->function_call_arg_push_count;
            if (function_call_arg_push_count > 0) {
                cur_stack_push_count -= function_call_arg_push_count;
                ir = add_add_rsp(ir, function_call_arg_push_count * 8);
            }
        }

        else if (ir->operation == X_ARG)
            cur_stack_push_count++;

        else if (ir->operation == X_EXTRA_ARG) {
            // This alignment push is only needed after structures that are aligned
            // on 16-bytes
            cur_stack_push_count++;
            if (ir->src1->function_call_arg_stack_padding) {
                cur_stack_push_count++;
                ir = add_sub_rsp(ir, 8);
            }
        }

        else if (ir->operation == X_CALL) {
            ir->operation = IR_NOP;

            Tac *orig_ir = ir;

            // Since floating point numbers aren't implemented, this is zero.
            if (ir->src1->function_symbol->type->function->is_variadic) {
                char *buffer;
                asprintf(&buffer, "movb $%d, %%vdb", ir->src1->function_call_sse_register_arg_count);
                ir = insert_x86_instruction(ir, X_MOV, new_preg_value(REG_RAX), 0, 0, buffer);
            }

            Tac *tac = new_instruction(X_CALL_FROM_FUNC);

            if (orig_ir->src1->function_symbol->type->function->is_external)
                 asprintf(&(tac->x86_template), "callq %s@PLT", orig_ir->src1->function_symbol->identifier);
            else
                 asprintf(&(tac->x86_template), "callq %s", orig_ir->src1->function_symbol->identifier);

            ir = insert_instruction_after(ir, tac);
        }

        else if (ir->operation == X_RET) {
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

// Output code from the IR of a function
static void output_function_body_code(Symbol *symbol) {
    int function_pc = symbol->type->function->param_count;

    for (Tac *tac = symbol->type->function->ir; tac; tac = tac->next) {
        if (tac->label) fprintf(f, ".L%d:\n", tac->label);
        if (tac->operation != IR_NOP) output_x86_operation(tac, function_pc);
    }
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
    fprintf(f, "    .text\n");
    for (int i = 0; i < global_scope->symbol_count; i++) {
        Symbol *symbol = global_scope->symbols[i];
        if (!symbol->scope->parent && symbol->type->type != TYPE_FUNCTION && !symbol->is_enum)
            fprintf(f, "    .comm   %s,%d,%d\n",
                symbol->identifier,
                get_type_size(symbol->type),
                get_type_alignment(symbol->type));
    }

    // Output string literals
    if (string_literal_count > 0) {
        fprintf(f, "\n    .section .rodata\n");
        for (int i = 0; i < string_literal_count; i++) {
            char *sl = string_literals[i];
            fprintf(f, ".SL%d:\n", i);
            fprintf(f, "    .string ");
            fprintf_escaped_string_literal(f, sl);
            fprintf(f, "\n");
        }
        fprintf(f, "\n");
    }

    // fprintf(f, "    .section    .rodata.cst16,"aM",@progbits,16");
    // fprintf(f, "    .p2align    4    ");

    // Output code
    fprintf(f, "    .text\n");

    // Output symbols for all non-external functions
    for (int i = 0; i < global_scope->symbol_count; i++) {
        Symbol *symbol = global_scope->symbols[i];
        if (symbol->type->type == TYPE_FUNCTION && !symbol->type->function->is_external && !symbol->type->function->is_static)
            fprintf(f, "    .globl  %s\n", symbol->identifier);
    }

    fprintf(f, "\n");

    label_count = 0; // Used in label renumbering

    string_literal_count = 0;

    // Output functions code
    need_ru4_to_ld_symbol = 0;
    need_ld_to_ru4_symbol = 0;
    for (int i = 0; i < global_scope->symbol_count; i++) {
        Symbol *symbol = global_scope->symbols[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->type->function->is_defined) {
            fprintf(f, "%s:\n", symbol->identifier);
            output_function_body_code(symbol);
            fprintf(f, "\n");
        }
        symbol++;
    }

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
                fprintf(f, "    .long   %d\n", *((int *) &ld));
                fprintf(f, "    .long   %d\n", *((int *) &ld + 1));
                fprintf(f, "    .long   %d\n", (*((int *) &ld + 2) & 0xffff));
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

    fclose(f);
}
