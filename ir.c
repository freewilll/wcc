#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

// Allocate a new local variable or tempoary
static int new_local_index(Function *function) {
    return -1 - function->local_symbol_count++;
}

void init_value(Value *v) {
    v->preg = -1;
    v->stack_index = 0;
    v->ssa_subscript = -1;
    v->live_range = -1;
}

Value *new_value() {
    Value *v = malloc(sizeof(Value));
    memset(v, 0, sizeof(Value));
    init_value(v);

    return v;
}

// Create a new typed integral constant value.
Value *new_integral_constant(int type_type, long value) {
    Value *cv = new_value();
    cv->int_value = value;
    cv->type = new_type(type_type);
    cv->is_constant = 1;
    return cv;
}

// Create a new typed floating point constant value.
Value *new_floating_point_constant(int type_type, long double value) {
    Value *cv = new_value();
    cv->fp_value = value;
    cv->type = new_type(type_type);
    cv->is_constant = 1;
    return cv;
}

// Duplicate a value
Value *dup_value(Value *src) {
    Value *dst = new_value();
    *dst = *src;
    dst->type = dup_type(src->type);

    return dst;
}

void add_tac_to_ir(Tac *tac) {
    tac->next = 0;

    if (!ir_start) {
        ir_start = tac;
        ir = tac;
    }
    else {
        tac->prev = ir;
        ir->next = tac;
        ir = tac;
    }
}

Tac *new_instruction(int operation) {
    Tac *tac = malloc(sizeof(Tac));
    memset(tac, 0, sizeof(Tac));
    tac->operation = operation;

    return tac;
}

// Add instruction to the global intermediate representation ir
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac = new_instruction(operation);
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    add_tac_to_ir(tac);

    return tac;
}

// Ensure the double linked list in an IR is correct by checking last pointers
void sanity_test_ir_linkage(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->next && tac->next->prev != tac) {
            printf("Linkage broken between:\n");
            print_instruction(stdout, tac, 0);
            print_instruction(stdout, tac->next, 0);
            panic("Bailing");
        }
    }
}

int fprintf_escaped_string_literal(void *f, char* sl) {
    int c = fprintf(f, "\"");
    while (*sl) {
             if (*sl == '\n') c += fprintf(f, "\\n");
        else if (*sl == '\t') c += fprintf(f, "\\t");
        else if (*sl == '\\') c += fprintf(f, "\\\\");
        else if (*sl == '"')  c += fprintf(f, "\\\"");
        else                  c += fprintf(f, "%c", *sl);
        sl++;
    }
    c += fprintf(f, "\"");

    return c;
}

int print_value(void *f, Value *v, int is_assignment_rhs) {
    int c = 0; // Count outputted characters

    if (!v) panic("print_value() got null");

    if (v->is_lvalue) c += printf("{l}");
    if (v->is_lvalue_in_register) c += printf("{lr}");
    if (is_assignment_rhs && !v->is_lvalue && !v->vreg && !v->is_constant) c += fprintf(f, "&");
    if (!is_assignment_rhs && v->is_lvalue && v->vreg) c += fprintf(f, "L");

    if (v->is_constant) {
        if (is_integer_type(v->type))
            c += fprintf(f, "%ld", v->int_value);
        else
            c += fprintf(f, "%Lf", v->fp_value);
    }
    else if (v->preg != -1)
        c += fprintf(f, "p%d", v->preg);
    else if (v->local_index)
        c += fprintf(f, "L[%d]", v->local_index);
    else if (v->stack_index)
        c += fprintf(f, "S[%d]", v->stack_index);
    else if (v->vreg) {
        c += fprintf(f, "r%d", v->vreg);
        if (v->ssa_subscript != -1) c += fprintf(f, "_%d", v->ssa_subscript);
    }
    else if (v->stack_index)
        c += fprintf(f, "s[%d]", v->stack_index);
    else if (v->global_symbol)
        c += fprintf(f, "%s", v->global_symbol->identifier);
    else if (v->is_string_literal)
        c += fprintf_escaped_string_literal(f, string_literals[v->string_literal_index]);
    else if (v->label)
        c += fprintf(f, "l%d", v->label);
    else if (v->function_symbol) {
        c += fprintf(f, "function:%s", v->function_symbol->identifier);
        return c;
    }
    else
        // What's this?
        c += fprintf(f, "?%ld", v->int_value);

    if (v->offset) c += fprintf(f, "[%d]", v->offset);

    if (!v->label) {
        c += fprintf(f, ":");
        c +=  print_type(f, v->type);
    }

    return c;
}

