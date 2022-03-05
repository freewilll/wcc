#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "wcc.h"

#define INITIAL_INITALIZERS_COUNT 32

typedef struct base_type {
    Type *type;
    int storage_class;
} BaseType;

typedef struct goto_backpatch {
    char *identifier;
    Tac *ir;
} GotoBackPatch;

int function_call_count; // Uniquely identify a function call within a function

StructOrUnion **all_structs_and_unions;  // All structs/unions defined globally.
int all_structs_and_unions_count;        // Number of structs/unions, complete and incomplete
int vreg_count;                          // Virtual register count for currently parsed function
int local_static_symbol_count;           // Amount of static objects with block scope

Value *controlling_case_value;  // Controlling value for the current switch statement
LongMap *case_values;           // Already seen case value in current switch statement
Tac *case_ir_start;             // Start of IR for current switch case conditional & jump statements
Tac *case_ir;                   // IR for current switch case conditional & jump statements
Value *case_default_label;      // Label for curren't switch's statement default case, if present
int seen_switch_default;        // Set to 1 if a default label has been seen within the current switch statement

Value **vs_bottom;       // Allocated value stack
Value **vs_start;        // Value stack start
Value **vs;              // Value stack current position

static Type *parse_struct_or_union_type_specifier(void);
static Type *parse_enum_type_specifier(void);
static TypeIterator *parse_initializer(TypeIterator *it, Value *value, Value *expression);
void check_and_or_operation_type(Value *src1, Value *src2);
Value *parse_expression_and_pop(int level);
static void parse_statement(void);
static void parse_expression(int level);
static void parse_compound_statement(void);

static BaseType *base_type;

static int value_stack_is_empty(void) {
    return vs == vs_start;
}

static Value *vtop(void) {
    if (vs == vs_start) error("Missing expression");
    return *vs;
}

// Allocate a new virtual register
static int new_vreg(void) {
    vreg_count++;
    if (vreg_count >= MAX_VREG_COUNT) panic_with_line_number("Exceeded max vreg count %d", MAX_VREG_COUNT);
    return vreg_count;
}

// Add an instruction and set the line number and filename
Tac *add_parser_instruction(int operation, Value *dst, Value *src1, Value *src2) {
    Origin *origin = wmalloc(sizeof(Origin));
    origin->filename = cur_filename ? strdup(cur_filename) : NULL;
    origin->line_number = cur_line;
    Tac *tac = add_instruction(operation, dst, src1, src2);
    tac->origin = origin;
    return tac;
}

static Value *decay_array_value(Value *v) {
    Value *result = dup_value(v);
    result->type = decay_array_to_pointer(v->type);

    return result;
}

// Push a value to the stack
static Value *push(Value *v) {
    *--vs = v;
    return v;
}

static void check_stack_has_value(void) {
    if (vs == vs_start) panic_with_line_number("Empty parser stack");
    if (vtop()->type->type == TYPE_VOID) error("Illegal attempt to use a void value");
}

// Pop a value from the stack
static Value *pop(void) {
    check_stack_has_value();

    Value *result = *vs++;

    return result;
}

// Make a void value
static Value *make_void_value(void) {
    Value *v = new_value();
    v->type = new_type(TYPE_VOID);
    v->vreg = new_vreg();
    return v;
}

// Push a void value to the stack
static void push_void(void) {
    push(make_void_value());
}

// Pop a void value from the stack,unless the stack is empty
static void *pop_void(void) {
    if (vs == vs_start) return NULL;

    vs++;
}

static Value *load_bit_field(Value *src1) {
    Value *dst = new_value();
    dst->type = new_type(TYPE_INT);
    dst->type->is_unsigned = src1->type->is_unsigned;
    dst->vreg = new_vreg();
    add_parser_instruction(IR_LOAD_BIT_FIELD, dst, src1, 0);

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
            add_parser_instruction(IR_MOVE, dst, src1, 0);
        }
        else {
            // Take the address of an array
            dst->local_index = 0;
            dst->global_symbol = 0;
            dst->type = decay_array_to_pointer(dst->type);
            add_parser_instruction(IR_ADDRESS_OF, dst, src1, 0);

            // If it's a pointer in a register with an offset, the offset needs to be
            // added to it.
            if (src1->vreg && src1->offset) {
                Value *tmp = dup_value(dst);
                tmp->vreg = new_vreg();
                add_parser_instruction(IR_ADD, tmp, dst, new_integral_constant(TYPE_INT, src1->offset));
                dst = tmp;
                src1->offset = 0; // Ensure downstream code doesn't deal with the offset again.
            }
        }
    }

    else if (src1->vreg && src1->is_lvalue) {
        // An lvalue in a register needs a dereference
        if (src1->type->type == TYPE_VOID) error("Cannot dereference a *void");
        if (src1->type->type == TYPE_STRUCT_OR_UNION) error("Cannot dereference a pointer to a struct/union");
        if (src1->type->type == TYPE_ARRAY) error("Cannot dereference a pointer to an array");

        src1 = dup_value(src1);
        src1->type = make_pointer(src1->type);
        src1->is_lvalue = 0;
        src1->type->is_const = 0;
        add_parser_instruction(IR_INDIRECT, dst, src1, 0);
    }

    else {
        // Load a value into a register. This could be a global or a local.
        dst->local_index = 0;
        dst->global_symbol = 0;
        add_parser_instruction(IR_MOVE, dst, src1, 0);
    }

    return dst;
}

// Pop and load.
static Value *pl(void) {
    return load(pop());
}

static int new_local_index(void) {
    return -1 - cur_function_symbol->function->local_symbol_count++;
}

// Create a new typed constant value and push it to the stack.
// type doesn't have to be dupped
static void push_integral_constant(int type_type, long value) {
    push(new_integral_constant(type_type, value));
}

// Push cur_long, using the lexer determined type cur_lexer_type
static void push_cur_long(void) {
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
static void push_cur_long_double(void) {
    Value *v = new_floating_point_constant(cur_lexer_type->type, cur_long_double);
    push(v);
}

Value *make_string_literal_value_from_cur_string_literal(void) {
    Value *value = new_value();

    value->type = make_array(new_type(cur_string_literal.is_wide_char ? TYPE_INT : TYPE_CHAR), cur_string_literal.size);
    value->string_literal_index = string_literal_count;
    value->is_string_literal = 1;
    if (string_literal_count > MAX_STRING_LITERALS) panic_with_line_number("Exceeded max string literals %d", MAX_STRING_LITERALS);
    string_literals[string_literal_count] = cur_string_literal;
    string_literal_count++;

    return value;
}

// Add an operation to the IR
static Tac *add_ir_op(int operation, Type *type, int vreg, Value *src1, Value *src2) {
    Value *v = new_value();
    v->vreg = vreg;
    v->type = dup_type(type);
    Tac *result = add_parser_instruction(operation, v, src1, src2);
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
        return dup_type(src1_type);

    Type *result;

    if (src1_type->type == TYPE_STRUCT_OR_UNION || src2_type->type == TYPE_STRUCT_OR_UNION)
        // Compatibility should already have been checked for at this point
        return src1_type;

    // If it's a ternary and one is a pointer and the other a pointer to void, then the result is a pointer to void.
    else if (src1_type->type == TYPE_PTR && is_pointer_to_void(src2->type)) return for_ternary ? src2->type : src1->type;
    else if (src2_type->type == TYPE_PTR && is_pointer_to_void(src1->type)) return for_ternary ? src1->type : src2->type;
    else if (for_ternary && (src1_type->type == TYPE_PTR || src1_type->type == TYPE_FUNCTION) && is_null_pointer(src2)) return src1->type;
    else if (for_ternary && (src2_type->type == TYPE_PTR || src2_type->type == TYPE_FUNCTION) && is_null_pointer(src1)) return src2->type;
    else if (for_ternary && src1_type->type == TYPE_PTR && src2_type->type == TYPE_PTR) return ternary_pointer_composite_type(src1->type, src2->type);
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
static Type *vs_operation_type(void) {
    return operation_type(vtop(), vs[1], 0);
}

int cur_token_is_type(void) {
    return (
        cur_token == TOK_SIGNED ||
        cur_token == TOK_UNSIGNED ||
        cur_token == TOK_INLINE ||
        cur_token == TOK_CONST ||
        cur_token == TOK_VOLATILE ||
        cur_token == TOK_RESTRICT ||
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
        cur_token == TOK_AUTO ||
        cur_token == TOK_REGISTER ||
        cur_token == TOK_EXTERN ||
        cur_token == TOK_STATIC ||
        cur_token == TOK_TYPEDEF_TYPE ||
        cur_token == TOK_ATTRIBUTE
    );
}

// How much will the ++, --, +=, -= operators increment a type?
static int get_type_inc_dec_size(Type *type) {
    return type->type == TYPE_PTR ? get_type_size(type->target) : 1;
}

static BaseType *dup_base_type(BaseType *base_type) {
    BaseType *result = wmalloc(sizeof(BaseType));
    result->type = dup_type(base_type->type);
    result->storage_class = base_type->storage_class;
    return result;
}

// Parse optional repeated __attribute__ ((...)), ignoring everything inside the ((...))
void parse_attributes(void) {
    while (1) {
        if (cur_token != TOK_ATTRIBUTE) return;

        next();

        consume(TOK_LPAREN, "(");
        consume(TOK_LPAREN, "(");
        int parentheses_nesting_level = 2;

        // Keep parsing until we break out of the ((
        while (cur_token != TOK_EOF) {
            if (cur_token == TOK_LPAREN) parentheses_nesting_level++;
            else if (cur_token == TOK_RPAREN) parentheses_nesting_level--;

            next();

            if (!parentheses_nesting_level) break;
        }
    }
}

// Parse
// - storage-class-specifiers: auto, register, static, extern
// - type-specifiers: void, int, signed, unsigned, ... long, double, struct, union
// - type-qualifiers: const, volatile
// - typedef type names
static BaseType *parse_declaration_specifiers() {
    Type *type = 0;

    // Type specifiers
    int seen_void = 0;
    int seen_char = 0;
    int seen_short = 0;
    int seen_int = 0;
    int seen_long = 0;
    int seen_long_double = 0;
    int seen_float = 0;
    int seen_double = 0;
    int seen_signed = 0;
    int seen_unsigned = 0;
    int seen_inline = 0;

    // Qualifiers
    int seen_const = 0;
    int seen_volatile = 0;
    int seen_restrict = 0;

    // Storage class specifiers
    int seen_auto = 0;
    int seen_register = 0;
    int seen_static = 0;
    int seen_extern = 0;

    int seen_typedef = 0;
    int seen_struct = 0;
    int seen_union = 0;
    int seen_enum = 0;

    int changed = 1;
    while (changed) {
        changed = 1;

        switch (cur_token) {
            case TOK_VOID:     next(); seen_void++;     type = new_type(TYPE_VOID); break;
            case TOK_CHAR:     next(); seen_char++;     type = new_type(TYPE_CHAR); break;
            case TOK_INT:      next(); seen_int++;      type = new_type(TYPE_INT); break;
            case TOK_FLOAT:    next(); seen_float++;    type = new_type(TYPE_FLOAT); break;
            case TOK_DOUBLE:   next(); seen_double++;   type = new_type(TYPE_DOUBLE); break;
            case TOK_SHORT:
                next();
                if (cur_token == TOK_INT) next();
                seen_short++;
                type = new_type(TYPE_SHORT);
                break;
            case TOK_LONG:
                next();
                if (cur_token == TOK_DOUBLE)   {
                    next();
                    seen_long_double++;
                    type = new_type(TYPE_LONG_DOUBLE);
                }
                else {
                    type = new_type(TYPE_LONG);
                    seen_long++;
                }
                break;
            case TOK_STRUCT:
                seen_struct++;
                type = dup_type(parse_struct_or_union_type_specifier());
                break;
            case TOK_UNION:
                seen_union++;
                type = dup_type(parse_struct_or_union_type_specifier());
                break;
            case TOK_ENUM:
                seen_enum++;
                type = dup_type(parse_enum_type_specifier());
                break;
            case TOK_TYPEDEF_TYPE:
                // If a typedef type has been encountered by the lexer and a type already
                // exists, then it's not a typedef type, but a redefinition of a typedef
                // type.
                if (!type) {
                    seen_typedef++;
                    type = dup_type(cur_lexer_type);
                    next();
                }
                else
                    changed = 0;

                break;

            case TOK_SIGNED:   next(); seen_signed++; break;
            case TOK_UNSIGNED: next(); seen_unsigned++; break;
            case TOK_INLINE:   next(); seen_inline++; break;
            case TOK_CONST:    next(); seen_const++; break;
            case TOK_VOLATILE: next(); seen_volatile++; break;
            case TOK_RESTRICT: next(); seen_restrict++; break;
            case TOK_AUTO:     next(); seen_auto++; break;
            case TOK_REGISTER: next(); seen_register++; break;
            case TOK_STATIC:   next(); seen_static++; break;
            case TOK_EXTERN:   next(); seen_extern++; break;

            case TOK_ATTRIBUTE: parse_attributes(); break;

            default: changed = 0;
        }
    }

    if (seen_long == 2) seen_long = 1;
    else if (seen_long > 2) error("Too many longs in type specifier");
    if (seen_int && seen_long) {
        seen_int = 0;
        type = new_type(TYPE_LONG);
    }

    int data_type_sum =
        seen_void + seen_char + seen_short + seen_int + seen_long +
        seen_float + seen_double + seen_long_double +
        seen_struct + seen_union + seen_enum;

    if ((data_type_sum == 0) && !seen_typedef)
        // Implicit int
        type = new_type(TYPE_INT);

    if (!type) panic_with_line_number("Type is null");

    if ((data_type_sum > 1))
        error("Two or more data types in declaration specifiers");

    if (seen_signed && seen_unsigned)
        error("Both ‘signed’ and ‘unsigned’ in declaration specifiers");

    int is_integer_type = type && (type->type == TYPE_CHAR || type->type == TYPE_SHORT || type->type == TYPE_INT || type->type == TYPE_LONG);
    if (!is_integer_type && (seen_signed || seen_unsigned))
        error("Signed/unsigned can only apply to integer types");

    if ((seen_auto + seen_register + seen_static + seen_extern > 1))
        error("Duplicate storage class specifiers");

    BaseType *base_type = wmalloc(sizeof(BaseType));
    base_type->type = type;
    base_type->storage_class = SC_NONE;

    if (seen_extern) base_type->storage_class = SC_EXTERN;
    if (seen_static) base_type->storage_class = SC_STATIC;
    if (seen_auto) base_type->storage_class = SC_AUTO;
    if (seen_register) base_type->storage_class = SC_REGISTER;

    if (seen_restrict) type->is_restrict = 1;

    if (seen_const) type->is_const = 1;
    if (seen_volatile) type->is_volatile = 1;

    if (seen_unsigned) type->is_unsigned = 1;

    return base_type;
}

// If an array type is declared with the const type qualifier (through the use of
// typedef), the array type is not const-qualified, but its element type is.
static Type *move_array_const(Type *type) {
    Type *result = type;

    while (type && type->type == TYPE_ARRAY && type->is_const) {
        type->is_const = 0;
        type->target->is_const = 1;
        type = type->target;
    }

    return result;
}

static Type *concat_types(Type *type1, Type *type2) {
    if (type1 == 0) return move_array_const(type2);
    else if (type2 == 0) return move_array_const(type1);
    else if (type1 == 0 && type2 == 0) panic_with_line_number("concat type got two null types");

    Type *type1_tail = type1;
    while (type1_tail->target) type1_tail = type1_tail->target;
    type1_tail->target = type2;

    if (type1_tail->type == TYPE_FUNCTION && type2->type == TYPE_ARRAY)
        error("Functions cannot return arrays");

    // Check arrays have complete elements
    if (type1_tail->type == TYPE_ARRAY) {
        if (type2->type == TYPE_STRUCT_OR_UNION && !get_type_size(type2))
            error("Array has incomplete element type");
    }

    return move_array_const(type1);
}

static Type *concat_base_type(Type *type, BaseType *base_type) {
    Type *result = concat_types(type, base_type->type);
    result->storage_class = base_type->storage_class;
    return result;
}

Type *parse_direct_declarator(void);

Type *parse_declarator(void) {
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
            while (cur_token == TOK_CONST || cur_token == TOK_VOLATILE || cur_token == TOK_RESTRICT) {
                if (cur_token == TOK_CONST) type->is_const = 1;
                else type->is_volatile = 1;
                next();
            }
        }
        else
            return type;
    }
}

