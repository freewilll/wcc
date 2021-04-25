#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "wcc.h"

void check_preg(int preg) {
    if (preg == -1) panic("Illegal attempt to output -1 preg");
    if (preg < 0 || preg >= PHYSICAL_REGISTER_COUNT) panic1d("Illegal preg %d", preg);
}

char *register_name(int preg) {
    char *names;
    char *buffer;

    check_preg(preg);
    names = "rax rbx rcx rdx rsi rdi rbp rsp r8  r9  r10 r11 r12 r13 r14 r15";
    if (preg == 8 || preg == 9) asprintf(&buffer, "%%%.2s", &names[preg * 4]);
    else                        asprintf(&buffer, "%%%.3s", &names[preg * 4]);
    return buffer;

}

void append_byte_register_name(char *buffer, int preg) {
    char *names;

    check_preg(preg);
    names = "al   bl   cl   dl   sil  dil  bpl  spl  r8b  r9b  r10b r11b r12b r13b r14b r15b";
         if (preg < 4)  sprintf(buffer, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) sprintf(buffer, "%%%.3s", &names[preg * 5]);
    else                sprintf(buffer, "%%%.4s", &names[preg * 5]);
}

void output_word_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "ax   bx   cx   dx   si   di   bp   sp   r8w  r9w  r10w r11w r12w r13w r14w r15w";
         if (preg < 8)  fprintf(f, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) fprintf(f, "%%%.3s", &names[preg * 5]);
    else                fprintf(f, "%%%.4s", &names[preg * 5]);
}

void append_word_register_name(char *buffer, int preg) {
    char *names;

    check_preg(preg);
    names = "ax   bx   cx   dx   si   di   bp   sp   r8w  r9w  r10w r11w r12w r13w r14w r15w";
         if (preg < 8)  sprintf(buffer, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) sprintf(buffer, "%%%.3s", &names[preg * 5]);
    else                sprintf(buffer, "%%%.4s", &names[preg * 5]);
}

void append_long_register_name(char *buffer, int preg) {
    char *names;

    check_preg(preg);
    names = "eax  ebx  ecx  edx  esi  edi  ebp  esp  r8d  r9d  r10d r11d r12d r13d r14d r15d";
    if (preg < 10) sprintf(buffer, "%%%.3s", &names[preg * 5]);
    else           sprintf(buffer, "%%%.4s", &names[preg * 5]);
}

void output_quad_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "rax rbx rcx rdx rsi rdi rbp rsp r8  r9  r10 r11 r12 r13 r14 r15";
    if (preg == 8 || preg == 9) fprintf(f, "%%%.2s", &names[preg * 4]);
    else                        fprintf(f, "%%%.3s", &names[preg * 4]);
}

void append_quad_register_name(char *buffer, int preg) {
    char *names;

    check_preg(preg);
    names = "rax rbx rcx rdx rsi rdi rbp rsp r8  r9  r10 r11 r12 r13 r14 r15";
    if (preg == 8 || preg == 9) sprintf(buffer, "%%%.2s", &names[preg * 4]);
    else                        sprintf(buffer, "%%%.3s", &names[preg * 4]);
}

// Get offset from the stack in bytes, from a stack_index.
// The stack layout for a function with 8 parameters (function_pc=8)
// stack index  offset/8  what
// 2            +3        arg 7 (last arg)
// 3            +2        arg 6
//              +1        return address
//              +0        Pushed BP
// -1           -1        first local variable / spilled register
// -2           -2        second local variable / spilled register
int get_stack_offset_from_index(int function_pc, int stack_index) {
    if (stack_index >= 2) {
        // Function parameter
        if (function_pc <= 6) printf("Unexpected positive stack_index %d for function with <= 6 parameters (%d)\n", stack_index, function_pc);
        return 8 * (function_pc - stack_index - 3);
    }
    else if (stack_index >= 0) printf("Unexpected stack_index %d\n", stack_index);
    else
        // Local variable. stack_index < 0. -1 is the first,-2 the second, etc
        return 8 * stack_index;
}