char *operation_string(int operation) {
         if (!operation)                            return "";
    else if (operation == IR_MOVE)                  return "IR_MOVE";
    else if (operation == IR_MOVE_PREG_CLASS)       return "IR_MOVE_PREG_CLASS";
    else if (operation == IR_MOVE_STACK_PTR)        return "IR_MOVE_STACK_PTR";
    else if (operation == IR_ADDRESS_OF)            return "IR_ADDRESS_OF";
    else if (operation == IR_INDIRECT)              return "IR_INDIRECT";
    else if (operation == IR_DECL_LOCAL_COMP_OBJ)   return "IR_DECL_LOCAL_COMP_OBJ";
    else if (operation == IR_START_CALL)            return "IR_START_CALL";
    else if (operation == IR_ARG)                   return "IR_ARG";
    else if (operation == IR_ARG_STACK_PADDING)     return "IR_ARG_STACK_PADDING";
    else if (operation == IR_CALL)                  return "IR_CALL";
    else if (operation == IR_CALL_ARG_REG)          return "IR_CALL_ARG_REG";
    else if (operation == IR_END_CALL)              return "IR_END_CALL";
    else if (operation == IR_RETURN)                return "IR_RETURN";
    else if (operation == IR_START_LOOP)            return "IR_START_LOOP";
    else if (operation == IR_END_LOOP)              return "IR_END_LOOP";
    else if (operation == IR_ALLOCATE_STACK)        return "IR_ALLOCATE_STACK";
    else if (operation == IR_MOVE_TO_PTR)           return "IR_MOVE_TO_PTR";
    else if (operation == IR_NOP)                   return "IR_NOP";
    else if (operation == IR_JMP)                   return "IR_JMP";
    else if (operation == IR_JZ)                    return "IR_JZ";
    else if (operation == IR_JNZ)                   return "IR_JNZ";
    else if (operation == IR_ADD)                   return "IR_ADD";
    else if (operation == IR_SUB)                   return "IR_SUB";
    else if (operation == IR_RSUB)                  return "IR_RSUB";
    else if (operation == IR_MUL)                   return "IR_MUL";
    else if (operation == IR_DIV)                   return "IR_DIV";
    else if (operation == IR_MOD)                   return "IR_MOD";
    else if (operation == IR_EQ)                    return "IR_EQ";
    else if (operation == IR_NE)                    return "IR_NE";
    else if (operation == IR_BNOT)                  return "IR_BNOT";
    else if (operation == IR_BOR)                   return "IR_BOR";
    else if (operation == IR_BAND)                  return "IR_BAND";
    else if (operation == IR_XOR)                   return "IR_XOR";
    else if (operation == IR_BSHL)                  return "IR_BSHL";
    else if (operation == IR_BSHR)                  return "IR_BSHR";
    else if (operation == IR_LT)                    return "IR_LT";
    else if (operation == IR_GT)                    return "IR_GT";
    else if (operation == IR_LE)                    return "IR_LE";
    else if (operation == IR_GE)                    return "IR_GE";
    else if (operation == IR_PHI_FUNCTION)          return "IR_PHI_FUNCTION";
    else if (operation == X_MOV)                    return "mov";
    else if (operation == X_ADD)                    return "add";
    else if (operation == X_MUL)                    return "mul";
    else if (operation == X_IDIV)                   return "idiv";
    else if (operation == X_CQTO)                   return "cqto";
    else if (operation == X_CMP)                    return "cmp";
    else if (operation == X_COMIS)                  return "comis";
    else if (operation == X_TEST)                   return "test";
    else if (operation == X_CMPZ)                   return "cmpz";
    else if (operation == X_JMP)                    return "jmp";
    else if (operation == X_JZ)                     return "jz";
    else if (operation == X_JNZ)                    return "jnz";
    else if (operation == X_JE)                     return "je";
    else if (operation == X_JNE)                    return "jne";
    else if (operation == X_JLT)                    return "jlt";
    else if (operation == X_JGT)                    return "jgt";
    else if (operation == X_JLE)                    return "jle";
    else if (operation == X_JGE)                    return "jge";
    else if (operation == X_JB)                     return "jb";
    else if (operation == X_JA)                     return "ja";
    else if (operation == X_JBE)                    return "jbe";
    else if (operation == X_JAE)                    return "jae";
    else if (operation == X_SETE)                   return "sete";
    else if (operation == X_SETNE)                  return "setne";
    else if (operation == X_SETP)                   return "setp";
    else if (operation == X_SETNP)                  return "setnp";
    else if (operation == X_SETLT)                  return "setlt";
    else if (operation == X_SETGT)                  return "setgt";
    else if (operation == X_SETLE)                  return "setle";
    else if (operation == X_SETGE)                  return "setge";
    else if (operation == X_MOVS)                   return "movs";
    else if (operation == X_MOVZ)                   return "movz";
    else if (operation == X_MOVC)                   return "movc";
    else panic1d("Unknown x86 operation %d", operation);
}