static Type *parse_function_type(void) {
    Type *function_type = new_type(TYPE_FUNCTION);
    function_type->function = wcalloc(1, sizeof(FunctionType));
    function_type->function->param_types = new_list(8);
    function_type->function->param_identifiers = new_list(8);

    enter_scope();
    function_type->function->scope = cur_scope;
    function_type->function->is_paramless = 1;

    int param_count = 0;
    while (1) {
        if (cur_token == TOK_RPAREN) break;

        int is_type_token = cur_token_is_type();

        if (cur_token == TOK_IDENTIFIER || is_type_token) {
            char *old_cur_type_identifier = cur_type_identifier;
            Type *type;

            if (is_type_token) {
                cur_type_identifier = 0;
                type = parse_type_name();
                function_type->function->is_paramless = 0;

                if (type->storage_class == SC_AUTO || type->storage_class == SC_STATIC || type->storage_class == SC_EXTERN)
                    error("Invalid storage for function parameter");
            }
            else {
                type = new_type(TYPE_INT);
                cur_type_identifier = cur_identifier;
                next();
            }

            if (type->type == TYPE_VOID) {
                cur_type_identifier = old_cur_type_identifier;
                break;
            }

            Symbol *param_symbol = 0;
            if (cur_type_identifier) param_symbol = new_symbol(cur_type_identifier);

            Type *symbol_type;

            // Convert parameter types
            if (type->type == TYPE_ARRAY)
                symbol_type = decay_array_to_pointer(type);
            else if (type->type == TYPE_FUNCTION) {
                symbol_type = make_pointer(type);
                type = symbol_type;
            }
            else if (type->type == TYPE_ENUM)
                symbol_type = new_type(TYPE_INT);
            else
                symbol_type = type;

            if (param_symbol) param_symbol->type = dup_type(symbol_type);

            append_to_list(function_type->function->param_types, dup_type(type));
            append_to_list(function_type->function->param_identifiers, cur_type_identifier);
            if (param_symbol) param_symbol->local_index = param_count;
            param_count++;

            cur_type_identifier = old_cur_type_identifier;
        }
        else if (cur_token == TOK_ELLIPSES) {
            function_type->function->is_variadic = 1;
            next();
        }
        else
            error("Expected type or )");

        if (cur_token == TOK_RPAREN) break;
        consume(TOK_COMMA, ",");
        if (cur_token == TOK_RPAREN) error("Expected expression");
    }

    function_type->function->param_count = param_count;

    exit_scope();

    consume(TOK_RPAREN, ")");

    return function_type;
}

// Parse old style K&R function declaration list,
static void parse_function_paramless_declaration_list(Function *function) {
    while (cur_token != TOK_LCURLY) {
        BaseType *base_type = parse_declaration_specifiers();
        while (cur_token != TOK_SEMI) {
            cur_type_identifier = 0;
            Type *type = concat_base_type(parse_declarator(), base_type);

            // Array parameters decay to a pointer
            if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
            if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

            // Associate type with param symbol
            if (!cur_type_identifier) error("Expected identifier");
            Symbol *symbol = lookup_symbol(cur_type_identifier, cur_scope, 0);
            if (!symbol)  error("Declaration for unknown parameter %s", cur_type_identifier);
            symbol->type = type;

            int found_identifier = 0;
            for (int i = 0; i < function->type->function->param_count; i++) {
                if (!strcmp(function->type->function->param_identifiers->elements[i], cur_type_identifier)) {
                    function->type->function->param_types->elements[i] = type;
                    found_identifier = 1;
                    break;
                }
            }
            if (!found_identifier) panic_with_line_number("unable to match function param identifier");

            if (cur_token != TOK_COMMA && cur_token != TOK_SEMI) error("Expected a ; or ,");
            if (cur_token == TOK_COMMA) next();
        }
        while (cur_token == TOK_SEMI) consume(TOK_SEMI, ";");
    }
}

