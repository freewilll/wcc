#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

// A useful function for debugging
void print_value_stack() {
    struct value **lvs, *v;

    printf("%-4s %-4s %-4s %-11s %-11s %-5s\n", "type", "vreg", "preg", "global_sym", "stack_index", "is_lv");
    lvs = vs;
    while (lvs != vs_start) {
        v = *lvs;
        printf("%-4d %-4d %-4d %-11s %-11d %-5d\n",
            v->type, v->vreg, v->preg, v->global_symbol ? v->global_symbol->identifier : 0, v->stack_index, v->is_lvalue);
        lvs++;
    }
}

struct value *new_value() {
    struct value *v;

    v = malloc(sizeof(struct value));
    memset(v, 0, sizeof(struct value));
    v->preg = -1;
    v->spilled_stack_index = -1;

    return v;
}

// Push a value to the stack
void push(struct value *v) {
    *--vs = v;
    vtop = *vs;
}

// Pop a value from the stack
struct value *pop() {
    struct value *result;

    result = *vs++;
    vtop = *vs;

    return result;
}

// Duplicate a value
struct value *dup_value(struct value *src) {
    struct value *dst;

    dst = new_value();
    dst->type                      = src->type;
    dst->vreg                      = src->vreg;
    dst->preg                      = src->preg;
    dst->is_lvalue                 = src->is_lvalue;
    dst->spilled_stack_index       = src->spilled_stack_index;
    dst->stack_index               = src->stack_index;
    dst->is_constant               = src->is_constant;
    dst->is_string_literal         = src->is_string_literal;
    dst->is_in_cpu_flags           = src->is_in_cpu_flags;
    dst->string_literal_index      = src->string_literal_index;
    dst->value                     = src->value;
    dst->function_symbol           = src->function_symbol;
    dst->function_call_arg_count   = src->function_call_arg_count;
    dst->global_symbol             = src->global_symbol;
    dst->label                     = src->label;

    return dst;
}

struct three_address_code *new_instruction(int operation) {
    struct three_address_code *tac;

    tac = malloc(sizeof(struct three_address_code));
    memset(tac, 0, sizeof(struct three_address_code));
    tac->operation = operation;

    return tac;
}

// Add instruction to the global intermediate representation ir
struct three_address_code *add_instruction(int operation, struct value *dst, struct value *src1, struct value *src2) {
    struct three_address_code *tac;

    tac = new_instruction(operation);
    tac->label = 0;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->next = 0;
    tac->prev = 0;
    tac->in_conditional = in_conditional;

    if (!ir_start) {
        ir_start = tac;
        ir = tac;
    }
    else {
        tac->prev = ir;
        ir->next = tac;
        ir = tac;
    }

    return tac;
}

// Allocate a new virtual register
int new_vreg() {
    vreg_count++;
    if (vreg_count >= MAX_VREG_COUNT) panic1d("Exceeded max vreg count %d", MAX_VREG_COUNT);
    return vreg_count;
}

// Load a value into a register.
struct value *load(struct value *src1) {
    struct value *dst;

    dst = dup_value(src1);
    dst->vreg = new_vreg();
    dst->is_lvalue = 0;
    dst->is_in_cpu_flags = 0;

    if (src1->vreg && src1->is_lvalue) {
        // An lvalue in a register needs a dereference
        add_instruction(IR_INDIRECT, dst, src1, 0);
    }
    else if (src1->vreg)
        panic("Internal error: unexpected register");
    else if (src1->is_in_cpu_flags) {
        dst->stack_index = 0;
        dst->global_symbol = 0;
        add_instruction(IR_ASSIGN, dst, src1, 0);
    }
    else {
        // Load a value into a register. This could be a global or a local.
        dst->stack_index = 0;
        dst->global_symbol = 0;
        add_instruction(IR_LOAD_VARIABLE, dst, src1, 0);
    }

    return dst;
}

// Turn an lvalue in a register into an rvalue by dereferencing it
struct value *make_rvalue(struct value *src1) {
    struct value *dst;

    if (!src1->is_lvalue) return src1;

    if (!src1->vreg) panic("Internal error: unexpected missing register");

    // It's an lvalue in a register. Dereference it into the same register
    src1 = dup_value(src1); // Ensure no side effects on src1
    src1->is_lvalue = 0;

    dst = dup_value(src1);
    dst->vreg = new_vreg();
    dst->stack_index = 0;
    dst->global_symbol = 0;
    add_instruction(IR_INDIRECT, dst, src1, 0);

    return dst;
}

// Pop and load. Pop a value from the stack and load it into a register if not already done.
// Lvalues are converted into rvalues.
struct value *pl() {
    if (vtop->is_constant) push(load_constant(pop()));
    if (vtop->is_in_cpu_flags) push(load(pop()));

    if (vtop->vreg) {
        if (vtop->is_lvalue) return make_rvalue(pop());
        else return pop();
    }

    return load(pop());
}

struct value *load_constant(struct value *cv) {
    struct value *v;

    v = new_value();
    v->vreg = new_vreg();
    v->type = TYPE_LONG;
    add_instruction(IR_LOAD_CONSTANT, v, cv, 0);
    return v;
}

// Create a new typed constant value
struct value *new_constant(int type, long value) {
    struct value *cv;

    cv = new_value();
    cv->value = value;
    cv->type = type;
    cv->is_constant = 1;
    return cv;
}

// Create a new typed constant value and push it to the stack
void push_constant(int type, long value) {
    push(new_constant(type, value));
}

