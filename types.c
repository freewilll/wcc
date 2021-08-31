#include <stdio.h>
#include <stdlib.h>

#include "wcc.h"

int print_type(void *f, Type *type) {
    int len = 0;

    if (!type) {
        len += printf(f, "(no type!)");
        return len;
    }

    Type *t = type;
    while (t->type == TYPE_PTR) {
        len += fprintf(f, "*");
        t = t->target;
    }

    int tt = t->type;

    if (type->is_unsigned) len += fprintf(f, "unsigned ");

         if (tt == TYPE_VOID)        len += fprintf(f, "void");
    else if (tt == TYPE_CHAR)        len += fprintf(f, "char");
    else if (tt == TYPE_INT)         len += fprintf(f, "int");
    else if (tt == TYPE_SHORT)       len += fprintf(f, "short");
    else if (tt == TYPE_LONG)        len += fprintf(f, "long");
    else if (tt == TYPE_FLOAT)       len += fprintf(f, "float");
    else if (tt == TYPE_DOUBLE)      len += fprintf(f, "double");
    else if (tt == TYPE_LONG_DOUBLE) len += fprintf(f, "long double");
    else if (tt == TYPE_STRUCT)      len += fprintf(f, "struct %s", t->struct_desc->identifier);
    else len += fprintf(f, "unknown tt %d", tt);

    return len;
}

char *sprint_type_in_english(Type *type) {
    char *buffer = malloc(256);
    char *start = buffer;

    int tt;
    while (type) {
        tt = type->type;

             if (tt == TYPE_VOID)        buffer += sprintf(buffer, "void");
        else if (tt == TYPE_CHAR)        buffer += sprintf(buffer, "char");
        else if (tt == TYPE_INT)         buffer += sprintf(buffer, "int");
        else if (tt == TYPE_SHORT)       buffer += sprintf(buffer, "short");
        else if (tt == TYPE_LONG)        buffer += sprintf(buffer, "long");
        else if (tt == TYPE_FLOAT)       buffer += sprintf(buffer, "float");
        else if (tt == TYPE_DOUBLE)      buffer += sprintf(buffer, "double");
        else if (tt == TYPE_LONG_DOUBLE) buffer += sprintf(buffer, "long double");
        else if (tt == TYPE_STRUCT)      buffer += sprintf(buffer, "struct %s", type->struct_desc->identifier);
        else if (tt == TYPE_PTR)         buffer += sprintf(buffer, "pointer to ");
        else if (tt == TYPE_FUNCTION)    buffer += sprintf(buffer, "function returning ");
        else if (tt == TYPE_ARRAY) {
            if (type->array_size == 0) buffer += sprintf(buffer, "array of ");
            else buffer += sprintf(buffer, "array[%d] of ", type->array_size);
        }
        else panic1d("Unknown type->type=%d", tt);

        type = type->target;
    }

    return start;
}

Type *new_type(int type) {
    Type *result = malloc(sizeof(Type));
    result->type = type;
    result->is_unsigned = 0;
    result->target = 0;
    result->struct_desc = 0;
    result->function = 0;
    result->array_size = 0;

    return result;
}

Type *dup_type(Type *src) {
    if (!src) return 0;

    Type *dst = malloc(sizeof(Type));
    dst->type           = src->type;
    dst->is_unsigned    = src->is_unsigned;
    dst->target         = src->target ? dup_type(src->target) : 0;
    dst->struct_desc    = src->struct_desc; // Note: not making a copy
    dst->function       = src->function; // Note: not making a copy
    dst->array_size     = src->array_size;

    return dst;
}

Type *make_pointer(Type *src) {
    Type *dst = new_type(TYPE_PTR);
    dst->target = dup_type(src);

    return dst;
}

Type *make_pointer_to_void() {
    return make_pointer(new_type(TYPE_VOID));
}

Type *make_struct_type(Struct *s) {
    Type *type = new_type(TYPE_STRUCT);
    type->struct_desc = s;

    return type;
}

Type *deref_pointer(Type *src) {
    if (src->type != TYPE_PTR) panic("Cannot dereference a non pointer");

    return dup_type(src->target);
}

