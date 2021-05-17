#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static Type *parse_struct_base_type(int allow_incomplete_structs);
static void expression(int level);

// Push a value to the stack
static Value *push(Value *v) {
    *--vs = v;
    vtop = *vs;
    return v;
}

// Pop a value from the stack
static Value *pop() {
    Value *result;

    result = *vs++;
    vtop = (vs == vs_start) ? 0 : *vs;

    return result;
}

// load a value into a register if not already done. lvalues are converted into
// rvalues.
static Value *load(Value *src1) {
    Value *dst;

    if (src1->is_constant) return src1;
    if (src1->vreg && !src1->is_lvalue) return src1;

    dst = dup_value(src1);
    dst->vreg = new_vreg();
    dst->is_lvalue = 0;

    if (src1->vreg && src1->is_lvalue) {
        // An lvalue in a register needs a dereference
        if (src1->type->type == TYPE_VOID) panic("Cannot dereference a *void");
        if (src1->type->type == TYPE_STRUCT) panic("Cannot dereference a pointer to a struct");

        src1 = dup_value(src1);
        src1->type = make_ptr(src1->type);
        src1->is_lvalue = 0;
        add_instruction(IR_INDIRECT, dst, src1, 0);
    }
    else {
        // Load a value into a register. This could be a global or a local.
        dst->local_index = 0;
        dst->global_symbol = 0;
        add_instruction(IR_MOVE, dst, src1, 0);
    }

    return dst;
}

// Pop and load.
static Value *pl() {
    return load(pop());
}

// Create a new typed constant value and push it to the stack.
// type doesn't have to be dupped
static void push_constant(int type_type, long value) {
    push(new_constant(type_type, value));
}

// Add an operation to the IR
static Tac *add_ir_op(int operation, Type *type, int vreg, Value *src1, Value *src2) {
    Value *v;
    Tac *result;

    v = new_value();
    v->vreg = vreg;
    v->type = dup_type(type);
    result = add_instruction(operation, v, src1, src2);
    push(v);

    return result;
}

Symbol *new_symbol() {
    Symbol *result;

    result = next_symbol;
    next_symbol++;

    if (next_symbol - symbol_table >= SYMBOL_TABLE_SIZE) panic("Exceeded symbol table size");

    return result;
}

// Search for a symbol in a scope. Returns zero if not found
static Symbol *lookup_symbol(char *name, int scope) {
    Symbol *s;

    s = symbol_table;
    while (s->identifier) {
        if (s->scope == scope && !strcmp((char *) s->identifier, name)) return s;
        s++;
    }

    if (scope != 0) return lookup_symbol(name, 0);

    return 0;
}

// Returns destination type of an operation with two operands
// https://en.cppreference.com/w/c/language/conversion
Type *operation_type(Type *src1, Type *src2) {
    Type *result;

    if (src1->type == TYPE_STRUCT || src2->type == TYPE_STRUCT) panic("Operations on structs not implemented");

    if (src1->type >= TYPE_PTR) return src1;
    else if (src2->type >= TYPE_PTR) return src2;

    // They are two integer types

    if (src1->type == TYPE_LONG || src2->type == TYPE_LONG)
        result = new_type(TYPE_LONG);
    else
        result = new_type(TYPE_INT);

    if (src1->type == src2->type)
        // If either is unsigned, the result is also unsigned
        result->is_unsigned = src1->is_unsigned || src2->is_unsigned;
    else {
        // types are different
        if (src1->is_unsigned == src2->is_unsigned)
            result->is_unsigned = src1->is_unsigned;
        else
            // The sign is different
            result->is_unsigned = (src1->type > src2->type && src1->is_unsigned) || (src1->type < src2->type && src2->is_unsigned) ? 1 : 0;
    }

    return result;
}

// Returns destination type of an operation using the top two values in the stack
static Type *vs_operation_type() {
    return operation_type(vtop->type, vs[1]->type);
}

static int cur_token_is_type() {
    return (
        cur_token == TOK_SIGNED ||
        cur_token == TOK_UNSIGNED ||
        cur_token == TOK_VOID ||
        cur_token == TOK_CHAR ||
        cur_token == TOK_SHORT ||
        cur_token == TOK_INT ||
        cur_token == TOK_LONG ||
        cur_token == TOK_STRUCT ||
        cur_token == TOK_TYPEDEF_TYPE);
}