// Add an operation to the IR
struct three_address_code *add_ir_op(int operation, int type, int vreg, struct value *src1, struct value *src2) {
    struct value *v;
    struct three_address_code *result;

    v = new_value();
    v->vreg = vreg;
    v->type = type;
    result = add_instruction(operation, v, src1, src2);
    push(v);

    return result;
}

struct symbol *new_symbol() {
    struct symbol *result;

    result = next_symbol;
    next_symbol++;

    if (next_symbol - symbol_table >= SYMBOL_TABLE_SIZE) panic("Exceeded symbol table size");

    return result;
}

// Search for a symbol in a scope. Returns zero if not found
struct symbol *lookup_symbol(char *name, int scope) {
    struct symbol *s;

    s = symbol_table;
    while (s->identifier) {
        if (s->scope == scope && !strcmp((char *) s->identifier, name)) return s;
        s++;
    }

    if (scope != 0) return lookup_symbol(name, 0);

    return 0;
}

// Returns destination type of an operation with two operands
int operation_type(struct value *src1, struct value *src2) {
    if (src1->type >= TYPE_PTR) return src1->type;
    else if (src2->type >= TYPE_PTR) return src2->type;
    else if (src1->type == TYPE_LONG || src2->type == TYPE_LONG) return TYPE_LONG;
    else return TYPE_INT;
}

// Returns destination type of an operation using the top two values in the stack
int vs_operation_type() {
    return operation_type(vtop, vs[1]);
}

int cur_token_is_type() {
    return (
        cur_token == TOK_VOID ||
        cur_token == TOK_CHAR ||
        cur_token == TOK_SHORT ||
        cur_token == TOK_INT ||
        cur_token == TOK_LONG ||
        cur_token == TOK_STRUCT);
}

int get_type_alignment(int type) {
         if (type  > TYPE_PTR)    return 8;
    else if (type == TYPE_CHAR)   return 1;
    else if (type == TYPE_SHORT)  return 2;
    else if (type == TYPE_INT)    return 4;
    else if (type == TYPE_LONG)   return 8;
    else if (type >= TYPE_STRUCT) panic("Alignment of structs not implemented");

    panic1d("align of unknown type %d", type);
}

int get_type_size(int type) {
         if (type == TYPE_VOID)   return sizeof(void);
    else if (type == TYPE_CHAR)   return sizeof(char);
    else if (type == TYPE_SHORT)  return sizeof(short);
    else if (type == TYPE_INT)    return sizeof(int);
    else if (type == TYPE_LONG)   return sizeof(long);
    else if (type >  TYPE_PTR)    return sizeof(void *);
    else if (type >= TYPE_STRUCT) return all_structs[type - TYPE_STRUCT]->size;

    panic1d("sizeof unknown type %d", type);
}

// How much will the ++, --, +=, -= operators increment a type?
int get_type_inc_dec_size(int type) {
    return type < TYPE_PTR ? 1 : get_type_size(type - TYPE_PTR);
}

// Parse type up to the point where identifiers or * are lexed
int parse_base_type(int allow_incomplete_structs) {
    int type;

         if (cur_token == TOK_VOID)   type = TYPE_VOID;
    else if (cur_token == TOK_CHAR)   type = TYPE_CHAR;
    else if (cur_token == TOK_SHORT)  type = TYPE_SHORT;
    else if (cur_token == TOK_INT)    type = TYPE_INT;
    else if (cur_token == TOK_LONG)   type = TYPE_LONG;
    else if (cur_token == TOK_STRUCT) return parse_struct_base_type(allow_incomplete_structs);
    else panic("Unable to determine type from token %d");

    next();

    return type;
}

// Parse type, including *
int parse_type() {
    int type;

    type = parse_base_type(0);
    while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

    return type;
}

// Allocate a new struct_desc
struct struct_desc *new_struct() {
    struct struct_desc *s;

    s = malloc(sizeof(struct struct_desc));
    all_structs[all_structs_count] = s;
    s->type = TYPE_STRUCT + all_structs_count++;
    s->members = malloc(sizeof(struct struct_member *) * MAX_STRUCT_MEMBERS);
    memset(s->members, 0, sizeof(struct struct_member *) * MAX_STRUCT_MEMBERS);

    return s;
}

// Search for a struct. Returns 0 if not found.
struct struct_desc *find_struct(char *identifier) {
    struct struct_desc *s;
    int i;

    for (i = 0; i < all_structs_count; i++)
        if (!strcmp(all_structs[i]->identifier, identifier))
            return all_structs[i];
    return 0;
}