void print_instruction(void *f, Tac *tac, int expect_preg) {
    int o = tac->operation;

    if (tac->label)
        fprintf(f, "l%-5d", tac->label);
    else
        fprintf(f, "      ");

    if (tac->x86_template) {
        char *buffer = render_x86_operation(tac, 0, expect_preg);
        fprintf(f, "%s\n", buffer);
        free(buffer);
        return;
    }

    if (tac->dst && o < X_START) {
        print_value(f, tac->dst, o != IR_MOVE);
        fprintf(f, " = ");
    }

    if (o == IR_MOVE_TO_PTR) {
        fprintf(f, "(");
        print_value(f, tac->src1, 0);
        fprintf(f, ") = ");
    }

    if (o == IR_MOVE || o == IR_MOVE_PREG_CLASS)
        print_value(f, tac->src1, 1);

    else if (o == IR_MOVE_STACK_PTR)
        fprintf(f, "rsp");

    else if (o == IR_DECL_LOCAL_COMP_OBJ)  {
        fprintf(f, "declare ");
        print_value(f, tac->src1, 1);
    }

    else if (o == IR_START_CALL) fprintf(f, "start call %ld", tac->src1->int_value);
    else if (o == IR_END_CALL) fprintf(f, "end call %ld", tac->src1->int_value);

    else if (o == IR_ARG || o == X_ARG) {
        fprintf(f, "arg for call %ld ", tac->src1->int_value);
        print_value(f, tac->src2, 1);
    }
    else if (o == IR_ARG_STACK_PADDING) {
        fprintf(f, "arg extra push");
    }

    else if (o == IR_CALL) {
        fprintf(f, "call \"%s\"", tac->src1->function_symbol->identifier);
    }

    else if (o == IR_RETURN) {
        if (tac->src1) {
            fprintf(f, "return ");
            print_value(f, tac->src1, 1);
        }
        else
            fprintf(f, "return");
    }

    else if (o == IR_START_LOOP) fprintf(f, "start loop par=%ld loop=%ld", tac->src1->int_value, tac->src2->int_value);
    else if (o == IR_END_LOOP)   fprintf(f, "end loop par=%ld loop=%ld",   tac->src1->int_value, tac->src2->int_value);

    else if (o == IR_MOVE_TO_PTR)
        print_value(f, tac->src2, 1);

    else if (o == IR_NOP)
        fprintf(f, "nop");

    else if (o == IR_JZ || o == IR_JNZ) {
             if (o == IR_JZ)  fprintf(f, "jz ");
        else if (o == IR_JNZ) fprintf(f, "jnz ");
        print_value(f, tac->src1, 1);
        fprintf(f, ", ");
        print_value(f, tac->src2, 1);
    }

    else if (o == IR_JMP)
        fprintf(f, "jmp l%d", tac->src1->label);

    else if (o == IR_PHI_FUNCTION) {
        fprintf(f, "Φ(");
        Value *v = tac->phi_values;
        int first = 1;
        while (v->type) {
            if (!first) fprintf(f, ", ");
            print_value(f, v, 1);
            first = 0;
            v++;
        }
        fprintf(f, ")");
    }

    else if (o == IR_INDIRECT)      {                               fprintf(f, "*");    print_value(f, tac->src1, 1); }
    else if (o == IR_ADDRESS_OF)    {                               fprintf(f, "&");    print_value(f, tac->src1, 1); }
    else if (o == IR_ADD)           { print_value(f, tac->src1, 1); fprintf(f, " + ");  print_value(f, tac->src2, 1); }
    else if (o == IR_SUB)           { print_value(f, tac->src1, 1); fprintf(f, " - ");  print_value(f, tac->src2, 1); }
    else if (o == IR_RSUB)          { print_value(f, tac->src2, 1); fprintf(f, " - ");  print_value(f, tac->src1, 1); fprintf(f, " [reverse]"); }
    else if (o == IR_MUL)           { print_value(f, tac->src1, 1); fprintf(f, " * ");  print_value(f, tac->src2, 1); }
    else if (o == IR_DIV)           { print_value(f, tac->src1, 1); fprintf(f, " / ");  print_value(f, tac->src2, 1); }
    else if (o == IR_MOD)           { print_value(f, tac->src1, 1); fprintf(f, " %% "); print_value(f, tac->src2, 1); }
    else if (o == IR_BNOT)          {                               fprintf(f, "~");    print_value(f, tac->src1, 1); }
    else if (o == IR_EQ)            { print_value(f, tac->src1, 1); fprintf(f, " == "); print_value(f, tac->src2, 1); }
    else if (o == IR_NE)            { print_value(f, tac->src1, 1); fprintf(f, " != "); print_value(f, tac->src2, 1); }
    else if (o == IR_GT)            { print_value(f, tac->src1, 1); fprintf(f, " > ");  print_value(f, tac->src2, 1); }
    else if (o == IR_LT)            { print_value(f, tac->src1, 1); fprintf(f, " < ");  print_value(f, tac->src2, 1); }
    else if (o == IR_GE)            { print_value(f, tac->src1, 1); fprintf(f, " >= "); print_value(f, tac->src2, 1); }
    else if (o == IR_LE)            { print_value(f, tac->src1, 1); fprintf(f, " <= "); print_value(f, tac->src2, 1); }
    else if (o == IR_BAND)          { print_value(f, tac->src1, 1); fprintf(f, " & ");  print_value(f, tac->src2, 1); }
    else if (o == IR_BOR)           { print_value(f, tac->src1, 1); fprintf(f, " | ");  print_value(f, tac->src2, 1); }
    else if (o == IR_XOR)           { print_value(f, tac->src1, 1); fprintf(f, " ^ ");  print_value(f, tac->src2, 1); }
    else if (o == IR_BSHL)          { print_value(f, tac->src1, 1); fprintf(f, " << "); print_value(f, tac->src2, 1); }
    else if (o == IR_BSHR)          { print_value(f, tac->src1, 1); fprintf(f, " >> "); print_value(f, tac->src2, 1); }
    else if (o == X_RET)            { fprintf(f, "ret "   ); if (tac->src1) print_value(f, tac->src1, 1); }
    else if (o == X_CALL)           { fprintf(f, "call "  ); print_value(f, tac->src1, 1); if (tac->dst) { printf(" -> "); print_value(f, tac->dst, 1); } }
    else if (o == IR_CALL_ARG_REG)  { fprintf(f, "call reg arg "); print_value(f, tac->src1, 1); }
    else if (o == IR_ALLOCATE_STACK){ fprintf(f, "allocate stack "); print_value(f, tac->src1, 1); }
    else if (o == X_LEA)            { fprintf(f, "lea "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }

    else if (o == X_MOV)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MOVS)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MOVZ)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MOVC)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_ADD)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_MUL)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_IDIV)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_CQTO)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X_CMP)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X_CMPZ)   { fprintf(f, "%-6s", operation_string(o)); fprintf(f, "0");              fprintf(f, ", "); print_value(f, tac->src1, 1); }
    else if (o == X_JMP)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JZ)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JNZ)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JE)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JNE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JLT)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JGT)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JLE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JGE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JB)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JA)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JBE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_JAE)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETE)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETNE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETLT)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETGT)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETLE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETGE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }

    else
        panic1d("print_instruction(): Unknown operation: %d", tac->operation);

    fprintf(f, "\n");
}

