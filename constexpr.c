#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static int global_override_integer_type;

void set_global_override_integer_type(int type) {
    global_override_integer_type = type;
}

// Converts a a non-int128 constant to an int128 one.
// v is modified in place. It's type is ignored; original_type is used
// to determine signedness.
void extend_int_constant_to_int128(Value *v, Type *original_type) {
    if (!original_type->is_unsigned)
        v->int128_value =v->int_value; // Sign extends
    else
        v->int128_value = (unsigned long) v->int_value; // Truncates
}

static Value *indirect(Value *value) {
    if (!is_pointer_type(value->type)) error("Cannot dereference a non-pointer");

    value->type = deref_pointer(value->type);

    return value;
}

static Value *evaluate_const_unary_int128_operation(int operation, Value *value) {
    __int128 r;

    if (operation == IR_LNOT || operation == IR_BNOT)
        panic("int128 operation not implemented in evaluate_const_unary_int128_operation()");

         if (operation == IR_ADD) r = +value->int128_value;
    else if (operation == IR_SUB) r = -value->int128_value;
    else return 0;

    // Perform integer promotions everywhere except for logical not,
    Value *v = new_value();
    v->type = new_type(TYPE_INT128);
    v->type->is_unsigned = value->type->is_unsigned;
    v->is_constant = 1;
    v->int128_value = r;

    return v;
}

static Value *evaluate_const_unary_int_operation(int operation, Value *value) {
    if (value->type->type == TYPE_INT128)
        return evaluate_const_unary_int128_operation(operation, value);

    long r;

         if (operation == IR_BNOT) r = ~value->int_value;
    else if (operation == IR_LNOT) r = !value->int_value;
    else if (operation == IR_ADD)  r = +value->int_value;
    else if (operation == IR_SUB)  r = -value->int_value;
    else return 0;

    // Perform integer promotions everywhere except for logical not,
    Value *v = new_value();
    v->type = operation == IR_LNOT ? new_type(TYPE_INT) : integer_promote_type(value->type);
    v->type->is_unsigned = value->type->is_unsigned;
    v->is_constant = 1;
    v->int_value = r;

    return v;
}

Value *evaluate_const_unary_fp_operation(int operation, Value *value) {
    int is_int;
    unsigned int i;
    long double ld;

         if (operation == IR_LNOT) { is_int = 1; i = !value->fp_value; }
    else if (operation == IR_ADD)  { is_int = 0; ld = +value->fp_value; }
    else if (operation == IR_SUB)  { is_int = 0; ld = -value->fp_value; }
    else return 0;

    // Perform integer promotions everywhere except for logical not,
    Value *v = new_value();
    v->type = operation == IR_LNOT ? new_type(TYPE_INT) : value->type;
    v->is_constant = 1;
    if (is_int) v->int_value = i; else v->fp_value = ld;

    return v;
}

// Cast src1 and src2 operands and evaluate a int128 operation
// Not all int128 operations are implemented, so this list isn't complete.
#define EVAL_INT128_BINOP_WITH_CAST(src, dst, cast) \
         if (operation == IR_ADD ) { r = (cast) src +  (cast) dst; } \
    else if (operation == IR_SUB ) { r = (cast) src -  (cast) dst; } \
    else if (operation == IR_MUL ) { r = (cast) src *  (cast) dst; } \
    else if (operation == IR_BAND) { r = (cast) src &  (cast) dst; } \
    else if (operation == IR_BOR ) { r = (cast) src |  (cast) dst; } \
    else if (operation == IR_XOR ) { r = (cast) src ^  (cast) dst; } \
    else if (operation == IR_BSHL) { r = (cast) src <<        dst; } \
    else if (operation == IR_BSHR) { r = (cast) src >>        dst; } \
    else if (operation == IR_ASHR) { r = (cast) src >>        dst; } \
    else if (operation == IR_EQ  ) { r = (cast) src == (cast) dst; } \
    else if (operation == IR_NE  ) { r = (cast) src != (cast) dst; } \
    else if (operation == IR_LT  ) { r = (cast) src <  (cast) dst; } \
    else if (operation == IR_GT  ) { r = (cast) src >  (cast) dst; } \
    else if (operation == IR_LE  ) { r = (cast) src <= (cast) dst; } \
    else if (operation == IR_GE  ) { r = (cast) src >= (cast) dst; } \
    else return 0