// Parse struct definitions and uses. Declarations aren't implemented.
int parse_struct_base_type(int allow_incomplete_structs) {
    char *identifier;
    struct struct_desc *s;
    struct struct_member *member;
    int member_count;
    int i, base_type, type, offset, is_packed, alignment, biggest_alignment;

    consume(TOK_STRUCT, "struct");

    // Check for packed attribute
    is_packed = 0;
    if (cur_token == TOK_ATTRIBUTE) {
        consume(TOK_ATTRIBUTE, "attribute");
        consume(TOK_LPAREN, "(");
        consume(TOK_LPAREN, "(");
        consume(TOK_PACKED, "packed");
        consume(TOK_RPAREN, ")");
        consume(TOK_RPAREN, ")");
        is_packed = 1;
    }

    identifier = cur_identifier;
    consume(TOK_IDENTIFIER, "identifier");
    if (cur_token == TOK_LCURLY) {
        // Struct definition

        consume(TOK_LCURLY, "{");

        s = find_struct(identifier);
        if (!s) s = new_struct();

        s->identifier = identifier;

        // Loop over members
        offset = 0;
        member_count = 0;
        biggest_alignment = 0;
        while (cur_token != TOK_RCURLY) {
            base_type = parse_base_type(1);
            while (cur_token != TOK_SEMI) {
                type = base_type;
                while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

                alignment = is_packed ? 1 : get_type_alignment(type);
                if (alignment > biggest_alignment) biggest_alignment = alignment;
                offset = ((offset + alignment  - 1) & (~(alignment - 1)));

                member = malloc(sizeof(struct struct_member));
                memset(member, 0, sizeof(struct struct_member));
                member->identifier = cur_identifier;
                member->type = type;
                member->offset = offset;
                s->members[member_count++] = member;

                offset += get_type_size(type);
                consume(TOK_IDENTIFIER, "identifier");
                if (cur_token == TOK_COMMA) next();
            }
            while (cur_token == TOK_SEMI) consume(TOK_SEMI, ";");
        }
        offset = ((offset + biggest_alignment  - 1) & (~(biggest_alignment - 1)));
        s->size = offset;
        s->is_incomplete = 0;
        consume(TOK_RCURLY, "}");
        return s->type;
    }
    else {
        // Struct use

        s = find_struct(identifier);
        if (s) return s->type; // Found a complete or incomplete struct

        if (allow_incomplete_structs) {
            // Didn't find a struct, but that's ok, create a incomplete one
            // to be populated later when it's defined.
            s = new_struct();
            s->identifier = identifier;
            s->is_incomplete = 1;
            return s->type;
        }
        else
            panic1s("Unknown struct %s", identifier);
    }
}

// Ensure there are no incomplete structs
void check_incomplete_structs() {
    int i;

    for (i = 0; i < all_structs_count; i++)
        if (all_structs[i]->is_incomplete)
            panic("There are incomplete structs");
}

void indirect() {
    struct value *dst, *src1;

    // The stack contains an rvalue which is a pointer. All that needs doing
    // is conversion of the rvalue into an lvalue on the stack and a type
    // change.
    src1 = pl();
    if (src1->is_lvalue) panic("Internal error: expected rvalue");
    if (!src1->vreg) panic("Internal error: expected a vreg");

    dst = new_value();
    dst->vreg = src1->vreg;
    dst->type = src1->type - TYPE_PTR;
    dst->is_lvalue = 1;
    push(dst);
}

// Search for a struct member. Panics if it doesn't exist
struct struct_member *lookup_struct_member(struct struct_desc *struct_desc, char *identifier) {
    struct struct_member **pmember;

    pmember = struct_desc->members;

    while (*pmember) {
        if (!strcmp((*pmember)->identifier, identifier)) return *pmember;
        pmember++;
    }

    panic2s("Unknown member %s in struct %s\n", identifier, struct_desc->identifier);
}

// Allocate a new label and create a value for it, for use in a jmp
struct value *new_label_dst() {
    struct value *v;

    v = new_value();
    v->label = ++label_count;

    return v;
}

// Add a no-op instruction with a label
void add_jmp_target_instruction(struct value *v) {
    struct three_address_code *tac;

    tac = add_instruction(IR_NOP, 0, 0, 0);
    tac->label = v->label;
}

void add_conditional_jump(int operation, struct value *dst) {
    if (vtop->is_in_cpu_flags)
        add_instruction(operation, 0, pop(), dst);
    else
        // Load the result into a register
        add_instruction(operation, 0, pl(), dst);
}

// Add instructions for && and || operators
void and_or_expr(int is_and) {
    struct value *dst, *ldst1, *ldst2, *ldst3;

    next();

    ldst1 = new_label_dst(); // Store zero
    ldst2 = new_label_dst(); // Second operand test
    ldst3 = new_label_dst(); // End

    // Destination register
    dst = new_value();
    dst->vreg = new_vreg();
    dst->type = TYPE_LONG;

    // Test first operand
    add_conditional_jump(is_and ? IR_JNZ : IR_JZ, ldst2);

    // Store zero & end
    add_jmp_target_instruction(ldst1);
    push_constant(TYPE_INT, is_and ? 0 : 1);         // Store 0
    add_instruction(IR_ASSIGN, dst, pl(), 0);
    push(dst);
    add_instruction(IR_JMP, 0, ldst3, 0);

    // Test second operand
    add_jmp_target_instruction(ldst2);
    expression(TOK_BITWISE_OR);
    add_conditional_jump(is_and ? IR_JZ : IR_JNZ, ldst1); // Store zero & end
    push_constant(TYPE_INT, is_and ? 1 : 0);          // Store 1
    add_instruction(IR_ASSIGN, dst, pl(), 0);
    push(dst);

    // End
    add_jmp_target_instruction(ldst3);
}