void print_ir(Function *function, char* name, int expect_preg) {
    if (name) fprintf(stdout, "%s:\n", name);

    int i = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        fprintf(stdout, "%-4d > ", i++);
        print_instruction(stdout, tac, expect_preg);
    }
    fprintf(stdout, "\n");
}

// Merge tac with the instruction after it. The next instruction is removed from the chain.
static void merge_instructions(Tac *tac, int ir_index, int allow_labelled_next) {
    if (!tac->next) panic("merge_instructions called on a tac without next\n");
    if (tac->next->label && !allow_labelled_next) panic("merge_instructions called on a tac with a label");

    Tac *next = tac->next;
    tac->next = next->next;
    if (tac->next) tac->next->prev = tac;

    // Nuke the references to the removed node to prevent confusion
    next->prev = 0;
    next->next = 0;
}

int make_function_call_count(Function *function) {
    // Need to count this IR's function_call_count
    int function_call_count = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->operation == IR_START_CALL) function_call_count++;

    return function_call_count;
}

// The arguments are pushed onto the stack right to left, but the ABI requries
// the seventh arg and later to be pushed in reverse order. Easiest is to flip
// all args backwards, so they are pushed left to right.
void reverse_function_argument_order(Function *function) {
    TacInterval *args;
    args = malloc(sizeof(TacInterval *) * 256);

    ir = function->ir;

    // Need to count this IR's function_call_count
    int function_call_count = make_function_call_count(function);

    for (int i = 0; i < function_call_count; i++) {
        Tac *tac = function->ir;
        int arg_count = 0;
        Tac *call_start = 0;
        Tac *call = 0;
        while (tac) {
            if (tac->operation == IR_START_CALL && tac->src1->int_value == i) {
                call_start = tac;
                tac = tac->next;
                args[arg_count].start = tac;
            }
            else if (tac->operation == IR_END_CALL && tac->src1->int_value == i) {
                call = tac->prev;
                tac = tac->next;
            }
            else if (tac->operation == IR_ARG && tac->src1->int_value == i) {
                args[arg_count].end = tac;
                tac = tac->next;

                if (tac->operation == IR_ARG_STACK_PADDING) {
                    args[arg_count].end = tac;
                    tac = tac->next;
                }

                arg_count++;
                if (tac->operation != IR_END_CALL) args[arg_count].start = tac;
            }
            else
                tac = tac->next;
        }

        if (arg_count > 1) {
            call_start->next = args[arg_count - 1].start;
            args[arg_count - 1].start->prev = call_start;
            args[0].end->next = call;
            call->prev = args[0].end;

            for (int j = 0; j < arg_count; j++) {
                // Rearrange args backwards from this IR
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> cs.end
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> p2.start -> p2.end -> cs.end
                if (j < arg_count - 1) {
                    args[j + 1].end->next = args[j].start;
                    args[j].start->prev = args[j + 1].end;
                }
            }
        }

    }

    free(args);
}

