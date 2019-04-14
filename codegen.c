#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "wc4.h"

void check_preg(int preg) {
    if (preg == -1) panic("Illegal attempt to output -1 preg");
    if (preg < 0 || preg >= PHYSICAL_REGISTER_COUNT) panic1d("Illegal preg %d", preg);
}

void output_byte_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "al   bl   cl   dl   sil  dil  bpl  spl  r8b  r9b  r10b r11b r12b r13b r14b r15b";
         if (preg < 4)  fprintf(f, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) fprintf(f, "%%%.3s", &names[preg * 5]);
    else                fprintf(f, "%%%.4s", &names[preg * 5]);
}

void output_word_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "ax   bx   cx   dx   si   di   bp   sp   r8w  r9w  r10w r11w r12w r13w r14w r15w";
         if (preg < 8)  fprintf(f, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) fprintf(f, "%%%.3s", &names[preg * 5]);
    else                fprintf(f, "%%%.4s", &names[preg * 5]);
}

void output_long_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "eax  ebx  ecx  edx  esi  edi  ebp  esp  r8d  r9d  r10d r11d r12d r13d r14d r15d";
    if (preg < 10) fprintf(f, "%%%.3s", &names[preg * 5]);
    else           fprintf(f, "%%%.4s", &names[preg * 5]);
}

void output_quad_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "rax rbx rcx rdx rsi rdi rbp rsp r8  r9  r10 r11 r12 r13 r14 r15";
    if (preg == 8 || preg == 9) fprintf(f, "%%%.2s", &names[preg * 4]);
    else                        fprintf(f, "%%%.3s", &names[preg * 4]);
}

void output_move_quad_register_to_register(int preg1, int preg2) {
    if (preg1 != preg2) {
        fprintf(f, "\tmovq\t");
        output_quad_register_name(preg1);
        fprintf(f, ", ");
        output_quad_register_name(preg2);
        fprintf(f, "\n");
    }
}

void output_type_specific_register_name(int type, int preg) {
         if (type == TYPE_CHAR)  output_byte_register_name(preg);
    else if (type == TYPE_SHORT) output_word_register_name(preg);
    else if (type == TYPE_INT)   output_long_register_name(preg);
    else                         output_quad_register_name(preg);
}

void output_op_instruction(char *instruction, int src, int dst) {
    fprintf(f, "\t%s\t", instruction);
    output_quad_register_name(src);
    fprintf(f, ", ");
    output_quad_register_name(dst);
    fprintf(f, "\n");
}

void _output_op(char *instruction, Tac *tac) {
    int v, dst, src1, src2;

    dst = tac->dst->preg;
    src1 = tac->src1->preg;
    src2 = tac->src2->preg;

    // Special cases for commutative operations
    if (tac->operation == IR_ADD || tac->operation == IR_MUL || tac->operation == IR_BOR || tac->operation == IR_BAND || tac->operation == IR_XOR) {
        if (dst == src1) {
            // pn = pn + pm
            output_move_quad_register_to_register(src1, dst);
            output_op_instruction(instruction, src2, dst);
            return;
        }
        else if (dst == src2) {
            // pn = pm + pn
            output_op_instruction(instruction, src1, dst);
            return;
        }
    }

    else if (tac->operation == IR_RSUB) {
        // The register allocator must not allow dst to be the same as src1,
        // otherwise we get an incorrect result:
        // mov src2 -> dst
        // dst = src1 - src1 = 0
        if (dst == src1) {
            print_instruction(stdout, tac);
            panic("Illegal attempt to use the same preg for dst and src1 in subtraction");
        }
    }

    output_move_quad_register_to_register(src2, dst);
    output_op_instruction(instruction, src1, dst);
}

void output_constant_operation(char *instruction, Tac *tac) {
    if (tac->src1->is_constant) {
        output_move_quad_register_to_register(tac->src2->preg, tac->dst->preg);
        fprintf(f, "\t%s\t$%ld", instruction, tac->src1->value);
        fprintf(f, ", ");
        output_quad_register_name(tac->dst->preg);
        fprintf(f, "\n");
    }
    else
        _output_op(instruction, tac);
}