// How much will the ++, --, +=, -= operators increment a type?
static int get_type_inc_dec_size(Type *type) {
    return type->type < TYPE_PTR ? 1 : get_type_size(deref_ptr(type));
}

// Parse type up to the point where identifiers or * are lexed
static Type *parse_base_type(int allow_incomplete_structs) {
    Type *type;
    int seen_signed;
    int seen_unsigned;

    seen_signed = 0;
    seen_unsigned = 0;
    if (cur_token == TOK_SIGNED) {
        seen_signed = 1;
        next();
    }
    else if (cur_token == TOK_UNSIGNED) {
        seen_unsigned = 1;
        next();
    }

         if (cur_token == TOK_VOID)         type = new_type(TYPE_VOID);
    else if (cur_token == TOK_CHAR)         type = new_type(TYPE_CHAR);
    else if (cur_token == TOK_SHORT)        type = new_type(TYPE_SHORT);
    else if (cur_token == TOK_INT)          type = new_type(TYPE_INT);
    else if (cur_token == TOK_LONG)         type = new_type(TYPE_LONG);
    else if (cur_token == TOK_STRUCT)       { next(); return parse_struct_base_type(allow_incomplete_structs); }
    else if (cur_token == TOK_TYPEDEF_TYPE) type = dup_type(cur_type);
    else panic1d("Unable to determine type from token %d", cur_token);

    if ((seen_unsigned || seen_signed) && !is_integer_type(type)) panic("Signed/unsigned can only apply to integer types");
    if (seen_unsigned && seen_signed && !is_integer_type(type)) panic("Both ‘signed’ and ‘unsigned’ in declaration specifiers");
    type->is_unsigned = seen_unsigned;

    next();
    if (cur_token == TOK_LONG && type->type == TYPE_LONG) next(); // On 64 bit, long longs are equivalent to longs
    if (cur_token == TOK_INT && (type->type == TYPE_SHORT || type->type == TYPE_INT || type->type == TYPE_LONG)) next();

    return type;
}

// Parse type, including *
static Type *parse_type() {
    Type *type;

    type = parse_base_type(0);
    while (cur_token == TOK_MULTIPLY) {
        type = make_ptr(type);
        next();
    }

    return type;
}

// Allocate a new Struct
static Struct *new_struct() {
    Struct *s;

    s = malloc(sizeof(Struct));
    all_structs[all_structs_count] = s;
    s->type = new_type(TYPE_STRUCT + all_structs_count++);
    s->members = malloc(sizeof(StructMember *) * MAX_STRUCT_MEMBERS);
    memset(s->members, 0, sizeof(StructMember *) * MAX_STRUCT_MEMBERS);

    return s;
}

// Search for a struct. Returns 0 if not found.
static Struct *find_struct(char *identifier) {
    int i;

    for (i = 0; i < all_structs_count; i++)
        if (!strcmp(all_structs[i]->identifier, identifier))
            return all_structs[i];
    return 0;
}