// Insert tac instruction before ir
void insert_instruction(Tac *ir, Tac *tac, int move_label) {
    Tac *prev = ir->prev;
    tac->prev = prev;
    tac->next = ir;
    ir->prev = tac;
    prev->next = tac;

    if (move_label) {
        tac->label = ir->label;
        ir->label = 0;
    }
}

void insert_instruction_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, int move_label) {
    Tac *tac = new_instruction(operation);
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    insert_instruction(ir, tac, move_label);
}

// Append tac to ir
Tac *insert_instruction_after(Tac *ir, Tac *tac) {
    Tac *next = ir->next;
    ir->next = tac;
    tac->prev = ir;
    tac->next = next;
    if (next) next->prev = tac;

    return tac;
}

Tac *insert_instruction_after_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac = new_instruction(operation);
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;

    Tac *next = ir->next;
    ir->next = tac;
    tac->prev = ir;
    tac->next = next;
    if (next) next->prev = tac;

    return tac;
}

// Delete an instruction inside a linked list. It cannot be the first instruction.
Tac *delete_instruction(Tac *tac) {
    if (!tac->prev) panic("Attemp to delete the first instruction");
    if (tac->next && tac->next->label) panic("Deleting an instruction would obliterate the next label");

    tac->prev->next = tac->next;
    if (tac->next) tac->next->prev = tac->prev;
    tac->next->label = tac->label;
    return tac->next;
}

static void renumber_label(Tac *ir, int l1, int l2) {
    for (Tac *t = ir; t; t = t->next) {
        if (t->src1 && t->src1->label == l1) t->src1->label = l2;
        if (t->src2 && t->src2->label == l1) t->src2->label = l2;
        if (t->label == l1) t->label = l2;
    }
}

// Renumber all labels so they are consecutive. Uses label_count global.
void renumber_labels(Function *function) {
    int temp_labels = -2;

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->label) {
            // Rename target label out of the way
            renumber_label(ir, label_count + 1, temp_labels);
            temp_labels--;

            // Replace t->label with label_count + 1
            renumber_label(ir, tac->label, -1);
            tac->label = ++label_count;
            renumber_label(ir, -1, tac->label);
        }
    }
}

static void merge_labels(Tac *ir, Tac *tac, int ir_index) {
    while(1) {
        if (!tac->label || !tac->next || !tac->next->label) return;

        Tac *deleted_tac = tac->next;
        merge_instructions(tac, ir_index, 1);
        renumber_label(ir, deleted_tac->label, tac->label);
    }
}

// When the parser completes, each jmp target is a nop with its own label. This
// can lead to consecutive nops with different labels. Merge those nops and labels
// into one.
void merge_consecutive_labels(Function *function) {
    int i = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        merge_labels(ir, tac, i);
        i++;
    }
}