Value *evaluate_const_binary_int128_operation(int operation, Value *src1, Value *src2) {
    if (operation == IR_DIV || operation == IR_MOD || operation == IR_LAND || operation == IR_LOR || operation == IR_LAND)
        panic("int128 operation not implemented in evaluate_const_binary_int128_operation()");

    if (operation == IR_DIV && src1->int128_value == 0) error("Division by zero");
    if (operation == IR_MOD && src1->int128_value == 0) error("Modulus by zero");

    int is_unsigned = src1->type->is_unsigned;

    unsigned __int128 r = 0;

         if ( is_unsigned) EVAL_INT128_BINOP_WITH_CAST(src1->int128_value, src2->int128_value, unsigned __int128);
    else if (!is_unsigned) EVAL_INT128_BINOP_WITH_CAST(src1->int128_value, src2->int128_value, signed __int128);

    Value *v = new_value();
    v->type = operation_type(src1, src2, 0);
    v->type->is_unsigned = is_unsigned;
    v->is_constant = 1;
    v->int128_value = r;

    return v;
}

// Cast src1 and src2 operands and evaluate a non-int128 operation
#define EVAL_INT_BINOP_WITH_CAST(src, dst, cast) \
         if (operation == IR_ADD ) { r = (cast) src +  (cast) dst; } \
    else if (operation == IR_SUB ) { r = (cast) src -  (cast) dst; } \
    else if (operation == IR_MUL ) { r = (cast) src *  (cast) dst; } \
    else if (operation == IR_DIV ) { r = (cast) src /  (cast) dst; } \
    else if (operation == IR_MOD ) { r = (cast) src %  (cast) dst; } \
    else if (operation == IR_LAND) { r = (cast) src &  (cast) dst; } \
    else if (operation == IR_LOR ) { r = (cast) src |  (cast) dst; } \
    else if (operation == IR_BAND) { r = (cast) src &  (cast) dst; } \
    else if (operation == IR_BOR ) { r = (cast) src |  (cast) dst; } \
    else if (operation == IR_XOR ) { r = (cast) src ^  (cast) dst; } \
    else if (operation == IR_BSHL) { r = (cast) src <<        dst; } \
    else if (operation == IR_BSHR) { r = (cast) src >>        dst; } \
    else if (operation == IR_ASHR) { r = (cast) src >>        dst; } \
    else if (operation == IR_EQ  ) { r = (cast) src == (cast) dst; } \
    else if (operation == IR_NE  ) { r = (cast) src != (cast) dst; } \
    else if (operation == IR_LT  ) { r = (cast) src <  (cast) dst; } \
    else if (operation == IR_GT  ) { r = (cast) src >  (cast) dst; } \
    else if (operation == IR_LE  ) { r = (cast) src <= (cast) dst; } \
    else if (operation == IR_GE  ) { r = (cast) src >= (cast) dst; } \
    else return 0

Value *evaluate_const_binary_int_operation(int operation, Value *src1, Value *src2) {
    if (src1->type->type == TYPE_INT128 || src2->type->type == TYPE_INT128)
        return evaluate_const_binary_int128_operation(operation, src1, src2);

    if (operation == IR_DIV && src1->int_value == 0) error("Division by zero");
    if (operation == IR_MOD && src1->int_value == 0) error("Modulus by zero");

    Type *result_type = operation_type(src1, src2, 0);
    int is_unsigned = result_type->is_unsigned;
    int is_long = result_type->type == TYPE_LONG;

    // Pointers behave like unsigned longs
    if (src1->type->type == TYPE_PTR || src2->type->type == TYPE_PTR) {
        is_unsigned = 1;
        is_long = 1;
    }

    unsigned long r = 0;

         if ( is_unsigned && !is_long) EVAL_INT_BINOP_WITH_CAST(src1->int_value, src2->int_value, unsigned int);
    else if ( is_unsigned &&  is_long) EVAL_INT_BINOP_WITH_CAST(src1->int_value, src2->int_value, unsigned long);
    else if (!is_unsigned && !is_long) EVAL_INT_BINOP_WITH_CAST(src1->int_value, src2->int_value, signed int);
    else if (!is_unsigned &&  is_long) EVAL_INT_BINOP_WITH_CAST(src1->int_value, src2->int_value, signed long);

    Value *v = new_value();
    v->type = operation_type(src1, src2, 0);
    v->type->is_unsigned = is_unsigned;
    v->is_constant = 1;
    v->int_value = r;

    return v;
}

