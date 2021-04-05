#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

// Allocate a new virtual register
int new_vreg() {
    vreg_count++;
    if (vreg_count >= MAX_VREG_COUNT) panic1d("Exceeded max vreg count %d", MAX_VREG_COUNT);
    return vreg_count;
}

// A useful function for debugging
void print_value_stack() {
    Value **lvs, *v;

    printf("%-4s %-4s %-4s %-11s %-11s %-5s\n", "type", "vreg", "preg", "global_sym", "local_index", "is_lv");
    lvs = vs;
    while (lvs != vs_start) {
        v = *lvs;
        printf("%-4d %-4d %-4d %-11s %-11d %-5d\n",
            v->type, v->vreg, v->preg, v->global_symbol ? v->global_symbol->identifier : 0, v->local_index, v->is_lvalue);
        lvs++;
    }
}

 void init_value(Value *v) {
    v->preg = -1;
    v->stack_index = 0;
    v->ssa_subscript = -1;
    v->live_range = -1;
}

Value *new_value() {
    Value *v;

    v = malloc(sizeof(Value));
    memset(v, 0, sizeof(Value));
    init_value(v);

    return v;
}

// Create a new typed constant value
Value *new_constant(int type, long value) {
    Value *cv;

    cv = new_value();
    cv->value = value;
    cv->type = type;
    cv->is_constant = 1;
    return cv;
}

// Duplicate a value
Value *dup_value(Value *src) {
    Value *dst;

    dst = new_value();
    dst->type                      = src->type;
    dst->vreg                      = src->vreg;
    dst->preg                      = src->preg;
    dst->is_lvalue                 = src->is_lvalue;
    dst->is_lvalue_in_register     = src->is_lvalue_in_register;
    dst->stack_index               = src->stack_index;
    dst->spilled                   = src->spilled;
    dst->local_index               = src->local_index;
    dst->is_constant               = src->is_constant;
    dst->is_string_literal         = src->is_string_literal;
    dst->string_literal_index      = src->string_literal_index;
    dst->value                     = src->value;
    dst->function_symbol           = src->function_symbol;
    dst->function_call_arg_count   = src->function_call_arg_count;
    dst->global_symbol             = src->global_symbol;
    dst->label                     = src->label;
    dst->ssa_subscript             = src->ssa_subscript;
    dst->live_range                = src->live_range;
    dst->x86_size                  = src->x86_size;
    dst->non_terminal              = src->non_terminal;

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
    Tac *tac;

    tac = malloc(sizeof(Tac));
    memset(tac, 0, sizeof(Tac));
    tac->operation = operation;

    return tac;
}

// Add instruction to the global intermediate representation ir
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    tac = new_instruction(operation);
    tac->label = 0;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->next = 0;
    tac->prev = 0;

    add_tac_to_ir(tac);

    return tac;
}

// Ensure the double linked list in an IR is correct by checking last pointers
void sanity_test_ir_linkage(Function *function) {
    Tac *tac;

    tac = function->ir;
    while (tac) {
        if (tac->next && tac->next->prev != tac) {
            printf("Linkage broken between:\n");
            print_instruction(stdout, tac);
            print_instruction(stdout, tac->next);
            panic("Bailing");
        }

        tac = tac->next;
    }
}

