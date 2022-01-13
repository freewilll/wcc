#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static int integers_are_longs;

static Value *indirect(Value *value) {
    if (!is_pointer_type(value->type)) error("Cannot dereference a non-pointer");

    value->type = deref_pointer(value->type);

    return value;
}

Value *evaluate_const_unary_int_operation(int operation, Value *value) {
    long r;

         if (operation == IR_BNOT) r = ~value->int_value;
    else if (operation == IR_LNOT) r = !value->int_value;
    else if (operation == IR_ADD)  r = +value->int_value;
    else if (operation == IR_SUB)  r = -value->int_value;
    else return 0;

    // Perform integer promotions everywhere except for logical not,
    Value *v = new_value();
    v->type = IR_LNOT ? new_type(TYPE_INT) : integer_promote_type(value->type);
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
    v->type = IR_LNOT ? new_type(TYPE_INT) : value->type;
    v->is_constant = 1;
    if (is_int) v->int_value = i; else v->fp_value = ld;

    return v;
}

Value *evaluate_const_binary_int_operation(int operation, Value *src1, Value *src2) {
    int is_unsigned = src1->type->is_unsigned;
    int is_long = src1->type->type == TYPE_LONG || src2->type->type == TYPE_LONG;

    int ui =  is_unsigned && !is_long;
    int ul =  is_unsigned &&  is_long;
    int si = !is_unsigned && !is_long;
    int sl = !is_unsigned &&  is_long;

    unsigned int  s1ui = (unsigned int)  src1->int_value;
    unsigned long s1ul = (unsigned long) src1->int_value;
    int           s1si = (int)           src1->int_value;
    long          s1sl = (long)          src1->int_value;

    unsigned int  s2ui = (unsigned int)  src2->int_value;
    unsigned long s2ul = (unsigned long) src2->int_value;
    int           s2si = (int)           src2->int_value;
    long          s2sl = (long)          src2->int_value;

    unsigned long r = 0;

    int divisor = ui ? s2ui : ul ? s2ul : si ? s2si : s2sl;
    if (operation == IR_DIV && divisor == 0) error("Division by zero");
    if (operation == IR_MOD && divisor == 0) error("Modulus by zero");

         if (operation == IR_ADD ) { if (ui) r = s1ui +  s2ui; else if (ul) r = s1ul +  s2ul; else if (si) r = s1si +  s2si; else if (sl) r = s1sl +  s2sl; }
    else if (operation == IR_SUB ) { if (ui) r = s1ui -  s2ui; else if (ul) r = s1ul -  s2ul; else if (si) r = s1si -  s2si; else if (sl) r = s1sl -  s2sl; }
    else if (operation == IR_MUL ) { if (ui) r = s1ui *  s2ui; else if (ul) r = s1ul *  s2ul; else if (si) r = s1si *  s2si; else if (sl) r = s1sl *  s2sl; }
    else if (operation == IR_DIV ) { if (ui) r = s1ui /  s2ui; else if (ul) r = s1ul /  s2ul; else if (si) r = s1si /  s2si; else if (sl) r = s1sl /  s2sl; }
    else if (operation == IR_MOD ) { if (ui) r = s1ui %  s2ui; else if (ul) r = s1ul %  s2ul; else if (si) r = s1si %  s2si; else if (sl) r = s1sl %  s2sl; }
    else if (operation == IR_LAND) { if (ui) r = s1ui &  s2ui; else if (ul) r = s1ul && s2ul; else if (si) r = s1si &  s2si; else if (sl) r = s1sl && s2sl; }
    else if (operation == IR_LOR ) { if (ui) r = s1ui |  s2ui; else if (ul) r = s1ul || s2ul; else if (si) r = s1si |  s2si; else if (sl) r = s1sl || s2sl; }
    else if (operation == IR_BAND) { if (ui) r = s1ui &  s2ui; else if (ul) r = s1ul &  s2ul; else if (si) r = s1si &  s2si; else if (sl) r = s1sl &  s2sl; }
    else if (operation == IR_BOR ) { if (ui) r = s1ui |  s2ui; else if (ul) r = s1ul |  s2ul; else if (si) r = s1si |  s2si; else if (sl) r = s1sl |  s2sl; }
    else if (operation == IR_XOR ) { if (ui) r = s1ui ^  s2ui; else if (ul) r = s1ul ^  s2ul; else if (si) r = s1si ^  s2si; else if (sl) r = s1sl ^  s2sl; }
    else if (operation == IR_BSHL) { if (ui) r = s1ui << s2ui; else if (ul) r = s1ul << s2ul; else if (si) r = s1si << s2si; else if (sl) r = s1sl << s2sl; }
    else if (operation == IR_BSHR) { if (ui) r = s1ui >> s2ui; else if (ul) r = s1ul >> s2ul; else if (si) r = s1si >> s2si; else if (sl) r = s1sl >> s2sl; }
    else if (operation == IR_ASHR) { if (ui) r = s1ui >> s2ui; else if (ul) r = s1ul >> s2ul; else if (si) r = s1si >> s2si; else if (sl) r = s1sl >> s2sl; }
    else if (operation == IR_EQ  ) { if (ui) r = s1ui == s2ui; else if (ul) r = s1ul == s2ul; else if (si) r = s1si == s2si; else if (sl) r = s1sl == s2sl; }
    else if (operation == IR_NE  ) { if (ui) r = s1ui != s2ui; else if (ul) r = s1ul != s2ul; else if (si) r = s1si != s2si; else if (sl) r = s1sl != s2sl; }
    else if (operation == IR_LT  ) { if (ui) r = s1ui <  s2ui; else if (ul) r = s1ul <  s2ul; else if (si) r = s1si <  s2si; else if (sl) r = s1sl <  s2sl; }
    else if (operation == IR_GT  ) { if (ui) r = s1ui >  s2ui; else if (ul) r = s1ul >  s2ul; else if (si) r = s1si >  s2si; else if (sl) r = s1sl >  s2sl; }
    else if (operation == IR_LE  ) { if (ui) r = s1ui <= s2ui; else if (ul) r = s1ul <= s2ul; else if (si) r = s1si <= s2si; else if (sl) r = s1sl <= s2sl; }
    else if (operation == IR_GE  ) { if (ui) r = s1ui >= s2ui; else if (ul) r = s1ul >= s2ul; else if (si) r = s1si >= s2si; else if (sl) r = s1sl >= s2sl; }
    else return 0;

    Value *v = new_value();
    v->type = operation_type(src1, src2, 0);
    v->type->is_unsigned = is_unsigned;
    v->is_constant = 1;
    v->int_value = r;

    return v;
}