#define CONVERT_FP_TO_INT(src, dst) { \
    if (src->type->is_unsigned) \
        dst = (unsigned long) src->int_value; \
    else \
        dst = (long) src->int_value; \
}

// Evaluate an expression that results in a FP value.
// One of the operands could be an integer.
Value* evaluate_const_binary_fp_operation(int operation, Value *src1, Value *src2, Type *type) {
    int is_int;
    unsigned int i;
    long double ld;

    long double vsrc1 = src1->fp_value;
    long double vsrc2 = src2->fp_value;

    // Convert integers to FP
    if (is_integer_type(src1->type)) CONVERT_FP_TO_INT(src1, vsrc1);
    if (is_integer_type(src2->type)) CONVERT_FP_TO_INT(src2, vsrc2);

         if (operation == IR_ADD) { is_int = 0; ld = vsrc1 +  vsrc2; }
    else if (operation == IR_SUB) { is_int = 0; ld = vsrc1 -  vsrc2; }
    else if (operation == IR_MUL) { is_int = 0; ld = vsrc1 *  vsrc2; }
    else if (operation == IR_DIV) { is_int = 0; ld = vsrc1 /  vsrc2; }
    else if (operation == IR_EQ ) { is_int = 1; i =  vsrc1 == vsrc2; }
    else if (operation == IR_NE ) { is_int = 1; i =  vsrc1 != vsrc2; }
    else if (operation == IR_LT ) { is_int = 1; i =  vsrc1 <  vsrc2; }
    else if (operation == IR_GT ) { is_int = 1; i =  vsrc1 >  vsrc2; }
    else if (operation == IR_LE ) { is_int = 1; i =  vsrc1 <= vsrc2; }
    else if (operation == IR_GE ) { is_int = 1; i =  vsrc1 >= vsrc2; }
    else return 0;

    Value *v = new_value();
    v->type = is_int ? new_type(TYPE_INT) : type;
    v->is_constant = 1;
    if (is_int) v->int_value = i; else v->fp_value = ld;

    return v;
}

Value *cast_constant_value(Value *src, Type *dst_type) {
    if (types_are_compatible(src->type, dst_type)) return src;

    Type *src_type = src->type;

    Value *dst = new_value();
    dst->is_constant = 1;
    dst->address_of_offset = src->address_of_offset;
    dst->int_value = src->int_value;
    dst->type = dst_type;
    dst->global_symbol = src->global_symbol;
    dst->is_address_of = src->is_address_of;
    dst->is_string_literal = src->is_string_literal;
    dst->string_literal_index = src->string_literal_index;

    // FP -> FP
    // Don't bother changing precision
    if (is_floating_point_type(src_type) && is_floating_point_type(dst_type)) {
        dst->fp_value = src->fp_value;
        return dst;
    }

    // FP -> integer
    if (is_floating_point_type(src_type) && !is_floating_point_type(dst_type)) {
        dst->int_value = src->fp_value;

        // For compatibililty with gcc when it comes to overflows
             if (dst_type->type == TYPE_CHAR  && !dst_type->is_unsigned && src->fp_value >        0xff) dst->int_value = 0x7f;
        else if (dst_type->type == TYPE_CHAR  &&  dst_type->is_unsigned && src->fp_value >        0xff) dst->int_value = 0xff;
             if (dst_type->type == TYPE_CHAR  && !dst_type->is_unsigned && src->fp_value <       -0x7f) dst->int_value = -0x80;
        else if (dst_type->type == TYPE_CHAR  &&  dst_type->is_unsigned && src->fp_value <       -0xff) dst->int_value = 0;
             if (dst_type->type == TYPE_SHORT && !dst_type->is_unsigned && src->fp_value >      0xffff) dst->int_value = 0x7fff;
        else if (dst_type->type == TYPE_SHORT &&  dst_type->is_unsigned && src->fp_value >      0xffff) dst->int_value = 0xffff;
             if (dst_type->type == TYPE_SHORT && !dst_type->is_unsigned && src->fp_value <     -0x7fff) dst->int_value = -0x8000;
        else if (dst_type->type == TYPE_SHORT &&  dst_type->is_unsigned && src->fp_value <     -0xffff) dst->int_value = 0;
             if (dst_type->type == TYPE_INT   && !dst_type->is_unsigned && src->fp_value >  0xffffffff) dst->int_value = 0x7fffffff;
        else if (dst_type->type == TYPE_INT   &&  dst_type->is_unsigned && src->fp_value >  0xffffffff) dst->int_value = 0xffffffff;
             if (dst_type->type == TYPE_INT   && !dst_type->is_unsigned && src->fp_value < -0x7fffffff) dst->int_value = -0x80000000;
        else if (dst_type->type == TYPE_INT   &&  dst_type->is_unsigned && src->fp_value < -0xffffffff) dst->int_value = 0;

        return dst;
    }

    // Integer -> FP
    if (is_floating_point_type(dst_type) && !is_floating_point_type(src_type)) {
        if (src_type->is_unsigned) dst->fp_value = (unsigned long) src->int_value;
        else dst->fp_value = src->int_value;
    }

    // Implicit else: integer -> integer, including int128.
    // If a signed int, the int_value is already sign extended
    // If a signed int128, the int128_value is already sign extended
    // For non-int128, take care of extending a smaller type to the full long int_value
    // For int128, take care of extending a smaller type to the full intt128 int128_value
         if (dst_type->type == TYPE_CHAR   && !dst_type->is_unsigned) dst->int_value    = (signed   char ) src->int_value; // Sign extends
    else if (dst_type->type == TYPE_CHAR   &&  dst_type->is_unsigned) dst->int_value    = (unsigned char ) src->int_value; // Truncates
    else if (dst_type->type == TYPE_SHORT  && !dst_type->is_unsigned) dst->int_value    = (         short) src->int_value; // Sign extends
    else if (dst_type->type == TYPE_SHORT  &&  dst_type->is_unsigned) dst->int_value    = (unsigned short) src->int_value; // Truncates
    else if (dst_type->type == TYPE_INT    && !dst_type->is_unsigned) dst->int_value    = (         int  ) src->int_value; // Sign extends
    else if (dst_type->type == TYPE_INT    &&  dst_type->is_unsigned) dst->int_value    = (unsigned int  ) src->int_value; // Truncates

    // Check for a cast upwards from a non-128 to an int128
    if (dst_type->type == TYPE_INT128 && src_type->type != TYPE_INT128) {
        dst->int_value = src->int_value;
        extend_int_constant_to_int128(dst, src_type);
    }

    return dst;
}