int fprintf_escaped_string_literal(void *f, char* sl) {
    int c;

    c = fprintf(f, "\"");
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

// Is going from type1 -> type2 a promotion, i.e. an increase of integer precision?
int is_promotion(int type1, int type2) {
    if (type1 > TYPE_LONG || type2 > TYPE_LONG) return 0;
    else return type1 < type2;
}

int print_type(void *f, int type) {
    int len;

    len = 0;

    while (type >= TYPE_PTR) {
        len += fprintf(f, "*");
        type -= TYPE_PTR;
    }

         if (type == TYPE_VOID)   len += fprintf(f, "void");
    else if (type == TYPE_CHAR)   len += fprintf(f, "char");
    else if (type == TYPE_INT)    len += fprintf(f, "int");
    else if (type == TYPE_SHORT)  len += fprintf(f, "short");
    else if (type == TYPE_LONG)   len += fprintf(f, "long");
    else if (type >= TYPE_STRUCT) len += fprintf(f, "struct %s", all_structs[type - TYPE_STRUCT]->identifier);
    else len += fprintf(f, "unknown type %d", type);

    return len;
}

int print_value(void *f, Value *v, int is_assignment_rhs) {
    int type, c;

    c = 0; // Count outputted characters

    if (!v) panic("print_value() got null");

    if (v->is_lvalue) c += printf("{l}");
    if (v->is_lvalue_in_register) c += printf("{lr}");
    if (is_assignment_rhs && !v->is_lvalue && !v->vreg) c += fprintf(f, "&");
    if (!is_assignment_rhs && v->is_lvalue && v->vreg) c += fprintf(f, "L");

    if (v->is_constant)
        c += fprintf(f, "%ld", v->value);
    else if (v->preg != -1)
        c += fprintf(f, "p%d", v->preg);
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
        c += fprintf(f, "?%ld", v->value);

    if (!v->label) {
        c += fprintf(f, ":");
        c +=  print_type(f, v->type);
    }

    return c;
}

char *operation_string(int operation) {
         if (!operation)                            return "";
    else if (operation == IR_MOVE)                  return "IR_MOVE";
    else if (operation == IR_ADDRESS_OF)            return "IR_ADDRESS_OF";
    else if (operation == IR_INDIRECT)              return "IR_INDIRECT";
    else if (operation == IR_START_CALL)            return "IR_START_CALL";
    else if (operation == IR_ARG)                   return "IR_ARG";
    else if (operation == IR_CALL)                  return "IR_CALL";
    else if (operation == IR_END_CALL)              return "IR_END_CALL";
    else if (operation == IR_RETURN)                return "IR_RETURN";
    else if (operation == IR_START_LOOP)            return "IR_START_LOOP";
    else if (operation == IR_END_LOOP)              return "IR_END_LOOP";
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
    else if (operation == X_MOVZBQ)                 return "movzbq";
    else if (operation == X_SETE)                   return "sete";
    else if (operation == X_SETNE)                  return "setne";
    else if (operation == X_SETLT)                  return "setlt";
    else if (operation == X_SETGT)                  return "setgt";
    else if (operation == X_SETLE)                  return "setle";
    else if (operation == X_SETGE)                  return "setge";
    else if (operation == X_MOVSBW)                 return "movsbw";
    else if (operation == X_MOVSBL)                 return "movsbl";
    else if (operation == X_MOVSBQ)                 return "movsbq";
    else if (operation == X_MOVSWL)                 return "movswl";
    else if (operation == X_MOVSWQ)                 return "movswq";
    else if (operation == X_MOVSLQ)                 return "movslq";
    else panic1d("Unknown x86 operation %d", operation);
}


void print_instruction(void *f, Tac *tac) {
    int first;
    char *buffer;
    Value *v;
    int o;

    o = tac->operation;

    if (tac->label)
        fprintf(f, "l%-5d", tac->label);
    else
        fprintf(f, "      ");

    if (tac->x86_template) {
        buffer = render_x86_operation(tac, 0, 0, 0);
        fprintf(f, "%s\n", buffer);
        free(buffer);
        return;
    }

    if (tac->dst && o < X_START) {
        print_value(f, tac->dst, o != IR_MOVE);
        fprintf(f, " = ");
    }

    if (o == IR_MOVE)
        print_value(f, tac->src1, 1);

    else if (o == IR_START_CALL) fprintf(f, "start call %ld", tac->src1->value);
    else if (o == IR_END_CALL) fprintf(f, "end call %ld", tac->src1->value);

    else if (o == IR_ARG || o == X_ARG) {
        fprintf(f, "arg for call %ld ", tac->src1->value);
        print_value(f, tac->src2, 1);
    }

    else if (o == IR_CALL) {
        fprintf(f, "call \"%s\" with %d params", tac->src1->function_symbol->identifier, tac->src1->function_call_arg_count);
    }

    else if (o == IR_RETURN) {
        if (tac->src1) {
            fprintf(f, "return ");
            print_value(f, tac->src1, 1);
        }
        else
            fprintf(f, "return");
    }

    else if (o == IR_START_LOOP) fprintf(f, "start loop par=%ld loop=%ld", tac->src1->value, tac->src2->value);
    else if (o == IR_END_LOOP)   fprintf(f, "end loop par=%ld loop=%ld",   tac->src1->value, tac->src2->value);

    else if (o == IR_MOVE_TO_PTR) {
        fprintf(f, "(move to ptr) ");
        print_value(f, tac->src2, 1);
    }

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
        v = tac->phi_values;
        first = 1;
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
    else if (o == X_CALL)           { fprintf(f, "call "  ); print_value(f, tac->src1, 1); }
    else if (o == X_LEA)            { fprintf(f, "lea "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }

    else if (o == X_MOV)    { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
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
    else if (o == X_MOVZBQ) { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETE)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETNE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETLT)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETGT)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETLE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_SETGE)  { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X_MOVSBW) { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X_MOVSBL) { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X_MOVSBQ) { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X_MOVSWL) { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X_MOVSWQ) { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X_MOVSLQ) { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }

    else
        panic1d("print_instruction(): Unknown operation: %d", tac->operation);

    fprintf(f, "\n");
}