void arithmetic_operation(int operation, int type) {
    // Pull two items from the stack and push the result. Code in the IR
    // is generated when the operands can't be valuated directly.

    struct value *src1, *src2, *t;
    struct three_address_code *tac;
    long v1, v2;
    int vreg;

    if (!type) type = vs_operation_type();

    if (    operation == IR_ADD || operation == IR_SUB || operation == IR_MUL || operation == IR_BAND || operation == IR_BOR || operation == IR_XOR ||
            operation == IR_EQ || operation == IR_NE || operation == IR_LT || operation == IR_GT || operation == IR_LE || operation == IR_GE ||
            operation == IR_BSHL || operation == IR_BSHR) {

        if ((vs[0])->is_constant && (vs[1])->is_constant) {
            v2 = pop()->value;
            v1 = pop()->value;
                 if (operation == IR_ADD)  push(new_constant(type, v1 +  v2));
            else if (operation == IR_SUB)  push(new_constant(type, v1 -  v2));
            else if (operation == IR_MUL)  push(new_constant(type, v1 *  v2));
            else if (operation == IR_BAND) push(new_constant(type, v1 &  v2));
            else if (operation == IR_BOR)  push(new_constant(type, v1 |  v2));
            else if (operation == IR_XOR)  push(new_constant(type, v1 ^  v2));
            else if (operation == IR_EQ)   push(new_constant(type, v1 == v2));
            else if (operation == IR_NE)   push(new_constant(type, v1 != v2));
            else if (operation == IR_LT)   push(new_constant(type, v1 <  v2));
            else if (operation == IR_GT)   push(new_constant(type, v1 >  v2));
            else if (operation == IR_LE)   push(new_constant(type, v1 <= v2));
            else if (operation == IR_GE)   push(new_constant(type, v1 >= v2));
            else if (operation == IR_BSHL) push(new_constant(type, v1 << v2));
            else if (operation == IR_BSHR) push(new_constant(type, v1 >> v2));
            else panic1d("Unknown constant operation: %d", operation);
            return;
        }

        if (vtop->is_constant) src2 = pop(); else src2 = pl();
        if (vtop->is_constant) src1 = pop(); else src1 = pl();

        // If the second arg is a constant, flip it to be the first for commutative operations
        if (operation != IR_SUB && operation != IR_BSHL && operation != IR_BSHR && src2->is_constant) {
            t = src1;
            src1 = src2;
            src2 = t;

            // Flip comparison operation
                 if (operation == IR_LT) operation = IR_GT;
            else if (operation == IR_GT) operation = IR_LT;
            else if (operation == IR_LE) operation = IR_GE;
            else if (operation == IR_GE) operation = IR_LE;
        }
    }
    else {
        src2 = pl();
        src1 = pl();
    }

    // Store the result in the CPU flags for comparison operations
    // It will get loaded into a register in later instructions if needed.
    if (operation == IR_GT || operation == IR_LT || operation == IR_GE || operation == IR_LE || operation == IR_EQ || operation == IR_NE)
        vreg = 0;
    else
        vreg = new_vreg();

    tac = add_ir_op(operation, type, vreg, src1, src2);

    if (!vreg) tac->dst->is_in_cpu_flags = 1;
}

void parse_arithmetic_operation(int level, int operation, int type) {
    next();
    expression(level);
    arithmetic_operation(operation, type);
}