Type *parse_direct_declarator(void) {
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

    else if (cur_token == TOK_LPAREN) {
        // (subtype)
        next();
        type = concat_types(type, parse_declarator());
        consume(TOK_RPAREN, ")");
    }

    while (1) {
        if (cur_token == TOK_LPAREN) {
            // Function
            next();
            type = concat_types(type, parse_function_type());
        }
        else if (cur_token == TOK_LBRACKET) {
            // Array [] or [<num>]
            next();

            int size = 0;
            if (cur_token != TOK_RBRACKET) {
                Value *v = parse_constant_integer_expression(0);
                size = v->int_value;
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

Type *parse_type_name(void) {
    BaseType *base_type = parse_declaration_specifiers();
    base_type->type->storage_class = base_type->storage_class;
    return concat_types(parse_declarator(), base_type->type);
}

// Allocate a new StructOrUnion
static Type *new_struct_or_union(char *tag_identifier) {
    StructOrUnion *s = wcalloc(1, sizeof(StructOrUnion));
    if (tag_identifier != 0) all_structs_and_unions[all_structs_and_unions_count++] = s;
    s->members = wcalloc(MAX_STRUCT_MEMBERS, sizeof(StructOrUnionMember *));

    Type *type = make_struct_or_union_type(s);

    if (tag_identifier) {
        Tag *tag = new_tag(tag_identifier);
        tag->type = type;
        type->tag = tag;
    }
    else
        type->tag = 0;

    return type;
}

// Search for a struct tag. Returns 0 if not found.
Type *find_struct_or_union(char *identifier, int is_union, int recurse) {
    Tag *tag = lookup_tag(identifier, cur_scope, recurse);

    if (!tag) return 0;

    if (tag->type->type == TYPE_STRUCT_OR_UNION) {
        if (tag->type->struct_or_union_desc->is_union != is_union)
            error("Tag %s is the wrong kind of tag", identifier);
        return tag->type;
    }
    else
        error("Tag %s is the wrong kind of tag", identifier);
}

static Type *find_enum(char *identifier) {
    Tag *tag = lookup_tag(identifier, cur_scope, 1);

    if (!tag) return 0;
    if (tag->type->type != TYPE_ENUM) error("Tag %s is the wrong kind of tag", identifier);

    return tag->type;
}

// Recursively add a struct member. In the simplest case, it just gets added. For anonymous
// structs/unions, the function is called recursively with the sub members.
static StructOrUnionMember *add_struct_member(char *identifier, Type *type, StructOrUnion *s, int *member_count) {
    StructOrUnionMember *member = wcalloc(1, sizeof(StructOrUnionMember));
    member->identifier = identifier;
    member->type = dup_type(type);

    if (*member_count == MAX_STRUCT_MEMBERS)
        panic_with_line_number("Exceeded max struct/union members %d", MAX_STRUCT_MEMBERS);

    s->members[(*member_count)++] = member;

    return member;
}

// Parse an optional __attribute(packed|aligned). Returns is_packed True/False
static int parse_struct_or_union_attribute(void) {
    // Check for optional packed & aligned attributes

    int is_packed = 0;

    if (cur_token == TOK_ATTRIBUTE) {
        consume(TOK_ATTRIBUTE, "attribute");
        consume(TOK_LPAREN, "(");
        consume(TOK_LPAREN, "(");

        if (cur_token == TOK_PACKED) {
            is_packed = 1;
            next();
        }
        else if (cur_token == TOK_ALIGNED) {
            // Ignore aligned and __aligned__
            next();
        }

        consume(TOK_RPAREN, ")");
        consume(TOK_RPAREN, ")");
        is_packed = 1;
    }

    return is_packed;
}

// Parse struct definitions and uses.
static Type *parse_struct_or_union_type_specifier(void) {
    // Parse a struct or union

    int is_union = cur_token == TOK_UNION;
    next();

    // Parse __attribute__ that might precede the definition
    int is_packed1 = parse_struct_or_union_attribute();

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
        if (identifier) type = find_struct_or_union(identifier, is_union, 0);
        if (!type) type = new_struct_or_union(identifier);

        StructOrUnion *s = type->struct_or_union_desc;
        s->is_union = is_union;

        // Loop over members
        int member_count = 0;
        while (cur_token != TOK_RCURLY) {
            BaseType *base_type = parse_declaration_specifiers();

            int done_parsing_members = 0;
            while (!done_parsing_members) {
                Type *type;

                int unnamed_bit_field = 0;
                if (cur_token != TOK_COLON) {
                    cur_type_identifier = 0;
                    type = concat_base_type(parse_declarator(), base_type);
                }

                else {
                    // Unnamed bit field
                    next();
                    cur_type_identifier = 0;
                    type = new_type(TYPE_INT);
                    unnamed_bit_field = 1;
                }

                // GCC Arrays of Length Zero extension
                // https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html
                int is_zero_length_array = type->type == TYPE_ARRAY && type->array_size == 0;

                if (!is_zero_length_array && is_incomplete_type(type)) error("Struct/union members cannot have an incomplete type");
                if (type->type == TYPE_FUNCTION) error("Struct/union members cannot have a function type");

                StructOrUnionMember *member = add_struct_member(cur_type_identifier, type, s, &member_count);

                if (member && unnamed_bit_field || cur_token == TOK_COLON) {
                    // Bit field
                    if (!unnamed_bit_field) next(); // consume TOK_COLON

                    if (type->type != TYPE_INT) error("Bit fields must be integers");

                    Value *v = parse_constant_integer_expression(0);
                    int bit_field_size = v->int_value;

                    if (cur_type_identifier && bit_field_size == 0) error("Invalid bit field size 0 for named member");
                    if (bit_field_size < 0 || bit_field_size > 32) error("Invalid bit field size %d", cur_long);

                    member->is_bit_field = 1;
                    member->bit_field_size = bit_field_size;
                }

                if (cur_token == TOK_COMMA) next();
                else if (cur_token == TOK_SEMI) {
                    next();
                    done_parsing_members = 1;
                }
                else error("Expected a ;, or ,");
            }
            if (cur_token == TOK_RCURLY) break;
        }
        consume(TOK_RCURLY, "}");

        // Parse __attribute__ that might come after the definition
        int is_packed2 = parse_struct_or_union_attribute();
        s->is_packed = is_packed1 || is_packed2;

        complete_struct_or_union(s);
        return type;
    }
    else {
        // Struct/union use

        Type *type = find_struct_or_union(identifier, is_union, 1);
        if (type) return type; // Found a complete or incomplete struct

        // Didn't find a struct, but that's ok, create a incomplete one
        // to be populated later when it's defined.
        type = new_struct_or_union(identifier);
        StructOrUnion *s = type->struct_or_union_desc;
        s->is_incomplete = 1;

        s->is_packed = is_packed1;

        s->is_union = is_union;
        return type;
    }
}

static Type *parse_enum_type_specifier(void) {
    next();

    Type *type = new_type(TYPE_ENUM);

    char *identifier = 0;

    // A typedef identifier be the same as a struct tag, in this context, the lexer
    // sees a typedef tag, but really it's a struct tag.
    if (cur_token == TOK_IDENTIFIER || cur_token == TOK_TYPEDEF_TYPE) {
        identifier = cur_identifier;

        Tag *tag = new_tag(cur_identifier);
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
            char *enum_value_identifier = cur_identifier;
            next();
            if (cur_token == TOK_EQ) {
                next();
                value = parse_constant_integer_expression(0)->int_value;
            }

            Symbol *s = new_symbol(enum_value_identifier);
            s->is_enum_value = 1;
            s->type = new_type(TYPE_INT);
            s->value = value++;
            s++;

            member_count++;

            if (cur_token != TOK_RCURLY) consume(TOK_COMMA, ",");
        }
        consume(TOK_RCURLY, "}");

        if (!member_count) error("An enum must have at least one member");
    }

    else {
        // Enum use

        Type *type = find_enum(identifier);
        if (!type) error("Unknown enum %s", identifier);
        return type;
    }

    return type;
}

void parse_typedef(void) {
    next();

    BaseType *base_type_with_storage_class = parse_declaration_specifiers(0);
    Type *base_type = base_type_with_storage_class->type;

    while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
        cur_type_identifier = 0;

        Type *type = concat_types(parse_declarator(), dup_type(base_type));

        parse_attributes();

        if (all_typedefs_count == MAX_TYPEDEFS) panic_with_line_number("Exceeded max typedefs");
        Typedef *td = wcalloc(1, sizeof(Typedef));
        td->identifier = cur_type_identifier;
        td->type = type;
        all_typedefs[all_typedefs_count++] = td;

        Symbol *symbol = new_symbol(cur_type_identifier);
        symbol->type = new_type(TYPE_TYPEDEF);
        symbol->type->target = type;

        if (cur_token != TOK_SEMI) consume(TOK_COMMA, ",");
        if (cur_token == TOK_SEMI) break;
    }
}

static void indirect(void) {
    // The stack contains an rvalue which is a pointer. All that needs doing
    // is conversion of the rvalue into an lvalue on the stack and a type
    // change.
    Value *src1 = pl();

    Type *target = src1->type->target;
    if (is_incomplete_type(target))
        error("Attempt to use an incomplete type");

    if (src1->is_lvalue) panic_with_line_number("Expected rvalue");

    Value *dst = dup_value(src1);
    dst->type = deref_pointer(src1->type);
    dst->is_lvalue = 1;
    push(dst);
}

// Search for a struct member. Panics if it doesn't exist
StructOrUnionMember *lookup_struct_or_union_member(Type *type, char *identifier) {
    StructOrUnionMember **pmember = type->struct_or_union_desc->members;

    while (*pmember) {
        char *member_identifier = (*pmember)->identifier;
        if (member_identifier && !strcmp(member_identifier, identifier)) return *pmember;
        pmember++;
    }

    error("Unknown member %s in struct %s\n", identifier, type->tag ? type->tag->identifier : "(anonymous)");
}

// Allocate a new label and create a value for it, for use in a jmp
Value *new_label_dst(void) {
    Value *v = new_value();
    v->label = ++label_count;

    return v;
}

// Add a no-op instruction with a label
static void add_jmp_target_instruction(Value *v) {
    Tac *tac = add_parser_instruction(IR_NOP, 0, 0, 0);
    tac->label = v->label;
}

static void add_conditional_jump(int operation, Value *dst) {
    // Load the result into a register
    add_parser_instruction(operation, 0, pl(), dst);
}

// Add instructions for && and || operators
static void and_or_expr(int is_and, int level) {
    Value *src1 = vtop();

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
    add_parser_instruction(IR_MOVE, dst, pl(), 0);
    add_parser_instruction(IR_JMP, 0, ldst3, 0);

    // Test second operand
    add_jmp_target_instruction(ldst2);
    parse_expression(level);
    Value *src2 = vtop();
    check_and_or_operation_type(src1, src2);
    add_conditional_jump(is_and ? IR_JZ : IR_JNZ, ldst1); // Store zero & end
    push_integral_constant(TYPE_INT, is_and ? 1 : 0);     // Store 1
    add_parser_instruction(IR_MOVE, dst, pl(), 0);
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
    add_parser_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

static Value *integer_promote(Value *v) {
    if (!is_integer_type(v->type)) error("Invalid operand, expected integer type");

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

    if (src->type->type == TYPE_PTR) error("Unable to convert a pointer to a long double");
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
    add_parser_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

// Convert a value to a double. This can be a constant, integer or SSE value
static Value *double_type_change(Value *src) {
    Value *dst;

    if (src->type->type == TYPE_PTR) error("Unable to convert a pointer to a double");
    if (src->type->type == TYPE_LONG_DOUBLE) panic_with_line_number("Unexpectedly got a long double -> double conversion");
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
    add_parser_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

// Convert a value to a float. This can be a constant, integer or float
static Value *float_type_change(Value *src) {
    Value *dst;

    if (src->type->type == TYPE_PTR) error("Unable to convert a pointer to a double");
    if (src->type->type == TYPE_LONG_DOUBLE) panic_with_line_number("Unexpectedly got a long double -> float conversion");
    if (src->type->type == TYPE_DOUBLE) panic_with_line_number("Unexpectedly got a double -> float conversion");
    if (src->type->type == TYPE_FLOAT) return src;

    if (src->is_constant)
        // src is an integer, convert to floating point value
        return convert_int_constant_to_floating_point(src, new_type(TYPE_FLOAT));

    // Add a move for the type change
    dst = dup_value(src);
    dst->vreg = 0;
    dst->local_index = new_local_index();
    dst->type = new_type(TYPE_FLOAT);
    add_parser_instruction(IR_MOVE, dst, src, 0);

    return dst;
}

static void arithmetic_operation(int operation, Type *dst_type) {
    // Pull two items from the stack and push the result. Code in the IR
    // is generated when the operands can't be evaluated directly.
    Type *common_type = vs_operation_type();
    if (!dst_type) dst_type = common_type;

    Value *src2 = pl();
    Value *src1 = pl();

    int src2_is_int = is_integer_type(src2->type);

    if (operation == IR_DIV && src2->is_constant && (src2_is_int && src2->int_value == 0 || !src2_is_int && src2->fp_value == 0))
        warning("Division by zero");

    if (operation == IR_MOD && src2->is_constant && src2->int_value == 0)
        warning("Modulus by zero");

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
            add_parser_instruction(IR_ADDRESS_OF, new_src1, src1, 0);
            src1 = new_src1;
        }

        else if (src2->type->type == TYPE_FUNCTION && src2->global_symbol && is_pointer_to_function_type(src1->type))  {
            // src2 is a global function, load the address into a register
            Value *new_src2 = new_value();
            new_src2->vreg = new_vreg();
            new_src2->type = make_pointer(dup_type(src2->type));
            add_parser_instruction(IR_ADDRESS_OF, new_src2, src2, 0);
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

void check_plus_operation_type(Value *src1, Value *src2, int for_array_subscript) {
    int src1_is_pointer = is_pointer_to_object_type(src1->type);
    int src1_is_integer = is_integer_type(src1->type);
    int src1_is_arithmetic = is_arithmetic_type(src1->type);
    int src2_is_pointer = is_pointer_to_object_type(src2->type);
    int src2_is_integer = is_integer_type(src2->type);
    int src2_is_arithmetic = is_arithmetic_type(src2->type);

    // Either both operands shall have arithmetic type, or one operand shall be a
    // pointer to an object type and the other shall have integral type.

    if (!for_array_subscript && src1_is_arithmetic && src2_is_arithmetic) return;
    else if (src1_is_pointer && src2_is_integer) return;
    else if (src2_is_pointer && src1_is_integer) return;
    else error("Invalid operands");
}

void check_minus_operation_type(Value *src1, Value *src2) {
    int src1_is_pointer = is_pointer_to_object_type(src1->type);
    int src1_is_arithmetic = is_arithmetic_type(src1->type);
    int src2_is_pointer = is_pointer_to_object_type(src2->type);
    int src2_is_integer = is_integer_type(src2->type);
    int src2_is_arithmetic = is_arithmetic_type(src2->type);

    // One of the following shall hold:
    // * both operands have arithmetic type;
    // * both operands are pointers to qualified or unqualified versions of compatible object types; or
    // * the left operand is a pointer to an object type and the right operand has integral type. (Decrementing is equivalent to subtracting 1.)
    if (
        (!(src1_is_arithmetic && src2_is_arithmetic)) &&
        (!(src1_is_pointer && src2_is_pointer && types_are_compatible_ignore_qualifiers(deref_pointer(src1->type), deref_pointer(src2->type)))) &&
        (!(src1_is_pointer && src2_is_integer))
    )
    error("Invalid operands to binary minus");
}

void check_bitwise_shift_operation_type(Value *src1, Value *src2) {
    if (!is_integer_type(src1->type)) error("Invalid operands to bitwise shift");
    if (!is_integer_type(src2->type)) error("Invalid operands to bitwise shift");
}

void check_and_or_operation_type(Value *src1, Value *src2) {
    if (!is_scalar_type(src2->type)) error("Invalid operands to &&/||");
    if (!is_scalar_type(src2->type)) error("Invalid operands to &&/||");
}

void check_unary_operation_type(int operation, Value *value) {
    if (operation == IR_BNOT && !is_integer_type(value->type)) error("Cannot use ~ on a non integer");
    if (operation == IR_LNOT && !is_scalar_type(value->type)) error("Cannot use ! on a non scalar");

    if (operation == IR_ADD || operation == IR_SUB)
        if (!is_arithmetic_type(value->type)) error("Can only use unary +/- on an arithmetic type");
}

void check_binary_operation_types(int operation, Value *src1, Value *src2) {
    if (operation == IR_ADD) return check_plus_operation_type(src1, src2, 0);
    if (operation == IR_SUB) return check_minus_operation_type(src1, src2);
    if (operation == IR_BSHR || operation == IR_BSHL || operation == IR_ASHR) return check_bitwise_shift_operation_type(src1, src2);
    if (operation == IR_LAND || operation == IR_LOR) return check_and_or_operation_type(src1, src2);

    int src1_is_arithmetic = is_arithmetic_type(src1->type);
    int src2_is_arithmetic = is_arithmetic_type(src2->type);
    int src1_is_integer = is_integer_type(src1->type);
    int src2_is_integer = is_integer_type(src2->type);
    int src1_is_pointer = is_pointer_type(src1->type);
    int src2_is_pointer = is_pointer_type(src2->type);

    int src1_is_function = src1->type->type == TYPE_FUNCTION || is_pointer_to_function_type(src1->type);
    int src2_is_function = src2->type->type == TYPE_FUNCTION || is_pointer_to_function_type(src2->type);

    if (operation == IR_MUL  && (!src1_is_arithmetic || !src2_is_arithmetic)) error("Invalid operands to binary *");
    if (operation == IR_DIV  && (!src1_is_arithmetic || !src2_is_arithmetic)) error("Invalid operands to binary /");
    if (operation == IR_MOD  && (!src1_is_integer    || !src2_is_integer))    error("Invalid operands to binary %");
    if (operation == IR_BAND && (!src1_is_integer    || !src2_is_integer))    error("Invalid operands to binary &");
    if (operation == IR_BOR  && (!src1_is_integer    || !src2_is_integer))    error("Invalid operands to binary |");
    if (operation == IR_XOR  && (!src1_is_integer    || !src2_is_integer))    error("Invalid operands to binary ^");

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
            (!(src1_is_pointer && src2_is_pointer && is_object_type(src1_type_deref) && is_object_type(src2_type_deref) && types_are_compatible_ignore_qualifiers(src1_type_deref, src2_type_deref))) &&
            (!(src1_is_pointer && src2_is_pointer && is_incomplete_type(src1_type_deref) && is_incomplete_type(src2_type_deref) && types_are_compatible_ignore_qualifiers(src1_type_deref, src2_type_deref)))
        )
            error("Invalid operands to relational operator");
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
            (!(src1_is_pointer && src2_is_pointer && types_are_compatible_ignore_qualifiers(src1_type_deref, src2_type_deref))) &&
            (!(src1_is_pointer && src2_is_pointer && is_pointer_to_void(src2->type))) &&
            (!(src2_is_pointer && src1_is_pointer && is_pointer_to_void(src1->type))) &&
            (!(src1_is_pointer && is_null_pointer(src2))) &&
            (!(src2_is_pointer && is_null_pointer(src1)))
        )
            error("Invalid operands to relational operator");
    }
}

static void parse_arithmetic_operation(int level, int operation, Type *type) {
    Value *src1 = vtop();
    parse_expression(level);
    Value *src2 = vtop();
    check_binary_operation_types(operation, src1, src2);

    arithmetic_operation(operation, type);
}

// Make a value out of a global symbol
Value *make_global_symbol_value(Symbol *symbol) {
    Value *v = new_value();
    v->type = dup_type(symbol->type);
    v->global_symbol = symbol;

    // Functions are rvalues, everything else is an lvalue
    v->is_lvalue = (symbol->type->type != TYPE_FUNCTION);

    return v;
}

// Make a value out of a local symbol
Value *make_local_symbol_value(Symbol *symbol) {
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

    return v;
}

// Make a value out of a symbol
Value *make_symbol_value(Symbol *symbol) {
   if (symbol->global_identifier)
        return make_global_symbol_value(symbol);
    else
        return make_local_symbol_value(symbol);
}

// Push the value for a symbol
static void push_symbol(Symbol *symbol) {
    push(make_symbol_value(symbol));
}

// If src is a function type and a global symbol, add an address of instruction to
// load it into a register.
Value *load_function(Value *src, Type *dst_type) {
    int dst_can_be_function = is_pointer_to_function_type(dst_type)  || dst_type->type == TYPE_FUNCTION || is_pointer_to_void(dst_type);
    int src_is_function = is_pointer_to_function_type(src->type) || src->type->type == TYPE_FUNCTION;

    if (dst_can_be_function && src_is_function) {
        if (src->type->type == TYPE_FUNCTION && src->global_symbol) {
            // Add instruction to load a global function into a register
            Value *src2 = new_value();
            src2->vreg = new_vreg();
            src2->type = make_pointer(dup_type(src->type));
            add_parser_instruction(IR_ADDRESS_OF, src2, src, 0);

            return src2;
        }

        return src;
    }

    return src;
}

// Add type change move if necessary and return the dst value
Value *add_convert_type_if_needed(Value *src, Type *dst_type) {
    if (dst_type->type == TYPE_FUNCTION) error("Function type mismatch");

    int dst_is_function = is_pointer_to_function_type(dst_type)  || dst_type->type == TYPE_FUNCTION;
    int src_is_function = is_pointer_to_function_type(src->type) || src->type->type == TYPE_FUNCTION;

    if ((dst_is_function && src_is_function) || (is_pointer_to_void(dst_type) && src->type->type == TYPE_FUNCTION))
        return load_function(src, dst_type);

    if (!type_eq(dst_type, src->type)) {
        if (src->is_constant) {
            if (dst_is_function && !is_null_pointer(src)) error("Function type mismatch");
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
        else if (dst_is_function && !src_is_function && !is_null_pointer(src) && !is_pointer_to_void(src->type))
            error("Function type mismatch");

        // Convert non constant
        Value *src2 = new_value();
        src2->vreg = new_vreg();
        src2->type = dup_type(dst_type);
        add_parser_instruction(IR_MOVE, src2, src, 0);
        return src2;
    }

    return src;
}

static void warn_of_incompatible_types_in_assignment(Type *dst, Type *src) {
    if (warn_assignment_types_incompatible) warning("Incompatible types in assignment");
}

static void check_simple_assignment_types(Value *dst, Value *src) {
    int dst_is_arithmetic = is_arithmetic_type(dst->type);
    int src_is_arithmetic = is_arithmetic_type(src->type);

    // Both are arithmetic types
    if (dst_is_arithmetic && src_is_arithmetic) return;

    // Both are compatible struct/union types
    if (dst->type->type == TYPE_STRUCT_OR_UNION && types_are_compatible_ignore_qualifiers(dst->type, src->type)) return;

    // Allow a function to be assigned to void *
    if (is_pointer_to_void(dst->type) && src->type->type == TYPE_FUNCTION) return;

    // Both are pointers with identical qualifiers, with the targets compatible
    if (dst->type->type == TYPE_PTR && src->type->type == TYPE_PTR) {
        if (src->type->target->is_const && !dst->type->target->is_const)
            warning("Assignment discards const qualifier");

        if (types_are_compatible_ignore_qualifiers(dst->type->target, src->type->target)) return;

        if (is_pointer_to_void(dst->type) || is_pointer_to_void(src->type)) return;

        warn_of_incompatible_types_in_assignment(dst->type, src->type);
        return;
    }

    if (dst->type->type == TYPE_PTR && is_null_pointer(src)) return;

    // Dst is a pointer to a function and src is a function
    if (is_pointer_to_function_type(dst->type) && src->type->type == TYPE_FUNCTION && types_are_compatible(dst->type->target, src->type)) return;

    // Fall back to ordinary type checking
    warn_of_incompatible_types_in_assignment(dst->type, src->type);
}

// Add instruction to assign a scalar value in src1 to dst
static void add_simple_assignment_instruction(Value *dst, Value *src1, int enforce_const) {
    if (!dst->is_lvalue) error("Cannot assign to an rvalue");
    if (enforce_const && !type_is_modifiable(dst->type)) error("Cannot assign to read-only variable");

    dst->is_lvalue = 1;

    check_simple_assignment_types(dst, src1);

    src1 = add_convert_type_if_needed(src1, dst->type);

    if (dst->bit_field_size)
        add_parser_instruction(IR_SAVE_BIT_FIELD, dst, src1, 0);
    else
        add_parser_instruction(IR_MOVE, dst, src1, 0);

    push(dst);
}

// Parse a simple assignment expression. The stack is the dst.
static void parse_simple_assignment(int enforce_const) {
    if (!vtop()->is_lvalue) error("Cannot assign to an rvalue");
    if (enforce_const && !type_is_modifiable(vtop()->type)) error("Cannot assign to read-only variable");

    Value *dst = pop();
    parse_expression(TOK_EQ);
    Value *src1 = pl();

    add_simple_assignment_instruction(dst, src1, enforce_const);
}

static void add_initializer(Value *dst, int offset, int size, Value *scalar) {
    Symbol *s = dst->global_symbol;
    if (!s) error("Attempt to add an initializer to value without a global_symbol");

    if (!s->initializers) s->initializers = new_list(INITIAL_INITALIZERS_COUNT);

    int bf_offset;
    int bf_bit_offset;
    int bf_bit_size;
    determine_bit_field_params(dst, &bf_offset, &bf_bit_offset, &bf_bit_size);
    if (dst->bit_field_size) offset += bf_offset;

    Initializer *in;
    if (dst->bit_field_size && s->initializers->length && ((Initializer *) s->initializers->elements[s->initializers->length - 1])->offset == offset)
        // Amend previous initializer for bit field
        in = (Initializer *) s->initializers->elements[s->initializers->length - 1];


    else {
        if (!dst->bit_field_size && s->initializers->length) {
            Initializer *prev = (Initializer *) s->initializers->elements[s->initializers->length - 1];
            if (prev->offset + prev->size != offset) {
                Initializer *zero = wcalloc(1, sizeof(Initializer));
                zero->offset = prev->offset + prev->size;
                zero->size = offset - prev->offset - prev->size;
                append_to_list(s->initializers, zero);
            }
        }

        in = wcalloc(1, sizeof(Initializer));
        append_to_list(s->initializers, in);
    }

    if (scalar) {
        if (!scalar->is_constant && !scalar->is_string_literal && !scalar->is_address_of && !scalar->type->type == TYPE_ARRAY)
            error("An initializer for an object with static storage duration must be a constant expression");

        if (!scalar->is_string_literal && !scalar->is_address_of && scalar->type->type != TYPE_ARRAY) scalar = cast_constant_value(scalar, dst->type);

        size = get_type_size(dst->type);

        if (scalar->type->type == TYPE_FLOAT) {
            in->data = wmalloc(sizeof(float));
            *((float *) in->data) = scalar->fp_value;
        }
        else if (scalar->type->type == TYPE_DOUBLE) {
            in->data = wmalloc(sizeof(double));
            *((double *) in->data) = scalar->fp_value;
        }
        else if (scalar->type->type == TYPE_LONG_DOUBLE)
            in->data = &scalar->fp_value;
        else if (dst->bit_field_size) {
            // Bit field
            if (!in->data) {
                in->data = wmalloc(sizeof(int));
                *((unsigned int *) in->data) = 0;
            }

            unsigned int *pi = (unsigned int *) in->data;
            unsigned int mask = bf_bit_size == 32 ? -1 : (1 << bf_bit_size) - 1;
            unsigned int inverted_shifted_mask = ~(mask << bf_bit_offset);
            (*pi) &= inverted_shifted_mask;
            (*pi) |= ((scalar->int_value & mask) << bf_bit_offset);
        }
        else if (scalar->is_address_of) {
            in->is_address_of = 1;
            in->address_of_offset = scalar->address_of_offset;
            in->is_string_literal = scalar->is_string_literal;
            in->string_literal_index = scalar->string_literal_index;
            in->symbol = scalar->global_symbol;
        }
        else if (scalar->is_string_literal) {
            in->is_string_literal = 1;
            in->string_literal_index = scalar->string_literal_index;
            in->address_of_offset = scalar->address_of_offset;
        }
        else if (scalar->type->type == TYPE_ARRAY) {
            in->symbol = scalar->global_symbol;
        }
        else
            in->data = &scalar->int_value;
    }

    in->size = size;
    in->offset = offset;
}

// Initialize value with zeroes starting with offset & size.
// Do it in increments of 8, 4, 2, 1, to minimize the amount of instructions
static void initialize_with_zeroes(Value *value, int offset, int size) {
    if (value->global_symbol)
        add_initializer(value, offset, size, 0);
    else {
        Value *src1 = new_integral_constant(TYPE_INT, size);
        Value *dst = dup_value(value);
        dst->offset += offset;
        add_parser_instruction(IR_ZERO, dst, src1, 0);
    }
}

// The first member of the union is going to get initialized. If the union size is
// larger than the size of the first member, zero the upper remainder
static void initialize_union_with_zeroes(Value *value, int offset) {
    int union_size = get_type_size(value->type);
    int first_member_size = get_type_size(value->type->struct_or_union_desc->members[0]->type);
    if (first_member_size < union_size) initialize_with_zeroes(value, offset + first_member_size, union_size - first_member_size);
}

// Initialize an array of chars or wchar_t with a string literal
static TypeIterator *initialize_with_string_literal(TypeIterator *it, Value *value, Value *string_literal) {
    StringLiteral *sl = &(string_literals[string_literal->string_literal_index]);

    if (sl->is_wide_char) {
        int *int_data = (int *) sl->data;
        int size = sl->size;

        for (int i = 0; i < size; i++) {
            Value *v = new_integral_constant(TYPE_INT, int_data[i]);
            it = parse_initializer(it, value, v);
        }
    }
    else {
        for (int i = 0; i < sl->size; i++) {
            Value *v = new_integral_constant(TYPE_INT, sl->data[i]);
            it = parse_initializer(it, value, v);
        }
    }

    return it;
}

// Parse an initializer for a variable with automatic storage. If expression is set, it
// is used as a initializer for a scalar value, otherwise, the expression is parsed.
static TypeIterator *parse_initializer(TypeIterator *it, Value *value, Value *expression) {
    parse_expression_function_type *parse_expr = value->global_symbol
        ? parse_constant_expression : parse_expression_and_pop;

    TypeIterator *outer_it = it;
    int initial_outer_offset = it->offset;

    if (cur_token == TOK_LCURLY) {
        // Parse {...} initializer

        next();
        if (cur_token == TOK_RCURLY) error("Empty array/struct/union initializer");

        // Loop over all comma separated expressions
        while (cur_token != TOK_RCURLY) {
            if (cur_token != TOK_LCURLY) {
                // Scalar
                it = parse_initializer(it, value, 0);
            }
            else {
                // Sub initializer {...}
                if (!type_iterator_done(it)) {
                    // When encountering a {} in a sub initializer, descend down
                    // one level from wherever the iterator currently is, and recurse.
                    TypeIterator *child;
                    child = type_iterator_descend(it);
                    child->parent = 0;
                    parse_initializer(child, value, 0);
                    it = type_iterator_next(it); // Advance the top level iterator
                }
                else {
                    // There are extraneous expressions, keep recursing and throw
                    // everything away
                    parse_initializer(it, value, 0);
                }
            }

            if (cur_token != TOK_RCURLY) consume(TOK_COMMA, ",");
        }
        consume(TOK_RCURLY, "}");
    }

    else {
        // Initialization of a scalar value

        if (type_iterator_done(it)) {
            // Parse and ignore any expressions if the iteration has run out
            if (!expression) parse_expr(TOK_EQ);
            return it;
        }

        Value *src;
        int initialize_string_literal = 0;
        Value *parsed_expression = 0;
        if (expression)
            src = expression;
        else {
            parsed_expression = parse_expr(TOK_EQ);

            if (parsed_expression->is_string_literal) {
                it = type_iterator_dig_for_string_literal(it);

                initialize_string_literal =
                    it->type->type == TYPE_ARRAY &&
                    (it->type->target->type == TYPE_CHAR || it->type->target->type == TYPE_INT);
                }
        }

        if (initialize_string_literal) {
            it = initialize_with_string_literal(it, value, parsed_expression);
            // Falls through to array size setting code
        }

        else {
            if (!expression) {
                if (value->global_symbol) src = parsed_expression;
                else src = load(parsed_expression);
            }

            // Recurse to deepest scalar unless the expression is a struct, which
            // can be assigned directly.
            if (src->type->type != TYPE_STRUCT_OR_UNION) it = type_iterator_dig(it);

            // For unions, ensure that the whole union is initialized, even if the
            // first member is smaller than the others.
            if (value->type->type == TYPE_STRUCT_OR_UNION && value->type->struct_or_union_desc->is_union)
                initialize_union_with_zeroes(value, it->offset);

            Value *child = dup_value(value);
            child->global_symbol = value->global_symbol;
            child->offset = it->offset;
            child->bit_field_offset = it->bit_field_offset;
            child->bit_field_size = it->bit_field_size;
            child->type = it->type;
            child->is_lvalue = 1;

            if (value->global_symbol)
                add_initializer(child, it->offset, 0, src);

            else
                add_simple_assignment_instruction(child, src, 0);

            return type_iterator_next(it);
        }
    }

    // Complete incomplete arrays
    if (outer_it->type->type == TYPE_ARRAY && outer_it->type->array_size == 0) {
        if (outer_it != it) {
            // An array was descended into, but not completed. Zero the elements first.
            int array_element_size = get_type_size(outer_it->type->target);
            int zeroes = array_element_size - ((it->offset - initial_outer_offset) % array_element_size);
            if (zeroes < 0) panic_with_line_number("Got negative zeroes");
            if (zeroes)
                initialize_with_zeroes(value, it->offset, zeroes);

            // Advance the index since the half-initialized element counts towards
            // the array size
            outer_it->index++;
        }

        // Complete the array
        outer_it->type->array_size = outer_it->index;
    }

    // If not all values where reached, set the remainder with zeroes
    if (!type_iterator_done(it)) {
        int outer_size = get_type_size((outer_it->type));
        int zeroes = initial_outer_offset + outer_size - it->offset;
        if (zeroes < 0) panic_with_line_number("Got negative zeroes");
        if (zeroes)
            initialize_with_zeroes(value, it->offset, zeroes);
    }

    return it;
}

// Prepare compound assignment
Value *prep_comp_assign(void) {
    next();

    if (!vtop()->is_lvalue) error("Cannot assign to an rvalue");
    if (!type_is_modifiable(vtop()->type)) error("Cannot assign to read-only variable");

    Value *v1 = vtop();           // lvalue
    push(load(dup_value(v1)));  // rvalue
    return v1;
}

// Finish compound assignment
static void finish_comp_assign(Value *v1) {
    push(add_convert_type_if_needed(pop(), v1->type));
    add_parser_instruction(IR_MOVE, v1, vtop(), 0);
}

static void parse_addition(int level, int for_array_subscript) {
    if (vtop()->type->type == TYPE_ARRAY) push(decay_array_value(pl()));
    Value *src1 = vtop();
    parse_expression(level);
    Value *src2 = vtop();
    if (vtop()->type->type == TYPE_ARRAY) push(decay_array_value(pl()));

    check_plus_operation_type(src1, src2, for_array_subscript);

    int src1_is_pointer = is_pointer_to_object_type(src1->type);
    int src2_is_pointer = is_pointer_to_object_type(src2->type);

    // Swap the operands so that the pointer comes first, for convenience
    if (!src1_is_pointer && src2_is_pointer) {
        Value *tmp = vs[0];
        vs[0] = vs[1];
        vs[1] = tmp;
        src1 = tmp;

        src2_is_pointer = 0;
    }

    int factor = get_type_inc_dec_size(vs[1]->type);

    if (factor > 1) {
        push_integral_constant(TYPE_INT, factor);
        arithmetic_operation(IR_MUL, 0);
    }

    arithmetic_operation(IR_ADD, 0);
}

static void parse_subtraction(int level) {
    Value *src1 = vtop();

    int factor = get_type_inc_dec_size(vtop()->type);

    parse_expression(level);
    Value *src2 = vtop();

    check_minus_operation_type(src1, src2);

    int src2_is_pointer = is_pointer_to_object_type(src2->type);

    if (factor > 1) {
        if (!src2_is_pointer) {
            push_integral_constant(TYPE_INT, factor);
            arithmetic_operation(IR_MUL, 0);
        }

        arithmetic_operation(IR_SUB, 0);

        if (src2_is_pointer) {
            vtop()->type = new_type(TYPE_LONG);
            push_integral_constant(TYPE_INT, factor);
            arithmetic_operation(IR_DIV, 0);
        }
    }
    else
        arithmetic_operation(IR_SUB, 0);

    if (src2_is_pointer) vtop()->type = new_type(TYPE_LONG);
}

static void parse_bitwise_shift(int level, int unsigned_operation, int signed_operation) {
    Value *src1 = integer_promote(pl());
    parse_expression(level);
    Value *src2 = integer_promote(pl());
    check_bitwise_shift_operation_type(src1, src2);
    int operation = src1->type->is_unsigned ? unsigned_operation : signed_operation;
    add_ir_op(operation, src1->type, new_vreg(), src1, src2);
}

// Parse a declaration
static void parse_declaration(void) {
    Symbol *symbol;

    cur_type_identifier = 0;
    Type *type = concat_base_type(parse_declarator(), dup_base_type(base_type));

    if (!cur_type_identifier) error("Expected an identifier");

    if (lookup_symbol(cur_type_identifier, cur_scope, 0)) error("Identifier redeclared: %s", cur_type_identifier);

    if (base_type->storage_class == SC_STATIC) {
        symbol = new_symbol(cur_type_identifier);
        symbol->type = dup_type(type);
        symbol->linkage = LINKAGE_INTERNAL;
        Function *function = cur_function_symbol->function;

        char *global_identifier;
        wasprintf(&global_identifier, "%s.%s.%d", cur_function_symbol->identifier, cur_type_identifier, function->static_symbols->length + 1);
        symbol->global_identifier = global_identifier;

        append_to_list(function->static_symbols, symbol);
    }
    else if (base_type->storage_class == SC_EXTERN || type->type == TYPE_FUNCTION) {
        // Add the identifier to the local scope, but give it a global identifier
        // without adding any linkage
        symbol = new_symbol(cur_type_identifier);
        symbol->type = dup_type(type);
        symbol->linkage = LINKAGE_EXPLICIT_EXTERNAL;
        symbol->global_identifier = cur_type_identifier;
    }
    else {
        symbol = new_symbol(cur_type_identifier);
        symbol->type = dup_type(type);
        symbol->linkage = LINKAGE_NONE;
        symbol->local_index = new_local_index();
    }

    // Ensure that incomplete arrays are completed if initialized
    Value *array_declaration = 0;
    if (symbol->type->type == TYPE_STRUCT_OR_UNION) {
        push_symbol(symbol);
        add_parser_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, pop(), 0);
    }

    if (symbol->type->type == TYPE_ARRAY) {
        push_symbol(symbol);
        array_declaration = pop();
        add_parser_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, array_declaration, 0);
    }

    if (cur_token == TOK_EQ) {
        push_symbol(symbol);
        BaseType *old_base_type = base_type;
        base_type = 0;
        next();
        Value *v = pop();

        if (v->type->type == TYPE_STRUCT_OR_UNION && is_incomplete_type(v->type))
            error("Attempt to use an incomplete struct or union in an initializer");

        parse_initializer(type_iterator(v->type), v, 0);
        symbol->type = v->type;
        if (array_declaration) array_declaration->type = v->type;
        base_type = old_base_type;
    }

    // If it's not initialized and incomplete, bail.
    else if (is_incomplete_type(symbol->type))
        error("Storage size is unknown");
}

Value *parse_expression_and_pop(int level) {
    parse_expression(level);
    return pop();
}

int parse_sizeof(parse_expression_function_type expr) {
    next();

    Type *type;
    Value *expression = 0;
    if (cur_token == TOK_LPAREN) {
        // Sizeof(type) or sizeof(expression)
        next();

        if (cur_token_is_type())
            type = parse_type_name();
        else
            expression = expr(TOK_COMMA);

        consume(TOK_RPAREN, ")");
    }
    else
        expression = expr(TOK_INC);

    if (expression) {
        if (expression->bit_field_size) error("Cannot take sizeof a bit field");
        type = expression->type;
    }

    if (is_incomplete_type(type)) error("Cannot take sizeof an incomplete type");

    return get_type_size(type);
}

// Push either an int or long double constant with value the size of v
static void push_value_size_constant(Value *v) {
    int size = get_type_inc_dec_size(v->type);
    if (v->type->type == TYPE_LONG_DOUBLE)
        push_floating_point_constant(TYPE_LONG_DOUBLE, size);
    else
        push_integral_constant(TYPE_INT, size);
}

static void parse_function_call(void) {
    Value *popped_function = pl();
    if (popped_function->type->type != TYPE_FUNCTION && !is_pointer_to_function_type(popped_function->type))
        error("Illegal attempt to call a non-function");

    Symbol *symbol = popped_function->global_symbol;
    Type *function_type = popped_function->type->type == TYPE_FUNCTION ? popped_function->type : popped_function->type->target;

    next();

    int function_call = function_call_count++;
    Value *src1 = make_function_call_value(function_call);
    add_parser_instruction(IR_START_CALL, 0, src1, 0);
    FunctionParamAllocation *fpa = init_function_param_allocaton(symbol ? symbol->global_identifier : "(anonymous)");

    // Allocate an integer slot if the function returns a slot in memory. The
    // RDI register must contain a pointer to the return value, set by the caller.
    int has_struct_or_union_return_value = 0;
    FunctionParamAllocation *rv_fpa = function_type->function->return_value_fpa;
    if (rv_fpa) {
        FunctionParamLocations *rv_fpl = &(rv_fpa->params[0]);
        if (rv_fpl->locations[0].stack_offset != -1) {
            add_function_param_to_allocation(fpa, make_pointer_to_void());
            has_struct_or_union_return_value = 1;
        }
    }

    while (1) {
        if (cur_token == TOK_RPAREN) break;

        if (!function_type->function->is_paramless && !function_type->function->is_variadic && function_type->function->param_count == 0)
            error("Too many arguments for function call");

        parse_expression(TOK_EQ);
        Value *arg = dup_value(src1);
        int fpa_arg_count = fpa->arg_count;
        int arg_count = fpa_arg_count - has_struct_or_union_return_value;
        arg->function_call_arg_index = arg_count;

        if (vtop()->type->type == TYPE_ARRAY) push(decay_array_value(pl()));
        if (vtop()->type->type == TYPE_ENUM) vtop()->type->type = TYPE_INT;

        // Convert type if needed
        if (!function_type->function->is_paramless && arg_count < function_type->function->param_count) {
            if (!type_eq(vtop()->type, function_type->function->param_types->elements[arg_count])) {
                Type *param_type = function_type->function->param_types->elements[arg_count];

                if (param_type->type == TYPE_ARRAY)
                    param_type = decay_array_to_pointer(param_type);
                else if (param_type->type == TYPE_ENUM)
                    param_type = new_type(TYPE_INT);

                push(add_convert_type_if_needed(pl(), param_type));
            }
        }
        else {
            if (!function_type->function->is_variadic && !function_type->function->is_paramless)
                error("Too many arguments for function call");

            Value *arg = pl();
            Type *type = apply_default_function_call_argument_promotions(arg->type);

            if (!type_eq(arg->type, type))
                push(add_convert_type_if_needed(arg, type));
            else
                push(arg);
        }

        add_function_param_to_allocation(fpa, vtop()->type);
        FunctionParamLocations *fpl = &(fpa->params[fpa_arg_count]);
        arg->function_call_arg_locations = fpl;
        arg->has_struct_or_union_return_value = has_struct_or_union_return_value;
        add_parser_instruction(IR_ARG, 0, arg, pl());

        // If a stack adjustment needs to take place to align 16-byte data
        // such as long doubles and structs with long doubles, an
        // IR_ARG_STACK_PADDING is inserted.
        if (fpl->locations[0].stack_padding >= 8) add_parser_instruction(IR_ARG_STACK_PADDING, 0, 0, 0);

        if (cur_token == TOK_RPAREN) break;
        consume(TOK_COMMA, ",");
        if (cur_token == TOK_RPAREN) error("Expected expression");
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
        add_parser_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, return_value, 0);
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

    add_parser_instruction(IR_CALL, return_value, function_value, 0);
    add_parser_instruction(IR_END_CALL, 0, src1, 0);

    if (return_value) push(return_value);
}

// Get a function arg, which must be of type va_list
static Value *parse_va_list() {
    parse_expression(TOK_EQ);
    Value *va_list = pop();
    Type *struct_or_union = find_struct_or_union("__va_list", 0, 1);
    Type *wcc_va_list_array_type = make_array(struct_or_union, 1);
    Type *wcc_va_list_pointer_type = make_pointer(struct_or_union);
    if (!types_are_compatible(va_list->type, wcc_va_list_array_type) && !types_are_compatible(va_list->type, wcc_va_list_pointer_type))
        error("Expected va_list type as first argument to va_start");

    return va_list;
}

// Parse void va_start(va, last_arg_variable)
static void parse_va_start() {
    next();
    consume(TOK_LPAREN, "(");

    // Get the first arg, which must be of type va_list
    Value *va_list = parse_va_list();

    // Get the second arg
    consume(TOK_COMMA, ",");
    parse_expression(TOK_EQ);
    Value *rightmost_param = pop();
    consume(TOK_RPAREN, ")");

    if (rightmost_param->local_index < 2)
        error("Expected function parameter as second argument to va_start");

    int rightmost_param_index = rightmost_param->local_index - 2;

    if (rightmost_param_index != cur_function_symbol->function->type->function->param_count - 1)
        error("Second argument to va_start isn't the rightmost function parameter");

    Value *param_index_value = new_value();
    param_index_value->type = new_type(TYPE_INT);
    param_index_value->int_value = rightmost_param_index;
    add_parser_instruction(IR_VA_START, 0, va_list, 0);

    Value *v = new_value();
    v->type = new_type(TYPE_VOID);
    push(v);
}

// Parse void va_arg(va, type)
static void parse_va_arg() {
    next();
    consume(TOK_LPAREN, "(");

    // Get the first arg, which must be of type va_list
    Value *va_list = parse_va_list();

    // Get the second arg, which must be a type
    consume(TOK_COMMA, ",");
    cur_type_identifier = 0;
    Type *type = parse_type_name();

    if (type->type == TYPE_ARRAY) error("Cannot use an array in va_arg");

    consume(TOK_RPAREN, ")");

    Value *dst = new_value();
    dst->type = type;
    if (type->type == TYPE_LONG_DOUBLE)
        dst->local_index = new_local_index();

    else if (type->type == TYPE_STRUCT_OR_UNION) {
        dst->local_index = new_local_index();
        add_parser_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, dst, 0);
    }

    else
        dst->vreg = new_vreg();

    add_parser_instruction(IR_VA_ARG, dst, va_list, 0);
    push(dst);
}