void print_ir(Function *function, char *name) {
    Tac *tac;
    int i;

    if (name) fprintf(stdout, "%s:\n", name);
    i = 0;
    tac = function->ir;
    while (tac) {
        fprintf(stdout, "%-4d > ", i++);
        print_instruction(stdout, tac);
        tac = tac->next;
    }
    fprintf(stdout, "\n");
}

// Merge tac with the instruction after it. The next instruction is removed from the chain.
void merge_instructions(Tac *tac, int ir_index, int allow_labelled_next) {
    int i, label;
    Tac *next;

    if (!tac->next) panic("merge_instructions called on a tac without next\n");
    if (tac->next->label && !allow_labelled_next) panic("merge_instructions called on a tac with a label");

    next = tac->next;
    tac->next = next->next;
    if (tac->next) tac->next->prev = tac;

    // Nuke the references to the removed node to prevent confusion
    next->prev = 0;
    next->next = 0;
}

// The arguments are pushed onto the stack right to left, but the ABI requries
// the seventh arg and later to be pushed in reverse order. Easiest is to flip
// all args backwards, so they are pushed left to right.
void reverse_function_argument_order(Function *function) {
    Tac *tac, *call_start, *call;
    int i, j, arg_count, function_call_count;

    TacInterval *args;

    ir = function->ir;
    args = malloc(sizeof(TacInterval *) * 256);

    // Need to count this IR's function_call_count
    function_call_count = 0;
    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_START_CALL) function_call_count++;
        tac = tac->next;
    }

    for (i = 0; i < function_call_count; i++) {
        tac = function->ir;
        arg_count = 0;
        call_start = 0;
        call = 0;
        while (tac) {
            if (tac->operation == IR_START_CALL && tac->src1->value == i) {
                call_start = tac;
                tac = tac->next;
                args[arg_count].start = tac;
            }
            else if (tac->operation == IR_END_CALL && tac->src1->value == i) {
                call = tac->prev;
                tac = tac->next;
            }
            else if (tac->operation == IR_ARG && tac->src1->value == i) {
                args[arg_count].end = tac;
                arg_count++;
                tac = tac->next;
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

            for (j = 0; j < arg_count; j++) {
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

Tac *insert_instruction(Tac *ir, Tac *tac, int move_label) {
    int i;
    Tac *prev;

    prev = ir->prev;
    tac->prev = prev;
    tac->next = ir;
    ir->prev = tac;
    prev->next = tac;

    if (move_label) {
        tac->label = ir->label;
        ir->label = 0;
    }
}

void renumber_label(Tac *ir, int l1, int l2) {
    Tac *t;

    t = ir;
    while (t) {
        if (t->src1 && t->src1->label == l1) t->src1->label = l2;
        if (t->src2 && t->src2->label == l1) t->src2->label = l2;
        if (t->label == l1) t->label = l2;
        t = t->next;
    }
}

// Renumber all labels so they are consecutive. Uses label_count global.
void renumber_labels(Function *function) {
    Tac *tac;
    int temp_labels;

    temp_labels = -2;

    tac = function->ir;
    while (tac) {
        if (tac->label) {
            // Rename target label out of the way
            renumber_label(ir, label_count + 1, temp_labels);
            temp_labels--;

            // Replace t->label with label_count + 1
            renumber_label(ir, tac->label, -1);
            tac->label = ++label_count;
            renumber_label(ir, -1, tac->label);
        }
        tac = tac->next;
    }
}

void merge_labels(Tac *ir, Tac *tac, int ir_index) {
    Tac *deleted_tac, *t;
    int l;

    while(1) {
        if (!tac->label || !tac->next || !tac->next->label) return;

        deleted_tac = tac->next;
        merge_instructions(tac, ir_index, 1);
        renumber_label(ir, deleted_tac->label, tac->label);
    }
}

// When the parser completes, each jmp target is a nop with its own label. This
// can lead to consecutive nops with different labels. Merge those nops and labels
// into one.
void merge_consecutive_labels(Function *function) {
    int i;
    Tac *tac;

    tac = function->ir;
    i = 0;
    while (tac) {
        merge_labels(ir, tac, i);
        tac = tac->next;
        i++;
    }
}

void assign_local_to_register(Value *v, int vreg) {
    v->local_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

// The parser allocates a local_index for temporaries and local variables. Allocate vregs
// for them unless any of them is used with an & operator, in which case, they must
// be on the stack.
void allocate_value_vregs(Function *function) {
    Tac *tac;
    int i, vreg;

    int *has_address_of;

    has_address_of = malloc(sizeof(int) * (function->local_symbol_count + 1));
    memset(has_address_of, 0, sizeof(int) * (function->local_symbol_count + 1));

    tac = function->ir;
    while (tac) {
        if (tac->operation == IR_ADDRESS_OF) has_address_of[-tac->src1->local_index] = 1;
        tac = tac ->next;
    }

    for (i = 1; i <= function->local_symbol_count; i++) {
        if (has_address_of[i]) continue;
        vreg = ++function->vreg_count;

        tac = function->ir;
        while (tac) {
            if (tac->dst  && tac->dst ->local_index == -i) assign_local_to_register(tac->dst , vreg);
            if (tac->src1 && tac->src1->local_index == -i) assign_local_to_register(tac->src1, vreg);
            if (tac->src2 && tac->src2->local_index == -i) assign_local_to_register(tac->src2, vreg);
            tac = tac ->next;
        }
    }

    function->vreg_count = vreg_count;
}

// For all values without a vreg, allocate a stack_index
void allocate_value_stack_indexes(Function *function) {
    int spilled_register_count;
    Tac *tac;
    int *stack_index_map;

    spilled_register_count = 0;

    stack_index_map = malloc((function->local_symbol_count + 1) * sizeof(int));
    memset(stack_index_map, -1, (function->local_symbol_count + 1) * sizeof(int));

    if (debug_ssa_mapping_local_stack_indexes) print_ir(function, 0);

    // Make stack_index_map
    tac = function->ir;
    while (tac) {
        // Map registers forced onto the stack due to use of &
        if (tac->dst && tac->dst->local_index < 0 && stack_index_map[-tac->dst->local_index] == -1)
            stack_index_map[-tac->dst->local_index] = spilled_register_count++;

        if (tac->src1 && tac->src1->local_index < 0 && stack_index_map[-tac->src1->local_index] == -1)
            stack_index_map[-tac->src1->local_index] = spilled_register_count++;

        if (tac->src2 && tac->src2->local_index < 0 && stack_index_map[-tac->src2->local_index] == -1)
            stack_index_map[-tac->src2->local_index] = spilled_register_count++;

        tac = tac ->next;
    }

    tac = function->ir;
    while (tac) {
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

        tac = tac ->next;
    }

    // From this point onwards, local_index has no meaning and downstream code must not use it.
    tac = function->ir;
    while (tac) {
        if (tac->dst ) tac->dst ->local_index = 0;
        if (tac->src1) tac->src1->local_index = 0;
        if (tac->src2) tac->src2->local_index = 0;

        tac = tac ->next;
    }

    function->spilled_register_count = spilled_register_count;

    if (debug_ssa_mapping_local_stack_indexes)
        printf("Spilled %d registers due to & use\n", spilled_register_count);

    if (debug_ssa_mapping_local_stack_indexes) print_ir(function, 0);
}

void post_process_function_parse(Function *function) {
    reverse_function_argument_order(function);
    merge_consecutive_labels(function);
    renumber_labels(function);
    allocate_value_vregs(function);
    allocate_value_stack_indexes(function);
}
