#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static Type *integer_promote_type(Type *type);
static Type *parse_struct_or_union_type_specifier();
static Type *parse_enum_type_specifier();
static void parse_directive();
static void parse_statement();
static void parse_expression(int level);

static Type *base_type;

int function_call_count; // Uniquely identify a function call within a function

StructOrUnion **all_structs_and_unions;  // All structs/unions defined globally.
int all_structs_and_unions_count;        // Number of structs/unions, complete and incomplete
int vreg_count;                          // Virtual register count for currently parsed function

// Allocate a new virtual register
static int new_vreg() {
    vreg_count++;
    if (vreg_count >= MAX_VREG_COUNT) panic1d("Exceeded max vreg count %d", MAX_VREG_COUNT);
    return vreg_count;
}

static Value *decay_array_value(Value *v) {
    Value *result = dup_value(v);
    result->type = decay_array_to_pointer(v->type);

    return result;
}

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

// Pop a void value from the stack, or nothing if the stack is empty
static void *pop_void() {
    if (vs == vs_start) return;

    vs++;
    vtop = *vs;
}

static Value *load_bit_field(Value *src1) {
    Value *dst = new_value();
    dst->type = new_type(TYPE_INT);
    dst->type->is_unsigned = src1->type->is_unsigned;
    dst->vreg = new_vreg();
    add_instruction(IR_LOAD_BIT_FIELD, dst, src1, 0);

    return dst;
}