void parse_struct_dot_arrow_expression(void) {
    // Struct/union member lookup

    if (cur_token == TOK_DOT) {
        if (vtop()->type->type != TYPE_STRUCT_OR_UNION) error("Can only use . on a struct or union");
        if (!vtop()->is_lvalue) error("Expected lvalue for struct . operation.");
    }
    else {
        if (!is_pointer_type(vtop()->type)) error("Cannot use -> on a non-pointer");
        if (vtop()->type->target->type != TYPE_STRUCT_OR_UNION) error("Can only use -> on a pointer to a struct or union");
        if (is_incomplete_type(vtop()->type->target)) error("Dereferencing a pointer to incomplete struct or union");
    }

    int is_dot = cur_token == TOK_DOT;

    next();
    consume(TOK_IDENTIFIER, "identifier");

    Type *str_type = is_dot ? vtop()->type : vtop()->type->target;
    StructOrUnionMember *member = lookup_struct_or_union_member(str_type, cur_identifier);

    int member_is_const = 0;
    member_is_const = !type_is_modifiable(str_type);

    if (!is_dot) indirect();

    // The value stack is immutable. An offset may need adding, so this must be done
    // on a copied value. Otherwise, e.g. a struct function return result may not get
    // persisted to the stack this is needed for e.g. int i = return_struct().i;
    Value *v = dup_value(pop());
    v->offset += member->offset;
    v->bit_field_offset = v->offset * 8 + (member->bit_field_offset & 7);
    v->bit_field_size = member->bit_field_size;
    v->type = dup_type(member->type);
    v->type->is_const = member_is_const;
    v->is_lvalue = 1;
    push(v);
}