void output_op(char *instruction, Tac *tac) {
    if (tac->operation == IR_ADD || tac->operation == IR_RSUB || tac->operation == IR_MUL || tac->operation == IR_BAND || tac->operation == IR_BOR || tac->operation == IR_XOR) {
        output_constant_operation(instruction, tac);
        return;
    }
    else
        _output_op(instruction, tac);
}

// src1 and src2 are swapped, to facilitate treating the first operand as a potential
// constant. src1 is output first, which is backwards to src1 <op> src2.
// Therefore, this function is called reverse.
void output_reverse_cmp(Tac *tac) {
    fprintf(f, "\tcmpq\t");

    if (tac->src1->is_constant)
        fprintf(f, "$%ld", tac->src1->value);
    else
        output_quad_register_name(tac->src1->preg);

    fprintf(f, ", ");
    output_quad_register_name(tac->src2->preg);
    fprintf(f, "\n");
}

void output_cmp_result_instruction(Tac *ir, char *instruction) {
    fprintf(f, "\t%s\t", instruction);
    output_byte_register_name(ir->dst->preg);
    fprintf(f, "\n");
}

void output_movzbq(Tac *ir) {
    fprintf(f, "\tmovzbq\t");
    output_byte_register_name(ir->dst->preg);
    fprintf(f, ", ");
    output_quad_register_name(ir->dst->preg);
    fprintf(f, "\n");
}

void output_type_specific_mov(int type) {
         if (type == TYPE_CHAR)  fprintf(f, "\tmovb\t");
    else if (type == TYPE_SHORT) fprintf(f, "\tmovw\t");
    else if (type == TYPE_INT)   fprintf(f, "\tmovl\t");
    else                         fprintf(f, "\tmovq\t");
}

void output_type_specific_sign_extend_mov(int type) {
         if (type == TYPE_CHAR)  fprintf(f, "\tmovsbq\t");
    else if (type == TYPE_SHORT) fprintf(f, "\tmovswq\t");
    else if (type == TYPE_INT)   fprintf(f, "\tmovslq\t");
    else                         fprintf(f, "\tmovq\t");
}

void output_type_specific_lea(int type) {
         if (type == TYPE_CHAR)  fprintf(f, "\tleasbq\t");
    else if (type == TYPE_SHORT) fprintf(f, "\tleaswq\t");
    else if (type == TYPE_INT)   fprintf(f, "\tleaslq\t");
    else                         fprintf(f, "\tleaq\t");
}

void output_reverse_cmp_operation(Tac *tac, char *instruction) {
    output_reverse_cmp(tac);
    if (tac->dst->is_in_cpu_flags) return;
    output_cmp_result_instruction(tac, instruction);
    output_movzbq(tac);
}

// Get offset from the stack in bytes, from a stack_index. This is a hairy function
// since it has to take the amount of params (pc) into account as well as the
// position where local variables start.
//
// The stack layout
// stack index  offset  var
// 1            +3     first passed arg
// 0            +2     second passed arg
//              +1     return address
//              +0     BP
//                     saved registers
// -1           -1     first local
// -2           -2     second local
int get_stack_offset_from_index(int function_pc, int stack_start, int stack_index) {
    int stack_offset;

    if (stack_index >= 2) {
        // Function argument
        stack_index -= 2; // 0=1st arg, 1=2nd arg, etc

        if (function_pc > 6) {
            stack_index = function_pc - 7 - stack_index;
            // Correct for split stack when there are more than 6 args
            if (stack_index < 0) {
                // Read pushed arg by the callee. arg 0 is at rsp-0x30, arg 2 at rsp-0x28 etc, ... arg 5 at rsp-0x08
                stack_offset = 8 * stack_index;
            }
            else {
                // Read pushed arg by the caller, arg 6 is at rsp+0x10, arg 7 at rsp+0x18, etc
                // The +2 is to bypass the pushed rbp and return address
                stack_offset = 8 * (stack_index + 2);
            }
        }
        else {
            // The first arg is at stack_index=0, second at stack_index=1
            // If there are e.g. 2 args:
            // arg 0 is at mov -0x10(%rbp), %rax
            // arg 1 is at mov -0x08(%rbp), %rax
            stack_offset = -8 * (stack_index + 1);
        }
    }
    else {
        // Local variable. v=-1 is the first, v=-2 the second, etc
        stack_offset = stack_start + 8 * (stack_index + 1);
    }

    return stack_offset;
}

