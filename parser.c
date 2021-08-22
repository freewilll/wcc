#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static Type *integer_promote_type(Type *type);
static Type *parse_struct_base_type(int allow_incomplete_structs);
static void parse_directive();
static void parse_statement();
static void parse_expression(int level);

// Push a value to the stack
static Value *push(Value *v) {
    *--vs = v;
    vtop = *vs;
    return v;
}

static void check_stack_has_value() {
    if (vs == vs_start) panic("Internal error: Empty parser stack");
    if (vtop->type->type == TYPE_VOID) panic("Illegal attempt to use a void value");
}

// Pop a value from the stack
static Value *pop() {
    check_stack_has_value();

    Value *result = *vs++;
    vtop = *vs;

    return result;
}

// load a value into a register if not already done. lvalues are converted into
// rvalues.
static Value *load(Value *src1) {
    if (src1->is_constant) return src1;
    if (src1->vreg && !src1->is_lvalue) return src1;

    Value *dst = dup_value(src1);
    dst->vreg = new_vreg();
    dst->is_lvalue = 0;

    if (src1->vreg && src1->is_lvalue) {
        // An lvalue in a register needs a dereference
        if (src1->type->type == TYPE_VOID) panic("Cannot dereference a *void");
        if (src1->type->type == TYPE_STRUCT) panic("Cannot dereference a pointer to a struct");

        src1 = dup_value(src1);
        src1->type = make_ptr(src1->type);
        src1->is_lvalue = 0;
        if (src1->type->type == TYPE_LONG_DOUBLE)
            add_instruction(IR_MOVE, dst, src1, 0);
        else
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

static int new_local_index() {
    return -1 - cur_function_symbol->function->local_symbol_count++;
}

// Create a new typed constant value and push it to the stack.
// type doesn't have to be dupped
static void push_integral_constant(int type_type, long value) {
    push(new_integral_constant(type_type, value));
}

// Push cur_long, using the lexer determined type cur_lexer_type
static void push_cur_long() {
    Value *v = new_integral_constant(cur_lexer_type->type, cur_long);
    v->type->is_unsigned = cur_lexer_type->is_unsigned;
    push(v);
}

// Create a new typed floating point constant value and push it to the stack.
// type doesn't have to be dupped
static void push_floating_point_constant(int type_type, long double value) {
    push(new_floating_point_constant(type_type, value));
}

// Push the currently lexed long double floating point number, cur_long_double
static void push_cur_long_double() {
    Value *v = new_floating_point_constant(cur_lexer_type->type, cur_long_double);
    push(v);
}

// Add an operation to the IR
static Tac *add_ir_op(int operation, Type *type, int vreg, Value *src1, Value *src2) {
    Value *v = new_value();
    v->vreg = vreg;
    v->type = dup_type(type);
    Tac *result = add_instruction(operation, v, src1, src2);
    push(v);

    return result;
}

static Symbol *new_symbol() {
    if (cur_scope->symbol_count == cur_scope->max_symbol_count)
        panic1d("Exceeded max symbol table size of %d symbols", cur_scope->max_symbol_count);

    Symbol *symbol = malloc(sizeof(Symbol));
    memset(symbol, 0, sizeof(Symbol));
    cur_scope->symbols[cur_scope->symbol_count++] = symbol;
    symbol->scope = cur_scope;

    return symbol;
}

// Search for a symbol in a scope and recurse to parents if not found.
// Returns zero if not found in any parents
static Symbol *lookup_symbol(char *name, Scope *scope, int recurse) {
    for (int i = 0; i < scope->symbol_count; i++) {
        Symbol *symbol = scope->symbols[i];
        if (!strcmp(symbol->identifier, name)) return symbol;
    }

    if (recurse && scope->parent) return lookup_symbol(name, scope->parent, recurse);

    return 0;
}

// Returns destination type of an operation with two operands
// https://en.cppreference.com/w/c/language/conversion
Type *operation_type(Type *src1, Type *src2) {
    Type *result;

    if (src1->type == TYPE_STRUCT || src2->type == TYPE_STRUCT) panic("Operations on structs not implemented");

    if (src1->type >= TYPE_PTR) return src1;
    else if (src2->type >= TYPE_PTR) return src2;

    // If either is a long double, promote both to long double
    if (src1->type == TYPE_LONG_DOUBLE || src2->type == TYPE_LONG_DOUBLE)
        return new_type(TYPE_LONG_DOUBLE);

    // If either is a double, promote both to double
    if (src1->type == TYPE_DOUBLE || src2->type == TYPE_DOUBLE)
        return new_type(TYPE_DOUBLE);

    // If either is a float, promote both to float
    if (src1->type == TYPE_FLOAT || src2->type == TYPE_FLOAT)
        return new_type(TYPE_FLOAT);

    // Otherwise, apply integral promotions
    src1 = integer_promote_type(src1);
    src2 = integer_promote_type(src2);

    // They are two integer types
    if (src1->type == TYPE_LONG || src2->type == TYPE_LONG)
        result = new_type(TYPE_LONG);
    else
        result = new_type(TYPE_INT);

    result->is_unsigned = is_integer_operation_result_unsigned(src1, src2);

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
        cur_token == TOK_FLOAT ||
        cur_token == TOK_DOUBLE ||
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

    int seen_signed = 0;
    int seen_unsigned = 0;
    int seen_long = 0;

    if (cur_token == TOK_SIGNED) {
        seen_signed = 1;
        next();
    }
    else if (cur_token == TOK_UNSIGNED) {
        seen_unsigned = 1;
        next();
    }

         if (cur_token == TOK_VOID)         { type = new_type(TYPE_VOID);   next(); }
    else if (cur_token == TOK_CHAR)         { type = new_type(TYPE_CHAR);   next(); }
    else if (cur_token == TOK_SHORT)        { type = new_type(TYPE_SHORT);  next(); }
    else if (cur_token == TOK_INT)          { type = new_type(TYPE_INT);    next(); }
    else if (cur_token == TOK_FLOAT)        { type = new_type(TYPE_FLOAT);  next(); }
    else if (cur_token == TOK_DOUBLE)       { type = new_type(TYPE_DOUBLE); next(); }
    else if (cur_token == TOK_LONG)         { type = new_type(TYPE_LONG);   next(); seen_long = 1; }
    else if (cur_token == TOK_STRUCT)       { next(); return parse_struct_base_type(allow_incomplete_structs); }
    else if (cur_token == TOK_TYPEDEF_TYPE) { type = dup_type(cur_lexer_type); next(); }
    else if (seen_signed || seen_unsigned)  type = new_type(TYPE_INT);
    else panic1d("Unable to determine type from token %d", cur_token);

    if ((seen_unsigned || seen_signed) && !is_integer_type(type)) panic("Signed/unsigned can only apply to integer types");
    if (seen_unsigned && seen_signed && !is_integer_type(type)) panic("Both ‘signed’ and ‘unsigned’ in declaration specifiers");
    type->is_unsigned = seen_unsigned;

    if (cur_token == TOK_LONG && type->type == TYPE_LONG) next(); // On 64 bit, long longs are equivalent to longs
    if (cur_token == TOK_INT && (type->type == TYPE_SHORT || type->type == TYPE_INT || type->type == TYPE_LONG)) next();

    if (cur_token == TOK_FLOAT) {
        type->type = TYPE_FLOAT;
        next();
    }

    if (cur_token == TOK_DOUBLE) {
        if (seen_signed || seen_unsigned) panic("Cannot have signed or unsigned in a long double");
        type->type = seen_long ? TYPE_LONG_DOUBLE : TYPE_DOUBLE;
        next();
    }

    return type;
}

// Parse type, including *
static Type *parse_type() {
    Type *type = parse_base_type(0);
    while (cur_token == TOK_MULTIPLY) {
        type = make_ptr(type);
        next();
    }

    return type;
}

// Allocate a new Struct
static Struct *new_struct() {
    Struct *s = malloc(sizeof(Struct));
    all_structs[all_structs_count] = s;
    s->type = new_type(TYPE_STRUCT + all_structs_count++);
    s->members = malloc(sizeof(StructMember *) * MAX_STRUCT_MEMBERS);
    memset(s->members, 0, sizeof(StructMember *) * MAX_STRUCT_MEMBERS);

    return s;
}

// Search for a struct. Returns 0 if not found.
static Struct *find_struct(char *identifier) {
    for (int i = 0; i < all_structs_count; i++)
        if (!strcmp(all_structs[i]->identifier, identifier))
            return all_structs[i];
    return 0;
}

// Parse struct definitions and uses. Declarations aren't implemented.
static Type *parse_struct_base_type(int allow_incomplete_structs) {
    // Check for packed attribute
    int is_packed = 0;
    if (cur_token == TOK_ATTRIBUTE) {
        consume(TOK_ATTRIBUTE, "attribute");
        consume(TOK_LPAREN, "(");
        consume(TOK_LPAREN, "(");
        consume(TOK_PACKED, "packed");
        consume(TOK_RPAREN, ")");
        consume(TOK_RPAREN, ")");
        is_packed = 1;
    }

    char *identifier = cur_identifier;
    consume(TOK_IDENTIFIER, "identifier");
    if (cur_token == TOK_LCURLY) {
        // Struct definition

        consume(TOK_LCURLY, "{");

        Struct *s = find_struct(identifier);
        if (!s) s = new_struct();

        s->identifier = identifier;

        // Loop over members
        int offset = 0;
        int member_count = 0;
        int biggest_alignment = 0;
        while (cur_token != TOK_RCURLY) {
            if (cur_token == TOK_HASH) {
                parse_directive();
                continue;
            }

            Type *base_type = parse_base_type(1);
            while (cur_token != TOK_SEMI) {
                Type *type = dup_type(base_type);
                while (cur_token == TOK_MULTIPLY) { type = make_ptr(type); next(); }

                int alignment = is_packed ? 1 : get_type_alignment(type);
                if (alignment > biggest_alignment) biggest_alignment = alignment;
                offset = ((offset + alignment  - 1) & (~(alignment - 1)));

                StructMember *member = malloc(sizeof(StructMember));
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

        Struct *s = find_struct(identifier);
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
    for (int i = 0; i < all_structs_count; i++)
        if (all_structs[i]->is_incomplete)
            panic("There are incomplete structs");
}

// Parse "typedef struct struct_id typedef_id"
static void parse_typedef() {
    next();

    if (cur_token != TOK_STRUCT) panic("Typedefs are only implemented for structs");
    next();

    Type *type = parse_struct_base_type(0);
    Struct *s = all_structs[type->type - TYPE_STRUCT];

    if (cur_token != TOK_IDENTIFIER) panic("Typedefs are only implemented for previously defined structs");

    if (all_typedefs_count == MAX_TYPEDEFS) panic("Exceeded max typedefs");

    Typedef *t = malloc(sizeof(Typedef));
    memset(t, 0, sizeof(Typedef));
    t->identifier = cur_identifier;
    t->struct_type = dup_type(s->type);
    all_typedefs[all_typedefs_count++] = t;

    next();
}

static void indirect() {
    // The stack contains an rvalue which is a pointer. All that needs doing
    // is conversion of the rvalue into an lvalue on the stack and a type
    // change.
    Value *src1 = pl();
    if (src1->is_lvalue) panic("Internal error: expected rvalue");
    if (!src1->vreg) panic("Internal error: expected a vreg");

    Value *dst = new_value();
    dst->vreg = src1->vreg;
    dst->type = deref_ptr(src1->type);
    dst->is_lvalue = 1;
    push(dst);
}

// Search for a struct member. Panics if it doesn't exist
static StructMember *lookup_struct_member(Struct *struct_desc, char *identifier) {
    StructMember **pmember = struct_desc->members;

    while (*pmember) {
        if (!strcmp((*pmember)->identifier, identifier)) return *pmember;
        pmember++;
    }

    panic2s("Unknown member %s in struct %s\n", identifier, struct_desc->identifier);
}

// Allocate a new label and create a value for it, for use in a jmp
static Value *new_label_dst() {
    Value *v = new_value();
    v->label = ++label_count;

    return v;
}

// Add a no-op instruction with a label
static void add_jmp_target_instruction(Value *v) {
    Tac *tac = add_instruction(IR_NOP, 0, 0, 0);
    tac->label = v->label;
}

static void add_conditional_jump(int operation, Value *dst) {
    // Load the result into a register
    add_instruction(operation, 0, pl(), dst);
}

// Add instructions for && and || operators
static void and_or_expr(int is_and) {
    next();

    Value *ldst1 = new_label_dst(); // Store zero
    Value *ldst2 = new_label_dst(); // Second operand test
    Value *ldst3 = new_label_dst(); // End

    // Destination register
    Value *dst = new_value();
    dst->vreg = new_vreg();
    dst->type = new_type(TYPE_INT);

    // Test first operand
    add_conditional_jump(is_and ? IR_JNZ : IR_JZ, ldst2);

    // Store zero & end
    add_jmp_target_instruction(ldst1);
    push_integral_constant(TYPE_INT, is_and ? 0 : 1); // Store 0
    add_instruction(IR_MOVE, dst, pl(), 0);
    push(dst);
    add_instruction(IR_JMP, 0, ldst3, 0);

    // Test second operand
    add_jmp_target_instruction(ldst2);
    parse_expression(TOK_BITWISE_OR);
    add_conditional_jump(is_and ? IR_JZ : IR_JNZ, ldst1); // Store zero & end
    push_integral_constant(TYPE_INT, is_and ? 1 : 0);     // Store 1
    add_instruction(IR_MOVE, dst, pl(), 0);
    push(dst);

    // End
    add_jmp_target_instruction(ldst3);
}

static Value *integer_type_change(Value *src, Type *type) {
    Value *dst;

    if (src->is_constant) {
        // The target type is always a long and the constant is already represented
        // as a long in cur_long, so nothing needs doing other than changing the
        // type.
        dst = dup_value(src);
        dst->type = dup_type(type);
        return dst;
    }

    // Add a move for the type change
    dst = dup_value(src);
    dst->vreg = new_vreg();
    dst->type = type;
    add_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

static Type *integer_promote_type(Type *type) {
    if (type->type >= TYPE_PTR) panic("Invalid operand, expected integer type");

    if (type->type >= TYPE_INT && type->type <= TYPE_LONG)
        return type;
    else
        return new_type(TYPE_INT); // An int can hold all the values
}

static Value *integer_promote(Value *v) {
    if (v->type->type >= TYPE_PTR) panic("Invalid operand, expected integer type");

    Type *type = integer_promote_type(v->type);
    if (type_eq(v->type, type))
        return v;
    else
        return integer_type_change(v, new_type(TYPE_INT));
}

Value *convert_int_constant_to_floating_point(Value *v, Type *dst_type) {
    Value *result = new_value();
    result->type = dup_type(dst_type);
    result->is_constant = 1;

    if (v->type->is_unsigned)
        result->fp_value = (unsigned long) v->int_value;
    else
        result->fp_value = v->int_value;

    return result;
}

// Convert a value to a long double. This can be a constant, integer, SSE, or long double value
static Value *long_double_type_change(Value *src) {
    Value *dst;

    if (src->type->type >= TYPE_PTR) panic("Unable to convert a pointer to a long double");
    if (src->type->type == TYPE_LONG_DOUBLE) return src;

    if (src->is_constant) {
        if (src->type->type == TYPE_FLOAT || src->type->type == TYPE_DOUBLE) {
            // Convert a float/double constant to long double
            dst = dup_value(src);
            dst->type = new_type(TYPE_LONG_DOUBLE);
            dst->fp_value = src->fp_value;
            return dst;
        }

        // Implicit else, src is an integer, convert to floating point value
        return convert_int_constant_to_floating_point(src, new_type(TYPE_LONG_DOUBLE));
    }

    // Add a move for the type change
    dst = dup_value(src);
    dst->vreg = 0;
    dst->local_index = new_local_index();
    dst->type = new_type(TYPE_LONG_DOUBLE);
    add_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

// Convert a value to a double. This can be a constant, integer or SSE value
static Value *double_type_change(Value *src) {
    Value *dst;

    if (src->type->type >= TYPE_PTR) panic("Unable to convert a pointer to a double");
    if (src->type->type == TYPE_LONG_DOUBLE) panic("Unexpectedly got a long double -> double conversion");
    if (src->type->type == TYPE_DOUBLE) return src;

    if (src->is_constant) {
        if (src->type->type == TYPE_FLOAT) {
            // Convert a float constant to long double
            dst = dup_value(src);
            dst->type = new_type(TYPE_DOUBLE);
            return dst;
        }

        // Implicit else, src is an integer, convert to floating point value
        return convert_int_constant_to_floating_point(src, new_type(TYPE_DOUBLE));
    }

    // Add a move for the type change
    dst = dup_value(src);
    dst->vreg = 0;
    dst->local_index = new_local_index();
    dst->type = new_type(TYPE_DOUBLE);
    add_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

// Convert a value to a float. This can be a constant, integer or float
static Value *float_type_change(Value *src) {
    Value *dst;

    if (src->type->type >= TYPE_PTR) panic("Unable to convert a pointer to a double");
    if (src->type->type == TYPE_LONG_DOUBLE) panic("Unexpectedly got a long double -> float conversion");
    if (src->type->type == TYPE_DOUBLE) panic("Unexpectedly got a double -> float conversion");
    if (src->type->type == TYPE_FLOAT) return src;

    if (src->is_constant)
        // src is an integer, convert to floating point value
        return convert_int_constant_to_floating_point(src, new_type(TYPE_FLOAT));

    // Add a move for the type change
    dst = dup_value(src);
    dst->vreg = 0;
    dst->local_index = new_local_index();
    dst->type = new_type(TYPE_FLOAT);
    add_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

static void arithmetic_operation(int operation, Type *type) {
    // Pull two items from the stack and push the result. Code in the IR
    // is generated when the operands can't be evaluated directly.
    Type *common_type = vs_operation_type();
    if (!type) type = common_type;

    Value *src2 = pl();
    Value *src1 = pl();

    if (is_integer_type(common_type) && is_integer_type(src1->type) && is_integer_type(src2->type)) {
        if (!type_eq(common_type, src1->type) && (src1->type->type <= type->type || src1->type->is_unsigned != common_type->is_unsigned))
            src1 = integer_type_change(src1, common_type);
        if (!type_eq(common_type, src2->type) && (src2->type->type <= type->type || src2->type->is_unsigned != common_type->is_unsigned))
            src2 = integer_type_change(src2, common_type);
    }

    else if (common_type->type == TYPE_LONG_DOUBLE) {
        src1 = long_double_type_change(src1);
        src2 = long_double_type_change(src2);
    }
    else if (common_type->type == TYPE_DOUBLE) {
        src1 = double_type_change(src1);
        src2 = double_type_change(src2);
    }
    else if (common_type->type == TYPE_FLOAT) {
        src1 = float_type_change(src1);
        src2 = float_type_change(src2);
    }

    add_ir_op(operation, type, new_vreg(), src1, src2);
}

static void check_arithmetic_operation_type(int operation, Value *src1, Value *src2) {
    int src1_is_arithmetic = is_arithmetic_type(src1->type);
    int src2_is_arithmetic = is_arithmetic_type(src2->type);
    int src1_is_integer = is_integer_type(src1->type);
    int src2_is_integer = is_integer_type(src2->type);
    int src1_is_pointer = is_pointer_type(src1->type);
    int src2_is_pointer = is_pointer_type(src2->type);

    if (operation == IR_MUL && (!src1_is_arithmetic || !src2_is_arithmetic)) panic("Invalid operands to binary *");
    if (operation == IR_DIV && (!src1_is_arithmetic || !src2_is_arithmetic)) panic("Invalid operands to binary /");
    if (operation == IR_MOD && (!src1_is_integer || !src2_is_integer)) panic("Invalid operands to binary %");

    if (operation == IR_LT || operation == IR_GT || operation == IR_LE || operation == IR_GE) {
        Type *src1_type_deref = 0;
        Type *src2_type_deref = 0;

        if (src1_is_pointer) src1_type_deref = deref_ptr(src1->type);
        if (src2_is_pointer) src2_type_deref = deref_ptr(src2->type);

        // One of the following shall hold:
        // * both operands have arithmetic type;
        // * both operands are pointers to qualified or unqualified versions of compatible object types; or
        // * both operands are pointers to qualified or unqualified versions of compatible incomplete types.
        //
        // Deviation from the spec: comparisons between arithmetic and pointers types are allowed
        if (
            (!((src1_is_arithmetic || src1_is_pointer) && (src2_is_arithmetic || src2_is_pointer))) &&
            (!(src1_is_pointer && src2_is_pointer && is_object_type(src1_type_deref) && is_object_type(src2_type_deref) && types_are_compabible(src1_type_deref, src2_type_deref))) &&
            (!(src1_is_pointer && src2_is_pointer && is_incomplete_type(src1_type_deref) && is_incomplete_type(src2_type_deref) && types_are_compabible(src1_type_deref, src2_type_deref)))
        )
        panic("Invalid operands to relational operator");
    }
}

static void parse_arithmetic_operation(int level, int operation, Type *type) {
    Value *src1 = vtop;
    next();
    parse_expression(level);
    Value *src2 = vtop;
    check_arithmetic_operation_type(operation, src1, src2);

    arithmetic_operation(operation, type);
}

static void push_local_symbol(Symbol *symbol) {
    Type *type = dup_type(symbol->type);

    // Local symbol
    Value *v = new_value();
    v->type = dup_type(type);
    v->is_lvalue = 1;

    if (symbol->local_index >= 0)
        // For historical and irrational sentimental reasons, pushed parameters start at
        // stack_index 2.
        v->local_index = symbol->local_index + 2;
    else
        // Local variable
        v->local_index = symbol->local_index;

    push(v);
}

// Add type change move if necessary and return the dst value
Value *add_convert_type_if_needed(Value *src, Type *dst_type) {
    if (!type_eq(dst_type, src->type)) {
        if (src->is_constant) {
            int src_is_int = is_integer_type(src->type);
            int dst_is_int = is_integer_type(dst_type);
            int src_is_sse = is_sse_floating_point_type(src->type);
            int dst_is_sse = is_sse_floating_point_type(dst_type);
            int src_is_ld = src->type->type == TYPE_LONG_DOUBLE;
            int dst_is_ld = dst_type->type == TYPE_LONG_DOUBLE;

            if ((src_is_sse && dst_is_ld) || (dst_is_sse && src_is_ld)) {
                // Type change for float/double <-> long double
                Value *src2 = dup_value(src);
                src2->type = dup_type(dst_type);
                return src2;
            }
            else if ((src_is_sse || src_is_ld) && dst_is_int) {
                // Convert floating point -> int
                Value *src2 = new_value();
                src2->type = new_type(dst_type->type <= TYPE_INT ? TYPE_INT : TYPE_LONG);
                src2->is_constant = 1;
                src2->int_value = src->fp_value;
                return src2;
            }
            else if (src_is_int && (dst_is_sse || dst_is_ld))
                return convert_int_constant_to_floating_point(src, dst_type);
            else if (src_is_sse && dst_is_sse && src->type->type != dst_type->type) {
                // Convert float -> double or double -> float
                Value *src2 = dup_value(src);
                src2->type = dup_type(dst_type);
                return src2;
            }

            // No change
            return src;
        }

        // Convert non constant
        Value *src2 = new_value();
        src2->vreg = new_vreg();
        src2->type = dup_type(dst_type);
        add_instruction(IR_MOVE, src2, src, 0);
        return src2;
    }

    return src;
}

static void parse_assignment() {
    next();
    if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");
    Value *dst = pop();
    parse_expression(TOK_EQ);
    Value *src1 = pl();
    dst->is_lvalue = 1;

    src1 = add_convert_type_if_needed(src1, dst->type);
    add_instruction(IR_MOVE, dst, src1, 0);
    push(dst);
}

static void parse_addition(int level, int require_pointer_arithmetic_operands) {
    int src1_is_pointer = is_pointer_to_object_type(vtop->type);
    int src1_is_integer = is_integer_type(vtop->type);
    int src1_is_arithmetic = is_arithmetic_type(vtop->type);
    parse_expression(level);
    int src2_is_pointer = is_pointer_to_object_type(vtop->type);
    int src2_is_integer = is_integer_type(vtop->type);
    int src2_is_arithmetic = is_arithmetic_type(vtop->type);

    // Either both operands shall have arithmetic type, or one operand shall be a
    // pointer to an object type and the other shall have integral type.
    if (
        (!src1_is_arithmetic || !src2_is_arithmetic) &&
        (!src1_is_pointer || !src2_is_integer) &&
        (!src2_is_pointer || !src1_is_integer)
    )
    panic("Invalid operands to binary plus");

    // Swap the operands so that the pointer comes first, for convenience
    if (!src1_is_pointer && src2_is_pointer) {
        Value *src1 = vs[0];
        vs[0] = vs[1];
        vs[1] = src1;

        src2_is_pointer = 0;
    }

    int factor = get_type_inc_dec_size(vs[1]->type);

    if (factor > 1) {
        if (!src2_is_pointer) {
            push_integral_constant(TYPE_INT, factor);
            arithmetic_operation(IR_MUL, 0);
        }
    }

    arithmetic_operation(IR_ADD, 0);
}

// Parse a declaration
static void parse_declaration() {
    Symbol *symbol;

    Type *base_type = parse_base_type(0);

    while (cur_token != TOK_SEMI && cur_token != TOK_EQ) {
        Type *type = dup_type(base_type);
        while (cur_token == TOK_MULTIPLY) { type = make_ptr(type); next(); }

        if (type->type >= TYPE_STRUCT && type->type < TYPE_PTR) panic("Direct usage of struct variables not implemented");
        if (cur_token == TOK_EQ) panic("Declarations with assignments aren't implemented");

        expect(TOK_IDENTIFIER, "identifier");

        if (lookup_symbol(cur_identifier, cur_scope, 0)) panic1s("Identifier redeclared: %s", cur_identifier);

        symbol = new_symbol();
        symbol->type = dup_type(type);
        symbol->identifier = cur_identifier;
        symbol->local_index = new_local_index();
        next();
        if (cur_token != TOK_SEMI && cur_token != TOK_EQ && cur_token != TOK_COMMA) panic("Expected =, ; or ,");
        if (cur_token == TOK_COMMA) next();
    }

    if (cur_token == TOK_EQ) {
        push_local_symbol(symbol);
        parse_assignment();
    }
}

// Push either an int or long double constant with value the size of v
static void push_value_size_constant(Value *v) {
    int size = get_type_inc_dec_size(v->type);
    if (v->type->type == TYPE_LONG_DOUBLE)
        push_floating_point_constant(TYPE_LONG_DOUBLE, size);
    else
        push_integral_constant(TYPE_INT, size);
}

// Parse an expression using top-down precedence climbing parsing
// https://en.cppreference.com/w/c/language/operator_precedence
// https://en.wikipedia.org/wiki/Operator-precedence_parser#Precedence_climbing_method
static void parse_expression(int level) {
    // Parse any tokens that can be at the start of an expression
    if (cur_token_is_type())
        parse_declaration();

    else if (cur_token == TOK_LOGICAL_NOT) {
        next();
        parse_expression(TOK_INC);
        if (!is_scalar_type(vtop->type)) panic("Cannot use ! on a non scalar");

        if (vtop->is_constant)
            push_integral_constant(TYPE_INT, !pop()->int_value);
        else {
            push_integral_constant(TYPE_INT, 0);
            arithmetic_operation(IR_EQ, new_type(TYPE_INT));
        }
    }

    else if (cur_token == TOK_BITWISE_NOT) {
        next();
        parse_expression(TOK_INC);
        if (!is_integer_type(vtop->type)) panic("Cannot use ~ on a non integer");
        push(integer_promote(pl()));
        Type *type = vtop->type;
        add_ir_op(IR_BNOT, type, new_vreg(), pl(), 0);
    }

    else if (cur_token == TOK_ADDRESS_OF) {
        next();
        parse_expression(TOK_INC);
        if (!vtop->is_lvalue) panic("Cannot take an address of an rvalue");

        Value *src1 = pop();
        add_ir_op(IR_ADDRESS_OF, make_ptr(src1->type), new_vreg(), src1, 0);
    }

    else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
        // Prefix increment & decrement

        int org_token = cur_token;
        next();
        parse_expression(TOK_DOT);

        if (!vtop->is_lvalue) panic("Cannot ++ or -- an rvalue");

        Value *v1 = pop();                 // lvalue
        Value *src1 = load(dup_value(v1)); // rvalue
        push(src1);
        push_value_size_constant(src1);
        arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
        add_instruction(IR_MOVE, v1, vtop, 0);
        push(v1); // Push the original lvalue back on the value stack
    }

    else if (cur_token == TOK_MULTIPLY) {
        next();
        parse_expression(TOK_INC);
        if (vtop->type->type <= TYPE_PTR) panic1d("Cannot dereference a non-pointer %d", vtop->type->type);
        indirect();
    }

    else if (cur_token == TOK_MINUS) {
        // Unary minus
        next();

        parse_expression(TOK_INC);
        if (!is_arithmetic_type(vtop->type)) panic("Can only use unary - on an arithmetic type");

        if (vtop->type->type == TYPE_LONG_DOUBLE)
            push_floating_point_constant(TYPE_LONG_DOUBLE, -1.0L);
        else if (vtop->type->type == TYPE_DOUBLE)
            push_floating_point_constant(TYPE_DOUBLE, -1.0L);
        else if (vtop->type->type == TYPE_FLOAT)
            push_floating_point_constant(TYPE_FLOAT, -1.0L);
        else
            push_integral_constant(TYPE_INT, -1);

        arithmetic_operation(IR_MUL, 0);
    }

    else if (cur_token == TOK_PLUS) {
        // Unary plus

        next();
        parse_expression(TOK_INC);
        if (!is_arithmetic_type(vtop->type)) panic("Can only use unary + on an arithmetic type");
        if (is_integer_type(vtop->type)) push(integer_promote(pl()));
    }

    else if (cur_token == TOK_LPAREN) {
        next();
        if (cur_token_is_type()) {
            // cast
            Type *org_type = parse_type();
            consume(TOK_RPAREN, ")");
            parse_expression(TOK_INC);

            Value *v1 = pl();
            if (v1->type != org_type) {
                Value *dst = new_value();
                dst->vreg = new_vreg();
                dst->type = dup_type(org_type);
                add_instruction(IR_MOVE, dst, v1, 0);
                push(dst);
            }
            else push(v1);
        }
        else {
            parse_expression(TOK_COMMA);
            consume(TOK_RPAREN, ")");
        }
    }

    else if (cur_token == TOK_INTEGER) {
        push_cur_long();
        next();
    }

    else if (cur_token == TOK_FLOATING_POINT_NUMBER) {
        push_cur_long_double();
        next();
    }

    else if (cur_token == TOK_STRING_LITERAL) {
        Value *dst = new_value();
        dst->vreg = new_vreg();
        dst->type = new_type(TYPE_CHAR + TYPE_PTR);

        Value *src1 = new_value();
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
        Symbol *symbol = lookup_symbol(cur_identifier, cur_scope, 1);
        if (!symbol) panic1s("Unknown symbol \"%s\"", cur_identifier);

        next();
        Type *type = dup_type(symbol->type);
        Scope *scope = symbol->scope;
        if (symbol->is_enum)
            push_integral_constant(TYPE_INT, symbol->value);
        else if (cur_token == TOK_LPAREN) {
            if (!symbol->function) panic1s("Illegal attempt to call a non-function %s", symbol->identifier);

            Function *function = symbol->function;

            // Function call
            int function_call = function_call_count++;
            next();
            Value *src1 = new_value();
            src1->int_value = function_call;
            src1->is_constant = 1;
            src1->type = new_type(TYPE_LONG);
            add_instruction(IR_START_CALL, 0, src1, 0);
            int arg_count = 0;
            int single_int_register_arg_count = 0;
            int single_sse_register_arg_count = 0;
            int offset = 0;
            int biggest_alignment = 0;

            while (1) {
                if (cur_token == TOK_RPAREN) break;
                parse_expression(TOK_COMMA);
                Value *arg = dup_value(src1);
                if (arg_count > MAX_FUNCTION_CALL_ARGS) panic1d("Maximum function call arg count of %d exceeded", MAX_FUNCTION_CALL_ARGS);

                arg->function_call_arg_index = arg_count;

                if (arg_count < function->param_count) {
                    if (!type_eq(src1->type, function->param_types[arg_count]))
                        push(add_convert_type_if_needed(pl(), function->param_types[arg_count]));
                }
                else {
                    // Apply default argument promotions
                    Value *src1 = pl();
                    Type *type;
                    if (src1->type->type < TYPE_INT) {
                        type = new_type(TYPE_INT);
                        if (src1->type->is_unsigned) type->is_unsigned = 1;
                    }
                    else if (src1->type->type == TYPE_FLOAT)
                        type = new_type(TYPE_DOUBLE);
                    else
                        type = src1->type;

                    if (!type_eq(src1->type, type))
                        push(add_convert_type_if_needed(src1, type));
                    else
                        push(src1);
                }

                int is_single_int_register = type_fits_in_single_int_register(vtop->type);
                int is_single_sse_register = is_sse_floating_point_type(vtop->type);
                int is_long_double = vtop->type->type == TYPE_LONG_DOUBLE;
                int is_push = is_long_double || (is_single_int_register && single_int_register_arg_count >= 6) || (is_single_sse_register && single_sse_register_arg_count >= 8);

                if (is_single_int_register)
                    arg->function_call_int_register_arg_index = single_int_register_arg_count < 6 ? single_int_register_arg_count : -1;
                else
                    arg->function_call_int_register_arg_index = -1;

                if (is_single_sse_register)
                    arg->function_call_sse_register_arg_index = single_sse_register_arg_count < 8 ? single_sse_register_arg_count : -1;
                else
                    arg->function_call_sse_register_arg_index = -1;

                if (is_push) {
                    int alignment = get_type_alignment(vtop->type);
                    if (alignment < 8) alignment = 8;
                    if (alignment > biggest_alignment) biggest_alignment = alignment;
                    int padding = ((offset + alignment  - 1) & (~(alignment - 1))) - offset;
                    offset += padding;
                    arg->function_call_arg_stack_padding = padding;
                    int type_size = get_type_size(vtop->type);
                    if (type_size < 8) type_size = 8;
                    offset += type_size;
                }

                add_instruction(IR_ARG, 0, arg, pl());
                single_int_register_arg_count += is_single_int_register;
                if (!is_push && is_single_sse_register) single_sse_register_arg_count++;
                arg_count++;
                if (cur_token == TOK_RPAREN) break;
                consume(TOK_COMMA, ",");
                if (cur_token == TOK_RPAREN) panic("Expected expression");
            }
            consume(TOK_RPAREN, ")");

            int padding = ((offset + biggest_alignment  - 1) & (~(biggest_alignment - 1))) - offset;
            int size = offset + padding;
            src1->function_call_arg_stack_padding = padding;

            Value *function_value = new_value();
            function_value->int_value = function_call;
            function_value->function_symbol = symbol;
            function_value->function_call_arg_push_count = size / 8;
            function_value->function_call_sse_register_arg_count = single_sse_register_arg_count;
            src1->function_call_arg_push_count = function_value->function_call_arg_push_count;

            Value *return_value = 0;
            if (type->type != TYPE_VOID) {
                return_value = new_value();
                return_value->vreg = new_vreg();
                return_value->type = dup_type(type);
            }
            else {
                Value *v = new_value();
                v->type = new_type(TYPE_VOID);
                push(v);
            }

            add_instruction(IR_CALL, return_value, function_value, 0);
            add_instruction(IR_END_CALL, 0, src1, 0);
            if (return_value) push(return_value);
        }

        else if (scope->parent == 0) {
            // Global symbol
            Value *src1 = new_value();
            src1->type = dup_type(type);
            src1->is_lvalue = 1;
            src1->global_symbol = symbol;
            push(src1);
        }

        else
            push_local_symbol(symbol);
    }

    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN, "(");
        Type *type;
        if (cur_token_is_type())
            type = parse_type();
        else {
            parse_expression(TOK_COMMA);
            type = pop()->type;
        }
        push_integral_constant(TYPE_LONG, get_type_size(type));
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

            parse_addition(TOK_COMMA, 1);
            consume(TOK_RBRACKET, "]");
            indirect();
        }

        else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
            // Postfix increment & decrement

            int org_token = cur_token;
            next();

            if (!vtop->is_lvalue) panic("Cannot ++ or -- an rvalue");
            if (!is_scalar_type(vtop->type)) panic("Cannot postfix ++ or -- on a non scalar");

            Value *v1 = pop();                 // lvalue
            Value *src1 = load(dup_value(v1)); // rvalue
            push(src1);
            push_value_size_constant(src1);
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

            Struct *str = all_structs[vtop->type->type - TYPE_PTR - TYPE_STRUCT];
            StructMember *member = lookup_struct_member(str, cur_identifier);
            indirect();

            vtop->type = dup_type(member->type);

            if (member->offset > 0) {
                // Make the struct lvalue into a pointer to struct rvalue for manipulation
                vtop->is_lvalue = 0;
                vtop->type = make_ptr(vtop->type);

                push_integral_constant(TYPE_INT, member->offset);
                arithmetic_operation(IR_ADD, 0);
            }

            vtop->type = dup_type(member->type);
            vtop->is_lvalue = 1;
            consume(TOK_IDENTIFIER, "identifier");
        }

        else if (cur_token == TOK_MULTIPLY) parse_arithmetic_operation(TOK_DOT, IR_MUL, 0);
        else if (cur_token == TOK_DIVIDE)   parse_arithmetic_operation(TOK_INC, IR_DIV, 0);
        else if (cur_token == TOK_MOD)      parse_arithmetic_operation(TOK_INC, IR_MOD, 0);

        else if (cur_token == TOK_PLUS) {
            next();
            parse_addition(TOK_MULTIPLY, 0);
        }

        else if (cur_token == TOK_MINUS) {
            Value *src1 = vtop;
            int src1_is_pointer = is_pointer_to_object_type(vtop->type);
            int src1_is_arithmetic = is_arithmetic_type(vtop->type);

            int factor = get_type_inc_dec_size(vtop->type);

            next();
            parse_expression(TOK_MULTIPLY);
            Value *src2 = vtop;

            int src2_is_pointer = is_pointer_to_object_type(vtop->type);
            int src2_is_integer = is_integer_type(vtop->type);
            int src2_is_arithmetic = is_arithmetic_type(vtop->type);

            // One of the following shall hold:
            // * both operands have arithmetic type;
            // * both operands are pointers to qualified or unqualified versions of compatible object types; or
            // * the left operand is a pointer to an object type and the right operand has integral type. (Decrementing is equivalent to subtracting 1.)
            if (
                (!(src1_is_arithmetic && src2_is_arithmetic)) &&
                (!(src1_is_pointer && src2_is_pointer && types_are_compabible(deref_ptr(src1->type), deref_ptr(src2->type)))) &&
                (!(src1_is_pointer && src2_is_integer))
            )
            panic("Invalid operands to binary minus");

            if (factor > 1) {
                if (!src2_is_pointer) {
                    push_integral_constant(TYPE_INT, factor);
                    arithmetic_operation(IR_MUL, 0);
                }

                arithmetic_operation(IR_SUB, 0);

                if (src2_is_pointer) {
                    vtop->type = new_type(TYPE_LONG);
                    push_integral_constant(TYPE_INT, factor);
                    arithmetic_operation(IR_DIV, 0);
                }
            }
            else
                arithmetic_operation(IR_SUB, 0);

            if (src2_is_pointer) vtop->type = new_type(TYPE_LONG);
        }

        else if (cur_token == TOK_BITWISE_LEFT || cur_token == TOK_BITWISE_RIGHT)  {
            int org_token = cur_token;
            next();
            if (!is_integer_type(vtop->type)) panic("Invalid operands to bitwise shift");
            Value *src1 = integer_promote(pl());
            parse_expression(level);
            if (!is_integer_type(vtop->type)) panic("Invalid operands to bitwise shift");
            Value *src2 = integer_promote(pl());
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
            Value *dst = new_value();
            dst->vreg = new_vreg();

            Value *ldst1 = new_label_dst(); // False case
            Value *ldst2 = new_label_dst(); // End
            add_conditional_jump(IR_JZ, ldst1);
            parse_expression(TOK_TERNARY);
            Value *src1 = vtop;
            add_instruction(IR_MOVE, dst, pl(), 0);
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of false case
            consume(TOK_COLON, ":");
            parse_expression(TOK_TERNARY);
            Value *src2 = vtop;
            dst->type = operation_type(src1->type, src2->type);
            add_instruction(IR_MOVE, dst, pl(), 0);
            push(dst);
            add_jmp_target_instruction(ldst2); // End
        }

        else if (cur_token == TOK_EQ) parse_assignment();

        else if (cur_token == TOK_PLUS_EQ || cur_token == TOK_MINUS_EQ) {
            Type *org_type = vtop->type;
            int org_token = cur_token;

            next();

            if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");

            Value *v1 = vtop;           // lvalue
            push(load(dup_value(v1)));  // rvalue

            int factor = get_type_inc_dec_size(org_type);
            parse_expression(TOK_EQ);

            if (factor > 1) {
                push_integral_constant(TYPE_INT, factor);
                arithmetic_operation(IR_MUL, new_type(TYPE_INT));
            }

            arithmetic_operation(org_token == TOK_PLUS_EQ ? IR_ADD : IR_SUB, 0);
            add_instruction(IR_MOVE, v1, vtop, 0);
        }
        else
            return; // Bail once we hit something unknown

    }
}

static void parse_iteration_conditional_expression(Value **lcond, Value **cur_loop_continue_dst, Value *lend) {
    *lcond  = new_label_dst();
    *cur_loop_continue_dst = *lcond;
    add_jmp_target_instruction(*lcond);

    parse_expression(TOK_COMMA);
    if (!is_scalar_type(vtop->type))
        panic("Expected scalar type for loop controlling expression");

    add_conditional_jump(IR_JZ, lend);
}

static void parse_iteration_statement() {
    enter_scope();

    int prev_loop = cur_loop;
    cur_loop = ++loop_count;
    Value *src1 = new_value();
    src1->int_value = prev_loop;
    Value *src2 = new_value();
    src2->int_value = cur_loop;
    add_instruction(IR_START_LOOP, 0, src1, src2);

    int loop_token = cur_token;
    next();

    // Preserve previous loop addresses so that we can have nested whiles/fors
    Value *old_loop_continue_dst = cur_loop_continue_dst;
    Value *old_loop_break_dst = cur_loop_break_dst;

    Value *linit = 0;
    Value *lcond = 0;
    Value *lincrement = 0;
    Value *lbody  = new_label_dst();
    Value *lend  = new_label_dst();
    cur_loop_continue_dst = 0;
    cur_loop_break_dst = lend;

    // Parse for
    if (loop_token == TOK_FOR) {
        consume(TOK_LPAREN, "(");

        // Init
        if (cur_token != TOK_SEMI) {
            linit  = new_label_dst();
            add_jmp_target_instruction(linit);
            parse_expression(TOK_COMMA);
        }
        consume(TOK_SEMI, ";");

        // Condition
        if (cur_token != TOK_SEMI) {
            parse_iteration_conditional_expression(&lcond, &cur_loop_continue_dst, lend);
            add_instruction(IR_JMP, 0, lbody, 0);
        }
        consume(TOK_SEMI, ";");

        // Increment
        if (cur_token != TOK_RPAREN) {
            lincrement  = new_label_dst();
            cur_loop_continue_dst = lincrement;
            add_jmp_target_instruction(lincrement);
            parse_expression(TOK_COMMA);
            if (lcond) add_instruction(IR_JMP, 0, lcond, 0);
        }

        consume(TOK_RPAREN, ")");
    }

    // Parse while
    else if (loop_token == TOK_WHILE) {
        consume(TOK_LPAREN, "(");
        parse_iteration_conditional_expression(&lcond, &cur_loop_continue_dst, lend);
        add_instruction(IR_JMP, 0, lbody, 0);
        consume(TOK_RPAREN, ")");
    }

    if (!cur_loop_continue_dst) cur_loop_continue_dst = lbody;
    add_jmp_target_instruction(lbody);
    parse_statement();

    // Parse do/while
    if (loop_token == TOK_DO) {
        consume(TOK_WHILE, "while");
        consume(TOK_LPAREN, "(");
        parse_iteration_conditional_expression(&lcond, &cur_loop_continue_dst, lend);
        consume(TOK_RPAREN, ")");
        expect(TOK_SEMI, ";");
        while (cur_token == TOK_SEMI) next();
    }

    if (lincrement)
        add_instruction(IR_JMP, 0, lincrement, 0);
    else if (lcond && loop_token != TOK_DO)
        add_instruction(IR_JMP, 0, lcond, 0);
    else if (lcond && loop_token == TOK_DO)
        add_instruction(IR_JMP, 0, lbody, 0);
    else
        add_instruction(IR_JMP, 0, lbody, 0);

    // Jump to the start of the body in a for loop like (;;i++)
    if (loop_token == TOK_FOR && !linit && !lcond && lincrement)
        add_instruction(IR_JMP, 0, lbody, 0);

    add_jmp_target_instruction(lend);

    // Restore previous loop addresses
    cur_loop_continue_dst = old_loop_continue_dst;
    cur_loop_break_dst = old_loop_break_dst;

    // Restore outer loop counter
    cur_loop = prev_loop;

    add_instruction(IR_END_LOOP, 0, src1, src2);

    exit_scope();
}

// Parse a statement
static void parse_statement() {
    vs = vs_start; // Reset value stack

    if (cur_token == TOK_HASH) {
        parse_directive();
        return;
    }

    if (cur_token_is_type()) {
        parse_declaration();
        expect(TOK_SEMI, ";");
        while (cur_token == TOK_SEMI) next();
        return;
    }

    if (cur_token == TOK_SEMI) {
        // Empty statement
        next();
        return;
    }

    else if (cur_token == TOK_LCURLY) {
        enter_scope();
        next();
        while (cur_token != TOK_RCURLY) parse_statement();
        consume(TOK_RCURLY, "}");
        exit_scope();
        return;
    }

    else if (cur_token == TOK_DO || cur_token == TOK_WHILE || cur_token == TOK_FOR)
        parse_iteration_statement();

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
        parse_expression(TOK_COMMA);
        if (!is_scalar_type(vtop->type)) panic("The controlling statement of an if statement must be a scalar");
        consume(TOK_RPAREN, ")");

        Value *ldst1 = new_label_dst(); // False case
        Value *ldst2 = new_label_dst(); // End
        add_conditional_jump(IR_JZ, ldst1);
        parse_statement();

        if (cur_token == TOK_HASH) parse_directive();

        if (cur_token == TOK_ELSE) {
            next();
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of else case
            parse_statement();
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
            parse_expression(TOK_COMMA);

            Value *src1;
            if (vtop && vtop->type->type == TYPE_VOID && cur_function_symbol->function->return_type->type == TYPE_VOID) {
                // Deal with case of returning the result of a void function in a void function
                // e.g. foo(); void bar() { return foo(); }
                vs++; // Remove from value stack
                src1 = 0;
            }
            else
                src1 = add_convert_type_if_needed(pl(), cur_function_symbol->function->return_type);

            add_instruction(IR_RETURN, 0, src1, 0);
        }
        consume(TOK_SEMI, ";");
    }

    else {
        parse_expression(TOK_COMMA);
        consume(TOK_SEMI, ";");
    }
}

// String the filename component from a path
static char *base_path(char *path) {
    int end = strlen(path) - 1;
    char *result = malloc(strlen(path) + 1);
    while (end >= 0 && path[end] != '/') end--;
    if (end >= 0) result = memcpy(result, path, end + 1);
    result[end + 1] = 0;

    return result;
}

static void parse_include() {
    if (parsing_header) panic("Nested header includes not impemented");

    consume(TOK_INCLUDE, "include");
    if (cur_token == TOK_LT) {
        // Ignore #include <...>
        next();
        while (cur_token != TOK_GT) next();
        next();
        return;
    }

    if (cur_token != TOK_STRING_LITERAL) panic("Expected string literal in #include");

    char *filename;
    asprintf(&filename, "%s%s", base_path(cur_filename), cur_string_literal);

    c_input        = input;
    c_input_size   = input_size;
    c_ip           = ip;
    c_cur_filename = cur_filename;
    c_cur_line     = cur_line;

    parsing_header = 1;
    init_lexer(filename);
}

static void parse_ifdefs() {
    if (cur_token == TOK_IFDEF && (in_ifdef || in_ifdef_else))
        panic("Nested ifdefs not implemented");
    else if (cur_token == TOK_ELSE) {
        // The true case has been parsed, skip else case
        next();
        if (!in_ifdef) panic("Got ELSE directive when not in an ifdef");
        while (cur_token != TOK_ENDIF) next();
        next();
        in_ifdef = 0;
        in_ifdef_else = 0;
        return;
    }
    else if (cur_token == TOK_ENDIF) {
        // Clean up
        next();
        if (!in_ifdef && !in_ifdef_else) panic("Got ENDIF without ifdef");
        in_ifdef = 0;
        in_ifdef_else = 0;
        return;
    }

    // Process an ifdef
    next();
    expect(TOK_IDENTIFIER, "identifier");
    int directive_set = !!map_get(directives, cur_identifier);
    next();

    if (directive_set) {
        // Parse true case
        in_ifdef = 1;
        return;
    }
    else {
        // Skip true case & look for #else or #endif
        while (1) {
            while (cur_token != TOK_HASH) {
                if (cur_token == TOK_EOF) panic("Got ifdef without else of endif");
                next();
            }
            next();
            if (cur_token == TOK_ELSE || cur_token == TOK_ENDIF) break;
        }
        if (cur_token == TOK_ELSE) {
            // Let else case be parsed
            next();
            in_ifdef_else = 1;
            return;
        }
        next(); // TOK_ENDIF, there's nothing more to do
    }
}

static void parse_directive() {
    consume(TOK_HASH, "#");
    if (cur_token == TOK_INCLUDE) parse_include();
    else if (cur_token == TOK_IFDEF || cur_token == TOK_ELSE || cur_token == TOK_ENDIF) parse_ifdefs();
    else if (cur_token == TOK_DEFINE) {
        next();
        expect(TOK_IDENTIFIER, "identifier");
        map_put(directives, cur_identifier, "");
        next();
    }
    else if (cur_token == TOK_UNDEF) {
        next();
        expect(TOK_IDENTIFIER, "identifier");
        map_delete(directives, cur_identifier);
        next();
    }
    else {
        panic1d("Unimplemented directive with token %d", cur_token);
    }
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
    int seen_function_declaration = 0;

    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token == TOK_HASH)
            parse_directive();
        else if (cur_token == TOK_EXTERN || cur_token == TOK_STATIC || cur_token_is_type() ) {
            // Variable or function definition

            int is_external = cur_token == TOK_EXTERN;
            int is_static = cur_token == TOK_STATIC;
            if (is_external || is_static) next();

            Type *base_type = parse_base_type(0);

            while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
                Type *type = base_type;
                while (cur_token == TOK_MULTIPLY) { type = make_ptr(type); next(); }

                if (type->type >= TYPE_STRUCT && type->type < TYPE_PTR) panic("Direct usage of struct variables not implemented");

                expect(TOK_IDENTIFIER, "identifier");

                Symbol *s = lookup_symbol(cur_identifier, global_scope, 1);
                if (!s) {
                    // Create a new symbol if it wasn't already declared. The
                    // previous declaration is left unchanged.

                    s = new_symbol();
                    s->type = dup_type(type);
                    s->identifier = cur_identifier;
                }

                next();

                if (cur_token == TOK_LPAREN) {
                    cur_function_symbol = s;
                    // Function declaration or definition

                    enter_scope();
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
                    s->function->local_symbol_count = 0;

                    int param_count = 0;
                    while (1) {
                        if (cur_token == TOK_RPAREN) break;

                        if (cur_token_is_type()) {
                            Type *type = parse_type();
                            if (type->type >= TYPE_STRUCT && type->type < TYPE_PTR) panic("Direct usage of struct variables not implemented");

                            expect(TOK_IDENTIFIER, "identifier");
                            Symbol *param_symbol = new_symbol();
                            param_symbol->type = dup_type(type);
                            param_symbol->identifier = cur_identifier;
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

                        vreg_count = 0; // Reset global vreg_count
                        function_call_count = 0;
                        cur_loop = 0;
                        loop_count = 0;

                        consume(TOK_LCURLY, "{");
                        while (cur_token != TOK_RCURLY) parse_statement();
                        consume(TOK_RCURLY, "}");

                        cur_function_symbol->function->vreg_count = vreg_count;
                    }
                    else
                        // Make it clear that this symbol will need to be backpatched if used
                        // before the definition has been processed.
                        cur_function_symbol->value = 0;

                    cur_scope = global_scope;

                    break; // Break out of function parameters loop
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

            int value = 0;
            while (cur_token != TOK_RCURLY) {
                expect(TOK_IDENTIFIER, "identifier");
                next();
                if (cur_token == TOK_EQ) {
                    next();
                    int sign = 1;
                    if (cur_token == TOK_MINUS) {
                        sign = -1;
                        next();
                    }
                    expect(TOK_INTEGER, "integer");
                    value = sign * cur_long;
                    next();
                }

                Symbol *s = new_symbol();
                s->is_enum = 1;
                s->type = new_type(TYPE_INT);
                s->identifier = cur_identifier;
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
    printf("Symbols:\n");

    for (int i = 0; i < global_scope->symbol_count; i++) {
        Symbol *symbol = global_scope->symbols[i];
        Type *type = symbol->type;
        char *identifier = symbol->identifier;
        long value = symbol->value;
        long local_index = symbol->local_index;
        int is_global = symbol->scope == global_scope;
        printf("%d %-3ld %-20ld ", is_global, local_index, value);
        int type_len = print_type(stdout, type);
        for (int j = 0; j < 24 - type_len; j++) printf(" ");
        printf("%s\n", identifier);
    }

    printf("\n");
}

void init_parser() {
    string_literals = malloc(sizeof(char *) * MAX_STRING_LITERALS);
    string_literal_count = 0;

    all_structs = malloc(sizeof(struct struct_desc *) * MAX_STRUCTS);
    all_structs_count = 0;

    all_typedefs = malloc(sizeof(struct typedef_desc *) * MAX_TYPEDEFS);
    all_typedefs_count = 0;

    vs_start = malloc(sizeof(struct value *) * VALUE_STACK_SIZE);
    vs_start += VALUE_STACK_SIZE; // The stack traditionally grows downwards
    label_count = 0;

    in_ifdef = 0;
    in_ifdef_else = 0;
}