void check_ternary_operation_types(Value *switcher, Value *src1, Value *src2) {
    if (switcher->type->type != TYPE_ARRAY && !is_scalar_type(switcher->type))
        error("Expected scalar type for first operand of ternary operator");

    int src1_is_arithmetic = is_arithmetic_type(src1->type);
    int src2_is_arithmetic = is_arithmetic_type(src2->type);
    int src1_is_pointer = is_pointer_type(src1->type);
    int src2_is_pointer = is_pointer_type(src2->type);

    Type *src1_type_deref = 0;
    Type *src2_type_deref = 0;

    if (src1_is_pointer) src1_type_deref = deref_pointer(src1->type);
    if (src2_is_pointer) src2_type_deref = deref_pointer(src2->type);

    // Convert functions to function pointers
    if (src1->type->type == TYPE_FUNCTION) { src1_is_pointer = 1; src1_type_deref = src1->type; }
    if (src2->type->type == TYPE_FUNCTION) { src2_is_pointer = 1; src2_type_deref = src2->type; }

    // One of the following shall hold for the second and third operands:
    // * both operands have arithmetic type;
    // * both operands have compatible structure or union types;
    // * both operands have void type;
    // * both operands are pointers to qualified or unqualified versions of compatible types;
    // * one operand is a pointer and the other is a null pointer constant; or
    // * one operand is a pointer to an object or incomplete type and the other is a pointer to a qualified or unqualified version of void .

    if (
        (!((src1_is_arithmetic) && (src2_is_arithmetic))) &&
        (!(src1->type->type == TYPE_STRUCT_OR_UNION && src2->type->type == TYPE_STRUCT_OR_UNION && types_are_compatible(src1->type, src2->type))) &&
        (!(src1->type->type == TYPE_VOID && src2->type->type == TYPE_VOID)) &&
        (!(src1_is_pointer && src2_is_pointer && types_are_compatible_ignore_qualifiers(src1_type_deref, src2_type_deref))) &&
        (!(src1_is_pointer && is_null_pointer(src2))) &&
        (!(src2_is_pointer && is_null_pointer(src1))) &&
        (!((is_pointer_to_object_type(src1->type) && is_pointer_to_void(src2->type)) ||
           (is_pointer_to_object_type(src2->type) && is_pointer_to_void(src1->type))))
    )
        error("Invalid operands to ternary operator");
}