char *render_x86_operation(Tac *tac, int function_pc, int expect_preg) {
    char *t, *result, *buffer;
    int i, x86_size, stack_offset, mnemonic_length;
    Value *v;

    t = tac->x86_template;
    if (!t) return 0;

    buffer = malloc(128);
    memset(buffer, 0, 128);
    result = buffer;

    while (*t && *t != ' ') *buffer++ = *t++;
    while (*t && *t == ' ') *t++;

    if (*t) {
        mnemonic_length = buffer - result;
        for (i = 0; i < 8 - mnemonic_length; i++) *buffer++ = ' ';
    }

    while (*t) {
        if (*t == '%') {
            if (t[1] == '%') {
                *buffer++ = '%';
                t += 1;
            }
            else {
                t++;

                if (t[0] != 'v') panic1s("Unknown placeholder in %s", tac->x86_template);

                t++;

                     if (t[0] == '1') v = tac->src1;
                else if (t[0] == '2') v = tac->src2;
                else if (t[0] == 'd') v = tac->dst;
                else panic1s("Indecipherable placeholder \"%s\"", tac->x86_template);

                x86_size = 0;
                     if (t[1] == 'b') { t++; x86_size = 1; }
                else if (t[1] == 'w') { t++; x86_size = 2; }
                else if (t[1] == 'l') { t++; x86_size = 3; }
                else if (t[1] == 'q') { t++; x86_size = 4; }

                if (!v) panic1s("Unexpectedly got a null value while the template %s is expecting it", tac->x86_template);

                if (!expect_preg && v->vreg) {
                    if (!x86_size) panic1s("Missing size on register value \"%s\"", tac->x86_template);
                    if (v->global_symbol) panic("Got global symbol in vreg");

                    *buffer++ = 'r';
                    sprintf(buffer, "%d", v->vreg);
                    while (*buffer) *buffer++;
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
                else if (v->is_constant)
                    sprintf(buffer, "%ld", v->value);
                else if (v->is_string_literal)
                    sprintf(buffer, ".SL%d(%%rip)", v->string_literal_index);
                else if (v->global_symbol)
                    sprintf(buffer, "%s(%%rip)", v->global_symbol->identifier);
                else if (v->stack_index) {
                    stack_offset = get_stack_offset_from_index(function_pc, v->stack_index);
                    sprintf(buffer, "%d(%%rbp)", stack_offset);
                }
                else if (v->label)
                    sprintf(buffer, ".l%d", v->label);
                else {
                    print_value(stdout, v, 0);
                    printf("\n");
                    panic("Don't know how to render template value");
                }
            }
        }
        else
            *buffer++ = *t;

        while (*buffer) *buffer++;
        t++;
    }

    return result;
}

void output_x86_operation(Tac *tac, int function_pc) {
    char *buffer;

    buffer = render_x86_operation(tac, function_pc, 1);
    if (buffer) {
        fprintf(f, "    %s\n", buffer);
        free(buffer);
    }
}

// Called once at startup to indicate which registers are preserved across function calls
void init_callee_saved_registers() {
    callee_saved_registers = malloc(sizeof(int) * (PHYSICAL_REGISTER_COUNT + 1));
    memset(callee_saved_registers, 0, sizeof(int) * (PHYSICAL_REGISTER_COUNT + 1));

    callee_saved_registers[REG_RBX] = 1;
    callee_saved_registers[REG_R12] = 1;
    callee_saved_registers[REG_R13] = 1;
    callee_saved_registers[REG_R14] = 1;
    callee_saved_registers[REG_R15] = 1;
}

// Determine which registers are used in a function, push them onto the stack and return the list
int *output_push_callee_saved_registers(Tac *tac) {
    int *saved_registers;
    int i;

    saved_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(saved_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    while (tac) {
        if (tac->dst  && tac->dst ->preg != -1 && callee_saved_registers[tac->dst ->preg]) saved_registers[tac->dst ->preg] = 1;
        if (tac->src1 && tac->src1->preg != -1 && callee_saved_registers[tac->src1->preg]) saved_registers[tac->src1->preg] = 1;
        if (tac->src2 && tac->src2->preg != -1 && callee_saved_registers[tac->src2->preg]) saved_registers[tac->src2->preg] = 1;
        tac = tac->next;
    }

    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (saved_registers[i]) {
            cur_stack_push_count++;
            fprintf(f, "    pushq   ");
            output_quad_register_name(i);
            fprintf(f, "\n");
        }
    }

    return saved_registers;
}

// Pop the callee saved registers back
void output_pop_callee_saved_registers(int *saved_registers) {
    int i;

    for (i = PHYSICAL_REGISTER_COUNT - 1; i >= 0; i--) {
        if (saved_registers[i]) {
            // Note: cur_stack_push_count isn't adjusted since that would
            // otherwise mess up stack alignments for function calls after
            // return statements.
            fprintf(f, "    popq    ");
            output_quad_register_name(i);
            fprintf(f, "\n");
        }
    }
}

// Output code from the IR of a function
void output_function_body_code(Symbol *symbol) {
    int i, stack_offset, function_call_count;
    Tac *tac;
    char *buffer;
    int function_pc;                    // The Function's param count
    int ac;                             // A function call's arg count
    int local_stack_size;               // Size of the stack containing local variables and spilled registers
    int *saved_registers;               // Callee saved registers
    int function_call_pushes;           // How many pushes are necessary for a function call
    int need_aligned_call_push;         // If an extra push has been done before function call args to align the stack
    int *processed_function_arguments;  // The amount of function call arguments that have already been processed

    char *s;
    int type;

    function_call_count = make_function_call_count(symbol->function);
    processed_function_arguments = malloc(sizeof(int) * function_call_count);

    function_pc = symbol->function->param_count;

    fprintf(f, "    push    %%rbp\n");
    fprintf(f, "    movq    %%rsp, %%rbp\n");
    cur_stack_push_count = 2; // Program counter and rbp

    // Allocate stack space for local variables and spilled registers
    local_stack_size = 8 * (symbol->function->spilled_register_count);

    // Allocate local stack
    if (local_stack_size > 0) {
        fprintf(f, "    subq    $%d, %%rsp\n", local_stack_size);
        cur_stack_push_count += local_stack_size / 8;
    }

    tac = symbol->function->ir;
    saved_registers = output_push_callee_saved_registers(tac);

    while (tac) {
        if (output_inline_ir) {
            fprintf(f, "    // ------------------------------------- ");
            print_instruction(f, tac);
        }

        if (tac->label) fprintf(f, ".l%d:\n", tac->label);

        if (tac->operation == IR_NOP || tac->operation == IR_START_LOOP || tac->operation == IR_END_LOOP);

        else if (tac->operation == IR_START_CALL) {
            processed_function_arguments[tac->src1->value] = tac->src1->function_call_arg_count - 1;

            // Align the stack. This is matched with a pop when the function call ends
            function_call_pushes = tac->src1->function_call_arg_count <= 6 ? 0 : tac->src1->function_call_arg_count - 6;
            need_aligned_call_push = ((cur_stack_push_count + function_call_pushes) % 2 == 1);
            if (need_aligned_call_push)  {
                tac->src1->pushed_stack_aligned_quad = 1;
                cur_stack_push_count++;
                fprintf(f, "    subq    $8, %%rsp\n");
            }
        }

        else if (tac->operation == IR_END_CALL) {
            if (tac->src1->pushed_stack_aligned_quad)  {
                cur_stack_push_count--;
                fprintf(f, "    addq    $8, %%rsp\n");
            }
        }

        else if (tac->operation == X_ARG && tac->src1->function_call_arg_index > 5) {
            // Dirty hack to get the register without a mnemonic
            buffer = render_x86_operation(tac, function_pc, 1);
            buffer += 8;

            // Push the value to the stack, for popping just before the function call
            cur_stack_push_count++;

            fprintf(f, "    pushq");
            fprintf(f, "   %s\n", buffer);
            processed_function_arguments[tac->src1->value]--;
        }

        else if (tac->operation == X_CALL) {
            ac = tac->src1->function_call_arg_count;

            // Since floating point numbers isn't implemented, this is zero.
            if (tac->src1->function_symbol->function->is_variadic)
                fprintf(f, "    movb    $0, %%al\n");

            if (tac->src1->function_symbol->function->is_external)
                fprintf(f, "    callq   %s@PLT\n", tac->src1->function_symbol->identifier);
            else
                fprintf(f, "    callq   %s\n", tac->src1->function_symbol->identifier);

            // Adjust the stack for any args that are on in stack
            if (ac > 6) {
                fprintf(f, "    addq    $%d, %%rsp\n", (ac - 6) * 8);
                cur_stack_push_count -= ac - 6;
            }
        }

        else if (tac->operation == X_RET) {
            output_x86_operation(tac, function_pc);
            output_pop_callee_saved_registers(saved_registers);
            fprintf(f, "    leaveq\n");
            fprintf(f, "    retq\n");
        }

        else
            output_x86_operation(tac, function_pc);

        tac = tac->next;
    }

    // Special case for main, return 0 if no return statement is present
    if (!strcmp(symbol->identifier, "main")) fprintf(f, "    movq    $0, %%rax\n");

    output_pop_callee_saved_registers(saved_registers);
    fprintf(f, "    leaveq\n");
    fprintf(f, "    retq\n");
}

// Output code for the translation unit
void output_code(char *input_filename, char *output_filename) {
    int i;
    Tac *tac;
    Symbol *symbol;
    char *sl;

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
    symbol = symbol_table;
    while (symbol->identifier) {
        if (!symbol->scope && !symbol->is_function)
            fprintf(f, "    .comm   %s,%d,%d\n",
                symbol->identifier,
                get_type_size(symbol->type),
                get_type_alignment(symbol->type));
        symbol++;
    }

    // Output string literals
    if (string_literal_count > 0) {
        fprintf(f, "\n    .section .rodata\n");
        for (i = 0; i < string_literal_count; i++) {
            sl = string_literals[i];
            fprintf(f, ".SL%d:\n", i);
            fprintf(f, "    .string ");
            fprintf_escaped_string_literal(f, sl);
            fprintf(f, "\n");
        }
        fprintf(f, "\n");
    }

    // Output code
    fprintf(f, "    .text\n");

    // Output symbols for all non-external functions
    symbol = symbol_table;
    while (symbol->identifier) {
        if (symbol->is_function && !symbol->function->is_external)
            fprintf(f, "    .globl  %s\n", symbol->identifier);
        symbol++;
    }
    fprintf(f, "\n");

    label_count = 0; // Used in label renumbering

    // Output functions code
    symbol = symbol_table;
    while (symbol->identifier) {
        if (symbol->is_function && symbol->function->is_defined) {
            fprintf(f, "%s:\n", symbol->identifier);
            output_function_body_code(symbol);
            fprintf(f, "\n");
        }
        symbol++;
    }

    fclose(f);
}