// Parse an expression using top-down precedence climbing parsing
// https://en.cppreference.com/w/c/language/operator_precedence
// https://en.wikipedia.org/wiki/Operator-precedence_parser#Precedence_climbing_method
void expression(int level) {
    int org_token;
    int org_type;
    int factor;
    int type;
    int scope;
    int function_call, arg_count;
    int prev_in_conditional;
    struct symbol *symbol;
    struct struct_desc *str;
    struct struct_member *member;
    struct value *v1, *v2, *dst, *src1, *src2, *ldst1, *ldst2, *function_value, *return_value;
    struct three_address_code *tac;

    // Parse any tokens that can be at the start of an expression
    if (cur_token == TOK_LOGICAL_NOT) {
        next();
        expression(TOK_INC);

        if (vtop->is_constant)
            push_constant(TYPE_LONG, !pop()->value);
        else {
            push_constant(TYPE_LONG, 0);
            arithmetic_operation(IR_EQ, TYPE_INT);
        }
    }

    else if (cur_token == TOK_BITWISE_NOT) {
        next();
        expression(TOK_INC);
        if (vtop->is_constant)
            push_constant(TYPE_LONG, ~pop()->value);
        else {
            type = vtop->type;
            tac = add_ir_op(IR_BNOT, type, new_vreg(), pl(), 0);
        }
    }

    else if (cur_token == TOK_ADDRESS_OF) {
        next();
        expression(TOK_INC);
        if (!vtop->is_lvalue) panic("Cannot take an address of an rvalue");
        vtop->is_lvalue = 0;
        vtop->type += TYPE_PTR;
    }

    else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
        // Prefix increment & decrement

        org_token = cur_token;
        next();
        expression(TOK_DOT);

        if (!vtop->is_lvalue) panic("Cannot ++ or -- an rvalue");

        v1 = pop();                 // lvalue
        src1 = load(dup_value(v1)); // rvalue
        push(src1);
        push_constant(TYPE_INT, get_type_inc_dec_size(src1->type));
        arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
        add_instruction(IR_ASSIGN, v1, vtop, 0);
        push(v1); // Push the original lvalue back on the value stack
    }

    else if (cur_token == TOK_MULTIPLY) {
        next();
        expression(TOK_INC);
        if (vtop->type <= TYPE_PTR) panic1d("Cannot dereference a non-pointer %d", vtop->type);
        indirect();
    }

    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            push_constant(TYPE_LONG, -cur_long);
            next();
        }
        else {
            push_constant(TYPE_LONG, -1);
            expression(TOK_INC);
            arithmetic_operation(IR_MUL, TYPE_LONG);
        }
    }

    else if (cur_token == TOK_LPAREN) {
        next();
        if (cur_token_is_type()) {
            // cast
            org_type = parse_type();
            consume(TOK_RPAREN, ")");
            expression(TOK_INC);
            vtop->type = org_type;
        }
        else {
            expression(TOK_COMMA);
            consume(TOK_RPAREN, ")");
        }
    }

    else if (cur_token == TOK_NUMBER) {
        push_constant(TYPE_LONG, cur_long);
        next();
    }

    else if (cur_token == TOK_STRING_LITERAL) {
        dst = new_value();
        dst->vreg = new_vreg();
        dst->type = TYPE_CHAR + TYPE_PTR;

        src1 = new_value();
        src1->type = TYPE_CHAR + TYPE_PTR;
        src1->string_literal_index = string_literal_count;
        src1->is_string_literal = 1;
        string_literals[string_literal_count++] = cur_string_literal;
        if (string_literal_count >= MAX_STRING_LITERALS) panic1d("Exceeded max string literals %d", MAX_STRING_LITERALS);

        push(dst);
        add_instruction(IR_LOAD_STRING_LITERAL, dst, src1, 0);
        next();
    }

    else if (cur_token == TOK_IDENTIFIER) {
        symbol = lookup_symbol(cur_identifier, cur_scope);
        if (!symbol) panic1s("Unknown symbol \"%s\"", cur_identifier);

        next();
        type = symbol->type;
        scope = symbol->scope;
        if (symbol->is_enum)
            push_constant(TYPE_LONG, symbol->value);
        else if (cur_token == TOK_LPAREN) {
            // Function call
            function_call = function_call_count++;
            next();
            src1 = new_value();
            src1->value = function_call;
            add_instruction(IR_START_CALL, 0, src1, 0);
            arg_count = 0;
            while (1) {
                if (cur_token == TOK_RPAREN) break;
                expression(TOK_COMMA);
                add_instruction(IR_ARG, 0, src1, pl());
                arg_count++;
                if (cur_token == TOK_RPAREN) break;
                consume(TOK_COMMA, ",");
                if (cur_token == TOK_RPAREN) panic("Expected expression");
            }
            consume(TOK_RPAREN, ")");

            function_value = new_value();
            function_value->function_symbol = symbol;
            function_value->function_call_arg_count = arg_count;

            return_value = 0;
            if (type != TYPE_VOID) {
                return_value = new_value();
                return_value->vreg = new_vreg();
                return_value->type = type;
            }
            add_instruction(IR_CALL, return_value, function_value, 0);
            add_instruction(IR_END_CALL, 0, src1, 0);
            if (return_value) push(return_value);
        }

        else if (scope == 0) {
            // Global symbol
            src1 = new_value();
            src1->type = type;
            src1->is_lvalue = 1;
            src1->global_symbol = symbol;
            push(src1);
        }

        else {
            // Local symbol
            src1 = new_value();
            src1->type = type;
            src1->is_lvalue = 1;

            if (symbol->stack_index >= 0)
                // Step over pushed PC and BP
                src1->stack_index = cur_function_symbol->function_param_count - symbol->stack_index + 1;
            else
                src1->stack_index = symbol->stack_index;

            push(src1);
        }
    }

    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN, "(");
        type = parse_type();
        push_constant(TYPE_INT, get_type_size(type));
        consume(TOK_RPAREN, ")");
    }

    else
        panic1d("Unexpected token %d in expression", cur_token);

    while (cur_token >= level) {
        // In order or precedence, highest first

        if (cur_token == TOK_LBRACKET) {
            next();

            if (vtop->type < TYPE_PTR)
                panic1d("Cannot do [] on a non-pointer for type %d", vtop->type);

            factor = get_type_inc_dec_size(vtop->type);

            src1 = pl(); // Turn it into an rvalue
            push(src1);

            org_type = vtop->type;
            expression(TOK_COMMA);

            if (factor > 1) {
                push_constant(TYPE_INT, factor);
                arithmetic_operation(IR_MUL, TYPE_INT);
            }

            type = vs_operation_type();
            arithmetic_operation(IR_ADD, type);

            consume(TOK_RBRACKET, "]");
            vtop->type = org_type - TYPE_PTR;
            vtop->is_lvalue = 1;
        }

        else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
            // Postfix increment & decrement

            org_token = cur_token;
            next();

            if (!vtop->is_lvalue) panic(" Cannot ++ or -- an rvalue");

            v1 = pop();                 // lvalue
            src1 = load(dup_value(v1)); // rvalue
            push(src1);
            push_constant(TYPE_INT, get_type_inc_dec_size(src1->type));
            arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
            add_instruction(IR_ASSIGN, v1, vtop, 0);
            pop(); // Pop the lvalue of the assignment off the stack
            push(src1); // Push the original rvalue back on the value stack
        }

        else if (cur_token == TOK_DOT || cur_token == TOK_ARROW) {
            if (cur_token == TOK_DOT) {
                // Struct member lookup

                if (vtop->type < TYPE_STRUCT || vtop->type >= TYPE_PTR) panic("Cannot use . on a non-struct");
                if (!vtop->is_lvalue) panic("Expected lvalue for struct . operation.");

                // Pretend the lvalue is a pointer to a struct
                vtop->is_lvalue = 0;
                vtop->type += TYPE_PTR;
            }

            if (vtop->type < TYPE_PTR) panic("Cannot use -> on a non-pointer");
            if (vtop->type < TYPE_STRUCT + TYPE_PTR) panic("Cannot use -> on a pointer to a non-struct");

            next();
            if (cur_token != TOK_IDENTIFIER) panic("Expected identifier\n");

            str = all_structs[vtop->type - TYPE_PTR - TYPE_STRUCT];
            member = lookup_struct_member(str, cur_identifier);
            indirect();

            if (member->offset > 0) {
                // Make the lvalue an rvalue for manipulation
                vtop->is_lvalue = 0;
                push_constant(TYPE_INT, member->offset);
                arithmetic_operation(IR_ADD, 0);
            }

            vtop->type = member->type;
            vtop->is_lvalue = 1;
            consume(TOK_IDENTIFIER, "identifier");
        }

        else if (cur_token == TOK_MULTIPLY) parse_arithmetic_operation(TOK_DOT, IR_MUL, 0);
        else if (cur_token == TOK_DIVIDE)   parse_arithmetic_operation(TOK_INC, IR_DIV, 0);
        else if (cur_token == TOK_MOD)      parse_arithmetic_operation(TOK_INC, IR_MOD, 0);

        else if (cur_token == TOK_PLUS || cur_token == TOK_MINUS) {
            factor = get_type_inc_dec_size(vtop->type);

            if (factor > 1) {
                org_token = cur_token;
                next();
                expression(TOK_MULTIPLY);

                if (factor > 1) {
                    push_constant(TYPE_INT, factor);
                    arithmetic_operation(IR_MUL, TYPE_INT);
                }

                arithmetic_operation(org_token == TOK_PLUS ? IR_ADD : IR_SUB , vs_operation_type());
            }
            else
                parse_arithmetic_operation(TOK_MULTIPLY, cur_token == TOK_PLUS ? IR_ADD : IR_SUB, 0);
        }

        else if (cur_token == TOK_BITWISE_LEFT)  parse_arithmetic_operation(TOK_PLUS,         IR_BSHL, 0);
        else if (cur_token == TOK_BITWISE_RIGHT) parse_arithmetic_operation(TOK_PLUS,         IR_BSHR, 0);
        else if (cur_token == TOK_LT)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LT,   TYPE_INT);
        else if (cur_token == TOK_GT)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GT,   TYPE_INT);
        else if (cur_token == TOK_LE)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LE,   TYPE_INT);
        else if (cur_token == TOK_GE)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GE,   TYPE_INT);
        else if (cur_token == TOK_DBL_EQ)        parse_arithmetic_operation(TOK_LT,           IR_EQ,   0);
        else if (cur_token == TOK_NOT_EQ)        parse_arithmetic_operation(TOK_LT,           IR_NE,   0);
        else if (cur_token == TOK_ADDRESS_OF)    parse_arithmetic_operation(TOK_DBL_EQ,       IR_BAND, 0);
        else if (cur_token == TOK_XOR)           parse_arithmetic_operation(TOK_ADDRESS_OF,   IR_XOR,  0);
        else if (cur_token == TOK_BITWISE_OR)    parse_arithmetic_operation(TOK_XOR,          IR_BOR,  0);
        else if (cur_token == TOK_AND)           and_or_expr(1);
        else if (cur_token == TOK_OR)            and_or_expr(0);

        else if (cur_token == TOK_TERNARY) {
            next();

            prev_in_conditional = in_conditional;
            in_conditional = 1;

            // Destination register
            dst = new_value();
            dst->vreg = new_vreg();

            ldst1 = new_label_dst(); // False case
            ldst2 = new_label_dst(); // End
            add_conditional_jump(IR_JZ, ldst1);
            expression(TOK_OR);
            src1 = vtop;
            add_instruction(IR_ASSIGN, dst, pl(), 0);
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of false case
            consume(TOK_COLON, ":");
            expression(TOK_OR);
            src2 = vtop;
            dst->type = operation_type(src1, src2);
            add_instruction(IR_ASSIGN, dst, pl(), 0);
            push(dst);
            add_jmp_target_instruction(ldst2); // End
            in_conditional = prev_in_conditional;
        }

        else if (cur_token == TOK_EQ) {
            next();
            if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");
            dst = pop();
            expression(TOK_EQ);
            src1 = pl();
            dst->is_lvalue = 1;
            add_instruction(IR_ASSIGN, dst, src1, 0);
            push(dst);
        }

        else if (cur_token == TOK_PLUS_EQ || cur_token == TOK_MINUS_EQ) {
            org_type = vtop->type;
            org_token = cur_token;

            next();

            if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");

            v1 = vtop;                  // lvalue
            push(load(dup_value(v1)));  // rvalue

            factor = get_type_inc_dec_size(org_type);
            expression(TOK_EQ);

            if (factor > 1) {
                push_constant(TYPE_INT, factor);
                arithmetic_operation(IR_MUL, TYPE_INT);
            }

            arithmetic_operation(org_token == TOK_PLUS_EQ ? IR_ADD : IR_SUB, type);
            vtop->type = org_type;
            add_instruction(IR_ASSIGN, v1, vtop, 0);
        }
        else
            return; // Bail once we hit something unknown

    }
}

