#include <stdlib.h>

#include "wcc.h"

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

int is_integer_type(Type *type) {
    return (type->type >= TYPE_CHAR && type->type <= TYPE_LONG);
}

int get_type_size(Type *type) {
    int t;

    t = type->type;
         if (t == TYPE_VOID)   return sizeof(void);
    else if (t == TYPE_CHAR)   return sizeof(char);
    else if (t == TYPE_SHORT)  return sizeof(short);
    else if (t == TYPE_INT)    return sizeof(int);
    else if (t == TYPE_LONG)   return sizeof(long);
    else if (t >  TYPE_PTR)    return sizeof(void *);
    else if (t >= TYPE_STRUCT) return all_structs[t - TYPE_STRUCT]->size;

    panic1d("sizeof unknown type %d", t);
}

int get_type_alignment(Type *type) {
    int t;

    t = type->type;

         if (t  > TYPE_PTR)    return 8;
    else if (t == TYPE_CHAR)   return 1;
    else if (t == TYPE_SHORT)  return 2;
    else if (t == TYPE_INT)    return 4;
    else if (t == TYPE_LONG)   return 8;
    else if (t >= TYPE_STRUCT) panic("Alignment of structs not implemented");

    panic1d("align of unknown type %d", t);
}