// load a value into a register if not already done. lvalues are converted into
// rvalues.
static Value *load(Value *src1) {
    if (src1->is_constant) return src1;
    if (src1->vreg && !src1->is_lvalue) return src1;
    if (src1->type->type == TYPE_STRUCT_OR_UNION) return src1;
    if (src1->type->type == TYPE_FUNCTION) return src1;
    if (src1->bit_field_size) return load_bit_field(src1);

    Value *dst = dup_value(src1);
    dst->vreg = new_vreg();
    dst->is_lvalue = 0;
    dst->type->is_const = 0;

    // Ensure an offset read from a struct/union isn't persisted into a register which might
    // get moved back onto a stack, e.g. for long doubles
    dst->offset = 0;

    if (src1->type->type == TYPE_ARRAY) {
        if (src1->is_string_literal) {
            // Load the address of a string literal into a register
            dst->type = decay_array_to_pointer(dst->type);
            dst->is_string_literal = 0;
            dst->string_literal_index = 0;
            add_instruction(IR_MOVE, dst, src1, 0);
        }
        else {
            // Take the address of an array
            dst->local_index = 0;
            dst->global_symbol = 0;
            dst->type = decay_array_to_pointer(dst->type);
            add_instruction(IR_ADDRESS_OF, dst, src1, 0);
        }
    }

    else if (src1->vreg && src1->is_lvalue) {
        // An lvalue in a register needs a dereference
        if (src1->type->type == TYPE_VOID) panic("Cannot dereference a *void");
        if (src1->type->type == TYPE_STRUCT_OR_UNION) panic("Cannot dereference a pointer to a struct/union");
        if (src1->type->type == TYPE_ARRAY) panic("Cannot dereference a pointer to an array");

        src1 = dup_value(src1);
        src1->type = make_pointer(src1->type);
        src1->is_lvalue = 0;
        src1->type->is_const = 0;
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
    return -1 - cur_function_symbol->type->function->local_symbol_count++;
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

// Returns destination type of an operation with two operands
// https://en.cppreference.com/w/c/language/conversion
Type *operation_type(Value *src1, Value *src2, int for_ternary) {
    Type *src1_type = src1->type;
    Type *src2_type = src2->type;

    // Decay arrays to pointers
    if (src1_type->type == TYPE_ARRAY) src1_type = decay_array_to_pointer(src1_type);
    if (src2_type->type == TYPE_ARRAY) src2_type = decay_array_to_pointer(src2_type);

    if (src1_type->type == TYPE_FUNCTION && src2_type->type == TYPE_FUNCTION)
        return new_type(TYPE_FUNCTION);

    Type *result;

    if (src1_type->type == TYPE_STRUCT_OR_UNION || src2_type->type == TYPE_STRUCT_OR_UNION)
        panic("Unexpected call to operation_type() on a structs/union");

    // If it's a ternary and one is a pointer and the other a pointer to void, then the result is a pointer to void.
    else if (src1_type->type == TYPE_PTR && is_pointer_to_void(src2->type)) return for_ternary ? src2->type : src1->type;
    else if (src2_type->type == TYPE_PTR && is_pointer_to_void(src1->type)) return for_ternary ? src1->type : src2->type;
    else if (src1_type->type == TYPE_PTR) return src1_type;
    else if (src2_type->type == TYPE_PTR) return src2_type;

    // If either is a long double, promote both to long double
    if (src1_type->type == TYPE_LONG_DOUBLE || src2_type->type == TYPE_LONG_DOUBLE)
        return new_type(TYPE_LONG_DOUBLE);

    // If either is a double, promote both to double
    if (src1_type->type == TYPE_DOUBLE || src2_type->type == TYPE_DOUBLE)
        return new_type(TYPE_DOUBLE);

    // If either is a float, promote both to float
    if (src1_type->type == TYPE_FLOAT || src2_type->type == TYPE_FLOAT)
        return new_type(TYPE_FLOAT);

    if (src1_type->type == TYPE_VOID || src2_type->type == TYPE_VOID)
        return new_type(TYPE_VOID);

    // Otherwise, apply integral promotions
    src1_type = integer_promote_type(src1_type);
    src2_type = integer_promote_type(src2_type);

    // They are two integer types
    if (src1_type->type == TYPE_LONG || src2_type->type == TYPE_LONG)
        result = new_type(TYPE_LONG);
    else
        result = new_type(TYPE_INT);

    result->is_unsigned = is_integer_operation_result_unsigned(src1_type, src2_type);

    return result;
}

// Returns destination type of an operation using the top two values in the stack
static Type *vs_operation_type() {
    return operation_type(vtop, vs[1], 0);
}

static int cur_token_is_type() {
    return (
        cur_token == TOK_SIGNED ||
        cur_token == TOK_UNSIGNED ||
        cur_token == TOK_CONST ||
        cur_token == TOK_VOLATILE ||
        cur_token == TOK_VOID ||
        cur_token == TOK_CHAR ||
        cur_token == TOK_SHORT ||
        cur_token == TOK_INT ||
        cur_token == TOK_FLOAT ||
        cur_token == TOK_DOUBLE ||
        cur_token == TOK_LONG ||
        cur_token == TOK_STRUCT ||
        cur_token == TOK_UNION ||
        cur_token == TOK_ENUM ||
        cur_token == TOK_TYPEDEF_TYPE
    );
}

// How much will the ++, --, +=, -= operators increment a type?
static int get_type_inc_dec_size(Type *type) {
    return type->type == TYPE_PTR ? get_type_size(type->target) : 1;
}

// Parse void, int, signed, unsigned, ... long, double, struct and typedef type names
static Type *parse_type_specifier() {
    Type *type;

    int seen_signed = 0;
    int seen_unsigned = 0;
    int seen_long = 0;
    int seen_const = 0;
    int seen_volatile = 0;

    if (cur_token == TOK_SIGNED) {
        seen_signed = 1;
        next();
    }
    else if (cur_token == TOK_UNSIGNED) {
        seen_unsigned = 1;
        next();
    }

    // Parse type qualifiers. They are allowed to be duplicated, e.g. const const
    while (cur_token == TOK_CONST || cur_token == TOK_VOLATILE) {
        if (cur_token == TOK_CONST) seen_const = 1;
        else  seen_volatile = 1;
        next();
    }

         if (cur_token == TOK_VOID)         { type = new_type(TYPE_VOID);   next(); }
    else if (cur_token == TOK_CHAR)         { type = new_type(TYPE_CHAR);   next(); }
    else if (cur_token == TOK_SHORT)        { type = new_type(TYPE_SHORT);  next(); }
    else if (cur_token == TOK_INT)          { type = new_type(TYPE_INT);    next(); }
    else if (cur_token == TOK_FLOAT)        { type = new_type(TYPE_FLOAT);  next(); }
    else if (cur_token == TOK_DOUBLE)       { type = new_type(TYPE_DOUBLE); next(); }
    else if (cur_token == TOK_LONG)         { type = new_type(TYPE_LONG);   next(); seen_long = 1; }

    else if (cur_token == TOK_STRUCT || cur_token == TOK_UNION)
        type = parse_struct_or_union_type_specifier();

    else if (cur_token == TOK_ENUM)
        type = parse_enum_type_specifier();

    else if (cur_token == TOK_TYPEDEF_TYPE) {
        type = dup_type(cur_lexer_type);
        next();
    }

    else if (seen_signed || seen_unsigned || seen_const || seen_volatile)
        type = new_type(TYPE_INT);

    else
        panic1d("Unable to determine type from token %d", cur_token);

    if ((seen_unsigned || seen_signed) && !is_integer_type(type)) panic("Signed/unsigned can only apply to integer types");
    if (seen_unsigned && seen_signed && !is_integer_type(type)) panic("Both ‘signed’ and ‘unsigned’ in declaration specifiers");

    if (seen_const) type->is_const = 1;
    if (seen_volatile) type->is_volatile = 1;

    if (is_integer_type(type)) {
        type->is_unsigned = seen_unsigned;

        if (cur_token == TOK_LONG && type->type == TYPE_LONG) next(); // On 64 bit, long longs are equivalent to longs
        if (cur_token == TOK_INT && (type->type == TYPE_SHORT || type->type == TYPE_INT || type->type == TYPE_LONG)) next();
    }

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

static Type *concat_types(Type *type1, Type *type2) {
    if (type1 == 0) return type2;
    else if (type2 == 0) return type1;
    else if (type1 ==0 && type2 == 0) panic("concat type got two null types");

    Type *type1_tail = type1;
    while (type1_tail->target) type1_tail = type1_tail->target;
    type1_tail->target = type2;

    return type1;
}

Type *parse_direct_declarator();

Type *parse_declarator() {
    Type *type = 0;

    while (1) {
        if (cur_token != TOK_MULTIPLY) {
            // Go up a level and return
            return concat_types(parse_direct_declarator(), type);
        }
        else if (cur_token == TOK_MULTIPLY) {
            // Pointer
            next();
            type = make_pointer(type);

            // Parse type qualifiers. They are allowed to be duplicated, e.g. const const
            while (cur_token == TOK_CONST || cur_token == TOK_VOLATILE) {
                if (cur_token == TOK_CONST) type->is_const = 1;
                else type->is_volatile = 1;
                next();
            }
        }
        else
            return type;
    }
}

static Type *parse_function(Type *return_type) {
    Type *function_type = new_type(TYPE_FUNCTION);
    function_type->function = new_function();
    function_type->function->param_types = malloc(sizeof(Type) * MAX_FUNCTION_CALL_ARGS);

    enter_scope();
    function_type->function->scope = cur_scope;

    int param_count = 0;
    while (1) {
        if (cur_token == TOK_RPAREN) break;

        if (cur_token_is_type()) {
            char *old_cur_type_identifier = cur_type_identifier;
            cur_type_identifier = 0;
            Type *type = parse_type_name();

            Symbol *param_symbol = new_symbol();
            Type *symbol_type;

            // Array parameters decay to a pointer
            if (type->type == TYPE_ARRAY)
                symbol_type = decay_array_to_pointer(type);
            else
                symbol_type = type;

            param_symbol->type = dup_type(symbol_type);

            param_symbol->identifier = cur_type_identifier;
            function_type->function->param_types[param_count] = dup_type(type);
            param_symbol->local_index = param_count++;

            cur_type_identifier = old_cur_type_identifier;
        }
        else if (cur_token == TOK_ELLIPSES) {
            function_type->function->is_variadic = 1;
            next();
        }
        else
            panic("Expected type or )");

        if (cur_token == TOK_RPAREN) break;
        consume(TOK_COMMA, ",");
        if (cur_token == TOK_RPAREN) panic("Expected expression");
    }

    function_type->function->param_count = param_count;

    exit_scope();

    consume(TOK_RPAREN, ")");

    return function_type;
}

Type *parse_direct_declarator() {
    Type *type = 0;

    if (cur_token == TOK_TYPEDEF_TYPE) {
        // Special case of redefining a typedef. The lexer identifies it as a typedef,
        // but in this context, it's an identifier;
        cur_token = TOK_IDENTIFIER;
    }

    if (cur_token == TOK_IDENTIFIER) {
        // Set cur_type_identifier only once. The caller is expected to set
        // cur_type_identifier to zero. This way, the first parsed identifier
        // is kept.
        if (!cur_type_identifier) cur_type_identifier = cur_identifier;
        next();
    }

    for (int i = 0; ; i++) {
        if (cur_token == TOK_LPAREN) {
            next();
            if (cur_token == TOK_RPAREN || cur_token_is_type()) {
                // Function
                type = concat_types(type, parse_function(type));
            }
            else if (i == 0) {
                // (subtype)
                type = concat_types(type, parse_declarator());
                consume(TOK_RPAREN, ")");
            }
            else
                panic("Expected )");
        }
        else if (cur_token == TOK_LBRACKET) {
            // Array [] or [<num>]
            next();
            int size = 0;
            if (cur_token == TOK_INTEGER) {
                size = cur_long;
                next();
            }

            Type *array_type = new_type(TYPE_ARRAY);
            array_type->array_size = size;
            type = concat_types(type, array_type);
            consume(TOK_RBRACKET, "]");
        }
        else
            return type;
    }
}

Type *parse_type_name() {
    return concat_types(parse_declarator(), parse_type_specifier());
}

// Allocate a new StructOrUnion
static Type *new_struct_or_union(char *tag_identifier) {
    StructOrUnion *s = malloc(sizeof(StructOrUnion));
    if (tag_identifier != 0) all_structs_and_unions[all_structs_and_unions_count++] = s;
    s->members = malloc(sizeof(StructOrUnionMember *) * MAX_STRUCT_MEMBERS);
    memset(s->members, 0, sizeof(StructOrUnionMember *) * MAX_STRUCT_MEMBERS);

    Type *type = make_struct_or_union_type(s);

    if (tag_identifier) {
        Tag *tag = new_tag();
        tag->identifier = tag_identifier;
        tag->type = type;
        type->tag = tag;
    }
    else
        type->tag = 0;

    return type;
}

// Search for a struct tag. Returns 0 if not found.
static Type *find_struct_or_union(char *identifier, int is_union) {
    Tag *tag = lookup_tag(identifier, cur_scope, 1);

    if (!tag) return 0;

    if (tag->type->type == TYPE_STRUCT_OR_UNION) {
        if (tag->type->struct_or_union_desc->is_union != is_union)
            panic1s("Tag %s is the wrong kind of tag", identifier);
        return tag->type;
    }
    else
        panic1s("Tag %s is the wrong kind of tag", identifier);
}

static Type *find_enum(char *identifier) {
    Tag *tag = lookup_tag(identifier, cur_scope, 1);

    if (!tag) return 0;
    if (tag->type->type != TYPE_ENUM) panic1s("Tag %s is the wrong kind of tag", identifier);

    return tag->type;
}

// Parse struct definitions and uses.
static Type *parse_struct_or_union_type_specifier() {
    // Parse a struct or union

    int is_union = cur_token == TOK_UNION;
    next();

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

    char *identifier = 0;
    // A typedef identifier be the same as a struct tag, in this context, the lexer
    // sees a typedef tag, but really it's a struct tag.
    if (cur_token == TOK_IDENTIFIER || cur_token == TOK_TYPEDEF_TYPE) {
        identifier = cur_identifier;
        next();
    }

    if (cur_token == TOK_LCURLY) {
        // Struct/union definition

        consume(TOK_LCURLY, "{");

        Type *type = 0;
        if (identifier) type = find_struct_or_union(identifier, is_union);
        if (!type) type = new_struct_or_union(identifier);

        StructOrUnion *s = type->struct_or_union_desc;
        s->is_packed = is_packed;
        s->is_union = is_union;

        // Loop over members
        int member_count = 0;
        while (cur_token != TOK_RCURLY) {
            if (cur_token == TOK_HASH) {
                parse_directive();
                continue;
            }

            Type *base_type = parse_type_specifier();

            // Catch e.g. struct { struct s { int i; }; } s; which isn't in the C90 spec
            if (base_type->type == TYPE_STRUCT_OR_UNION && cur_token == TOK_SEMI)
                panic("Structs/unions members must have a name");

            while (cur_token != TOK_SEMI) {
                Type *type;

                int unnamed_bit_field = 0;
                if (cur_token != TOK_COLON) {
                    cur_type_identifier = 0;
                    type = concat_types(parse_declarator(), base_type);
                }

                else {
                    // Unnamed bit field
                    next();
                    cur_type_identifier = 0;
                    type = new_type(TYPE_INT);
                    unnamed_bit_field = 1;
                }

                StructOrUnionMember *member = malloc(sizeof(StructOrUnionMember));
                memset(member, 0, sizeof(StructOrUnionMember));
                member->identifier = cur_type_identifier;
                member->type = dup_type(type);
                s->members[member_count++] = member;

                if (unnamed_bit_field || cur_token == TOK_COLON) {
                    // Bit field
                    if (!unnamed_bit_field) next(); // consume TOK_COLON

                    if (type->type != TYPE_INT) panic("Bit fields must be integers");
                    if (cur_token != TOK_INTEGER) panic("Expected an integer value for a bit field");
                    if (cur_type_identifier && cur_long == 0) panic("Invalid bit field size 0 for named member");
                    if (cur_long < 0 || cur_long > 32) panic1d("Invalid bit field size %d", cur_long);

                    member->is_bit_field = 1;
                    member->bit_field_size = cur_long;

                    next();
                }

                if (cur_token != TOK_COMMA && cur_token != TOK_SEMI) panic("Expected a ; or ,");

                if (cur_token == TOK_COMMA) next();
            }
            while (cur_token == TOK_SEMI) consume(TOK_SEMI, ";");
        }
        consume(TOK_RCURLY, "}");

        complete_struct_or_union(s);
        return type;
    }
    else {
        // Struct/union use

        Type *type = find_struct_or_union(identifier, is_union);
        if (type) return type; // Found a complete or incomplete struct

        // Didn't find a struct, but that's ok, create a incomplete one
        // to be populated later when it's defined.
        type = new_struct_or_union(identifier);
        StructOrUnion *s = type->struct_or_union_desc;
        s->is_incomplete = 1;
        s->is_packed = is_packed;
        s->is_union = is_union;
        return type;
    }
}

static Type *parse_enum_type_specifier() {
    next();

    Type *type = new_type(TYPE_ENUM);

    char *identifier = 0;

    // A typedef identifier be the same as a struct tag, in this context, the lexer
    // sees a typedef tag, but really it's a struct tag.
    if (cur_token == TOK_IDENTIFIER || cur_token == TOK_TYPEDEF_TYPE) {
        identifier = cur_identifier;

        Tag *tag = new_tag();
        tag->identifier = cur_identifier;
        tag->type = type;
        type->tag = tag;

        next();
    }
    else
        type->tag = 0;

    if (cur_token == TOK_LCURLY) {
        // Enum definition

        consume(TOK_LCURLY, "{");

        int member_count = 0;
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
            s->is_enum_value = 1;
            s->type = new_type(TYPE_INT);
            s->identifier = cur_identifier;
            s->value = value++;
            s++;

            member_count++;

            if (cur_token == TOK_COMMA) next();
        }
        consume(TOK_RCURLY, "}");

        if (!member_count) panic("An enum must have at least one member");
    }

    else {
        // Enum use

        Type *type = find_enum(identifier);
        if (!type) panic1s("Unknown enum %s", identifier);
        return type;
    }

    return type;
}

static void parse_typedef() {
    next();

    Type *base_type = parse_type_specifier();

    while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
        cur_type_identifier = 0;

        Type *type = concat_types(parse_declarator(), dup_type(base_type));

        if (all_typedefs_count == MAX_TYPEDEFS) panic("Exceeded max typedefs");
        Typedef *td = malloc(sizeof(Typedef));
        memset(td, 0, sizeof(Typedef));
        td->identifier = cur_type_identifier;
        td->type = type;
        all_typedefs[all_typedefs_count++] = td;

        Symbol *symbol = new_symbol();
        symbol->type = new_type(TYPE_TYPEDEF);
        symbol->type->target = type;
        symbol->identifier = cur_type_identifier;

        if (cur_token == TOK_COMMA) next();
        if (cur_token == TOK_SEMI) break;
    }
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
    dst->type = deref_pointer(src1->type);
    dst->is_lvalue = 1;
    push(dst);
}

// Search for a struct member. Panics if it doesn't exist
static StructOrUnionMember *lookup_struct_or_union_member(Type *type, char *identifier) {
    StructOrUnionMember **pmember = type->struct_or_union_desc->members;

    while (*pmember) {
        char *member_identifier = (*pmember)->identifier;
        if (member_identifier && !strcmp(member_identifier, identifier)) return *pmember;
        pmember++;
    }

    panic2s("Unknown member %s in struct %s\n", identifier, type->tag ? type->tag->identifier : "(anonymous)");
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
    if (!is_scalar_type(vtop->type)) panic("Invalid operands to &&/||");
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
    if (!is_scalar_type(vtop->type)) panic("Invalid operands to &&/||");
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
    if (!is_integer_type(type)) panic("Invalid operand, expected integer type");

    if (type->type >= TYPE_INT && type->type <= TYPE_LONG)
        return type;
    else
        return new_type(TYPE_INT); // An int can hold all the values
}

static Value *integer_promote(Value *v) {
    if (!is_integer_type(v->type)) panic("Invalid operand, expected integer type");

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

    if (src->type->type == TYPE_PTR) panic("Unable to convert a pointer to a long double");
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

    if (src->type->type == TYPE_PTR) panic("Unable to convert a pointer to a double");
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

    if (src->type->type == TYPE_PTR) panic("Unable to convert a pointer to a double");
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

static void arithmetic_operation(int operation, Type *dst_type) {
    // Pull two items from the stack and push the result. Code in the IR
    // is generated when the operands can't be evaluated directly.
    Type *common_type = vs_operation_type();
    if (!dst_type) dst_type = common_type;

    Value *src2 = pl();
    Value *src1 = pl();

    // In an equality compariosn, swap operands so that the constant is second.
    // Instruction selection doesn't have rules that deal with a constant first operand.
    if (operation == IR_EQ || operation == IR_NE) {
        if (src1->is_constant && !src2->is_constant) {
            Value *t = src1;
            src1 = src2;
            src2 = t;
        }

        if (common_type->type == TYPE_FUNCTION && src1->global_symbol && src2->global_symbol)  {
            // Compare two global functions with each other
            push_integral_constant(TYPE_INT, src1->global_symbol == src2->global_symbol);
            return;
        }

        else if (src1->type->type == TYPE_FUNCTION && src1->global_symbol && is_pointer_to_function_type(src2->type))  {
            // src1 is a global function, load the address into a register
            Value *new_src1 = new_value();
            new_src1->vreg = new_vreg();
            new_src1->type = make_pointer(dup_type(src1->type));
            add_instruction(IR_ADDRESS_OF, new_src1, src1, 0);
            src1 = new_src1;
        }

        else if (src2->type->type == TYPE_FUNCTION && src2->global_symbol && is_pointer_to_function_type(src1->type))  {
            // src2 is a global function, load the address into a register
            Value *new_src2 = new_value();
            new_src2->vreg = new_vreg();
            new_src2->type = make_pointer(dup_type(src2->type));
            add_instruction(IR_ADDRESS_OF, new_src2, src2, 0);
            src2 = new_src2;
        }
    }

    if (is_integer_type(common_type) && is_integer_type(src1->type) && is_integer_type(src2->type)) {
        if (!type_eq(common_type, src1->type) && (src1->type->type <= common_type->type || src1->type->is_unsigned != common_type->is_unsigned))
            src1 = integer_type_change(src1, common_type);
        if (!type_eq(common_type, src2->type) && (src2->type->type <= common_type->type || src2->type->is_unsigned != common_type->is_unsigned))
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

    add_ir_op(operation, dst_type, new_vreg(), src1, src2);
}

static void check_arithmetic_operation_type(int operation, Value *src1, Value *src2) {
    int src1_is_arithmetic = is_arithmetic_type(src1->type);
    int src2_is_arithmetic = is_arithmetic_type(src2->type);
    int src1_is_integer = is_integer_type(src1->type);
    int src2_is_integer = is_integer_type(src2->type);
    int src1_is_pointer = is_pointer_type(src1->type);
    int src2_is_pointer = is_pointer_type(src2->type);

    int src1_is_function = src1->type->type == TYPE_FUNCTION || is_pointer_to_function_type(src1->type);
    int src2_is_function = src2->type->type == TYPE_FUNCTION || is_pointer_to_function_type(src2->type);

    if (operation == IR_MUL  && (!src1_is_arithmetic || !src2_is_arithmetic)) panic("Invalid operands to binary *");
    if (operation == IR_DIV  && (!src1_is_arithmetic || !src2_is_arithmetic)) panic("Invalid operands to binary /");
    if (operation == IR_MOD  && (!src1_is_integer    || !src2_is_integer))    panic("Invalid operands to binary %");
    if (operation == IR_BAND && (!src1_is_integer    || !src2_is_integer))    panic("Invalid operands to binary &");
    if (operation == IR_BOR  && (!src1_is_integer    || !src2_is_integer))    panic("Invalid operands to binary |");
    if (operation == IR_XOR  && (!src1_is_integer    || !src2_is_integer))    panic("Invalid operands to binary ^");

    if (operation == IR_LT || operation == IR_GT || operation == IR_LE || operation == IR_GE) {
        Type *src1_type_deref = 0;
        Type *src2_type_deref = 0;

        if (src1_is_pointer) src1_type_deref = deref_pointer(src1->type);
        if (src2_is_pointer) src2_type_deref = deref_pointer(src2->type);

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

    if (operation == IR_EQ || operation == IR_NE) {
        Type *src1_type_deref = 0;
        Type *src2_type_deref = 0;

        if (src1_is_pointer) src1_type_deref = deref_pointer(src1->type);
        if (src2_is_pointer) src2_type_deref = deref_pointer(src2->type);

        // One of the following shall hold:
        // * both operands have arithmetic type;
        // * both operands are pointers to qualified or unqualified versions of compatible types;
        // * one operand is a pointer to an object or incomplete type and the other is a qualified or unqualified version of void ; or
        // * one operand is a pointer and the other is a null pointer constant.
        //
        // Deviation from the spec: comparisons between arithmetic and pointers types are allowed
        if (
            (!((src1_is_arithmetic) && (src2_is_arithmetic))) &&
            (!((src1_is_function) && (src2_is_function))) &&
            (!(src1_is_pointer && src2_is_pointer && types_are_compabible(src1_type_deref, src2_type_deref))) &&
            (!(src1_is_pointer && src2_is_pointer && is_pointer_to_void(src2->type))) &&
            (!(src2_is_pointer && src1_is_pointer && is_pointer_to_void(src1->type))) &&
            (!(src1_is_pointer && is_null_pointer(src2))) &&
            (!(src2_is_pointer && is_null_pointer(src1)))
        )
            panic("Invalid operands to relational operator");
    }
}

static void parse_arithmetic_operation(int level, int operation, Type *type) {
    Value *src1 = vtop;
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

    // Arrays are rvalues, everything else are lvalues
    v->is_lvalue = v->type->type != TYPE_ARRAY;

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
    if (dst_type->type == TYPE_FUNCTION) panic("Function type mismatch");

    int dst_is_function = is_pointer_to_function_type(dst_type);
    int src_is_function = is_pointer_to_function_type(src->type) || src->type->type == TYPE_FUNCTION;

    if (dst_is_function && src_is_function) {
        if (src->type->type == TYPE_FUNCTION && src->global_symbol) {
            // Add instruction to load a global function into a register
            Value *src2 = new_value();
            src2->vreg = new_vreg();
            src2->type = make_pointer(dup_type(src->type));
            add_instruction(IR_ADDRESS_OF, src2, src, 0);

            return src2;
        }

        return src;
    }

    if (!type_eq(dst_type, src->type)) {
        if (src->is_constant) {
            if (dst_is_function && !is_null_pointer(src)) panic("Function type mismatch");
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

        // Implicit else: src is not a constant
        else if (dst_is_function && !src_is_function)
            panic("Function type mismatch");

        // Convert non constant
        Value *src2 = new_value();
        src2->vreg = new_vreg();
        src2->type = dup_type(dst_type);
        add_instruction(IR_MOVE, src2, src, 0);
        return src2;
    }

    return src;
}

static void parse_assignment(int enforce_const) {
    next();
    if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");
    if (enforce_const && !type_is_modifiable(vtop->type)) panic("Cannot assign to read-only variable");

    Value *dst = pop();
    parse_expression(TOK_EQ);
    Value *src1 = pl();

    dst->is_lvalue = 1;

    src1 = add_convert_type_if_needed(src1, dst->type);

    if (dst->bit_field_size)
        add_instruction(IR_SAVE_BIT_FIELD, dst, src1, 0);
    else
        add_instruction(IR_MOVE, dst, src1, 0);

    push(dst);
}

// Prepare compound assignment
Value *prep_comp_assign() {
    next();

    if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");
    if (!type_is_modifiable(vtop->type)) panic("Cannot assign to read-only variable");

    Value *v1 = vtop;           // lvalue
    push(load(dup_value(v1)));  // rvalue
    return v1;
}

// Finish compound assignment
static void finish_comp_assign(Value *v1) {
    add_instruction(IR_MOVE, v1, vtop, 0);
}

static void parse_addition(int level) {
    if (vtop->type->type == TYPE_ARRAY) push(decay_array_value(pl()));
    int src1_is_pointer = is_pointer_to_object_type(vtop->type);
    int src1_is_integer = is_integer_type(vtop->type);
    int src1_is_arithmetic = is_arithmetic_type(vtop->type);
    parse_expression(level);
    if (vtop->type->type == TYPE_ARRAY) push(decay_array_value(pl()));
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

static void parse_subtraction(int level) {
    Value *src1 = vtop;
    int src1_is_pointer = is_pointer_to_object_type(vtop->type);
    int src1_is_arithmetic = is_arithmetic_type(vtop->type);

    int factor = get_type_inc_dec_size(vtop->type);

    parse_expression(level);
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
        (!(src1_is_pointer && src2_is_pointer && types_are_compabible(deref_pointer(src1->type), deref_pointer(src2->type)))) &&
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

static void parse_bitwise_shift(int level, int operation) {
    if (!is_integer_type(vtop->type)) panic("Invalid operands to bitwise shift");
    Value *src1 = integer_promote(pl());
    parse_expression(level);
    if (!is_integer_type(vtop->type)) panic("Invalid operands to bitwise shift");
    Value *src2 = integer_promote(pl());
    add_ir_op(operation, src1->type, new_vreg(), src1, src2);
}

// Parse a declaration
static void parse_declaration() {
    Symbol *symbol;

    cur_type_identifier = 0;
    Type *type = concat_types(parse_declarator(), dup_type(base_type));

    if (!cur_type_identifier) panic("Expected an identifier");

    if (lookup_symbol(cur_type_identifier, cur_scope, 0)) panic1s("Identifier redeclared: %s", cur_type_identifier);

    symbol = new_symbol();
    symbol->type = dup_type(type);
    symbol->identifier = cur_type_identifier;
    symbol->local_index = new_local_index();

    if (symbol->type->type == TYPE_STRUCT_OR_UNION) {
        if (symbol->type->struct_or_union_desc->is_incomplete)
            panic("Attempt to use an incomplete struct or union");

        push_local_symbol(symbol);
        add_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, pop(), 0);
    }


    if (symbol->type->type == TYPE_ARRAY) {
        push_local_symbol(symbol);
        add_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, pop(), 0);
    }

    if (cur_token == TOK_EQ) {
        push_local_symbol(symbol);
        Type *old_base_type = base_type;
        base_type = 0;
        parse_assignment(0);
        base_type = old_base_type;
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

static void parse_function_call() {
    Value *popped_function = pl();
    if (popped_function->type->type != TYPE_FUNCTION && !is_pointer_to_function_type(popped_function->type))
        panic("Illegal attempt to call a non-function");

    Symbol *symbol = popped_function->global_symbol;
    Type *function_type = popped_function->type->function ? popped_function->type : popped_function->type->target;
    Function *function = function_type->function;

    next();

    int function_call = function_call_count++;
    Value *src1 = make_function_call_value(function_call);
    add_instruction(IR_START_CALL, 0, src1, 0);
    FunctionParamAllocation *fpa = init_function_param_allocaton(symbol ? symbol->identifier : "(anonymous)");

    while (1) {
        if (cur_token == TOK_RPAREN) break;
        parse_expression(TOK_EQ);
        Value *arg = dup_value(src1);
        int arg_count = fpa->arg_count;
        arg->function_call_arg_index = arg_count;

        if (vtop->type->type == TYPE_ARRAY) push(decay_array_value(pl()));

        // Convert type if needed
        if (arg_count < function->param_count) {
            if (!type_eq(vtop->type, function->param_types[arg_count])) {
                Type *param_type = function->param_types[arg_count];

                if (param_type->type == TYPE_ARRAY)
                    param_type = decay_array_to_pointer(param_type);

                push(add_convert_type_if_needed(pl(), param_type));
            }
        }
        else {
            // Apply default argument promotions & decay arrays
            Value *src1 = pl();
            Type *type;
            if (src1->type->type < TYPE_INT) {
                type = new_type(TYPE_INT);
                if (src1->type->is_unsigned) type->is_unsigned = 1;
            }
            else if (src1->type->type == TYPE_FLOAT)
                type = new_type(TYPE_DOUBLE);
            else if (src1->type->type == TYPE_ARRAY)
                type = decay_array_to_pointer(src1->type);
            else
                type = src1->type;

            if (!type_eq(src1->type, type))
                push(add_convert_type_if_needed(src1, type));
            else
                push(src1);
        }

        add_function_param_to_allocation(fpa, vtop->type);
        FunctionParamLocations *fpl = &(fpa->params[arg_count]);
        arg->function_call_arg_locations = fpl;
        add_instruction(IR_ARG, 0, arg, pl());

        // If a stack adjustment needs to take place to align 16-byte data
        // such as long doubles and structs with long doubles, an
        // IR_ARG_STACK_PADDING is inserted.
        if (fpl->locations[0].stack_padding >= 8) add_instruction(IR_ARG_STACK_PADDING, 0, 0, 0);

        if (cur_token == TOK_RPAREN) break;
        consume(TOK_COMMA, ",");
        if (cur_token == TOK_RPAREN) panic("Expected expression");
    }
    consume(TOK_RPAREN, ")");

    finalize_function_param_allocation(fpa);
    src1->function_call_arg_stack_padding = fpa->padding;

    Value *function_value = new_value();
    function_value->int_value = function_call;
    function_value->function_symbol = symbol;
    function_value->global_symbol = symbol;
    function_value->type = function_type;
    function_value->local_index = popped_function->local_index;
    function_value->vreg = popped_function->vreg;
    function_value->function_call_arg_push_count = (fpa->size + 7) / 8;
    function_value->function_call_sse_register_arg_count = fpa->single_sse_register_arg_count;

    // LIVE_RANGE_PREG_XMM01_INDEX is the max set value
    function_value->return_value_live_ranges = new_set(LIVE_RANGE_PREG_XMM01_INDEX);

    src1->function_call_arg_push_count = function_value->function_call_arg_push_count;

    Type *return_type = function_type->target;
    Value *return_value = 0;
    if (return_type->type == TYPE_STRUCT_OR_UNION) {
        return_value = new_value();
        return_value->local_index = new_local_index();
        return_value->is_lvalue = 1;
        return_value->type = dup_type(return_type);

        // Declare the temp so that the stack allocation code knows the
        // size of the struct
        add_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, return_value, 0);
    }
    else if (return_type->type != TYPE_VOID) {
        return_value = new_value();
        return_value->vreg = new_vreg();
        return_value->type = dup_type(return_type);
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

// Parse an expression using top-down precedence climbing parsing
// https://en.cppreference.com/w/c/language/operator_precedence
// https://en.wikipedia.org/wiki/Operator-precedence_parser#Precedence_climbing_method
static void parse_expression(int level) {
    // Parse any tokens that can be at the start of an expression
    if (cur_token_is_type()) {
        base_type = parse_type_specifier();
        parse_expression(TOK_COMMA);
        base_type = 0;
    }

    else if (cur_token == TOK_TYPEDEF)
        parse_typedef();

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

        // Arrays are rvalues as well as lvalues. Otherwise, an lvalue is required
        if ((vtop->type->type == TYPE_ARRAY && vtop->is_lvalue) && !vtop->is_lvalue)
            panic("Cannot take an address of an rvalue");

        if (vtop->bit_field_size) panic("Cannot take an address of a bit-field");

        Value *src1 = pop();
        add_ir_op(IR_ADDRESS_OF, make_pointer(src1->type), new_vreg(), src1, 0);
        if (src1->offset && src1->vreg) {
            // Non-vreg offsets are outputted by codegen. For addresses in registers, it
            // needs to be calculated.
            push_integral_constant(TYPE_INT, src1->offset);
            arithmetic_operation(IR_ADD, 0);
            src1->offset = 0;
        }
    }

    else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
        // Prefix increment & decrement

        int org_token = cur_token;
        next();
        parse_expression(TOK_DOT);

        if (!vtop->is_lvalue) panic("Cannot ++ or -- an rvalue");
        if (!type_is_modifiable(vtop->type)) panic("Cannot assign to read-only variable");

        Value *v1 = pop();                 // lvalue
        Value *src1 = load(dup_value(v1)); // rvalue
        push(src1);
        push_value_size_constant(src1);
        arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
        add_instruction(IR_MOVE, v1, vtop, 0);
        push(v1); // Push the original lvalue back on the value stack
    }

    else if (cur_token == TOK_MULTIPLY) {
        if (base_type)
            parse_declaration();
        else {
            next();
            parse_expression(TOK_INC);

            // Special case: indirects on function types are no-ops.
            if (vtop->type->type != TYPE_FUNCTION)  {
                if (!is_pointer_or_array_type(vtop->type)) panic("Cannot dereference a non-pointer");
                indirect();
            }
        }
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
        if (base_type)
            parse_declaration();

        else {
            next();
            if (cur_token_is_type()) {
                // Cast
                Type *org_type = parse_type_name();
                consume(TOK_RPAREN, ")");
                parse_expression(TOK_INC);

                Value *v1 = pl();
                // Special case for (void *) int-constant

                if (is_pointer_to_void(org_type) && (is_integer_type(v1->type) || is_pointer_to_void(v1->type)) && v1->is_constant) {
                    Value *dst = new_value();
                    dst->is_constant =1;
                    dst->int_value = v1->int_value;
                    dst->type = make_pointer_to_void();
                    push(dst);
                }
                else if (v1->type != org_type) {
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
        int size = strlen(cur_string_literal) + 1;

        Value *dst = new_value();
        dst->type = make_array(new_type(TYPE_CHAR), size);
        dst->string_literal_index = string_literal_count;
        dst->is_string_literal = 1;
        if (string_literal_count > MAX_STRING_LITERALS) panic1d("Exceeded max string literals %d", MAX_STRING_LITERALS);
        string_literals[string_literal_count++] = cur_string_literal;

        push(dst);
        next();
    }

    else if (cur_token == TOK_IDENTIFIER) {
        if (base_type)
            parse_declaration();

        else {
            // It's an existing symbol
            Symbol *symbol = lookup_symbol(cur_identifier, cur_scope, 1);
            if (!symbol) panic1s("Unknown symbol \"%s\"", cur_identifier);

            next();
            Type *type = dup_type(symbol->type);;
            Scope *scope = symbol->scope;

            if (symbol->is_enum_value)
                push_integral_constant(TYPE_INT, symbol->value);

            else if (scope->parent == 0) {
                // Global symbol
                Value *src1 = new_value();
                src1->type = dup_type(type);

                // Functions are rvalues, everything else is an lvalue
                src1->is_lvalue = (type->type != TYPE_FUNCTION);

                src1->global_symbol = symbol;
                push(src1);
            }

            else
                push_local_symbol(symbol);
        }
    }

    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN, "(");
        Type *type;
        if (cur_token_is_type())
            type = parse_type_name();
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

            if (vtop->type->type != TYPE_PTR && vtop->type->type != TYPE_ARRAY)
                panic("Invalid operator [] on a non-pointer and non-array");

            parse_addition(TOK_COMMA);
            consume(TOK_RBRACKET, "]");
            indirect();
        }

        else if (cur_token == TOK_LPAREN)
            // Function call
            parse_function_call();

        else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
            // Postfix increment & decrement

            int org_token = cur_token;
            next();

            if (!vtop->is_lvalue) panic("Cannot ++ or -- an rvalue");
            if (!is_scalar_type(vtop->type)) panic("Cannot postfix ++ or -- on a non scalar");
            if (!type_is_modifiable(vtop->type)) panic("Cannot assign to read-only variable");

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
            // Struct/union member lookup

            if (cur_token == TOK_DOT) {
                if (vtop->type->type != TYPE_STRUCT_OR_UNION) panic("Can only use . on a struct or union");
                if (!vtop->is_lvalue) panic("Expected lvalue for struct . operation.");
            }
            else {
                if (vtop->type->type != TYPE_PTR) panic("Cannot use -> on a non-pointer");
                if (vtop->type->target->type != TYPE_STRUCT_OR_UNION) panic("Can only use -> on a pointer to a struct or union");
            }

            int is_dot = cur_token == TOK_DOT;

            next();
            consume(TOK_IDENTIFIER, "identifier");

            Type *str_type = is_dot ? vtop->type : vtop->type->target;
            StructOrUnionMember *member = lookup_struct_or_union_member(str_type, cur_identifier);

            if (!type_is_modifiable(str_type)) {
                member->type = dup_type(member->type);
                member->type->is_const = 1;
            }

            if (!is_dot) indirect();

            vtop->offset = vtop->offset + member->offset;
            vtop->bit_field_offset = vtop->offset * 8 + (member->bit_field_offset & 7);
            vtop->bit_field_size = member->bit_field_size;
            vtop->type = dup_type(member->type);
            vtop->is_lvalue = 1;
        }

        else if (cur_token == TOK_MULTIPLY) { next(); parse_arithmetic_operation(TOK_DOT, IR_MUL, 0); }
        else if (cur_token == TOK_DIVIDE)   { next(); parse_arithmetic_operation(TOK_INC, IR_DIV, 0); }
        else if (cur_token == TOK_MOD)      { next(); parse_arithmetic_operation(TOK_INC, IR_MOD, 0); }

        else if (cur_token == TOK_PLUS) {
            next();
            parse_addition(TOK_MULTIPLY);
        }

        else if (cur_token == TOK_MINUS) {
            next();
            parse_subtraction(TOK_MULTIPLY);
        }

        else if (cur_token == TOK_BITWISE_RIGHT) {
            next();
            parse_bitwise_shift(level, IR_BSHR);
        }

        else if (cur_token == TOK_BITWISE_LEFT) {
            next();
            parse_bitwise_shift(level, IR_BSHL);
        }

        else if (cur_token == TOK_LT)            { next(); parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LT,   new_type(TYPE_INT)); }
        else if (cur_token == TOK_GT)            { next(); parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GT,   new_type(TYPE_INT)); }
        else if (cur_token == TOK_LE)            { next(); parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LE,   new_type(TYPE_INT)); }
        else if (cur_token == TOK_GE)            { next(); parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GE,   new_type(TYPE_INT)); }
        else if (cur_token == TOK_DBL_EQ)        { next(); parse_arithmetic_operation(TOK_LT,           IR_EQ,   new_type(TYPE_INT)); }
        else if (cur_token == TOK_NOT_EQ)        { next(); parse_arithmetic_operation(TOK_LT,           IR_NE,   new_type(TYPE_INT)); }
        else if (cur_token == TOK_ADDRESS_OF)    { next(); parse_arithmetic_operation(TOK_DBL_EQ,       IR_BAND, 0); }
        else if (cur_token == TOK_XOR)           { next(); parse_arithmetic_operation(TOK_ADDRESS_OF,   IR_XOR,  0); }
        else if (cur_token == TOK_BITWISE_OR)    { next(); parse_arithmetic_operation(TOK_XOR,          IR_BOR,  0); }

        else if (cur_token == TOK_AND)           and_or_expr(1);
        else if (cur_token == TOK_OR)            and_or_expr(0);

        else if (cur_token == TOK_TERNARY) {
            next();

            if (!is_scalar_type(vtop->type)) panic("Expected scalar type for first operand of ternary operator");

            // Destination register
            Value *dst = new_value();
            dst->vreg = new_vreg();

            Value *ldst1 = new_label_dst(); // False case
            Value *ldst2 = new_label_dst(); // End
            add_conditional_jump(IR_JZ, ldst1);
            parse_expression(TOK_TERNARY);
            Value *src1 = vtop;
            if (vtop->type->type != TYPE_VOID) add_instruction(IR_MOVE, dst, pl(), 0);
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of false case
            consume(TOK_COLON, ":");
            parse_expression(TOK_TERNARY);
            Value *src2 = vtop;

            // Decay arrays to pointers
            if (src1->type->type == TYPE_ARRAY) src1 = decay_array_value(src1);
            if (src2->type->type == TYPE_ARRAY) src2 = decay_array_value(src2);

            int src1_is_arithmetic = is_arithmetic_type(src1->type);
            int src2_is_arithmetic = is_arithmetic_type(src2->type);
            int src1_is_pointer = is_pointer_type(src1->type);
            int src2_is_pointer = is_pointer_type(src2->type);

            Type *src1_type_deref = 0;
            Type *src2_type_deref = 0;

            if (src1_is_pointer) src1_type_deref = deref_pointer(src1->type);
            if (src2_is_pointer) src2_type_deref = deref_pointer(src2->type);

            // One of the following shall hold for the second and third operands:
            // * both operands have arithmetic type;
            // * both operands have compatible structure or union types; TODO
            // * both operands have void type;
            // * both operands are pointers to qualified or unqualified versions of compatible types;
            // * one operand is a pointer and the other is a null pointer constant; or
            // * one operand is a pointer to an object or incomplete type and the other is a pointer to a qualified or unqualified version of void .

            if (
                (!((src1_is_arithmetic) && (src2_is_arithmetic))) &&
                (!(src1->type->type == TYPE_VOID && src2->type->type == TYPE_VOID)) &&
                (!(src1_is_pointer && src2_is_pointer && types_are_compabible(src1_type_deref, src2_type_deref))) &&
                (!(src1_is_pointer && is_null_pointer(src2))) &&
                (!(src2_is_pointer && is_null_pointer(src1))) &&
                (!((is_pointer_to_object_type(src1->type) && is_pointer_to_void(src2->type)) ||
                   (is_pointer_to_object_type(src2->type) && is_pointer_to_void(src1->type))))
            )
                panic("Invalid operands to ternary operator");

            dst->type = operation_type(src1, src2, 1);
            if (vtop->type->type != TYPE_VOID) add_instruction(IR_MOVE, dst, pl(), 0);
            push(dst);
            add_jmp_target_instruction(ldst2); // End
        }

        else if (cur_token == TOK_EQ) parse_assignment(1);

        // Composite assignments
        else if (cur_token == TOK_PLUS_EQ)          { Value *v = prep_comp_assign(); parse_addition(TOK_EQ);                         finish_comp_assign(v); }
        else if (cur_token == TOK_MINUS_EQ)         { Value *v = prep_comp_assign(); parse_subtraction(TOK_EQ);                      finish_comp_assign(v); }
        else if (cur_token == TOK_MULTIPLY_EQ)      { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_MUL,  0); finish_comp_assign(v); }
        else if (cur_token == TOK_DIVIDE_EQ)        { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_DIV,  0); finish_comp_assign(v); }
        else if (cur_token == TOK_MOD_EQ)           { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_MOD,  0); finish_comp_assign(v); }
        else if (cur_token == TOK_BITWISE_AND_EQ)   { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_BAND, 0); finish_comp_assign(v); }
        else if (cur_token == TOK_BITWISE_OR_EQ)    { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_BOR,  0); finish_comp_assign(v); }
        else if (cur_token == TOK_BITWISE_XOR_EQ)   { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_XOR,  0); finish_comp_assign(v); }
        else if (cur_token == TOK_BITWISE_RIGHT_EQ) { Value *v = prep_comp_assign(); parse_bitwise_shift(TOK_EQ, IR_BSHR);           finish_comp_assign(v); }
        else if (cur_token == TOK_BITWISE_LEFT_EQ)  { Value *v = prep_comp_assign(); parse_bitwise_shift(TOK_EQ, IR_BSHL);           finish_comp_assign(v); }

        else if (cur_token == TOK_COMMA) {
            // Replace the outcome from the previous expression on the stack with
            // void and process the next expression.

            pop_void();
            Value *v = new_value();
            v->type = new_type(TYPE_VOID);
            v->vreg = new_vreg();
            push(v);
            next();
            parse_expression(TOK_COMMA);
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

static void parse_compound_statement() {
    consume(TOK_LCURLY, "{");
    while (cur_token != TOK_RCURLY) parse_statement();
    consume(TOK_RCURLY, "}");
}

// Parse a statement
static void parse_statement() {
    vs = vs_start; // Reset value stack
    base_type = 0; // Reset base type

    if (cur_token == TOK_HASH) {
        parse_directive();
        return;
    }

    if (cur_token_is_type()) {
        base_type = parse_type_specifier();
        if (cur_token == TOK_SEMI && (base_type->type == TYPE_STRUCT_OR_UNION || base_type->type == TYPE_ENUM))
            next();
        else
            parse_expression(TOK_COMMA);
    }

    else if (cur_token == TOK_SEMI) {
        // Empty statement
        next();
        return;
    }

    else if (cur_token == TOK_LCURLY) {
        enter_scope();
        parse_compound_statement();
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
            if (vtop && vtop->type->type == TYPE_VOID && cur_function_symbol->type->function->return_type->type == TYPE_VOID) {
                // Deal with case of returning the result of a void function in a void function
                // e.g. foo(); void bar() { return foo(); }
                vs++; // Remove from value stack
                src1 = 0;
            }
            else
                src1 = add_convert_type_if_needed(pl(), cur_function_symbol->type->function->return_type);

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
    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)
            next();
        else if (cur_token == TOK_HASH)
            parse_directive();
        else if (cur_token == TOK_EXTERN || cur_token == TOK_STATIC || cur_token_is_type() ) {
            // Variable or function definition

            int is_external = cur_token == TOK_EXTERN;
            int is_static = cur_token == TOK_STATIC;
            if (is_external || is_static) next();

            Type *base_type = parse_type_specifier();

            while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
                cur_type_identifier = 0;

                if (base_type->type == TYPE_STRUCT_OR_UNION && base_type->struct_or_union_desc->is_incomplete)
                    panic("Attempt to use an incomplete struct or union");

                Type *type = concat_types(parse_declarator(), dup_type(base_type));

                if (!cur_type_identifier) panic("Expected an identifier");

                Symbol *s = lookup_symbol(cur_type_identifier, global_scope, 1);
                if (!s) {
                    // Create a new symbol if it wasn't already declared. The
                    // previous declaration is left unchanged.

                    s = new_symbol();
                    s->type = dup_type(type);
                    s->identifier = cur_type_identifier;
                }

                if ((s->type->type == TYPE_FUNCTION) != (type->type == TYPE_FUNCTION))
                    panic1s("%s redeclared as different kind of symbol", cur_type_identifier);


                if (type->type == TYPE_FUNCTION) {
                    // Function declaration or definition

                    // Setup the intermediate representation with a dummy no operation instruction.
                    ir_start = 0;
                    ir_start = add_instruction(IR_NOP, 0, 0, 0);

                    s->type->function->return_type = type->target;
                    s->type->function->ir = ir_start;
                    s->type->function->is_external = is_external;
                    s->type->function->is_static = is_static;
                    s->type->function->local_symbol_count = 0;

                    if (type->target->type == TYPE_STRUCT_OR_UNION) {
                        FunctionParamAllocation *fpa = init_function_param_allocaton(cur_type_identifier);
                        add_function_param_to_allocation(fpa, type->target);
                        s->type->function->return_value_fpa = fpa;
                    }

                    // type->function->scope is left entered by the type parser
                    cur_scope = type->function->scope;
                    cur_function_symbol = s;

                    // Parse function declaration
                    if (cur_token == TOK_LCURLY) {
                        // Reset globals for a new function
                        vreg_count = 0;
                        function_call_count = 0;
                        cur_loop = 0;
                        loop_count = 0;

                        parse_compound_statement();

                        cur_function_symbol->type->function->is_defined = 1;
                        cur_function_symbol->type->function->vreg_count = vreg_count;
                    }
                    else
                        // Make it clear that this symbol will need to be backpatched if used
                        // before the definition has been processed.
                        cur_function_symbol->value = 0;

                    exit_scope();

                    break; // Break out of function parameters loop
                }

                if (cur_token == TOK_COMMA) next();
            }

            if (cur_token == TOK_SEMI) next();
        }

        else if (cur_token == TOK_STRUCT || cur_token == TOK_UNION || cur_token == TOK_ENUM) {
            parse_type_specifier();
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

    all_structs_and_unions = malloc(sizeof(struct struct_or_union_desc *) * MAX_STRUCTS_AND_UNIONS);
    all_structs_and_unions_count = 0;

    all_typedefs = malloc(sizeof(struct typedef_desc *) * MAX_TYPEDEFS);
    all_typedefs_count = 0;

    vs_start = malloc(sizeof(struct value *) * VALUE_STACK_SIZE);
    vs_start += VALUE_STACK_SIZE; // The stack traditionally grows downwards
    label_count = 0;

    in_ifdef = 0;
    in_ifdef_else = 0;
}