static void assign_local_to_register(Value *v, int vreg) {
    v->local_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

// The parser allocates a local_index for temporaries and local variables. Allocate
// vregs for them unless any of them is used with an & operator, or are long doubles, in
// which case, they must be on the stack.
// Function parameters aren't touched in this function
void allocate_value_vregs(Function *function) {
    int *on_stack = malloc(sizeof(int) * (function->local_symbol_count + 1));
    memset(on_stack, 0, sizeof(int) * (function->local_symbol_count + 1));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Variables that are used with the & operator
        if (tac->operation == IR_ADDRESS_OF && tac->src1->local_index < 0) on_stack[-tac->src1->local_index] = 1;

        // Long doubles already on the stack are left on the stack
        if (tac->dst  && tac->dst ->type && tac->dst ->type->type == TYPE_LONG_DOUBLE && tac->dst ->local_index < 0) on_stack[-tac->dst ->local_index] = 1;
        if (tac->src1 && tac->src1->type && tac->src1->type->type == TYPE_LONG_DOUBLE && tac->src1->local_index < 0) on_stack[-tac->src1->local_index] = 1;
        if (tac->src2 && tac->src2->type && tac->src2->type->type == TYPE_LONG_DOUBLE && tac->src2->local_index < 0) on_stack[-tac->src2->local_index] = 1;

        // Structs and unions already on the stack are left on the stack
        if (tac->dst  && tac->dst ->type && tac->dst ->type->type == TYPE_STRUCT_OR_UNION && tac->dst ->local_index < 0) on_stack[-tac->dst ->local_index] = 1;
        if (tac->src1 && tac->src1->type && tac->src1->type->type == TYPE_STRUCT_OR_UNION && tac->src1->local_index < 0) on_stack[-tac->src1->local_index] = 1;
        if (tac->src2 && tac->src2->type && tac->src2->type->type == TYPE_STRUCT_OR_UNION && tac->src2->local_index < 0) on_stack[-tac->src2->local_index] = 1;
    }

    for (int i = 1; i <= function->local_symbol_count; i++) {
        if (on_stack[i]) continue;
        int vreg = ++function->vreg_count;

        for (Tac *tac = function->ir; tac; tac = tac->next) {
            if (tac->dst  && tac->dst ->local_index == -i) assign_local_to_register(tac->dst , vreg);
            if (tac->src1 && tac->src1->local_index == -i) assign_local_to_register(tac->src1, vreg);
            if (tac->src2 && tac->src2->local_index == -i) assign_local_to_register(tac->src2, vreg);
        }
    }
}

// IR_JZ and IR_JNZ aren't implemented in the backend for SSE & long doubles.
// Convert:
// - IR_JZ  => IR_EQ with 0.0 & IR_JNZ
// - IR_JNZ => IR_NE with 0.0 & IR_JNZ
void convert_long_doubles_jz_and_jnz(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation == IR_JZ || ir->operation == IR_JNZ) && is_floating_point_type(ir->src1->type)) {
            ir->operation = ir->operation == IR_JZ ? IR_EQ : IR_NE;
            Tac *tac = malloc(sizeof(Tac));
            memset(tac, 0, sizeof(Tac));
            tac->operation = IR_JNZ;
            ir->dst = new_value();
            ir->dst->type = new_type(TYPE_INT);
            ir->dst->vreg = new_vreg();
            tac->src1 = ir->dst;
            tac->src2 = ir->src2;
            ir->src2 = new_floating_point_constant(ir->src1->type->type, 0.0L);
            ir = insert_instruction_after(ir, tac);
        }
    }
}

// Long double rvalues never live in registers, ensure all of them are on the stack
void move_long_doubles_to_the_stack(Function *function) {
    make_vreg_count(function, 0);
    int *local_indexes = malloc(sizeof(int) * (function->vreg_count + 1));
    memset(local_indexes, 0, sizeof(int) * (function->vreg_count + 1));

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Long doubles in vregs are moved to the stack
        if (tac->dst  && tac->dst ->vreg && tac->dst ->type && tac->dst ->type->type == TYPE_LONG_DOUBLE && !tac->dst ->is_lvalue && !local_indexes[tac->dst ->vreg])
            local_indexes[tac->dst ->vreg] = new_local_index(function);
        if (tac->src1 && tac->src1->vreg && tac->src1->type && tac->src1->type->type == TYPE_LONG_DOUBLE && !tac->src1->is_lvalue && !local_indexes[tac->src1->vreg])
            local_indexes[tac->src1->vreg] = new_local_index(function);
        if (tac->src2 && tac->src2->vreg && tac->src2->type && tac->src2->type->type == TYPE_LONG_DOUBLE && !tac->src2->is_lvalue && !local_indexes[tac->src2->vreg])
            local_indexes[tac->src2->vreg] = new_local_index(function);
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac-> dst && tac-> dst->vreg && local_indexes[tac-> dst->vreg]) { tac-> dst->local_index = local_indexes[tac-> dst->vreg]; tac-> dst->vreg = 0; }
        if (tac->src1 && tac->src1->vreg && local_indexes[tac->src1->vreg]) { tac->src1->local_index = local_indexes[tac->src1->vreg]; tac->src1->vreg = 0; }
        if (tac->src2 && tac->src2->vreg && local_indexes[tac->src2->vreg]) { tac->src2->local_index = local_indexes[tac->src2->vreg]; tac->src2->vreg = 0; }
    }
}

void make_stack_register_count(Function *function) {
    int min = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Map registers forced onto the stack due to use of &
        if (tac-> dst && tac-> dst->stack_index < 0 && tac-> dst->stack_index < min) min = tac-> dst->stack_index;
        if (tac->src1 && tac->src1->stack_index < 0 && tac->src1->stack_index < min) min = tac->src1->stack_index;
        if (tac->src2 && tac->src2->stack_index < 0 && tac->src2->stack_index < min) min = tac->src2->stack_index;
    }
    function->stack_register_count = -min;
}

