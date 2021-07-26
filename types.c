#include <stdio.h>
#include <stdlib.h>

#include "wcc.h"

int print_type(void *f, Type *type) {
    int len = 0;

    if (!type) {
        len += printf(f, "(no type!)");
        return len;
    }

    int tt = type->type;
    while (tt >= TYPE_PTR) {
        len += fprintf(f, "*");
        tt -= TYPE_PTR;
    }

    if (type->is_unsigned) len += fprintf(f, "unsigned ");

         if (tt == TYPE_VOID)        len += fprintf(f, "void");
    else if (tt == TYPE_CHAR)        len += fprintf(f, "char");
    else if (tt == TYPE_INT)         len += fprintf(f, "int");
    else if (tt == TYPE_SHORT)       len += fprintf(f, "short");
    else if (tt == TYPE_LONG)        len += fprintf(f, "long");
    else if (tt == TYPE_LONG_DOUBLE) len += fprintf(f, "long double");
    else if (tt >= TYPE_STRUCT)      len += fprintf(f, "struct %s", all_structs[tt - TYPE_STRUCT]->identifier);
    else len += fprintf(f, "unknown tt %d", tt);

    return len;
}

Type *new_type(int type) {
    Type *result;

    result = malloc(sizeof(Type));
    result->type = type;
    result->is_unsigned = 0;

    return result;
}

Type *dup_type(Type *src) {
    Type *dst;

    if (!src) return 0;

    dst = new_type(src->type);
    dst->is_unsigned = src->is_unsigned;

    return dst;
}

Type *make_ptr(Type *src) {
    Type *dst;

    dst = dup_type(src);
    dst->type += TYPE_PTR;

    return dst;
}

Type *deref_ptr(Type *src) {
    Type *dst;

    if (src->type < TYPE_PTR) panic("Cannot dereference a non pointer");

    dst = dup_type(src);
    dst->type -= TYPE_PTR;

    return dst;
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

int is_scalar_type(Type *type) {
    return 1;
}

int type_fits_in_single_register(Type *type) {
    return ((type->type >= TYPE_CHAR && type->type <= TYPE_LONG) || (type->type >= TYPE_PTR));
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
    else if (t >  TYPE_PTR)         return sizeof(void *);
    else if (t >= TYPE_STRUCT)      return all_structs[t - TYPE_STRUCT]->size;

    panic1d("sizeof unknown type %d", t);
}

int get_type_alignment(Type *type) {
    int t;

    t = type->type;

         if (t  > TYPE_PTR)          return 8;
    else if (t == TYPE_CHAR)         return 1;
    else if (t == TYPE_SHORT)        return 2;
    else if (t == TYPE_INT)          return 4;
    else if (t == TYPE_LONG)         return 8;
    else if (t == TYPE_LONG_DOUBLE)  return 16;
    else if (t >= TYPE_STRUCT) panic("Alignment of structs not implemented");

    panic1d("align of unknown type %d", t);
}

int type_eq(Type *type1, Type *type2) {
    return (type1->type == type2->type && type1->is_unsigned == type2->is_unsigned);
}