void parse_ternary_expression(void) {
    Value *switcher = vtop();

    // Destination register
    Value *dst = new_value();

    Value *ldst1 = new_label_dst(); // False case
    Value *ldst2 = new_label_dst(); // End
    add_conditional_jump(IR_JZ, ldst1);
    parse_expression(TOK_COMMA); // See https://en.cppreference.com/w/c/language/operator_precedence#cite_note-3
    Value *src1 = vtop();
    if (vtop()->type->type != TYPE_VOID) add_parser_instruction(IR_MOVE, dst, pl(), 0);
    add_parser_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
    add_jmp_target_instruction(ldst1);    // Start of false case
    consume(TOK_COLON, ":");
    parse_expression(TOK_TERNARY);
    Value *src2 = vtop();

    // Decay arrays to pointers
    if (src1->type->type == TYPE_ARRAY) src1 = decay_array_value(src1);
    if (src2->type->type == TYPE_ARRAY) src2 = decay_array_value(src2);

    check_ternary_operation_types(switcher, src1, src2);

    dst->type = operation_type(src1, src2, 1);

    if (dst->type->type == TYPE_STRUCT_OR_UNION) {
        dst->local_index = new_local_index();
        add_parser_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, dst, 0);
    }
    else
        dst->vreg = new_vreg();

    // Convert dst function to pointer to function
    if (dst->type->type == TYPE_FUNCTION) dst->type = make_pointer(dst->type);

    if (vtop()->type->type != TYPE_VOID) add_parser_instruction(IR_MOVE, dst, pl(), 0);
    push(dst);
    add_jmp_target_instruction(ldst2); // End
}

static void init_value_stack(void) {
    vs_bottom = wmalloc(sizeof(struct value *) * VALUE_STACK_SIZE);
    vs_start = vs_bottom + VALUE_STACK_SIZE; // The stack traditionally grows downwards
}

static void free_value_stack(void) {
    free(vs_bottom);
}

// Parse GNU extension Statements and Declarations in Expressions
// https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
static Value *parse_statement_expression(void) {
    // Backup the old value stack, since it needs to be reset when the statement is done
    Value **old_vs_start = vs_start;
    Value **old_vs = vs;

    init_value_stack();
    enter_scope();
    parse_compound_statement();

    Value *result;
    if (value_stack_is_empty() || vtop()->type->type == TYPE_VOID)
        result = make_void_value();
    else
        result = pop();

    // Restore the original value stack
    vs_start = old_vs_start;
    vs = old_vs;

    push(result);

    return result;
}

// Parse an expression using top-down precedence climbing parsing
// https://en.cppreference.com/w/c/language/operator_precedence
// https://en.wikipedia.org/wiki/Operator-precedence_parser#Precedence_climbing_method
static void parse_expression(int level) {
    // Parse any tokens that can be at the start of an expression

    // If base type has a value, then the current token cannot be a type. The
    // exception is if it's a typedef which has been redeclared as a variable, in which
    // case, cur_token_is_type will identify it as a typedef, when it should be
    // treated as an identifier.
    if (!base_type && cur_token_is_type()) {
        base_type = parse_declaration_specifiers();
        parse_expression(TOK_COMMA);
        base_type = 0;
    }

    else switch(cur_token) {
        // A typedef an also be used as an identifier in a labelled statement
        case TOK_TYPEDEF:
            parse_typedef();
            break;

        case TOK_LOGICAL_NOT:
            next();
            parse_expression(TOK_INC);
            check_unary_operation_type(IR_LNOT, vtop());

            if (vtop()->is_constant)
                push_integral_constant(TYPE_INT, !pop()->int_value);
            else {
                push_integral_constant(TYPE_INT, 0);
                arithmetic_operation(IR_EQ, new_type(TYPE_INT));
            }
            break;

        case TOK_BITWISE_NOT:
            next();
            parse_expression(TOK_INC);
            check_unary_operation_type(IR_BNOT, vtop());
            push(integer_promote(pl()));
            Type *type = vtop()->type;
            add_ir_op(IR_BNOT, type, new_vreg(), pl(), 0);
            break;

        case TOK_AMPERSAND:
            next();
            parse_expression(TOK_INC);

            // Arrays are rvalues as well as lvalues. Otherwise, an lvalue is required
            if ((vtop()->type->type == TYPE_ARRAY && vtop()->is_lvalue) && !vtop()->is_lvalue)
                error("Cannot take an address of an rvalue");

            if (vtop()->bit_field_size) error("Cannot take an address of a bit-field");

            Value *src1 = pop();

            if (src1->is_constant)
                // Special case of taking an address of a constant. This happens with
                // the offsetof macro which does something like:
                // define offsetof(STRUCTURE,FIELD) ((int) ((size_t)&((STRUCTURE*)0)->FIELD))
                // e.g. (int) ((size_t) &((struct s *) 0)->m);
                push_integral_constant(TYPE_LONG, src1->int_value + src1->offset);

            else {
                add_ir_op(IR_ADDRESS_OF, make_pointer(src1->type), new_vreg(), src1, 0);
                if (src1->offset && src1->vreg) {
                    // Non-vreg offsets are outputted by codegen. For addresses in registers, it
                    // needs to be calculated.
                    push_integral_constant(TYPE_INT, src1->offset);
                    arithmetic_operation(IR_ADD, 0);
                    src1->offset = 0;
                }
            }

            break;

        case TOK_INC:
        case TOK_DEC: {
            // Prefix increment & decrement

            int org_token = cur_token;
            next();
            parse_expression(TOK_DOT);

            if (!vtop()->is_lvalue) error("Cannot ++ or -- an rvalue");
            if (!type_is_modifiable(vtop()->type)) error("Cannot assign to read-only variable");

            Value *v1 = pop();                 // lvalue
            Value *src1 = load(dup_value(v1)); // rvalue
            push(src1);
            push_value_size_constant(src1);
            arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
            push(add_convert_type_if_needed(pop(), v1->type));
            add_parser_instruction(IR_MOVE, v1, vtop(), 0);

            break;
        }

        case TOK_MULTIPLY:
            if (base_type)
                parse_declaration();
            else {
                next();
                parse_expression(TOK_INC);

                // Special case: indirects on function types are no-ops.
                if (vtop()->type->type != TYPE_FUNCTION)  {
                    if (!is_pointer_or_array_type(vtop()->type)) error("Cannot dereference a non-pointer");
                    indirect();
                }
            }
            break;

        case TOK_PLUS:
            // Unary plus

            next();
            parse_expression(TOK_INC);
            check_unary_operation_type(IR_ADD, vtop());
            if (is_integer_type(vtop()->type)) push(integer_promote(pl()));
            break;

        case TOK_MINUS:
            // Unary minus
            next();

            parse_expression(TOK_INC);
            check_unary_operation_type(IR_SUB, vtop());

            if (vtop()->is_constant) {
                if (is_sse_floating_point_type(vtop()->type))
                    vtop()->fp_value = -vtop()->fp_value;
                else {
                    push_integral_constant(TYPE_INT, -1);
                    arithmetic_operation(IR_MUL, 0);
                }
            }
            else {
                if (vtop()->type->type == TYPE_LONG_DOUBLE)
                    push_floating_point_constant(TYPE_LONG_DOUBLE, -1.0L);
                else if (vtop()->type->type == TYPE_DOUBLE)
                    push_floating_point_constant(TYPE_DOUBLE, -1.0L);
                else if (vtop()->type->type == TYPE_FLOAT)
                    push_floating_point_constant(TYPE_FLOAT, -1.0L);
                else
                    push_integral_constant(TYPE_INT, -1);

                arithmetic_operation(IR_MUL, 0);
            }

            break;

        case TOK_LPAREN:
            if (base_type)
                parse_declaration();

            else {
                next();

                if (cur_token == TOK_LCURLY) {
                    parse_statement_expression();
                    consume(TOK_RPAREN, ")");
                }

                else if (cur_token_is_type()) {
                    // Cast
                    Type *dst_type = parse_type_name();
                    consume(TOK_RPAREN, ")");

                    parse_expression(TOK_INC);
                    Value *v1 = pl();

                    if (dst_type->type == TYPE_VOID)
                        push_void();

                    else if (v1->is_constant) {
                        // Special case for (void *) int-constant
                        if (is_pointer_to_void(dst_type) && (is_integer_type(v1->type) || is_pointer_to_void(v1->type)) && v1->is_constant) {
                            Value *dst = new_value();
                            dst->is_constant =1;
                            dst->int_value = v1->int_value;
                            dst->type = make_pointer_to_void();
                            push(dst);
                        }
                        else
                            // Cast integer constant
                            push(cast_constant_value(v1, dst_type));
                    }
                    else if (dst_type->type != TYPE_VOID && v1->type != dst_type) {
                        if (v1->type->type == TYPE_FUNCTION) v1 = load_function(v1, dst_type);

                        // Add move instruction
                        Value *dst = new_value();
                        dst->vreg = new_vreg();
                        dst->type = dup_type(dst_type);
                        add_parser_instruction(IR_MOVE, dst, v1, 0);
                        push(dst);
                    }
                    else push(v1);
                }
                else {
                    // Sub expression
                    parse_expression(TOK_COMMA);
                    consume(TOK_RPAREN, ")");
                }
            }
            break;

        case TOK_INTEGER:
            push_cur_long();
            next();
            break;

        case TOK_FLOATING_POINT_NUMBER:
            push_cur_long_double();
            next();
            break;

        case TOK_STRING_LITERAL: {
            push(make_string_literal_value_from_cur_string_literal());
            next();
            break;
        }

        case TOK_IDENTIFIER:

        // In this context, the lexer can identify a typedef which has been redeclared as a variable.
        // Treat it as if it was an identifier
        case TOK_TYPEDEF_TYPE:
            if (base_type)
                parse_declaration();

            else {
                // It's a symbol
                if (!strcmp(cur_identifier, "va_start"))
                    parse_va_start();

                else if (!strcmp(cur_identifier, "va_arg"))
                    parse_va_arg();

                else {
                    // Look up symbol
                    Symbol *symbol = lookup_symbol(cur_identifier, cur_scope, 1);
                    if (!symbol) error("Unknown symbol \"%s\"", cur_identifier);

                    next();

                    if (symbol->is_enum_value)
                        push_integral_constant(TYPE_INT, symbol->value);
                    else
                        push_symbol(symbol);
                }
            }
            break;

        case TOK_SIZEOF:
            push_integral_constant(TYPE_LONG, parse_sizeof(parse_expression_and_pop));
            break;

        default:
            error("Unexpected token %d in expression", cur_token);
    }

    // The next token is an operator
    while (cur_token >= level) {
        // In order or precedence, highest first

        switch (cur_token) {
            case TOK_LBRACKET:
                next();

                parse_addition(TOK_COMMA, 1);
                consume(TOK_RBRACKET, "]");
                indirect();
                break;

            case TOK_LPAREN:
                // Function call
                parse_function_call();
                break;

            case TOK_INC:
            case TOK_DEC: {
                // Postfix increment & decrement

                int org_token = cur_token;
                next();

                if (!vtop()->is_lvalue) error("Cannot ++ or -- an rvalue");
                if (!is_scalar_type(vtop()->type)) error("Cannot postfix ++ or -- on a non scalar");
                if (!type_is_modifiable(vtop()->type)) error("Cannot assign to read-only variable");

                Value *v1 = pop();                 // lvalue
                Value *src1 = load(dup_value(v1)); // rvalue
                push(src1);
                push_value_size_constant(src1);
                arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
                push(add_convert_type_if_needed(pop(), v1->type));
                add_parser_instruction(IR_MOVE, v1, vtop(), 0);
                pop(); // Pop the lvalue of the assignment off the stack
                push(src1); // Push the original rvalue back on the value stack
                break;
            }

            case TOK_DOT:
            case TOK_ARROW: {
                parse_struct_dot_arrow_expression();
                break;
            }

            // In order of precedence
            case TOK_MULTIPLY:         next(); parse_arithmetic_operation(TOK_DOT, IR_MUL, 0); break;
            case TOK_DIVIDE:           next(); parse_arithmetic_operation(TOK_DOT, IR_DIV, 0); break;
            case TOK_MOD:              next(); parse_arithmetic_operation(TOK_DOT, IR_MOD, 0); break;
            case TOK_PLUS:             next(); parse_addition(TOK_MULTIPLY, 0); break;
            case TOK_MINUS:            next(); parse_subtraction(TOK_MULTIPLY); break;
            case TOK_BITWISE_RIGHT:    next(); parse_bitwise_shift(TOK_PLUS, IR_BSHR, IR_ASHR); break;
            case TOK_BITWISE_LEFT:     next(); parse_bitwise_shift(TOK_PLUS, IR_BSHL, IR_BSHL); break;
            case TOK_LT:               next(); parse_arithmetic_operation(TOK_BITWISE_RIGHT, IR_LT,   new_type(TYPE_INT)); break;
            case TOK_GT:               next(); parse_arithmetic_operation(TOK_BITWISE_RIGHT, IR_GT,   new_type(TYPE_INT)); break;
            case TOK_LE:               next(); parse_arithmetic_operation(TOK_BITWISE_RIGHT, IR_LE,   new_type(TYPE_INT)); break;
            case TOK_GE:               next(); parse_arithmetic_operation(TOK_BITWISE_RIGHT, IR_GE,   new_type(TYPE_INT)); break;
            case TOK_DBL_EQ:           next(); parse_arithmetic_operation(TOK_LT,            IR_EQ,   new_type(TYPE_INT)); break;
            case TOK_NOT_EQ:           next(); parse_arithmetic_operation(TOK_LT,            IR_NE,   new_type(TYPE_INT)); break;
            case TOK_AMPERSAND:        next(); parse_arithmetic_operation(TOK_DBL_EQ,        IR_BAND, 0); break;
            case TOK_XOR:              next(); parse_arithmetic_operation(TOK_AMPERSAND,     IR_XOR,  0); break;
            case TOK_BITWISE_OR:       next(); parse_arithmetic_operation(TOK_XOR,           IR_BOR,  0); break;
            case TOK_AND:              next(); and_or_expr(1, TOK_BITWISE_OR); break;
            case TOK_OR:               next(); and_or_expr(0, TOK_AND);        break;
            case TOK_TERNARY:          next(); parse_ternary_expression(); break;
            case TOK_EQ:               next(); parse_simple_assignment(1); break;
            case TOK_PLUS_EQ:          { Value *v = prep_comp_assign(); parse_addition(TOK_EQ, 0);                      finish_comp_assign(v); break; }
            case TOK_MINUS_EQ:         { Value *v = prep_comp_assign(); parse_subtraction(TOK_EQ);                      finish_comp_assign(v); break; }
            case TOK_MULTIPLY_EQ:      { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_MUL,  0); finish_comp_assign(v); break; }
            case TOK_DIVIDE_EQ:        { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_DIV,  0); finish_comp_assign(v); break; }
            case TOK_MOD_EQ:           { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_MOD,  0); finish_comp_assign(v); break; }
            case TOK_BITWISE_AND_EQ:   { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_BAND, 0); finish_comp_assign(v); break; }
            case TOK_BITWISE_OR_EQ:    { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_BOR,  0); finish_comp_assign(v); break; }
            case TOK_BITWISE_XOR_EQ:   { Value *v = prep_comp_assign(); parse_arithmetic_operation(TOK_EQ, IR_XOR,  0); finish_comp_assign(v); break; }
            case TOK_BITWISE_RIGHT_EQ: { Value *v = prep_comp_assign(); parse_bitwise_shift(TOK_EQ, IR_BSHR, IR_ASHR);  finish_comp_assign(v); break; }
            case TOK_BITWISE_LEFT_EQ:  { Value *v = prep_comp_assign(); parse_bitwise_shift(TOK_EQ, IR_BSHL, IR_BSHL);  finish_comp_assign(v); break; }

            case TOK_COMMA: {
                // Delete the last expression (if any) on thestack

                pop_void();
                next();
                parse_expression(TOK_COMMA);

                break;
            }

            default:
                return; // Bail once we hit something unknown
        }
    }
}