// Parse a statement
void statement() {
    struct value *ldst1, *ldst2, *linit, *lcond, *lafter, *lbody, *lend, *old_loop_continue_dst, *old_loop_break_dst, *src1, *src2;
    struct three_address_code *tac;
    int loop_token;
    int prev_in_conditional;
    int prev_loop;

    vs = vs_start; // Reset value stack

    if (cur_token_is_type()) panic("Declarations must be at the top of a function");

    if (cur_token == TOK_SEMI) {
        // Empty statement
        next();
        return;
    }

    else if (cur_token == TOK_LCURLY) {
        next();
        while (cur_token != TOK_RCURLY) statement();
        consume(TOK_RCURLY, "}");
        return;
    }

    else if (cur_token == TOK_WHILE || cur_token == TOK_FOR) {
        prev_loop = cur_loop;
        cur_loop = ++loop_count;
        src1 = new_value();
        src1->value = prev_loop;
        src2 = new_value();
        src2->value = cur_loop;
        add_instruction(IR_START_LOOP, 0, src1, src2);

        loop_token = cur_token;
        next();
        consume(TOK_LPAREN, "(");

        // Preserve previous loop addresses so that we can have nested whiles/fors
        old_loop_continue_dst = cur_loop_continue_dst;
        old_loop_break_dst = cur_loop_break_dst;

        linit = lcond = lafter = 0;
        lbody  = new_label_dst();
        lend  = new_label_dst();
        cur_loop_continue_dst = 0;
        cur_loop_break_dst = lend;

        if (loop_token == TOK_FOR) {
            if (cur_token != TOK_SEMI) {
                linit  = new_label_dst();
                add_jmp_target_instruction(linit);
                expression(TOK_COMMA);
            }
            consume(TOK_SEMI, ";");

            if (cur_token != TOK_SEMI) {
                lcond  = new_label_dst();
                cur_loop_continue_dst = lcond;
                add_jmp_target_instruction(lcond);
                expression(TOK_COMMA);
                add_conditional_jump(IR_JZ, lend);
                add_instruction(IR_JMP, 0, lbody, 0);
            }
            consume(TOK_SEMI, ";");

            if (cur_token != TOK_RPAREN) {
                lafter  = new_label_dst();
                cur_loop_continue_dst = lafter;
                add_jmp_target_instruction(lafter);
                expression(TOK_COMMA);
                if (lcond) add_instruction(IR_JMP, 0, lcond, 0);
            }
        }

        else {
            lcond  = new_label_dst();
            cur_loop_continue_dst = lcond;
            add_jmp_target_instruction(lcond);
            expression(TOK_COMMA);
            add_conditional_jump(IR_JZ, lend);
        }
        consume(TOK_RPAREN, ")");

        if (!cur_loop_continue_dst) cur_loop_continue_dst = lbody;
        add_jmp_target_instruction(lbody);
        statement();

        if (lafter)
            add_instruction(IR_JMP, 0, lafter, 0);
        else if (lcond)
            add_instruction(IR_JMP, 0, lcond, 0);
        else
            add_instruction(IR_JMP, 0, lbody, 0);

        // Jump to the start of the body in a for loop like (;;i++)
        if (loop_token == TOK_FOR && !linit && !lcond && lafter)
            add_instruction(IR_JMP, 0, lbody, 0);

        add_jmp_target_instruction(lend);

        // Restore previous loop addresses
        cur_loop_continue_dst = old_loop_continue_dst;
        cur_loop_break_dst = old_loop_break_dst;

        // Restore outer loop counter
        cur_loop = prev_loop;

        add_instruction(IR_END_LOOP, 0, src1, src2);
    }

    else if (cur_token == TOK_CONTINUE) {
        next();
        add_instruction(IR_JMP, 0, cur_loop_continue_dst, 0);
        consume(TOK_SEMI, ";");
    }

    else if (cur_token == TOK_BREAK) {
        next();
        add_instruction(IR_JMP, 0, cur_loop_break_dst, 0);
        consume(TOK_SEMI, ";");
    }

    else if (cur_token == TOK_IF) {
        next();

        prev_in_conditional = in_conditional;
        in_conditional = 1;

        consume(TOK_LPAREN, "(");
        expression(TOK_COMMA);
        consume(TOK_RPAREN, ")");

        ldst1 = new_label_dst(); // False case
        ldst2 = new_label_dst(); // End
        add_conditional_jump(IR_JZ, ldst1);
        statement();

        if (cur_token == TOK_ELSE) {
            next();
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of else case
            statement();
        }
        else {
            add_jmp_target_instruction(ldst1); // Start of else case
        }

        // End
        add_jmp_target_instruction(ldst2);

        in_conditional = prev_in_conditional;
    }

    else if (cur_token == TOK_RETURN) {
        next();
        if (cur_token == TOK_SEMI) {
            add_instruction(IR_RETURN, 0, 0, 0);
        }
        else {
            expression(TOK_COMMA);
            add_instruction(IR_RETURN, 0, pl(), 0);
        }
        consume(TOK_SEMI, ";");
    }

    else {
        expression(TOK_COMMA);
        consume(TOK_SEMI, ";");

        // Discard the destination of a function call to a non-void function
        // since it's not being assigned to anything and
        if (ir[-1].operation == IR_CALL && ir[-1].dst) {
            ir[-1].dst = 0;
            --vreg_count;
            pl();
        }
    }
}