void output_x86_operation(Tac *tac) {
    char *t;
    Value *v;

    t = tac->x86_template;

    fprintf(f, "\t");

    while (*t) {
        if (*t == '%') {
            if (t[1] == '%') {
                fprintf(f, "%%");
                t += 1;
            }
            else {
                if (t[2] == '1') v = tac->src1;
                else if (t[2] == '2') v = tac->src2;
                else panic1d("Unknown placeholder number %d", t[2]);

                if (!v) panic1s("Unexpectedly got a null value while the template %s is expecting it", tac->x86_template);
                if (v->is_constant)
                    fprintf(f, "%ld", v->value);
                else if (v->is_string_literal)
                    fprintf(f, "%d", v->string_literal_index);
                else if (v->preg != -1)
                    output_quad_register_name(v->preg);
                else if (v->global_symbol)
                    fprintf(f, "%s", v->global_symbol->identifier);
                else if (v->label)
                    fprintf(f, "%d", v->label);
                else panic("Don't know how to render template value");
                t += 2;
            }
        }
        else
            fprintf(f, "%c", *t);

        t++;
    }

    fprintf(f, "\n");
}

// Called once at startup to indicate which registers are preserved across function calls
void init_callee_saved_registers() {
    callee_saved_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(callee_saved_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

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
            fprintf(f, "\tpushq\t");
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
            cur_stack_push_count--;
            fprintf(f, "\tpopq\t");
            output_quad_register_name(i);
            fprintf(f, "\n");
        }
    }
}

void output_pre_instruction_spill(Tac *ir, int function_pc, int stack_start) {
    int stack_offset;

    // Load src1 into r10
    if (ir->operation != IR_LOAD_VARIABLE && ir->src1 && ir->src1->preg == -1 && ir->src1->stack_index < 0) {
        stack_offset = get_stack_offset_from_index(function_pc, stack_start, ir->src1->stack_index);

        if (ir->operation == IR_ASSIGN_TO_REG_LVALUE)
            fprintf(f, "\tmovq\t");
        else
            output_type_specific_sign_extend_mov(ir->src1->type);

        fprintf(f, "%d(%%rbp), %%r10\n", stack_offset);
        ir->src1 = dup_value(ir->src1); // Ensure no side effects
        ir->src1->preg = REG_R10;
    }

    // Load src2 into r11
    if (ir->src2 && ir->src2->preg == -1 && ir->src2->stack_index < 0) {
        stack_offset = get_stack_offset_from_index(function_pc, stack_start, ir->src2->stack_index);
        output_type_specific_sign_extend_mov(ir->src2->type);
        fprintf(f, "%d(%%rbp), %%r11\n", stack_offset);
        ir->src2 = dup_value(ir->src2); // Ensure no side effects
        ir->src2->preg = REG_R11;
    }

    if (ir->operation != IR_ASSIGN && ir->dst && ir->dst->preg == -1 && ir->dst->stack_index < 0) {
        // Set the dst preg to r10 or r11 depending on what the operation type has set

        ir->dst = dup_value(ir->dst); // Ensure no side effects
        if (ir->operation == IR_BSHR || ir->operation == IR_BSHL)
            ir->dst->preg = REG_R10;
        else
            ir->dst->preg = REG_R11;
    }

    // If the operation is an assignment and If there is an lvalue on the stack, move it into r11.
    // The assign code will use that to store the result of the assignment.
    if (ir->operation == IR_ASSIGN && ir->dst->preg == -1 && ir->dst->is_lvalue && ir->dst->stack_index < 0) {
        stack_offset = get_stack_offset_from_index(function_pc, stack_start, ir->dst->stack_index);
        fprintf(f, "\tmovq\t%d(%%rbp), %%r11\n", stack_offset);
    }
}