static Value *parse_unary_expression(int operation) {
    next();
    Value *value = parse_constant_expression(TOK_INC);
    check_unary_operation_type(operation, value);
    if (is_floating_point_type(value->type))
        return evaluate_const_unary_fp_operation(operation, value);
    else
        return evaluate_const_unary_int_operation(operation, value);
}

static Value *parse_address_plus_minus(int operation, Value *value1, Value *value2) {
    if (value1->is_string_literal) value1->type = decay_array_to_pointer(value1->type);
    if (value2->is_string_literal) value2->type = decay_array_to_pointer(value2->type);

    if (operation == IR_ADD) check_plus_operation_type(value1, value2, 0);
    else if (operation == IR_SUB) check_minus_operation_type(value1, value2);

    Value *result;

    if (operation == IR_ADD) {
        int arithmetic_value;
        if (is_pointer_type(value1->type)) {
            result = dup_value(value1);
            arithmetic_value = value2->int_value;
        }
        else {
            result = dup_value(value2);
            arithmetic_value = value1->int_value;
        }

        int size = arithmetic_value * get_type_size(result->type->target);

        if (result->is_address_of || result->is_string_literal)
            result->address_of_offset += size;
        else
            result->int_value += size;
    }
    else {
        if (is_pointer_type(value2->type)) {
            // Pointer-pointer subtraction
            int size =  get_type_size(value1->type->target);

            if (value1->global_symbol != value2->global_symbol)
                error("Operand mismatch in constant pointer subtraction");

            int difference = value1->address_of_offset + value1->int_value - value2->address_of_offset - value2->int_value;
            result = new_integral_constant(TYPE_LONG, difference / size);

        }
        else {
            result = dup_value(value1);
            int size =  value2->int_value * get_type_size(result->type->target);
            if (result->is_address_of || result->is_string_literal)
                result->address_of_offset -= size;
            else
                result->int_value -= size;
        }
    }

    return result;
}

Value *parse_binary_constant_expression(int operation, Value *value1, Value *value2) {
    Type *result_type = operation_type(value1, value2, 0);
    if (is_floating_point_type(result_type))
        return evaluate_const_binary_fp_operation(operation, value1, value2, result_type);
    else
        return evaluate_const_binary_int_operation(operation, value1, value2);
}