static void parse_iteration_conditional_expression(Value **lcond, Value **cur_loop_continue_dst, Value *lend) {
    *lcond  = new_label_dst();
    *cur_loop_continue_dst = *lcond;
    add_jmp_target_instruction(*lcond);

    parse_expression(TOK_COMMA);
    if (!is_scalar_type(vtop()->type))
        error("Expected scalar type for loop controlling expression");

    add_conditional_jump(IR_JZ, lend);
}

static void parse_iteration_statement(void) {
    enter_scope();

    int prev_loop = cur_loop;
    cur_loop = ++loop_count;
    Value *src1 = new_value();
    src1->int_value = prev_loop;
    Value *src2 = new_value();
    src2->int_value = cur_loop;
    add_parser_instruction(IR_START_LOOP, 0, src1, src2);

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
        }
        add_parser_instruction(IR_JMP, 0, lbody, 0);
        consume(TOK_SEMI, ";");

        // Increment
        if (cur_token != TOK_RPAREN) {
            lincrement  = new_label_dst();
            cur_loop_continue_dst = lincrement;
            add_jmp_target_instruction(lincrement);
            parse_expression(TOK_COMMA);
            if (lcond) add_parser_instruction(IR_JMP, 0, lcond, 0);
        }

        consume(TOK_RPAREN, ")");
    }

    // Parse while
    else if (loop_token == TOK_WHILE) {
        consume(TOK_LPAREN, "(");
        parse_iteration_conditional_expression(&lcond, &cur_loop_continue_dst, lend);
        add_parser_instruction(IR_JMP, 0, lbody, 0);
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
        add_parser_instruction(IR_JMP, 0, lincrement, 0);
    else if (lcond && loop_token != TOK_DO)
        add_parser_instruction(IR_JMP, 0, lcond, 0);
    else if (lcond && loop_token == TOK_DO)
        add_parser_instruction(IR_JMP, 0, lbody, 0);
    else
        add_parser_instruction(IR_JMP, 0, lbody, 0);

    // Jump to the start of the body in a for loop like (;;i++)
    if (loop_token == TOK_FOR && !linit && !lcond && lincrement)
        add_parser_instruction(IR_JMP, 0, lbody, 0);

    add_jmp_target_instruction(lend);

    // Restore previous loop addresses
    cur_loop_continue_dst = old_loop_continue_dst;
    cur_loop_break_dst = old_loop_break_dst;

    // Restore outer loop counter
    cur_loop = prev_loop;

    add_parser_instruction(IR_END_LOOP, 0, src1, src2);

    exit_scope();
}

static void parse_compound_statement(void) {
    consume(TOK_LCURLY, "{");
    while (cur_token != TOK_RCURLY) parse_statement();

    // The scope must be exited before parsing of }, so that the lexer lexes the next token in the correct scope.
    exit_scope();
    consume(TOK_RCURLY, "}");
}

static void parse_switch_statement(void) {
    next();

    consume(TOK_LPAREN, "(");
    parse_expression(TOK_COMMA);
    consume(TOK_RPAREN, ")");

    // Maintain two IRs:
    // - ir has all the statement code and is used as normal
    // - case_ir has all the case comparison and jump statements

    Tac *old_case_ir_start            = case_ir_start;
    Tac *old_case_ir                  = case_ir;
    Value *old_case_default_label     = case_default_label;
    Value *old_loop_break_dst         = cur_loop_break_dst;
    Value *old_controlling_case_value = controlling_case_value;
    LongMap *old_case_values          = case_values;
    int old_seen_switch_default       = seen_switch_default;

    controlling_case_value = pl();

    if (!is_integer_type(controlling_case_value->type))
        error("The controlling expression of a switch statement is not an integral type");

    // Add an entry to the implicit switch stack
    case_ir_start = new_instruction(IR_NOP);
    case_ir = case_ir_start;
    Tac *root = ir;
    case_default_label = 0;
    cur_loop_break_dst = new_label_dst();
    case_values = new_longmap();
    seen_switch_default = 0;

    parse_statement();

    // Reorder IR so that it becomes
    // ... -> root -> case_ir -> jmp default -> statement_ir -> break label

    // Make root -> case_ir
    Tac *statement_ir_start = root->next;
    root->next = case_ir_start;
    case_ir_start->prev = root;
    ir = case_ir_start;
    while (ir->next) ir = ir->next;

    // Add jump to default label, if present, otherwise to the break label
    add_parser_instruction(IR_JMP, 0, case_default_label ? case_default_label : cur_loop_break_dst, 0);

    // Add statement IR
    ir->next = statement_ir_start;
    if (statement_ir_start) statement_ir_start->prev = ir;
    while (ir->next) ir = ir->next;

    // Add final break label
    add_jmp_target_instruction(cur_loop_break_dst);

    // Pop the old switch state back, if any
    cur_loop_break_dst      = old_loop_break_dst;
    case_default_label      = old_case_default_label;
    case_ir                 = old_case_ir;
    case_ir_start           = old_case_ir_start;
    controlling_case_value  = old_controlling_case_value;
    case_values             = old_case_values;
    seen_switch_default     = old_seen_switch_default;
}

static void parse_case_statement(void) {
    next();

    if (!controlling_case_value) error("Case label not within a switch statement");

    Value *v = parse_constant_integer_expression(0);

    long value = controlling_case_value->type->type == TYPE_INT
        ? (int) v->int_value
        : v->int_value;

    // Ensure there are no duplicates
    if (longmap_get(case_values, value))
        error("Duplicate switch case value");
    else
        longmap_put(case_values, value, (void *) 1);

    // Convert to controlling expression type if necessary
    if (v->type->type != controlling_case_value->type->type) {
        v->type = dup_type(controlling_case_value->type);
        v->int_value = value;
    }

    push(v);
    push(controlling_case_value);
    consume(TOK_COLON, ":");

    Value *ldst = new_label_dst();

    // Add comparison & jump to current switch's case IR
    Tac *org_ir = ir;
    ir = case_ir;
    arithmetic_operation(IR_EQ, controlling_case_value->type);
    add_conditional_jump(IR_JNZ, ldst);
    case_ir = ir;
    ir = org_ir;

    add_jmp_target_instruction(ldst);
    if (cur_token != TOK_CASE && cur_token != TOK_RCURLY) parse_statement();
}

static void parse_default_statement() {
    next();

    if (!controlling_case_value) error("Default label not within a switch statement");

    if (seen_switch_default) error("Duplicate default label");
    seen_switch_default = 1;

    consume(TOK_COLON, ":");

    case_default_label = new_label_dst();
    add_jmp_target_instruction(case_default_label);
    parse_statement();
}

static void parse_if_statement(void) {
    consume(TOK_IF, "if");

    consume(TOK_LPAREN, "(");
    parse_expression(TOK_COMMA);

    if (vtop()->type->type != TYPE_ARRAY && !is_scalar_type(vtop()->type))
        error("The controlling statement of an if statement must be a scalar");

    consume(TOK_RPAREN, ")");

    Value *ldst1 = new_label_dst(); // False case
    Value *ldst2 = new_label_dst(); // End
    add_conditional_jump(IR_JZ, ldst1);
    parse_statement();

    if (cur_token == TOK_ELSE) {
        next();
        add_parser_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
        add_jmp_target_instruction(ldst1);    // Start of else case
        parse_statement();
    }
    else {
        add_jmp_target_instruction(ldst1); // End of true case
    }

    // End
    add_jmp_target_instruction(ldst2);
}

static void parse_return_statement(void) {
    consume(TOK_RETURN, "return");
    if (cur_token == TOK_SEMI) {
        add_parser_instruction(IR_RETURN, 0, 0, 0);
    }
    else {
        parse_expression(TOK_COMMA);

        if (!value_stack_is_empty() && vtop()->type->type != TYPE_VOID && cur_function_symbol->function->type->target->type == TYPE_VOID)
            error("Return with a value in a function returning void");

        Value *src1;
        if (!value_stack_is_empty() && vtop()->type->type == TYPE_VOID && cur_function_symbol->function->type->target->type == TYPE_VOID) {
            // Deal with case of returning the result of a void function in a void function
            // e.g. foo(); void bar() { return foo(); }
            vs++; // Remove from value stack
            src1 = 0;
        }
        else
            src1 = add_convert_type_if_needed(pl(), cur_function_symbol->function->type->target);

        add_parser_instruction(IR_RETURN, 0, src1, 0);
    }
    consume(TOK_SEMI, ";");
}