// Parse function body
void function_body() {
    int local_symbol_count;
    int base_type, type;
    struct symbol *s;

    vreg_count = 0; // Reset global vreg_count
    local_symbol_count = 0;
    function_call_count = 0;
    cur_loop = 0;
    loop_count = 0;
    in_conditional = 0;

    consume(TOK_LCURLY, "{");

    // Parse symbols first
    while (cur_token_is_type()) {
        base_type = parse_base_type(0);

        while (cur_token != TOK_SEMI) {
            type = base_type;
            while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

            if (type >= TYPE_STRUCT && type < TYPE_PTR) panic("Direct usage of struct variables not implemented");
            if (cur_token == TOK_EQ) panic("Declarations with assignments aren't implemented");

            expect(TOK_IDENTIFIER, "identifier");
            s = new_symbol();
            s->type = type;
            s->identifier = cur_identifier;
            s->scope = cur_scope;
            s->stack_index = -1 - local_symbol_count++;
            next();
            if (cur_token != TOK_SEMI && cur_token != TOK_COMMA) panic("Expected ; or ,");
            if (cur_token == TOK_COMMA) next();
        }
        expect(TOK_SEMI, ";");
        while (cur_token == TOK_SEMI) next();
    }

    cur_function_symbol->function_local_symbol_count = local_symbol_count;
    cur_function_symbol->function_call_count = function_call_count;

    while (cur_token != TOK_RCURLY) statement();

    consume(TOK_RCURLY, "}");
}