// allocate a stack_index values:
// - without a vreg,
// - used in a & expression,
// - are long double
//
// Pushed variables by the caller have local_index >= 2 and are mapped through.
void allocate_value_stack_indexes(Function *function) {
    int stack_register_count = 0;

    int *stack_index_map = malloc((function->local_symbol_count + 1) * sizeof(int));
    memset(stack_index_map, -1, (function->local_symbol_count + 1) * sizeof(int));

    if (debug_ssa_mapping_local_stack_indexes) print_ir(function, 0, 0);

    // Make stack_index_map
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Map registers forced onto the stack
        if (tac->dst && tac->dst->local_index < 0 && stack_index_map[-tac->dst->local_index] == -1)
            stack_index_map[-tac->dst->local_index] = stack_register_count++;

        if (tac->src1 && tac->src1->local_index < 0 && stack_index_map[-tac->src1->local_index] == -1)
            stack_index_map[-tac->src1->local_index] = stack_register_count++;

        if (tac->src2 && tac->src2->local_index < 0 && stack_index_map[-tac->src2->local_index] == -1)
            stack_index_map[-tac->src2->local_index] = stack_register_count++;
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        // Map registers forced onto the stack
        if (tac->dst && tac->dst->local_index < 0)
            tac->dst->stack_index = -stack_index_map[-tac->dst->local_index] - 1;
        if (tac->src1 && tac->src1->local_index < 0)
            tac->src1->stack_index = -stack_index_map[-tac->src1->local_index] - 1;
        if (tac->src2 && tac->src2->local_index < 0)
            tac->src2->stack_index = -stack_index_map[-tac->src2->local_index] - 1;

        // Map function call parameters
        if (tac->dst  && tac->dst ->local_index > 0) tac->dst ->stack_index = tac->dst ->local_index;
        if (tac->src1 && tac->src1->local_index > 0) tac->src1->stack_index = tac->src1->local_index;
        if (tac->src2 && tac->src2->local_index > 0) tac->src2->stack_index = tac->src2->local_index;
    }

    // From this point onwards, local_index has no meaning and downstream code must not use it.
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst ) tac->dst ->local_index = 0;
        if (tac->src1) tac->src1->local_index = 0;
        if (tac->src2) tac->src2->local_index = 0;
    }

    function->stack_register_count = stack_register_count;

    if (debug_ssa_mapping_local_stack_indexes)
        printf("Moved %d registers to stack\n", stack_register_count);

    if (debug_ssa_mapping_local_stack_indexes) print_ir(function, 0, 0);
}

// If a function returns a value and it's called, the parser will always allocate a
// vreg for the result. This function removes the vreg if it's not used anywhere further
// on.
void remove_unused_function_call_results(Function *function) {
    int vreg_count  = 0;
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg > vreg_count) vreg_count = tac->dst ->vreg;
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg > vreg_count) vreg_count = tac->src1->vreg;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg > vreg_count) vreg_count = tac->src2->vreg;
    }

    char *used_vregs = malloc(sizeof(char) * (vreg_count + 1));
    memset(used_vregs, 0, sizeof(char) * (vreg_count + 1));

    Tac *tac = function->ir;
    while (tac->next) tac = tac->next;

    while (tac) {
        if (tac->src1 && tac->src1->vreg) used_vregs[tac->src1->vreg] = 1;
        if (tac->src2 && tac->src2->vreg) used_vregs[tac->src2->vreg] = 1;
        if (tac->operation == IR_MOVE && tac->dst->vreg && tac->dst->is_lvalue) used_vregs[tac->dst->vreg] = 1;
        if (tac->operation == IR_CALL && tac->dst && tac->dst->vreg && !used_vregs[tac->dst->vreg]) tac->dst = 0;

        tac = tac->prev;
    }

    free(used_vregs);
}

static Value *insert_address_of_instruction(Function *function, Tac **ir, Value *src) {
    Value *v = new_value();

    v->vreg = new_vreg();
    v->vreg = ++function->vreg_count;
    v->type = make_pointer_to_void();
    *ir = insert_instruction_after_from_operation(*ir, IR_ADDRESS_OF, v, src, 0);

    return v;
}