// Parse struct definitions and uses. Declarations aren't implemented.
static Type *parse_struct_base_type(int allow_incomplete_structs) {
    char *identifier;
    Struct *s;
    StructMember *member;
    int member_count;
    int offset, is_packed, alignment, biggest_alignment;
    Type *base_type, *type;

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
                type = dup_type(base_type);
                while (cur_token == TOK_MULTIPLY) { type = make_ptr(type); next(); }

                alignment = is_packed ? 1 : get_type_alignment(type);
                if (alignment > biggest_alignment) biggest_alignment = alignment;
                offset = ((offset + alignment  - 1) & (~(alignment - 1)));

                member = malloc(sizeof(StructMember));
                memset(member, 0, sizeof(StructMember));
                member->identifier = cur_identifier;
                member->type = dup_type(type);
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

// Parse "typedef struct struct_id typedef_id"
static void parse_typedef() {
    Struct *s;
    Typedef *t;

    next();

    if (cur_token != TOK_STRUCT) panic("Typedefs are only implemented for structs");
    next();

    cur_type = parse_struct_base_type(0);
    s = all_structs[cur_type->type - TYPE_STRUCT];

    if (cur_token != TOK_IDENTIFIER) panic("Typedefs are only implemented for previously defined structs");

    if (all_typedefs_count == MAX_TYPEDEFS) panic("Exceeded max typedefs");

    t = malloc(sizeof(Typedef));
    memset(t, 0, sizeof(Typedef));
    t->identifier = cur_identifier;
    t->struct_type = dup_type(s->type);
    all_typedefs[all_typedefs_count++] = t;

    next();
}

static void indirect() {
    Value *dst, *src1;

    // The stack contains an rvalue which is a pointer. All that needs doing
    // is conversion of the rvalue into an lvalue on the stack and a type
    // change.
    src1 = pl();
    if (src1->is_lvalue) panic("Internal error: expected rvalue");
    if (!src1->vreg) panic("Internal error: expected a vreg");

    dst = new_value();
    dst->vreg = src1->vreg;
    dst->type = deref_ptr(src1->type);
    dst->is_lvalue = 1;
    push(dst);
}

// Search for a struct member. Panics if it doesn't exist
static StructMember *lookup_struct_member(Struct *struct_desc, char *identifier) {
    StructMember **pmember;

    pmember = struct_desc->members;

    while (*pmember) {
        if (!strcmp((*pmember)->identifier, identifier)) return *pmember;
        pmember++;
    }

    panic2s("Unknown member %s in struct %s\n", identifier, struct_desc->identifier);
}

// Allocate a new label and create a value for it, for use in a jmp
static Value *new_label_dst() {
    Value *v;

    v = new_value();
    v->label = ++label_count;

    return v;
}

// Add a no-op instruction with a label
static void add_jmp_target_instruction(Value *v) {
    Tac *tac;

    tac = add_instruction(IR_NOP, 0, 0, 0);
    tac->label = v->label;
}

static void add_conditional_jump(int operation, Value *dst) {
        // Load the result into a register
    add_instruction(operation, 0, pl(), dst);
}

// Add instructions for && and || operators
static void and_or_expr(int is_and) {
    Value *dst, *ldst1, *ldst2, *ldst3;

    next();

    ldst1 = new_label_dst(); // Store zero
    ldst2 = new_label_dst(); // Second operand test
    ldst3 = new_label_dst(); // End

    // Destination register
    dst = new_value();
    dst->vreg = new_vreg();
    dst->type = new_type(TYPE_INT);

    // Test first operand
    add_conditional_jump(is_and ? IR_JNZ : IR_JZ, ldst2);

    // Store zero & end
    add_jmp_target_instruction(ldst1);
    push_constant(TYPE_INT, is_and ? 0 : 1); // Store 0
    add_instruction(IR_MOVE, dst, pl(), 0);
    push(dst);
    add_instruction(IR_JMP, 0, ldst3, 0);

    // Test second operand
    add_jmp_target_instruction(ldst2);
    expression(TOK_BITWISE_OR);
    add_conditional_jump(is_and ? IR_JZ : IR_JNZ, ldst1); // Store zero & end
    push_constant(TYPE_INT, is_and ? 1 : 0);              // Store 1
    add_instruction(IR_MOVE, dst, pl(), 0);
    push(dst);

    // End
    add_jmp_target_instruction(ldst3);
}

static Value *add_type_change_move(Value *src, Type *type) {
    Value *dst;

    dst = dup_value(src);
    dst->vreg = new_vreg();
    dst->type = type;
    add_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

static Value *integer_promote(Value *v) {
    if (v->type->type >= TYPE_PTR) panic("Invalid operand, expected integer type");

    if (v->type->type >= TYPE_INT && v->type->type <= TYPE_LONG) return v;
    else return add_type_change_move(v, new_type(TYPE_INT));
}

static void arithmetic_operation(int operation, Type *type) {
    Type *common_type;

    // Pull two items from the stack and push the result. Code in the IR
    // is generated when the operands can't be evaluated directly.

    Value *src1, *src2;

    common_type = vs_operation_type();
    if (!type) type = common_type;

    src2 = pl();
    src1 = pl();

    if (is_integer_type(common_type) && is_integer_type(src1->type) && is_integer_type(src2->type)) {
        if (!type_eq(common_type, src1->type) && src1->type->type <= type->type) src1 = add_type_change_move(src1, common_type);
        if (!type_eq(common_type, src2->type) && src2->type->type <= type->type) src2 = add_type_change_move(src2, common_type);
    }

    add_ir_op(operation, type, new_vreg(), src1, src2);
}

static void parse_arithmetic_operation(int level, int operation, Type *type) {
    next();
    expression(level);
    arithmetic_operation(operation, type);
}

// Parse an expression using top-down precedence climbing parsing
// https://en.cppreference.com/w/c/language/operator_precedence
// https://en.wikipedia.org/wiki/Operator-precedence_parser#Precedence_climbing_method
static void expression(int level) {
    int org_token;
    Type *org_type;
    int factor;
    Type *type;
    int scope;
    int function_call, arg_count, is_pointer;
    Symbol *symbol;
    Struct *str;
    StructMember *member;
    Value *v1, *dst, *src1, *src2, *ldst1, *ldst2, *function_value, *return_value, *arg;

    // Parse any tokens that can be at the start of an expression
    if (cur_token == TOK_LOGICAL_NOT) {
        next();
        expression(TOK_INC);

        if (vtop->is_constant)
            push_constant(TYPE_INT, !pop()->value);
        else {
            push_constant(TYPE_INT, 0);
            arithmetic_operation(IR_EQ, new_type(TYPE_INT));
        }
    }

    else if (cur_token == TOK_BITWISE_NOT) {
        next();
        expression(TOK_INC);
        if (vtop->is_constant)
            push_constant(TYPE_LONG, ~pop()->value);
        else {
            type = vtop->type;
            add_ir_op(IR_BNOT, type, new_vreg(), pl(), 0);
        }
    }

    else if (cur_token == TOK_ADDRESS_OF) {
        next();
        expression(TOK_INC);
        if (!vtop->is_lvalue) panic("Cannot take an address of an rvalue");

        src1 = pop();
        add_ir_op(IR_ADDRESS_OF, make_ptr(src1->type), new_vreg(), src1, 0);
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
        add_instruction(IR_MOVE, v1, vtop, 0);
        push(v1); // Push the original lvalue back on the value stack
    }

    else if (cur_token == TOK_MULTIPLY) {
        next();
        expression(TOK_INC);
        if (vtop->type->type <= TYPE_PTR) panic1d("Cannot dereference a non-pointer %d", vtop->type->type);
        indirect();
    }

    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            push_constant(TYPE_LONG, -cur_long);
            next();
        }
        else {
            push_constant(TYPE_INT, -1);
            expression(TOK_INC);
            arithmetic_operation(IR_MUL, 0);
        }
    }

    else if (cur_token == TOK_LPAREN) {
        next();
        if (cur_token_is_type()) {
            // cast
            org_type = parse_type();
            consume(TOK_RPAREN, ")");
            expression(TOK_INC);

            v1 = pl();
            if (v1->type != org_type) {
                dst = new_value();
                dst->vreg = new_vreg();
                dst->type = dup_type(org_type);
                add_instruction(IR_MOVE, dst, v1, 0);
                push(dst);
            }
            else push(v1);
        }
        else {
            expression(TOK_COMMA);
            consume(TOK_RPAREN, ")");
        }
    }

    else if (cur_token == TOK_NUMBER) {
        if (cur_long >= -2147483648 && cur_long <= 2147483647)
            push_constant(TYPE_INT, cur_long);
        else
            push_constant(TYPE_LONG, cur_long);
        next();
    }

    else if (cur_token == TOK_STRING_LITERAL) {
        dst = new_value();
        dst->vreg = new_vreg();
        dst->type = new_type(TYPE_CHAR + TYPE_PTR);

        src1 = new_value();
        src1->type = new_type(TYPE_CHAR + TYPE_PTR);
        src1->string_literal_index = string_literal_count;
        src1->is_string_literal = 1;
        string_literals[string_literal_count++] = cur_string_literal;
        if (string_literal_count >= MAX_STRING_LITERALS) panic1d("Exceeded max string literals %d", MAX_STRING_LITERALS);

        push(dst);
        add_instruction(IR_MOVE, dst, src1, 0);
        next();
    }

    else if (cur_token == TOK_IDENTIFIER) {
        symbol = lookup_symbol(cur_identifier, cur_scope);
        if (!symbol) panic1s("Unknown symbol \"%s\"", cur_identifier);

        next();
        type = dup_type(symbol->type);
        scope = symbol->scope;
        if (symbol->is_enum)
            push_constant(TYPE_INT, symbol->value);
        else if (cur_token == TOK_LPAREN) {
            // Function call
            function_call = function_call_count++;
            next();
            src1 = new_value();
            src1->value = function_call;
            src1->is_constant = 1;
            src1->type = new_type(TYPE_LONG);
            add_instruction(IR_START_CALL, 0, src1, 0);
            arg_count = 0;
            while (1) {
                if (cur_token == TOK_RPAREN) break;
                expression(TOK_COMMA);
                arg = dup_value(src1);
                if (arg_count > MAX_FUNCTION_CALL_ARGS) panic1d("Maximum function call arg count of %d exceeded", MAX_FUNCTION_CALL_ARGS);
                arg->function_call_arg_index = arg_count;
                add_instruction(IR_ARG, 0, arg, pl());
                arg_count++;
                if (cur_token == TOK_RPAREN) break;
                consume(TOK_COMMA, ",");
                if (cur_token == TOK_RPAREN) panic("Expected expression");
            }
            consume(TOK_RPAREN, ")");

            function_value = new_value();
            function_value->value = function_call;
            function_value->function_symbol = symbol;
            function_value->function_call_arg_count = arg_count;
            src1->function_call_arg_count = arg_count;

            return_value = 0;
            if (type->type != TYPE_VOID) {
                return_value = new_value();
                return_value->vreg = new_vreg();
                return_value->type = dup_type(type);
            }
            add_instruction(IR_CALL, return_value, function_value, 0);
            add_instruction(IR_END_CALL, 0, src1, 0);
            if (return_value) push(return_value);
        }

        else if (scope == 0) {
            // Global symbol
            src1 = new_value();
            src1->type = dup_type(type);
            src1->is_lvalue = 1;
            src1->global_symbol = symbol;
            push(src1);
        }

        else {
            // Local symbol
            src1 = new_value();
            src1->type = dup_type(type);
            src1->is_lvalue = 1;

            if (symbol->local_index >= 0)
                // Step over pushed PC and BP
                src1->local_index = cur_function_symbol->function->param_count - symbol->local_index + 1;
            else
                src1->local_index = symbol->local_index;

            push(src1);
        }
    }

    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN, "(");
        if (cur_token_is_type())
            type = parse_type();
        else {
            expression(TOK_COMMA);
            type = pop()->type;
        }
        push_constant(TYPE_LONG, get_type_size(type));
        consume(TOK_RPAREN, ")");
    }

    else
        panic1d("Unexpected token %d in expression", cur_token);

    while (cur_token >= level) {
        // In order or precedence, highest first

        if (cur_token == TOK_LBRACKET) {
            next();

            if (vtop->type->type < TYPE_PTR)
                panic1d("Cannot do [] on a non-pointer for type %d", vtop->type->type);

            factor = get_type_inc_dec_size(vtop->type);

            expression(TOK_COMMA);

            if (factor > 1) {
                push_constant(TYPE_INT, factor);
                arithmetic_operation(IR_MUL, 0);
            }

            arithmetic_operation(IR_ADD, 0);
            consume(TOK_RBRACKET, "]");
            indirect();
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
            add_instruction(IR_MOVE, v1, vtop, 0);
            pop(); // Pop the lvalue of the assignment off the stack
            push(src1); // Push the original rvalue back on the value stack
        }

        else if (cur_token == TOK_DOT || cur_token == TOK_ARROW) {
            if (cur_token == TOK_DOT) {
                // Struct member lookup

                if (vtop->type->type < TYPE_STRUCT || vtop->type->type >= TYPE_PTR) panic("Cannot use . on a non-struct");
                if (!vtop->is_lvalue) panic("Expected lvalue for struct . operation.");

                // Pretend the lvalue is a pointer to a struct
                vtop->is_lvalue = 0;
                vtop->type = make_ptr(vtop->type);
            }

            if (vtop->type->type < TYPE_PTR) panic("Cannot use -> on a non-pointer");
            if (vtop->type->type < TYPE_STRUCT + TYPE_PTR) panic("Cannot use -> on a pointer to a non-struct");

            next();
            if (cur_token != TOK_IDENTIFIER) panic("Expected identifier\n");

            str = all_structs[vtop->type->type - TYPE_PTR - TYPE_STRUCT];
            member = lookup_struct_member(str, cur_identifier);
            indirect();

            vtop->type = dup_type(member->type);

            if (member->offset > 0) {
                // Make the struct lvalue into a pointer to struct rvalue for manipulation
                vtop->is_lvalue = 0;
                vtop->type = make_ptr(vtop->type);

                push_constant(TYPE_INT, member->offset);
                arithmetic_operation(IR_ADD, 0);
            }

            vtop->type = dup_type(member->type);
            vtop->is_lvalue = 1;
            consume(TOK_IDENTIFIER, "identifier");
        }

        else if (cur_token == TOK_MULTIPLY) parse_arithmetic_operation(TOK_DOT, IR_MUL, 0);
        else if (cur_token == TOK_DIVIDE)   parse_arithmetic_operation(TOK_INC, IR_DIV, 0);
        else if (cur_token == TOK_MOD)      parse_arithmetic_operation(TOK_INC, IR_MOD, 0);

        else if (cur_token == TOK_PLUS || cur_token == TOK_MINUS) {
            factor = get_type_inc_dec_size(vtop->type);
            org_token = cur_token;

            next();
            expression(TOK_MULTIPLY);
            is_pointer = vtop->type->type >= TYPE_PTR;

            if (factor > 1) {
                if (!is_pointer) {
                    push_constant(TYPE_INT, factor);
                    arithmetic_operation(IR_MUL, new_type(TYPE_INT));
                }

                arithmetic_operation(org_token == TOK_PLUS ? IR_ADD : IR_SUB, 0);

                if (org_token == TOK_MINUS && is_pointer) {
                    push_constant(TYPE_INT, factor);
                    arithmetic_operation(IR_DIV, new_type(TYPE_LONG));
                }
            }
            else
                arithmetic_operation(org_token == TOK_PLUS ? IR_ADD : IR_SUB, 0);

            if (org_token == TOK_MINUS && is_pointer) vtop->type = new_type(TYPE_LONG);
        }

        else if (cur_token == TOK_BITWISE_LEFT || cur_token == TOK_BITWISE_RIGHT)  {
            org_token = cur_token;
            next();
            src1 = integer_promote(pl());
            expression(level);
            src2 = integer_promote(pl());
            add_ir_op(org_token == TOK_BITWISE_LEFT ? IR_BSHL : IR_BSHR, src1->type, new_vreg(), src1, src2);
        }

        else if (cur_token == TOK_LT)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LT,   new_type(TYPE_INT));
        else if (cur_token == TOK_GT)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GT,   new_type(TYPE_INT));
        else if (cur_token == TOK_LE)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LE,   new_type(TYPE_INT));
        else if (cur_token == TOK_GE)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GE,   new_type(TYPE_INT));
        else if (cur_token == TOK_DBL_EQ)        parse_arithmetic_operation(TOK_LT,           IR_EQ,   new_type(TYPE_INT));
        else if (cur_token == TOK_NOT_EQ)        parse_arithmetic_operation(TOK_LT,           IR_NE,   new_type(TYPE_INT));
        else if (cur_token == TOK_ADDRESS_OF)    parse_arithmetic_operation(TOK_DBL_EQ,       IR_BAND, 0);
        else if (cur_token == TOK_XOR)           parse_arithmetic_operation(TOK_ADDRESS_OF,   IR_XOR,  0);
        else if (cur_token == TOK_BITWISE_OR)    parse_arithmetic_operation(TOK_XOR,          IR_BOR,  0);
        else if (cur_token == TOK_AND)           and_or_expr(1);
        else if (cur_token == TOK_OR)            and_or_expr(0);

        else if (cur_token == TOK_TERNARY) {
            next();

            // Destination register
            dst = new_value();
            dst->vreg = new_vreg();

            ldst1 = new_label_dst(); // False case
            ldst2 = new_label_dst(); // End
            add_conditional_jump(IR_JZ, ldst1);
            expression(TOK_TERNARY);
            src1 = vtop;
            add_instruction(IR_MOVE, dst, pl(), 0);
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of false case
            consume(TOK_COLON, ":");
            expression(TOK_TERNARY);
            src2 = vtop;
            dst->type = operation_type(src1->type, src2->type);
            add_instruction(IR_MOVE, dst, pl(), 0);
            push(dst);
            add_jmp_target_instruction(ldst2); // End
        }

        else if (cur_token == TOK_EQ) {
            next();
            if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");
            dst = pop();
            expression(TOK_EQ);
            src1 = pl();
            dst->is_lvalue = 1;
            add_instruction(IR_MOVE, dst, src1, 0);
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
                arithmetic_operation(IR_MUL, new_type(TYPE_INT));
            }

            arithmetic_operation(org_token == TOK_PLUS_EQ ? IR_ADD : IR_SUB, type);
            vtop->type = dup_type(org_type);
            add_instruction(IR_MOVE, v1, vtop, 0);
        }
        else
            return; // Bail once we hit something unknown

    }
}