void parse_directive() {
    char *filename;

    if (parsing_header) panic("Nested headers not impemented");

    consume(TOK_HASH, "#");
    consume(TOK_INCLUDE, "include");
    if (cur_token == TOK_LT) {
        // Ignore #include <...>
        next();
        while (cur_token != TOK_GT) next();
        next();
        return;
    }

    if (cur_token != TOK_STRING_LITERAL) panic("Expected string literal in #include");

    filename = cur_string_literal;

    c_input        = input;
    c_input_size   = input_size;
    c_ip           = ip;
    c_cur_filename = cur_filename;
    c_cur_line     = cur_line;

    parsing_header = 1;
    init_lexer(filename);
}

void finish_parsing_header() {
    input        = c_input;
    input_size   = c_input_size;
    ip           = c_ip;
    cur_filename = c_cur_filename;
    cur_line     = c_cur_line;

    parsing_header = 0;
    next();
}

// Parse a translation unit
void parse() {
    int base_type, type;
    long value;                         // Enum value
    int param_count;                    // Number of parameters to a function
    int seen_function_declaration;      // If a function has been seen, then variable declarations afterwards are forbidden
    struct symbol *param_symbol, *s;
    int i, sign;

    cur_scope = 0;
    seen_function_declaration = 0;

    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token == TOK_HASH )
            parse_directive();
        else if (cur_token_is_type() ) {
            // Variable or function definition
            base_type = parse_base_type(0);

            while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
                type = base_type;
                while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

                if (type >= TYPE_STRUCT && type < TYPE_PTR) panic("Direct usage of struct variables not implemented");

                expect(TOK_IDENTIFIER, "identifier");

                s = lookup_symbol(cur_identifier, 0);
                if (!s) {
                    // Create a new symbol if it wasn't already declared. The
                    // previous declaration is left unchanged.

                    s = new_symbol();
                    s->type = type;
                    s->identifier = cur_identifier;
                    s->scope = 0;
                }

                next();

                if (cur_token == TOK_LPAREN) {
                    cur_function_symbol = s;
                    // Function declaration or definition

                    cur_scope++;
                    next();

                    // Setup the intermediate representation with a dummy no operation instruction.
                    ir_start = 0;
                    ir_start = add_instruction(IR_NOP, 0, 0, 0);
                    s->function_ir = ir_start;
                    s->is_function = 1;

                    param_count = 0;
                    while (1) {
                        if (cur_token == TOK_RPAREN) break;

                        if (cur_token_is_type()) {
                            type = parse_type();
                            if (type >= TYPE_STRUCT && type < TYPE_PTR) panic("Direct usage of struct variables not implemented");
                        }
                        else
                            panic("Expected type or )");

                        expect(TOK_IDENTIFIER, "identifier");
                        param_symbol = new_symbol();
                        param_symbol->type = type;
                        param_symbol->identifier = cur_identifier;
                        param_symbol->scope = cur_scope;
                        param_symbol->stack_index = param_count++;
                        next();

                        if (cur_token == TOK_RPAREN) break;
                        consume(TOK_COMMA, ",");
                        if (cur_token == TOK_RPAREN) panic("Expected expression");
                    }

                    s->function_param_count = param_count;
                    cur_function_symbol = s;
                    consume(TOK_RPAREN, ")");

                    if (cur_token == TOK_LCURLY) {
                        seen_function_declaration = 1;
                        cur_function_symbol->function_is_defined = 1;
                        cur_function_symbol->function_param_count = param_count;
                        function_body();
                        cur_function_symbol->function_vreg_count = vreg_count;
                    }
                    else
                        // Make it clear that this symbol will need to be backpatched if used
                        // before the definition has been processed.
                        cur_function_symbol->value = 0;

                    break;
                }
                else {
                    // Global symbol
                    if (seen_function_declaration) panic("Global variables must precede all functions");
                }

                if (cur_token == TOK_COMMA) next();
            }

            if (cur_token == TOK_SEMI) next();
        }

        else if (cur_token == TOK_ENUM) {
            consume(TOK_ENUM, "enum");
            consume(TOK_LCURLY, "{");

            value = 0;
            while (cur_token != TOK_RCURLY) {
                expect(TOK_IDENTIFIER, "identifier");
                next();
                if (cur_token == TOK_EQ) {
                    next();
                    sign = 1;
                    if (cur_token == TOK_MINUS) {
                        sign = -1;
                        next();
                    }
                    expect(TOK_NUMBER, "number");
                    value = sign * cur_long;
                    next();
                }

                s = new_symbol();
                s->is_enum = 1;
                s->type = TYPE_LONG;
                s->identifier = cur_identifier;
                s->scope = 0;
                s->value = value++;
                s++;

                if (cur_token == TOK_COMMA) next();
            }
            consume(TOK_RCURLY, "}");
            consume(TOK_SEMI, ";");
        }

        else if (cur_token == TOK_STRUCT) {
            parse_base_type(0); // It's a struct declaration
            consume(TOK_SEMI, ";");
        }

        else panic("Expected global declaration or function");
    }
}