static void *insert_arg_instruction(Tac **ir, Value *function_call_value, Value *v, int int_arg_index) {
    Value *arg_value = dup_value(function_call_value);
    arg_value->function_call_arg_index = int_arg_index;

    FunctionParamLocations *fpl = malloc(sizeof(FunctionParamLocations));
    arg_value->function_call_arg_locations = fpl;
    fpl->locations = malloc(sizeof(FunctionParamLocation));
    memset(fpl->locations, -1, sizeof(FunctionParamLocation));
    fpl->count = 1;
    fpl->locations[0].int_register = int_arg_index;
    fpl->locations[0].sse_register = -1;

    *ir = insert_instruction_after_from_operation(*ir, IR_ARG, 0, arg_value, v);
}

// Add memcpy calls for struct/union -> struct/union copies
Tac *copy_memory_with_memcpy(Function *function, Tac *ir, Value *dst, Value *src1, int size) {

    int function_call_count = make_function_call_count(function);

    // Add start call instruction
    Value *call_value = make_function_call_value(function_call_count++);
    ir = insert_instruction_after_from_operation(ir, IR_START_CALL, 0, call_value, 0);

    // Load of addresses of src1, dst & make size value
    Value *src1_value;
    if (src1->is_lvalue && src1->vreg)
        src1_value = src1;
    else
        src1_value = insert_address_of_instruction(function, &ir, src1);

    Value *dst_value;
    if (dst->is_lvalue && dst->vreg)
        dst_value = dst;
    else
        dst_value = insert_address_of_instruction(function, &ir, dst);

    Value *size_value = new_integral_constant(TYPE_LONG, size);

    // Add arg instructions
    insert_arg_instruction(&ir, call_value, dst_value, 0);
    insert_arg_instruction(&ir, call_value, src1_value, 1);
    insert_arg_instruction(&ir, call_value, size_value, 2);

    // Add call instruction
    Value *function_value = new_value();
    function_value->int_value = call_value->int_value;
    function_value->function_symbol = memcpy_symbol;
    function_value->function_call_arg_push_count = 0;
    function_value->function_call_sse_register_arg_count = 0;
    call_value->function_call_arg_push_count = 0;
    call_value->function_call_arg_stack_padding = 0;
    ir = insert_instruction_after_from_operation(ir, IR_CALL, 0, function_value, 0);

    // Add end call instruction
    ir = insert_instruction_after_from_operation(ir, IR_END_CALL, 0, call_value, 0);

    return ir;
}

// Copy struct/unions using registers
static Tac *copy_memory_with_registers(Function *function, Tac *ir, Value *dst, Value *src1, int size) {
    int dst_offset = dst->offset;
    int src1_offset = src1->offset;

    // Do moves of size 8, 4, 2, 1 in order until there are no moves left
    for (int i = 3; i >= 0; i -= 1) {
        int step = 1 << i;

        while (size >= step) {
            Value *temp_value = new_value();
            temp_value->type = new_type(TYPE_CHAR + i);
            temp_value->vreg = ++function->vreg_count;

            Value *offsetted_dst = dup_value(dst);
            offsetted_dst->offset = dst_offset;
            offsetted_dst->type = temp_value->type;
            Value *offsetted_src1 = dup_value(src1);
            offsetted_src1->offset = src1_offset;
            offsetted_src1->type = temp_value->type;

            if (src1->is_lvalue && src1->vreg) {
                // Make it a pointer and add an indirect instruction
                Value *indirect_src1 = dup_value(offsetted_src1);
                indirect_src1->type = make_pointer(temp_value->type);
                indirect_src1->is_lvalue = 0;
                ir = insert_instruction_after_from_operation(ir, IR_INDIRECT, temp_value, indirect_src1, 0);
            } else
                ir = insert_instruction_after_from_operation(ir, IR_MOVE, temp_value, offsetted_src1, 0);

            ir = insert_instruction_after_from_operation(ir, IR_MOVE, offsetted_dst, temp_value, 0);

            size -= step;
            dst_offset += step;
            src1_offset += step;
        }
    }

    return ir;
}

void add_memory_copy(Function *function, Tac *ir, Value *dst, Value *src1, int size) {
    // If either src or dst is an lvalue in a register, or the struct size > 32,
    // use a memory copy. Otherwise use temporary registers to do the copy.
    if (size > 32)
        ir = copy_memory_with_memcpy(function, ir, dst, src1, size);
    else
        ir = copy_memory_with_registers(function, ir, dst, src1, size);
}

// Add memcpy calls for struct/union -> struct/union copies
void process_struct_and_union_copies(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation != IR_MOVE) continue;
        if (ir->src1->type->type != TYPE_STRUCT_OR_UNION) continue;
        if (ir->dst->type->type != ir->src1->type->type) panic("Mismatched struct/union copy type");

        int size = get_type_size(ir->dst->type);
        Value *dst = ir->dst;
        Value *src1 = ir->src1;

        ir->operation = IR_NOP;
        ir->dst = 0;
        ir->src1 = 0;

        add_memory_copy(function, ir, dst, src1, size);
    }
}