static void parse_label_statement(char *identifier) {
    if (cur_token == TOK_COLON) {
        next();
        Value *ldst = new_label_dst();
        add_jmp_target_instruction(ldst);
        Value *dst = strmap_get(cur_function_symbol->function->labels, identifier);
        if (dst) error("Duplicate label %s", identifier);
        strmap_put(cur_function_symbol->function->labels, identifier, ldst);
    }
    else {
        rewind_lexer();
        parse_expression(TOK_COMMA);
        consume(TOK_SEMI, ";");
    }
}

static void parse_goto_statement(void) {
    next();

    // A typedef an also be used as an identifier in a goto statement
    if (cur_token != TOK_TYPEDEF_TYPE && cur_token != TOK_IDENTIFIER) panic_with_line_number("Expected an identifier");

    Value *ldst = strmap_get(cur_function_symbol->function->labels, cur_identifier);
    if (ldst)
        add_parser_instruction(IR_JMP, 0, ldst, 0);
    else {
        Tac *ir = add_parser_instruction(IR_JMP, 0, 0, 0);
        GotoBackPatch *gbp = wmalloc(sizeof(GotoBackPatch));
        gbp->identifier = cur_identifier;
        gbp->ir = ir;
        append_to_cll(cur_function_symbol->function->goto_backpatches, gbp);
    }
    next();
    consume(TOK_SEMI, ";");
}

static void backpatch_gotos(void) {
    CircularLinkedList *goto_backpatches = cur_function_symbol->function->goto_backpatches;
    if (!goto_backpatches) return;

    CircularLinkedList *head = goto_backpatches->next;
    CircularLinkedList *gbp_cll = head;
    do {
        GotoBackPatch *gbp = gbp_cll->target;

        Value *ldst = strmap_get(cur_function_symbol->function->labels, gbp->identifier);
        if (!ldst) error("Unknown label %s", gbp->identifier);
        gbp->ir->src1 = ldst;
        free(gbp);

        gbp_cll = gbp_cll->next;
    } while (gbp_cll != head);

    free_circular_linked_list(goto_backpatches);
}

static void add_va_register_save_area(void) {
    Type *type = find_struct_or_union("__wcc_register_save_area", 0, 1);
    if (!type) panic_with_line_number("Unable to find __wcc_register_save_area");
    Value *v = new_value();
    v->type = type;
    v->local_index = new_local_index();
    v->is_lvalue = 1;
    cur_function_symbol->function->register_save_area = v;
    add_parser_instruction(IR_DECL_LOCAL_COMP_OBJ, 0, v, 0);
}

// Parse a statement
static void parse_statement(void) {
    vs = vs_start; // Reset value stack
    base_type = 0; // Reset base type

    if (cur_token_is_type()) {
        int is_typedef = cur_token == TOK_TYPEDEF_TYPE;
        char *identifier = cur_identifier;

        base_type = parse_declaration_specifiers();
        if (cur_token == TOK_SEMI && (base_type->type->type == TYPE_STRUCT_OR_UNION || base_type->type->type == TYPE_ENUM))
            next();
        else if (cur_token == TOK_COLON && is_typedef)
            // Special case for a typedef used as a labelled statement
            parse_label_statement(identifier);
        else
            parse_expression(TOK_COMMA);
    }

    else switch (cur_token) {
        case TOK_SEMI:
            // Empty statement
            next();
            return;

        case TOK_LCURLY:
            enter_scope();
            parse_compound_statement();
            return;

        case TOK_DO:
        case TOK_WHILE:
        case TOK_FOR:
            parse_iteration_statement();
            break;

        case TOK_CONTINUE:
            next();
            add_parser_instruction(IR_JMP, 0, cur_loop_continue_dst, 0);
            consume(TOK_SEMI, ";");
            break;

        case TOK_BREAK:
            next();
            add_parser_instruction(IR_JMP, 0, cur_loop_break_dst, 0);
            consume(TOK_SEMI, ";");
            break;

        case TOK_IF:
            parse_if_statement();
            break;

        case TOK_SWITCH:
            parse_switch_statement();
            break;

        case TOK_CASE:
            parse_case_statement();
            break;

        case TOK_DEFAULT:
            parse_default_statement();
            break;

        case TOK_RETURN:
            parse_return_statement();
            break;

        case TOK_IDENTIFIER: {
            char *identifier = cur_identifier;
            next();
            parse_label_statement(identifier);
            break;
        }

        case TOK_GOTO:
            parse_goto_statement();
            break;

        default:
            parse_expression(TOK_COMMA);
            consume(TOK_SEMI, ";");
    }
}

// Parse function definition and possible declaration
static int parse_function(Type *type, int linkage, Symbol *symbol, Symbol *original_symbol) {
    // Setup the intermediate representation with a dummy no operation instruction.
    ir_start = 0;
    ir_start = add_parser_instruction(IR_NOP, 0, 0, 0);

    int is_defined = original_symbol && original_symbol->function->is_defined;

    if (!is_defined) {
        symbol->linkage = linkage;
        symbol->function = new_function();

        symbol->function->type = type;
        symbol->function->ir = ir_start;
        symbol->function->local_symbol_count = 0;
        symbol->function->labels = new_strmap();
        symbol->function->goto_backpatches = 0;

        if (type->target->type == TYPE_STRUCT_OR_UNION) {
            FunctionParamAllocation *fpa = init_function_param_allocaton(cur_type_identifier);
            add_function_param_to_allocation(fpa, type->target);
            type->function->return_value_fpa = fpa;
        }
    }
    else
        symbol->function = original_symbol->function;

    // The scope is left entered by the type parser
    cur_scope = type->function->scope;

    // Parse optional old style declaration list
    if (type->function->is_paramless && cur_token != TOK_SEMI && cur_token != TOK_COMMA && cur_token != TOK_ATTRIBUTE)
        parse_function_paramless_declaration_list(symbol->function);

    if (original_symbol && !types_are_compatible(original_symbol->type, type))
        error("Incompatible function types");

    if (original_symbol && original_symbol->function->is_defined && cur_token == TOK_LCURLY) {
        error("Redefinition of %s", cur_type_identifier);
    }

    cur_function_symbol = symbol;

    if (original_symbol) {
        if (!types_are_compatible(type, original_symbol->type)) error("Incompatible types");

        // Merge types if it's a redeclaration
        if (!is_defined) symbol->type = composite_type(type, original_symbol->type);
    }
    else
        symbol->type = type;

    // Parse __asm__ ("replacement_global_identifier");
    if (cur_token == TOK_ASM) {
        next();
        consume(TOK_LPAREN, "(");
        consume(TOK_STRING_LITERAL, "string literal");
        symbol->global_identifier = cur_string_literal.data;
        consume(TOK_RPAREN, ")");
    }

    int is_definition = 0;

    // Parse function declaration
    if (cur_token == TOK_LCURLY) {
        is_definition = 1;

        // Ensure parameters have identifiers
        for (int i = 0; i < symbol->function->type->function->param_count; i++)
            if (!symbol->function->type->function->param_identifiers->elements[i])
                error("Missing identifier for parameter in function definition");

        // Reset globals for a new function
        vreg_count = 0;
        function_call_count = 0;
        cur_loop = 0;
        loop_count = 0;

        if (type->function->is_variadic) add_va_register_save_area();

        cur_function_symbol->function->static_symbols = new_list(128);

        parse_compound_statement();

        cur_function_symbol->function->is_defined = 1;
        cur_function_symbol->function->vreg_count = vreg_count;
        backpatch_gotos();
    }
    else {
        // Make it clear that this symbol will need to be backpatched if used
        // before the definition has been processed.
        cur_function_symbol->value = 0;
        exit_scope();
    }

    return is_definition;
}

// Ensure linkage is the same for a symbol that has been redeclared.
int redefined_symbol_linkage(int linkage1, int linkage2, char *cur_type_identifier) {
    if (linkage1 == linkage2) return linkage1;

    // Allow, e.g. static int i; extern int i;
    if (linkage1 == LINKAGE_INTERNAL && linkage2 == LINKAGE_EXPLICIT_EXTERNAL)
        return LINKAGE_INTERNAL;

    // If either is an implicit external, then the result is an implicit external
    if (
            (linkage1 == LINKAGE_IMPLICIT_EXTERNAL && linkage2 == LINKAGE_EXPLICIT_EXTERNAL) ||
            (linkage1 == LINKAGE_EXPLICIT_EXTERNAL && linkage2 == LINKAGE_IMPLICIT_EXTERNAL) ||
            (linkage1 == LINKAGE_IMPLICIT_EXTERNAL && linkage2 == LINKAGE_IMPLICIT_EXTERNAL))
        return LINKAGE_IMPLICIT_EXTERNAL;

    // Otherwise if either is explicit external, the result is an explicit external
    if (linkage1 == LINKAGE_EXPLICIT_EXTERNAL && linkage2 == LINKAGE_EXPLICIT_EXTERNAL)
        return LINKAGE_EXPLICIT_EXTERNAL;

    error("Mismatching linkage in redeclared identifier %s", cur_type_identifier);
}

// Parse a translation unit
void parse(void) {
    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)
            next();

        if (cur_token_is_type() || cur_token == TOK_IDENTIFIER || cur_token == TOK_MULTIPLY) {
            // Variable or function definition

            BaseType *base_type;

            if (cur_token == TOK_IDENTIFIER || cur_token == TOK_MULTIPLY) {
                // Implicit int
                base_type = wmalloc(sizeof(BaseType));
                base_type->type = new_type(TYPE_INT);
                base_type->storage_class = SC_NONE;
            }
            else
                base_type = parse_declaration_specifiers();

            while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
                cur_type_identifier = 0;

                if (base_type->storage_class == SC_AUTO)
                    error("auto not allowed in global scope");
                if (base_type->storage_class == SC_REGISTER)
                    error("register not allowed in global scope");

                Type *type = concat_base_type(parse_declarator(), dup_base_type(base_type));

                if (!cur_type_identifier) error("Expected an identifier");

                Symbol *original_symbol = lookup_symbol(cur_type_identifier, global_scope, 1);
                Symbol *symbol;
                if (!original_symbol) {
                    // Create a new symbol if it wasn't already declared.

                    symbol = new_symbol(cur_type_identifier);
                    symbol->type = dup_type(type);
                    symbol->global_identifier = cur_type_identifier;
                }
                else
                    symbol = original_symbol;

                if ((symbol->type->type == TYPE_FUNCTION) != (type->type == TYPE_FUNCTION))
                    error("%s redeclared as different kind of symbol", cur_type_identifier);

                int linkage =
                    base_type->storage_class == SC_STATIC ? LINKAGE_INTERNAL
                    : base_type->storage_class == SC_EXTERN ? LINKAGE_EXPLICIT_EXTERNAL
                    : LINKAGE_IMPLICIT_EXTERNAL;

                if (original_symbol)
                    linkage = redefined_symbol_linkage(original_symbol->linkage, linkage, cur_type_identifier);

                symbol->linkage = linkage;
                if (type->type == TYPE_FUNCTION) {
                    int is_definition = parse_function(type, linkage, symbol, original_symbol);
                    if (is_definition) break; // No more declarations are possible, break out of comma parsing loop
                }
                else if (cur_token == TOK_EQ) {
                    if (original_symbol && original_symbol->initializers) error("Redefinition of %s", cur_type_identifier);

                    BaseType *old_base_type = base_type;
                    base_type = 0;
                    next();
                    Value *v = make_global_symbol_value(symbol);

                    if (v->type->type == TYPE_STRUCT_OR_UNION && is_incomplete_type(v->type))
                        error("Attempt to use an incomplete struct or union in an initializer");

                    parse_initializer(type_iterator(v->type), v, 0);
                    symbol->type = v->type;
                    base_type = old_base_type;
                }

                else {
                    // Non-function

                    if (original_symbol) {
                        // Merge types if it's a redeclaration
                        if (!types_are_compatible(type, original_symbol->type)) error("Incompatible types");
                        symbol->type = composite_type(type, original_symbol->type);
                    }
                    else
                        symbol->type = type;

                }

                parse_attributes();

                if (cur_token != TOK_SEMI) consume(TOK_COMMA, ", or ; in declaration");
            }

            if (cur_token == TOK_SEMI) next();
        }

        else if (cur_token == TOK_TYPEDEF) {
            parse_typedef();
            consume(TOK_SEMI, ";");
        }

        else error("Expected global declaration or function");
    }
}

void dump_symbols(void) {
    printf("Symbols:\n");

    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
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

void init_parser(void) {
    init_scopes();

    string_literals = wmalloc(sizeof(StringLiteral) * MAX_STRING_LITERALS);
    string_literal_count = 0;

    all_structs_and_unions = wmalloc(sizeof(struct struct_or_union_desc *) * MAX_STRUCTS_AND_UNIONS);
    all_structs_and_unions_count = 0;

    all_typedefs = wmalloc(sizeof(struct typedef_desc *) * MAX_TYPEDEFS);
    all_typedefs_count = 0;

    init_value_stack();

    label_count = 0;
    local_static_symbol_count = 0;

    controlling_case_value = 0;
}

void free_parser(void) {
    free(string_literals);
    free(all_structs_and_unions);
    free(all_typedefs);
    free_value_stack();
    free_scopes();
}