void output_post_instruction_spill(Tac *ir, int function_pc, int stack_start) {
    int stack_offset;
    int assign;

    assign = 0;
    if (ir->dst && ir->dst->preg != -1 && ir->dst->stack_index < 0) {
        // Output a mov for assignments that are a copy from a cpu flag to a local variable on the stack
        if (ir->operation == IR_ASSIGN && ir->src1->is_in_cpu_flags && ir->dst->stack_index) assign = 1;

        // Output a mov for assignments that are a register copy.
        if (!(ir->operation == IR_ASSIGN && (ir->dst->stack_index || ir->dst->global_symbol || ir->dst->is_lvalue || ir->dst->is_in_cpu_flags))) assign = 1;

        if (assign) {
            stack_offset = get_stack_offset_from_index(function_pc, stack_start, ir->dst->stack_index);
            fprintf(f, "\tmovq\t");
            output_quad_register_name(ir->dst->preg);
            fprintf(f, ", %d(%%rbp)\n", stack_offset);
        }
    }
}

// Output code from the IR of a function
void output_function_body_code(Symbol *symbol) {
    int i, stack_offset;
    Tac *tac;
    int function_pc;                    // The Function's param count
    int ac;                             // A function call's arg count
    int local_stack_size;               // Size of the stack containing local variables and spilled registers
    int stack_start;                    // Stack start for the local variables
    int *saved_registers;               // Callee saved registers
    int need_aligned_call_push;         // If an extra push has been done before function call args to align the stack
    char *s;

    cur_stack_push_count++;
    function_pc = symbol->function->param_count;

    fprintf(f, "\tpush\t%%rbp\n");
    fprintf(f, "\tmovq\t%%rsp, %%rbp\n");

    // Push up to the first 6 args onto the stack, so all args are on the stack with leftmost arg first.
    // Arg 7 and onwards are already pushed.

    // Push the args in the registers on the stack. The order for all args is right to left.
    if (function_pc >= 6) { cur_stack_push_count++; fprintf(f, "\tpush\t%%r9\n");  }
    if (function_pc >= 5) { cur_stack_push_count++; fprintf(f, "\tpush\t%%r8\n");  }
    if (function_pc >= 4) { cur_stack_push_count++; fprintf(f, "\tpush\t%%rcx\n"); }
    if (function_pc >= 3) { cur_stack_push_count++; fprintf(f, "\tpush\t%%rdx\n"); }
    if (function_pc >= 2) { cur_stack_push_count++; fprintf(f, "\tpush\t%%rsi\n"); }
    if (function_pc >= 1) { cur_stack_push_count++; fprintf(f, "\tpush\t%%rdi\n"); }

    // Calculate stack start for locals. reduce by pushed bsp and above pushed args.
    stack_start = -8 - 8 * (function_pc <= 6 ? function_pc : 6);

    // Allocate stack space for local variables and spilled registers
    local_stack_size = 8 * (symbol->function->spilled_register_count);

    // Allocate local stack
    if (local_stack_size > 0) {
        fprintf(f, "\tsubq\t$%d, %%rsp\n", local_stack_size);
        cur_stack_push_count += local_stack_size / 8;
    }

    tac = symbol->function->ir;
    saved_registers = output_push_callee_saved_registers(tac);

    while (tac) {
        if (output_inline_ir) {
            fprintf(f, "\t// ------------------------------------- ");
            print_instruction(f, tac);
        }

        output_pre_instruction_spill(tac, function_pc, stack_start);

        if (tac->label) fprintf(f, ".l%d:\n", tac->label);

        if (tac->operation == IR_NOP || tac->operation == IR_START_LOOP || tac->operation == IR_END_LOOP);

        else if (tac->operation > X_START && tac->operation != X_RET && tac->operation != X_ARG)
            output_x86_operation(tac);

        else if (tac->operation == IR_LOAD_CONSTANT) {
            fprintf(f, "\tmovq\t$%ld, ", tac->src1->value);
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_LOAD_STRING_LITERAL) {
            fprintf(f, "\tleaq\t.SL%d(%%rip), ", tac->src1->string_literal_index);
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_LOAD_VARIABLE) {
            if (tac->src1->global_symbol) {
                if (!tac->src1->is_lvalue) {
                    output_type_specific_lea(tac->src1->type);
                    fprintf(f, "%s(%%rip), ", tac->src1->global_symbol->identifier);
                }
                else {
                    output_type_specific_sign_extend_mov(tac->src1->type);
                    fprintf(f, "%s(%%rip), ", tac->src1->global_symbol->identifier);
                }
            }
            else {
                stack_offset = get_stack_offset_from_index(function_pc, stack_start, tac->src1->stack_index);
                if (!tac->src1->is_lvalue) {
                    output_type_specific_lea(tac->src1->type);
                    fprintf(f, "%d(%%rbp), ", stack_offset);
                }
                else {
                    output_type_specific_sign_extend_mov(tac->src1->type);
                    fprintf(f, "%d(%%rbp), ", stack_offset);
                }
            }

            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_INDIRECT) {
            output_type_specific_sign_extend_mov(tac->dst->type);
            fprintf(f, "(");
            output_quad_register_name(tac->src1->preg);
            fprintf(f, "), ");
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_START_CALL) {
            // Align the stack. This is matched with a pop when the function call ends
            need_aligned_call_push = ((cur_stack_push_count + tac->src1->function_call_arg_count) % 2 == 0);
            if (need_aligned_call_push)  {
                tac->src1->pushed_stack_aligned_quad = 1;
                cur_stack_push_count++;
                fprintf(f, "\tsubq\t$8, %%rsp\n");
            }
        }

        else if (tac->operation == IR_END_CALL) {
            if (tac->src1->pushed_stack_aligned_quad)  {
                cur_stack_push_count--;
                fprintf(f, "\taddq\t$8, %%rsp\n");
            }
        }

        else if (tac->operation == IR_ARG) {
            cur_stack_push_count++;
            fprintf(f, "\tpushq\t");
            output_quad_register_name(tac->src2->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == X_ARG) {
            cur_stack_push_count++;
            output_x86_operation(tac);
        }

        else if (tac->operation == IR_CALL) {
            // Read the first 6 args from the stack in right to left order
            ac = tac->src1->function_call_arg_count;

            if (ac >= 1) { cur_stack_push_count--; fprintf(f, "\tpopq\t%%rdi\n"); }
            if (ac >= 2) { cur_stack_push_count--; fprintf(f, "\tpopq\t%%rsi\n"); }
            if (ac >= 3) { cur_stack_push_count--; fprintf(f, "\tpopq\t%%rdx\n"); }
            if (ac >= 4) { cur_stack_push_count--; fprintf(f, "\tpopq\t%%rcx\n"); }
            if (ac >= 5) { cur_stack_push_count--; fprintf(f, "\tpopq\t%%r8\n");  }
            if (ac >= 6) { cur_stack_push_count--; fprintf(f, "\tpopq\t%%r9\n");  }

            // Variadic functions have the number of floating point arguments passed in al.
            // Since floating point numbers isn't implemented, this is zero.
            if (tac->src1->function_symbol->function->is_variadic)
                fprintf(f, "\tmovb\t$0, %%al\n");

            if (tac->src1->function_symbol->function->builtin)
                fprintf(f, "\tcallq\t%s@PLT\n", tac->src1->function_symbol->identifier);
            else
                fprintf(f, "\tcallq\t%s\n", tac->src1->function_symbol->identifier);

            // For all builtins that return something smaller an int, extend it to a quad
            if (tac->dst) {
                if (tac->dst->type <= TYPE_CHAR)  fprintf(f, "\tcbtw\n");
                if (tac->dst->type <= TYPE_SHORT) fprintf(f, "\tcwtl\n");
                if (tac->dst->type <= TYPE_INT)   fprintf(f, "\tcltq\n");

                fprintf(f, "\tmovq\t%%rax, ");
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }

            // Adjust the stack for any args that are on in stack
            if (ac > 6) {
                fprintf(f, "\taddq\t$%d, %%rsp\n", (ac - 6) * 8);
                cur_stack_push_count -= ac - 6;
            }
        }

        else if (tac->operation == X_RET) {
            output_x86_operation(tac);
            output_pop_callee_saved_registers(saved_registers);
            fprintf(f, "\tleaveq\n");
            fprintf(f, "\tretq\n");
        }

        else if (tac->operation == IR_RETURN) {
            if (tac->src1) {
                if (tac->src1->preg != REG_RAX) {
                    fprintf(f, "\tmovq\t");
                    output_quad_register_name(tac->src1->preg);
                    fprintf(f, ", ");
                    output_quad_register_name(REG_RAX);
                    fprintf(f, "\n");
                }
            }
            output_pop_callee_saved_registers(saved_registers);
            fprintf(f, "\tleaveq\n");
            fprintf(f, "\tretq\n");
        }

        else if (tac->operation == IR_ASSIGN) {
            if (tac->dst->is_in_cpu_flags); // Do nothing

            else if (tac->src1->is_in_cpu_flags) {
                     if (tac->prev->operation == IR_EQ) s = "sete";
                else if (tac->prev->operation == IR_NE) s = "setne";
                else if (tac->prev->operation == IR_LT) s = "setg";
                else if (tac->prev->operation == IR_GT) s = "setl";
                else if (tac->prev->operation == IR_LE) s = "setge";
                else if (tac->prev->operation == IR_GE) s = "setle";
                else panic1d("Internal error: unknown comparison operation %d", tac->prev->operation);
                output_cmp_result_instruction(tac, s);
                output_movzbq(tac);
            }

            else if (tac->dst->global_symbol) {
                // dst a global
                if (tac->dst->preg != -1) panic("Unexpected preg in assign for global");
                output_type_specific_mov(tac->dst->type);
                output_type_specific_register_name(tac->dst->type, tac->src1->preg);
                fprintf(f, ", ");
                fprintf(f, "%s(%%rip)\n", tac->dst->global_symbol->identifier);
            }

            else if (tac->dst->preg != -1) {
                // Register copy
                fprintf(f, "\tmovq\t");
                output_quad_register_name(tac->src1->preg);
                fprintf(f, ", ");
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }

            else if (!tac->dst->stack_index && tac->dst->is_lvalue)
                panic("Internal error: Assign to lvalue in register should have been rewritten to IR_ASSIGN_TO_REG_LVALUE");

            else {
                // dst is a local variable on the stack
                output_type_specific_mov(tac->dst->type);
                output_type_specific_register_name(tac->dst->type, tac->src1->preg);
                fprintf(f, ", ");
                stack_offset = get_stack_offset_from_index(function_pc, stack_start, tac->dst->stack_index);
                fprintf(f, "%d(%%rbp)\n", stack_offset);
            }
        }

        else if (tac->operation == IR_ASSIGN_TO_REG_LVALUE) {
            // dst is an lvalue in a register. The difference with the regular
            // IS_ASSIGN is that src1 is the destination and src2 is the src.
            // The reason for that is that it makes the SSA calulation easier
            // since both dst and src1 are values in registers that are read
            // but not written to in this instruction.

            output_type_specific_mov(tac->src1->type);
            output_type_specific_register_name(tac->src1->type, tac->src2->preg);
            fprintf(f, ", ");
            fprintf(f, "(");
            output_quad_register_name(tac->src1->preg);
            fprintf(f, ")\n");
        }

        else if (tac->operation == IR_JZ || tac->operation == IR_JNZ) {
            if (tac->src1->is_in_cpu_flags) {
                // src1 and src2 is backwards to facilitate constant handling,
                // hence the flipping of the comparision direction.
                     if (tac->prev->operation == IR_GT && tac->operation == IR_JNZ) s = "jl";
                else if (tac->prev->operation == IR_GT && tac->operation == IR_JZ)  s = "jnl";
                else if (tac->prev->operation == IR_LT && tac->operation == IR_JNZ) s = "jg";
                else if (tac->prev->operation == IR_LT && tac->operation == IR_JZ)  s = "jng";
                else if (tac->prev->operation == IR_GE && tac->operation == IR_JNZ) s = "jle";
                else if (tac->prev->operation == IR_GE && tac->operation == IR_JZ)  s = "jnle";
                else if (tac->prev->operation == IR_LE && tac->operation == IR_JNZ) s = "jge";
                else if (tac->prev->operation == IR_LE && tac->operation == IR_JZ)  s = "jnge";
                else if (tac->prev->operation == IR_EQ && tac->operation == IR_JNZ) s = "je";
                else if (tac->prev->operation == IR_EQ && tac->operation == IR_JZ)  s = "jne";
                else if (tac->prev->operation == IR_NE && tac->operation == IR_JNZ) s = "jne";
                else if (tac->prev->operation == IR_NE && tac->operation == IR_JZ)  s = "je";

                else {
                    panic1d("Unknown comparison operator %d", tac->prev->operation);
                    exit(1);
                }

                fprintf(f, "\t%s\t.l%d\n", s, tac->src2->label);
            }

            else  {
                // The condition is in a register. Check if it's zero.
                fprintf(f, "\tcmpq\t$0x0, ");
                output_quad_register_name(tac->src1->preg);
                fprintf(f, "\n");
                fprintf(f, "\t%s\t.l%d\n", tac->operation == IR_JZ ? "je" : "jne", tac->src2->label);
            }
        }

        else if (tac->operation == IR_JMP)
            fprintf(f, "\tjmp\t.l%d\n", tac->src1->label);

        else if (tac->operation == IR_EQ) output_reverse_cmp_operation(tac, "sete");
        else if (tac->operation == IR_NE) output_reverse_cmp_operation(tac, "setne");
        else if (tac->operation == IR_LT) output_reverse_cmp_operation(tac, "setg");
        else if (tac->operation == IR_GT) output_reverse_cmp_operation(tac, "setl");
        else if (tac->operation == IR_LE) output_reverse_cmp_operation(tac, "setge");
        else if (tac->operation == IR_GE) output_reverse_cmp_operation(tac, "setle");

        else if (tac->operation == IR_BOR)  output_op("orq",   tac);
        else if (tac->operation == IR_BAND) output_op("andq",  tac);
        else if (tac->operation == IR_XOR)  output_op("xorq",  tac);
        else if (tac->operation == IR_ADD)  output_op("addq",  tac);
        else if (tac->operation == IR_RSUB) output_op("subq",  tac);
        else if (tac->operation == IR_MUL)  output_op("imulq", tac);

        else if (tac->operation == IR_DIV || tac->operation == IR_MOD) {
            // This is slightly ugly. src1 is the dividend and needs doubling in size and placing in the RDX register.
            // It could have been put in the RDX register in the first place.
            // The quotient is stored in RAX and remainder in RDX, but is then copied
            // to whatever register is allocated for the dst, which might as well have been RAX or RDX for the respective quotient and remainders.

            fprintf(f, "\tmovq\t");
            output_quad_register_name(tac->src1->preg);
            fprintf(f, ", %%rax\n");
            fprintf(f, "\tcqto\n");
            output_move_quad_register_to_register(tac->src2->preg, tac->dst->preg);
            fprintf(f, "\tidivq\t");
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");

            if (tac->operation == IR_DIV)
                output_move_quad_register_to_register(REG_RAX, tac->dst->preg);
            else
                output_move_quad_register_to_register(REG_RDX, tac->dst->preg);
        }

        else if (tac->operation == IR_BNOT)  {
            output_move_quad_register_to_register(tac->src1->preg, tac->dst->preg);
            fprintf(f, "\tnot\t");
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_BSHL || tac->operation == IR_BSHR) {
            if (tac->src2->is_constant) {
                // Shift a non-constant by a constant amount
                output_move_quad_register_to_register(tac->src1->preg, tac->dst->preg);
                fprintf(f, "\t%s\t$%ld, ", tac->operation == IR_BSHL ? "shl" : "sar", tac->src2->value);
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }
            else {
                fprintf(f, "\tmovq\t");
                output_quad_register_name(tac->src2->preg);
                fprintf(f, ", %%rcx\n");

                if (tac->src1->is_constant) {
                    // Shift a constant by a non-constant amount
                    fprintf(f, "\tmovq\t$%ld, ", tac->src1->value);
                    output_quad_register_name(tac->dst->preg);
                    fprintf(f, "\n");
                }
                else
                    // Shift a non-constant by a non-constant amount
                    output_move_quad_register_to_register(tac->src1->preg, tac->dst->preg);

                fprintf(f, "\t%s\t%%cl, ", tac->operation == IR_BSHL ? "shl" : "sar");
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }
        }

        else
            panic1d("output_function_body_code(): Unknown operation: %d", tac->operation);

        output_post_instruction_spill(tac, function_pc, stack_start);

        tac = tac->next;
    }

    // Special case for main, return 0 if no return statement is present
    if (!strcmp(symbol->identifier, "main")) fprintf(f, "\tmovq\t$0, %%rax\n");

    output_pop_callee_saved_registers(saved_registers);
    fprintf(f, "\tleaveq\n");
    fprintf(f, "\tretq\n");
}

// Output code for the translation unit
void output_code(char *input_filename, char *output_filename) {
    int i;
    Tac *tac;
    Symbol *s;
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

    fprintf(f, "\t.file\t\"%s\"\n", input_filename);

    // Output symbols
    fprintf(f, "\t.text\n");
    s = symbol_table;
    while (s->identifier) {
        if (s->scope || s->is_function) { s++; continue; };
        fprintf(f, "\t.comm %s,%d,%d\n", s->identifier, get_type_size(s->type), get_type_alignment(s->type));
        s++;
    }

    // Output string literals
    if (string_literal_count > 0) {
        fprintf(f, "\n\t.section\t.rodata\n");
        for (i = 0; i < string_literal_count; i++) {
            sl = string_literals[i];
            fprintf(f, ".SL%d:\n", i);
            fprintf(f, "\t.string ");
            fprintf_escaped_string_literal(f, sl);
            fprintf(f, "\n");
        }
        fprintf(f, "\n");
    }

    // Output code
    fprintf(f, "\t.text\n");

    // Output symbols for all functions
    s = symbol_table;
    while (s->identifier) {
        if (s->is_function) fprintf(f, "\t.globl\t%s\n", s->identifier);
        s++;
    }
    fprintf(f, "\n");

    label_count = 0; // Used in label renumbering

    // Generate body code for all functions
    s = symbol_table;
    while (s->identifier) {
        if (!s->is_function || !s->function->is_defined) { s++; continue; }

        cur_stack_push_count = 0;

        fprintf(f, "%s:\n", s->identifier);

        if (print_ir1) print_intermediate_representation(s->function, s->identifier);

        vreg_count = s->function->vreg_count;
        optimize_ir(s);
        s->function->vreg_count = vreg_count;

        if (print_ir2) print_intermediate_representation(s->function, s->identifier);

        if (instruction_selection_wip)
            experimental_instruction_selection(s);
        else
            optimize_and_allocate_registers(s->function);

        if (print_ir3) print_intermediate_representation(s->function, s->identifier);

        output_function_body_code(s);
        fprintf(f, "\n");
        s++;
    }

    fclose(f);
}