Value* evaluate_const_binary_fp_operation(int operation, Value *src1, Value *src2, Type *type) {
    int is_int;
    unsigned int i;
    long double ld;

         if (operation == IR_ADD) { is_int = 0; ld = src1->fp_value +  src2->fp_value; }
    else if (operation == IR_SUB) { is_int = 0; ld = src1->fp_value -  src2->fp_value; }
    else if (operation == IR_MUL) { is_int = 0; ld = src1->fp_value *  src2->fp_value; }
    else if (operation == IR_DIV) { is_int = 0; ld = src1->fp_value /  src2->fp_value; }
    else if (operation == IR_EQ ) { is_int = 1; i =  src1->fp_value == src2->fp_value; }
    else if (operation == IR_NE ) { is_int = 1; i =  src1->fp_value != src2->fp_value; }
    else if (operation == IR_LT ) { is_int = 1; i =  src1->fp_value <  src2->fp_value; }
    else if (operation == IR_GT ) { is_int = 1; i =  src1->fp_value >  src2->fp_value; }
    else if (operation == IR_LE ) { is_int = 1; i =  src1->fp_value <= src2->fp_value; }
    else if (operation == IR_GE ) { is_int = 1; i =  src1->fp_value >= src2->fp_value; }
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

    // Implicit else: integer -> integer
    // int_value is already sign extended to a long, so this doesn't have to be done again.
    // The only thing that needs doing is truncation to the target type, if it's smaller
    // than a long.
         if (dst_type->type == TYPE_CHAR  && !dst_type->is_unsigned) dst->int_value = (         char ) src->int_value;
    else if (dst_type->type == TYPE_CHAR  &&  dst_type->is_unsigned) dst->int_value = (unsigned char ) src->int_value;
    else if (dst_type->type == TYPE_SHORT && !dst_type->is_unsigned) dst->int_value = (         short) src->int_value;
    else if (dst_type->type == TYPE_SHORT &&  dst_type->is_unsigned) dst->int_value = (unsigned short) src->int_value;
    else if (dst_type->type == TYPE_INT   && !dst_type->is_unsigned) dst->int_value = (         int  ) src->int_value;
    else if (dst_type->type == TYPE_INT   &&  dst_type->is_unsigned) dst->int_value = (unsigned int  ) src->int_value;
    else                                                             dst->int_value =                  src->int_value;

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

static Value *parse_binary_expression(int operation, int second_level, Value *value1) {
    next();
    Value *value2 = parse_constant_expression(second_level);
    check_binary_operation_types(operation, value1, value2);

    if ((operation == IR_ADD || operation == IR_SUB) && (is_pointer_type(value1->type) || is_pointer_type(value2->type)) || value1->is_string_literal || value2->is_string_literal)
        return parse_address_plus_minus(operation, value1, value2);

    Type *result_type = operation_type(value1, value2, 0);
    if (is_floating_point_type(result_type))
        return evaluate_const_binary_fp_operation(operation, value1, value2, result_type);
    else
        return evaluate_const_binary_int_operation(operation, value1, value2);
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

        case TOK_INTEGER:
            value = new_integral_constant(integers_are_longs ? TYPE_LONG : cur_lexer_type->type, cur_long);
            next();
            break;

        case TOK_FLOATING_POINT_NUMBER:
            value = new_floating_point_constant(cur_lexer_type->type, cur_long_double);
            next();
            break;

        case TOK_STRING_LITERAL:
            value = make_string_literal_value_from_cur_string_literal();
            next();
            break;

        case TOK_IDENTIFIER: {
            Symbol *symbol = lookup_symbol(cur_identifier, cur_scope, 1);
            if (!symbol) error("Unknown symbol \"%s\"", cur_identifier);

            next();

            if (symbol->is_enum_value)
                value = new_integral_constant(TYPE_INT, symbol->value);
            else if (symbol->type->type == TYPE_FUNCTION) {
                // Convert a function to a &function
                value = make_symbol_value(symbol);
                value->type = make_pointer(value->type);
                value->is_address_of = 1;
            }
            else
                value = make_symbol_value(symbol);

            value->global_symbol = symbol;

            break;
        }

        case TOK_SIZEOF: {
            int size = parse_sizeof(parse_constant_expression);
            value = new_integral_constant(TYPE_LONG, size);
            break;
        }

        default:
            error("Unexpected token %d in constant expression", cur_token);
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

Value *parse_constant_integer_expression(int all_longs) {
    integers_are_longs = all_longs;

    Value *value = parse_constant_expression(TOK_EQ);
    if (!value->is_constant) error("Expected a constant expression");
    if (!is_integer_type(value->type)) error("Expected an integer constant expression");
    return value;
}