// Parse a statement
static void statement() {
    Value *ldst1, *ldst2, *linit, *lcond, *lafter, *lbody, *lend, *old_loop_continue_dst, *old_loop_break_dst, *src1, *src2;
    int loop_token;
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
            add_jmp_target_instruction(ldst1); // End of true case
        }

        // End
        add_jmp_target_instruction(ldst2);
    }

    else if (cur_token == TOK_RETURN) {
        next();
        if (cur_token == TOK_SEMI) {
            add_instruction(IR_RETURN, 0, 0, 0);
        }
        else {
            expression(TOK_COMMA);
            src1 = pop();
            if (src1) src1 = load(src1);
            add_instruction(IR_RETURN, 0, src1, 0);
        }
        consume(TOK_SEMI, ";");
    }

    else {
        expression(TOK_COMMA);
        consume(TOK_SEMI, ";");
    }
}

// Parse function body
static void function_body() {
    int local_symbol_count;
    Type *base_type, *type;
    Symbol *s;

    vreg_count = 0; // Reset global vreg_count
    local_symbol_count = 0;
    function_call_count = 0;
    cur_loop = 0;
    loop_count = 0;

    consume(TOK_LCURLY, "{");

    // Parse symbols first
    while (cur_token_is_type()) {
        base_type = parse_base_type(0);

        while (cur_token != TOK_SEMI) {
            type = dup_type(base_type);
            while (cur_token == TOK_MULTIPLY) { type = make_ptr(type); next(); }

            if (type->type >= TYPE_STRUCT && type->type < TYPE_PTR) panic("Direct usage of struct variables not implemented");
            if (cur_token == TOK_EQ) panic("Declarations with assignments aren't implemented");

            expect(TOK_IDENTIFIER, "identifier");
            s = new_symbol();
            s->type = dup_type(type);
            s->identifier = cur_identifier;
            s->scope = cur_scope;
            s->local_index = -1 - local_symbol_count++;
            next();
            if (cur_token != TOK_SEMI && cur_token != TOK_COMMA) panic("Expected ; or ,");
            if (cur_token == TOK_COMMA) next();
        }
        expect(TOK_SEMI, ";");
        while (cur_token == TOK_SEMI) next();
    }

    cur_function_symbol->function->local_symbol_count = local_symbol_count;
    cur_function_symbol->function->call_count = function_call_count;

    while (cur_token != TOK_RCURLY) statement();

    consume(TOK_RCURLY, "}");
}