static Value *parse_binary_expression(int operation, int second_level, Value *value1) {
    next();
    Value *value2 = parse_constant_expression(second_level);
    check_binary_operation_types(operation, value1, value2);

    if ((operation == IR_ADD || operation == IR_SUB) && (is_pointer_type(value1->type) || is_pointer_type(value2->type)) || value1->is_string_literal || value2->is_string_literal)
        return parse_address_plus_minus(operation, value1, value2);

    return parse_binary_constant_expression(operation, value1, value2);
}

static Value *parse_ternary(Value *value) {
    next();
    int which = is_floating_point_type(value->type) ? !!value->fp_value : !!value->int_value;
    Value *value1 = parse_constant_expression(TOK_TERNARY);
    consume(TOK_COLON, ":");
    Value *value2 = parse_constant_expression(TOK_TERNARY);
    check_ternary_operation_types(value, value1, value2);
    return which ? value1 : value2;
}

// Parse a constant expression using top-down precedence climbing parsing
// https://en.cppreference.com/w/c/language/operator_precedence
// https://en.wikipedia.org/wiki/Operator-precedence_parser#Precedence_climbing_method
Value *parse_constant_expression(int level) {
    Value *value;

    switch(cur_token) {
        case TOK_LOGICAL_NOT: value = parse_unary_expression(IR_LNOT); break;
        case TOK_BITWISE_NOT: value = parse_unary_expression(IR_BNOT); break;

        case TOK_MULTIPLY:
            // Indirect
            next();
            value = parse_constant_expression(TOK_INC);
            value = indirect(value);

            break;

        case TOK_AMPERSAND:
            // Address of

            next();
            value = parse_constant_expression(TOK_INC);
            if (value->bit_field_size) error("Cannot take an address of a bit-field");

            if (!value->is_string_literal && !value->global_symbol && !value->is_constant)
                error("Illegal operand to & in constant expression");

            if (value->is_constant) {
                value->int_value += value->address_of_offset;
                value->is_constant = 0;
            }

            value->is_address_of = 1;

            // Hacky horror, only create a pointer unless it's already a pointer to a function
            if (!is_pointer_to_function_type(value->type))
                value->type = make_pointer(value->type);

            break;

        case TOK_PLUS:  value = parse_unary_expression(IR_ADD); break;
        case TOK_MINUS: value = parse_unary_expression(IR_SUB); break;

        case TOK_LPAREN:
            next();
            if (cur_token_is_type()) {
                // Cast
                Type *type = parse_type_name();
                consume(TOK_RPAREN, ")");
                value = parse_constant_expression(TOK_INC);
                value = cast_constant_value(value, type);
            }
            else {
                // (sub expression)
                value = parse_constant_expression(TOK_COMMA);
                consume(TOK_RPAREN, ")");
            }
            break;

        case TOK_INTEGER: {
            value = new_integral_constant(global_override_integer_type ? global_override_integer_type : cur_lexer_type->type, cur_long);
            value->type->is_unsigned = cur_lexer_type->is_unsigned;

            if (global_override_integer_type == TYPE_INT128) {
                value->type->type = TYPE_INT128;
                value->int128_value = cur_long; // Set the upper 8 bytes to zero
            }

            next();
            break;
        }

        case TOK_FLOATING_POINT_NUMBER:
            value = new_floating_point_constant(cur_lexer_type->type, cur_long_double);
            next();
            break;

        case TOK_STRING_LITERAL:
            value = make_string_literal_value_from_cur_string_literal();
            next();
            break;

        case TOK_IDENTIFIER: {
            if (!cur_scope) panic("Attempt to lookup a symbol %s without cur_scope existing", cur_identifier);
            Symbol *symbol = lookup_symbol(cur_identifier, cur_scope, 1);
            if (!symbol) error("Unknown symbol \"%s\"", cur_identifier);

            next();

            if (symbol->is_enum_value) {
                value = new_integral_constant(TYPE_INT, symbol->value);
            }
            else if (symbol->type->type == TYPE_FUNCTION) {
                // Convert a function to a &function
                value = make_symbol_value(symbol);
                value->type = make_pointer(value->type);
                value->is_address_of = 1;
                value->global_symbol = symbol;
            }
            else {
                value = make_symbol_value(symbol);
                value->global_symbol = symbol;
            }

            break;
        }

        case TOK_SIZEOF: {
            int size = parse_sizeof(parse_constant_expression);
            value = new_integral_constant(TYPE_LONG, size);
            break;
        }

        default:
            error("Unexpected %s in constant expression", token_names[cur_token]);
    }

    while (cur_token >= level) {
        switch (cur_token) {
            // In order of descending operator precedence

            case TOK_LBRACKET:
                next();

                if (value->type->type != TYPE_PTR && value->type->type != TYPE_ARRAY)
                    error("Invalid operator [] on a non-pointer and non-array");

                Value *subscript_value = parse_constant_expression(TOK_COMMA);
                consume(TOK_RBRACKET, "]");

                if (!subscript_value->is_constant || !is_integer_type(subscript_value->type))
                    error("Expected an integer constant integer expression in []");

                value->address_of_offset += get_type_size(value->type->target) * subscript_value->int_value;
                value->type = value->type->target;

                break;

            case TOK_DOT:
            case TOK_ARROW:
                if (cur_token == TOK_DOT && value->type->type != TYPE_STRUCT_OR_UNION)
                    error("Can only use . on a struct or union");

                if (cur_token != TOK_DOT) {
                    if (!is_pointer_type(value->type)) error("Cannot use -> on a non-pointer");
                    if (value->type->target->type != TYPE_STRUCT_OR_UNION) error("Can only use -> on a pointer to a struct or union");
                    if (is_incomplete_type(value->type->target)) error("Dereferencing a pointer to incomplete struct or union");
                }

                int is_dot = cur_token == TOK_DOT;

                next();
                consume(TOK_IDENTIFIER, "identifier");

                if (!is_dot) value = indirect(value);

                StructOrUnionMember *member = lookup_struct_or_union_member(value->type, cur_identifier);

                value->address_of_offset += member->offset;
                value->bit_field_size = member->bit_field_size;
                value->type = dup_type(member->type);

                break;

            case TOK_MULTIPLY:       value = parse_binary_expression(IR_MUL,  TOK_DOT,          value); break;
            case TOK_DIVIDE:         value = parse_binary_expression(IR_DIV,  TOK_DOT,          value); break;
            case TOK_MOD:            value = parse_binary_expression(IR_MOD,  TOK_DOT,          value); break;
            case TOK_PLUS:           value = parse_binary_expression(IR_ADD,  TOK_MULTIPLY,     value); break;
            case TOK_MINUS:          value = parse_binary_expression(IR_SUB,  TOK_MULTIPLY,     value); break;
            case TOK_BITWISE_RIGHT:  value = parse_binary_expression(IR_BSHR, TOK_PLUS,         value); break;
            case TOK_BITWISE_LEFT:   value = parse_binary_expression(IR_BSHL, TOK_PLUS,         value); break;
            case TOK_LT:             value = parse_binary_expression(IR_LT,   TOK_BITWISE_RIGHT,value); break;
            case TOK_GT:             value = parse_binary_expression(IR_GT,   TOK_BITWISE_RIGHT,value); break;
            case TOK_LE:             value = parse_binary_expression(IR_LE,   TOK_BITWISE_RIGHT,value); break;
            case TOK_GE:             value = parse_binary_expression(IR_GE,   TOK_BITWISE_RIGHT,value); break;
            case TOK_DBL_EQ:         value = parse_binary_expression(IR_EQ,   TOK_LT,           value); break;
            case TOK_NOT_EQ:         value = parse_binary_expression(IR_NE,   TOK_LT,           value); break;
            case TOK_AMPERSAND:      value = parse_binary_expression(IR_BAND, TOK_DBL_EQ,       value); break;
            case TOK_XOR:            value = parse_binary_expression(IR_XOR,  TOK_AMPERSAND,    value); break;
            case TOK_BITWISE_OR:     value = parse_binary_expression(IR_BOR,  TOK_XOR,          value); break;
            case TOK_AND:            value = parse_binary_expression(IR_LAND, TOK_BITWISE_OR,   value); break;
            case TOK_OR:             value = parse_binary_expression(IR_LOR,  TOK_AND,          value); break;

            case TOK_TERNARY: {
                value = parse_ternary(value);
                break;
            }

            default:
                return value; // Bail once we hit something unknown
        }
    }

    return value;
}

Value *parse_constant_integer_expression(int override_integer_type) {
    set_global_override_integer_type(override_integer_type);

    Value *value = parse_constant_expression(TOK_EQ);
    if (!value->is_constant) error("Expected a constant expression");
    if (!is_integer_type(value->type)) error("Expected an integer constant expression");
    return value;
}
