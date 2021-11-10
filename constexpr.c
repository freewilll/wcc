#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

Type *common_const_expression_type(Value *src1, Value *src2) {
    if (src1->type->type == TYPE_LONG_DOUBLE || src2->type->type == TYPE_LONG_DOUBLE)
        return new_type(TYPE_LONG_DOUBLE);
    else if (src1->type->type == TYPE_DOUBLE || src2->type->type == TYPE_DOUBLE)
        return new_type(TYPE_DOUBLE);
    else if (src1->type->type == TYPE_FLOAT || src2->type->type == TYPE_FLOAT)
        return new_type(TYPE_FLOAT);
    else if (src1->type->type <= TYPE_INT && src2->type->type <= TYPE_INT)
        return new_type(TYPE_INT);
    else
        return new_type(TYPE_LONG);
}

Value *evaluate_const_unary_int_operation(int operation, Value *value) {
    long r;

         if (operation == IR_BNOT) r = ~value->int_value;
    else if (operation == IR_LNOT) r = !value->int_value;
    else if (operation == IR_ADD)  r = +value->int_value;
    else if (operation == IR_SUB)  r = -value->int_value;
    else return 0;

    Value *v = new_value();
    v->type = new_type(TYPE_LONG);
    v->type->is_unsigned = value->type->is_unsigned;
    v->is_constant = 1;
    v->int_value = r;

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
    v->type = new_type(TYPE_LONG);
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
    v->type = is_int ? new_type(TYPE_LONG) : type;
    v->is_constant = 1;
    if (is_int) v->int_value = i; else v->fp_value = ld;

    return v;
}

Value *cast_constant_value(Value *src, Type *dst_type) {
    if (types_are_compatible(src->type, dst_type)) return src;

    Type *src_type = src->type;

    Value *dst = new_value();
    dst->is_constant = 1;
    dst->type = dst_type;

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
    return evaluate_const_unary_int_operation(operation, value);
}

static Value *parse_binary_expression(int operation, int second_level, Value *value1) {
    next();
    Value *value2 = parse_constant_expression(second_level);
    check_binary_operation_types(operation, value1, value2);
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

        case TOK_AMPERSAND:
            panic("TODO constant expressions of addresses TOK_AMPERSAND");
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

                if (!is_arithmetic_type(value->type))
                    panic("Can only cast from arithmetic types in an integer constant");
                if (!is_integer_type(type))
                    panic("Can only cast to integer types in an integer constant");

                value = cast_constant_value(value, type);
            }
            else {
                // (sub expression)
                value = parse_constant_expression(TOK_COMMA);
                consume(TOK_RPAREN, ")");
            }
            break;

        case TOK_INTEGER:
            value = new_integral_constant(cur_lexer_type->type, cur_long);
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
            if (!symbol) panic("Unknown symbol \"%s\"", cur_identifier);

            next();

            if (symbol->is_enum_value)
                value = new_integral_constant(TYPE_INT, symbol->value);
            else
                value = make_symbol_value(symbol);

            break;
        }

        case TOK_SIZEOF: {
            int size = parse_sizeof(parse_constant_expression);
            value = new_integral_constant(TYPE_LONG, size);
            break;
        }

        default:
            panic("Unexpected token %d in constant expression", cur_token);
    }

    while (cur_token >= level) {
        switch (cur_token) {
            // In order of descending operator precedence

            case TOK_LBRACKET:
                panic("TODO constant expressions of addresses TOK_LBRACKET");
                break;

            case TOK_DOT:
                panic("TODO constant expressions of addresses TOK_DOT");
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

Value *parse_constant_integer_expression() {
    Value *value = parse_constant_expression(TOK_EQ);
    if (!value->is_constant) panic("Expected a constant expression");
    if (!is_integer_type(value->type)) panic("Expected an integer constant expression");
    return value;
}