// Integral and floating types are collectively called arithmetic types.
// Arithmetic types and pointer types are collectively called scalar types.
// Array and structure types are collectively called aggregate types.
int is_integer_type(Type *type) {
    return (type->type >= TYPE_CHAR && type->type <= TYPE_LONG);
}

int is_floating_point_type(Type *type) {
    return (type->type >= TYPE_FLOAT && type->type <= TYPE_LONG_DOUBLE);
}

// Can tye type be stored in one of the XMM* registers?
int is_sse_floating_point_type(Type *type) {
    return (type->type >= TYPE_FLOAT && type->type <= TYPE_DOUBLE);
}

int is_arithmetic_type(Type *type) {
    return is_integer_type(type) || is_floating_point_type(type);
}

int is_scalar_type(Type *type) {
    return 1;
}

int is_object_type(Type *type) {
    return 1;
}

int is_incomplete_type(Type *type) {
    return 0;
}

int is_pointer_type(Type *type) {
    return type->type == TYPE_PTR;
}

int is_pointer_to_object_type(Type *type) {
    return type->type == TYPE_PTR;
}

int is_null_pointer(Value *v) {
    Type *type = v->type;

    if (is_integer_type(type) && v->is_constant && v->int_value == 0) return 1;
    else if (type->type == TYPE_PTR && type->target->type == TYPE_VOID &&
             v->is_constant && v->int_value == 0) return 1;
    else return 0;
}

int is_pointer_to_void(Type *type) {
    return type->type == TYPE_PTR && type->target->type == TYPE_VOID;
}

int type_fits_in_single_int_register(Type *type) {
    return ((type->type >= TYPE_CHAR && type->type <= TYPE_LONG) || (type->type == TYPE_PTR));
}

int get_type_size(Type *type) {
    int t;

    t = type->type;
         if (t == TYPE_VOID)        return sizeof(void);
    else if (t == TYPE_CHAR)        return sizeof(char);
    else if (t == TYPE_SHORT)       return sizeof(short);
    else if (t == TYPE_INT)         return sizeof(int);
    else if (t == TYPE_LONG)        return sizeof(long);
    else if (t == TYPE_FLOAT)       return sizeof(float);
    else if (t == TYPE_DOUBLE)      return sizeof(double);
    else if (t == TYPE_LONG_DOUBLE) return sizeof(long double);
    else if (t == TYPE_PTR)         return sizeof(void *);
    else if (t == TYPE_STRUCT)      return type->struct_desc->size;

    panic1d("sizeof unknown type %d", t);
}

int get_type_alignment(Type *type) {
    int t;

    t = type->type;

         if (t == TYPE_PTR)          return 8;
    else if (t == TYPE_CHAR)         return 1;
    else if (t == TYPE_SHORT)        return 2;
    else if (t == TYPE_INT)          return 4;
    else if (t == TYPE_LONG)         return 8;
    else if (t == TYPE_FLOAT)        return 4;
    else if (t == TYPE_DOUBLE)       return 8;
    else if (t == TYPE_LONG_DOUBLE)  return 16;
    else if (t == TYPE_STRUCT) panic("Alignment of structs not implemented");

    panic1d("align of unknown type %d", t);
}

int type_eq(Type *type1, Type *type2) {
    if (type1->type != type2->type) return 0;
    if (type1->type == TYPE_PTR) return type_eq(type1->target, type2->target);
    return (type1->is_unsigned == type2->is_unsigned);
}

int types_are_compabible(Type *type1, Type *type2) {
    return type_eq(type1, type2);
}

int is_integer_operation_result_unsigned(Type *src1, Type *src2) {
    int is_insigned;

    if (src1->type == src2->type)
        // If either is unsigned, the result is also unsigned
        is_insigned = src1->is_unsigned || src2->is_unsigned;
    else {
        // types are different
        if (src1->is_unsigned == src2->is_unsigned)
            is_insigned = src1->is_unsigned;
        else
            is_insigned = (src1->type > src2->type && src1->is_unsigned) || (src1->type < src2->type && src2->is_unsigned) ? 1 : 0;
    }

    return is_insigned;
}