// String the filename component from a path
static char *base_path(char *path) {
    int end;
    char *result;

    end = strlen(path) - 1;
    result = malloc(strlen(path) + 1);
    while (end >= 0 && path[end] != '/') end--;
    if (end >= 0) result = memcpy(result, path, end + 1);
    result[end + 1] = 0;

    return result;
}

static void parse_directive() {
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

    asprintf(&filename, "%s%s", base_path(cur_filename), cur_string_literal);

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
    Type *base_type, *type;
    long value;                         // Enum value
    int param_count;                    // Number of parameters to a function
    int seen_function_declaration;      // If a function has been seen, then variable declarations afterwards are forbidden
    int is_external;                    // For functions and globals with external linkage
    int is_static;                      // Is a private function in the translation unit
    Symbol *param_symbol, *s;
    int sign;

    cur_scope = 0;
    seen_function_declaration = 0;

    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token == TOK_HASH )
            parse_directive();
        else if (cur_token == TOK_EXTERN || cur_token == TOK_STATIC || cur_token_is_type() ) {
            // Variable or function definition

            is_external = cur_token == TOK_EXTERN;
            is_static = cur_token == TOK_STATIC;
            if (is_external || is_static) next();

            base_type = parse_base_type(0);

            while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
                type = base_type;
                while (cur_token == TOK_MULTIPLY) { type = make_ptr(type); next(); }

                if (type->type >= TYPE_STRUCT && type->type < TYPE_PTR) panic("Direct usage of struct variables not implemented");

                expect(TOK_IDENTIFIER, "identifier");

                s = lookup_symbol(cur_identifier, 0);
                if (!s) {
                    // Create a new symbol if it wasn't already declared. The
                    // previous declaration is left unchanged.

                    s = new_symbol();
                    s->type = dup_type(type);
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
                    s->is_function = 1;
                    s->function = malloc(sizeof(Function));
                    memset(s->function, 0, sizeof(Function));
                    s->function->identifier = s->identifier;
                    s->function->return_type = dup_type(type);
                    s->function->param_types = malloc(sizeof(Type) * MAX_FUNCTION_CALL_ARGS);
                    s->function->ir = ir_start;
                    s->function->is_external = is_external;
                    s->function->is_static = is_static;

                    param_count = 0;
                    while (1) {
                        if (cur_token == TOK_RPAREN) break;

                        if (cur_token_is_type()) {
                            type = parse_type();
                            if (type->type >= TYPE_STRUCT && type->type < TYPE_PTR) panic("Direct usage of struct variables not implemented");

                            expect(TOK_IDENTIFIER, "identifier");
                            param_symbol = new_symbol();
                            param_symbol->type = dup_type(type);
                            param_symbol->identifier = cur_identifier;
                            param_symbol->scope = cur_scope;
                            s->function->param_types[param_count] = dup_type(type);
                            param_symbol->local_index = param_count++;
                            next();
                        }
                        else if (cur_token == TOK_ELLIPSES) {
                            s->function->is_variadic = 1;
                            next();
                        }
                        else
                            panic("Expected type or )");

                        if (cur_token == TOK_RPAREN) break;
                        consume(TOK_COMMA, ",");
                        if (cur_token == TOK_RPAREN) panic("Expected expression");
                    }

                    s->function->param_count = param_count;
                    cur_function_symbol = s;
                    consume(TOK_RPAREN, ")");

                    if (cur_token == TOK_LCURLY) {
                        seen_function_declaration = 1;
                        cur_function_symbol->function->is_defined = 1;
                        cur_function_symbol->function->param_count = param_count;
                        function_body();
                        cur_function_symbol->function->vreg_count = vreg_count;
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
                s->type = new_type(TYPE_INT);
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

        else if (cur_token == TOK_TYPEDEF) {
            parse_typedef();
            consume(TOK_SEMI, ";");
        }

        else panic("Expected global declaration or function");
    }
}

void dump_symbols() {
    long scope, value, local_index;
    Type *type;
    Symbol *s;
    char *identifier;
    int i, type_len;

    printf("Symbols:\n");
    s = symbol_table;
    while (s->identifier) {
        type = s->type;
        identifier = (char *) s->identifier;
        scope = s->scope;
        value = s->value;
        local_index = s->local_index;
        printf("%-5d %-3ld %-3ld %-20ld ", type->type, scope, local_index, value);
        type_len = print_type(stdout, type);
        for (i = 0; i < 24 - type_len; i++) printf(" ");
        printf("%s\n", identifier);
        s++;
    }
    printf("\n");
}

void init_parser() {
    symbol_table = malloc(SYMBOL_TABLE_SIZE);
    memset(symbol_table, 0, SYMBOL_TABLE_SIZE);
    next_symbol = symbol_table;

    string_literals = malloc(MAX_STRING_LITERALS);
    string_literal_count = 0;

    all_structs = malloc(sizeof(struct struct_desc *) * MAX_STRUCTS);
    all_structs_count = 0;

    all_typedefs = malloc(sizeof(struct typedef_desc *) * MAX_TYPEDEFS);
    all_typedefs_count = 0;

    vs_start = malloc(sizeof(struct value *) * VALUE_STACK_SIZE);
    vs_start += VALUE_STACK_SIZE; // The stack traditionally grows downwards
    label_count = 0;
}
