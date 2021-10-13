#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    if (t->is_const)    len += fprintf(f, "const ");
    if (t->is_volatile) len += fprintf(f, "volatile ");
    if (t->is_unsigned) len += fprintf(f, "unsigned ");

         if (tt == TYPE_VOID)             len += fprintf(f, "void");
    else if (tt == TYPE_CHAR)             len += fprintf(f, "char");
    else if (tt == TYPE_INT)              len += fprintf(f, "int");
    else if (tt == TYPE_SHORT)            len += fprintf(f, "short");
    else if (tt == TYPE_LONG)             len += fprintf(f, "long");
    else if (tt == TYPE_FLOAT)            len += fprintf(f, "float");
    else if (tt == TYPE_DOUBLE)           len += fprintf(f, "double");
    else if (tt == TYPE_LONG_DOUBLE)      len += fprintf(f, "long double");
    else if (tt == TYPE_STRUCT_OR_UNION)  {
        if (t->struct_or_union_desc->is_union)
            len += fprintf(f, "union %s", t->struct_or_union_desc->identifier);
        else
            len += fprintf(f, "struct %s", t->struct_or_union_desc->identifier);
    }
    else len += fprintf(f, "unknown tt %d", tt);

    return len;
}

char *sprint_type_in_english(Type *type) {
    char *buffer = malloc(256);
    char *start = buffer;

    int tt;
    while (type) {
        tt = type->type;

        if (type->is_const)    buffer += sprintf(buffer, "const ");
        if (type->is_volatile) buffer += sprintf(buffer, "volatile ");
        if (type->is_unsigned) buffer += sprintf(buffer, "unsigned ");

             if (tt == TYPE_VOID)        buffer += sprintf(buffer, "void");
        else if (tt == TYPE_CHAR)        buffer += sprintf(buffer, "char");
        else if (tt == TYPE_INT)         buffer += sprintf(buffer, "int");
        else if (tt == TYPE_SHORT)       buffer += sprintf(buffer, "short");
        else if (tt == TYPE_LONG)        buffer += sprintf(buffer, "long");
        else if (tt == TYPE_FLOAT)       buffer += sprintf(buffer, "float");
        else if (tt == TYPE_DOUBLE)      buffer += sprintf(buffer, "double");
        else if (tt == TYPE_LONG_DOUBLE) buffer += sprintf(buffer, "long double");
        else if (tt == TYPE_PTR)         buffer += sprintf(buffer, "pointer to ");

        else if (tt == TYPE_ARRAY) {
            if (type->array_size == 0) buffer += sprintf(buffer, "array of ");
            else buffer += sprintf(buffer, "array[%d] of ", type->array_size);
        }

        else if (tt == TYPE_STRUCT_OR_UNION) {
            if (type->struct_or_union_desc->identifier) {
                if (type->struct_or_union_desc->is_union)
                    buffer += sprintf(buffer, "union %s {", type->struct_or_union_desc->identifier);
                else
                    buffer += sprintf(buffer, "struct %s {", type->struct_or_union_desc->identifier);
            }
            else {
                if (type->struct_or_union_desc->is_union)
                    buffer += sprintf(buffer, "union {");
                else
                    buffer += sprintf(buffer, "struct {");
            }

            int first = 1;
            for (StructOrUnionMember **pmember = type->struct_or_union_desc->members; *pmember; pmember++) {
                if (!first) buffer += sprintf(buffer, ", "); else first = 0;
                char *member_english = sprint_type_in_english((*pmember)->type);
                buffer += sprintf(buffer, "%s as %s", (*pmember)->identifier, member_english);
            }

            buffer += sprintf(buffer, "}");
        }

        else if (tt == TYPE_FUNCTION) {
            buffer += sprintf(buffer, "function(");
            int first = 1;
            for (int i = 0; i < type->function->param_count; i++) {
                if (!first) buffer += sprintf(buffer, ", "); else first = 0;
                buffer += sprintf(buffer, "%s", sprint_type_in_english(type->function->param_types[i]));
            }

            if (type->function->is_variadic)buffer += sprintf(buffer, ", ...");

            buffer += sprintf(buffer, ") returning ");
        }

        else
            panic1d("Unknown type->type=%d", tt);

        type = type->target;
    }

    return start;
}

Type *new_type(int type) {
    Type *result = malloc(sizeof(Type));
    result->type = type;
    result->is_unsigned = 0;
    result->target = 0;
    result->struct_or_union_desc = 0;
    result->function = 0;
    result->array_size = 0;
    result->is_const = 0;
    result->is_volatile = 0;

    return result;
}

static StructOrUnionMember *dup_struct_or_union_member(StructOrUnionMember *src) {
    StructOrUnionMember *dst = malloc(sizeof(StructOrUnionMember));
    dst->identifier = strdup(src->identifier);
    dst->type = dup_type(src->type);
    dst->offset = src->offset;
    dst->is_bit_field = src->is_bit_field;
    dst->bit_field_size = src->bit_field_size;
    dst->bit_field_offset = src->bit_field_offset;
    return dst;
}

StructOrUnion *dup_struct_or_union(StructOrUnion *src) {
    StructOrUnion *dst = malloc(sizeof(StructOrUnion));

    dst->identifier    = src->identifier ? strdup(src->identifier) : 0;
    dst->size          = src->size;
    dst->is_incomplete = src->is_incomplete;
    dst->is_packed     = src->is_packed;
    dst->is_union      = src->is_union;

    int len;
    for (len = 0; src->members[len]; len++);

    dst->members = malloc(sizeof(StructOrUnionMember *) * (len + 1));
    memset(dst->members, 0, sizeof(StructOrUnionMember *) * (len + 1));

    for (int i = 0; src->members[i]; i++)
        dst->members[i] = dup_struct_or_union_member(src->members[i]);

    return dst;
}

Type *dup_type(Type *src) {
    if (!src) return 0;

    Type *dst = malloc(sizeof(Type));
    *dst = *src;
    dst->target = dup_type(src->target);

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
    return (type->type >= TYPE_CHAR && type->type <= TYPE_PTR);
}

int is_object_type(Type *type) {
    return (type->type != TYPE_FUNCTION);
}

int is_incomplete_type(Type *type) {
    return 0; // TODO
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
         if (t == TYPE_VOID)            return sizeof(void);
    else if (t == TYPE_CHAR)            return sizeof(char);
    else if (t == TYPE_SHORT)           return sizeof(short);
    else if (t == TYPE_INT)             return sizeof(int);
    else if (t == TYPE_LONG)            return sizeof(long);
    else if (t == TYPE_FLOAT)           return sizeof(float);
    else if (t == TYPE_DOUBLE)          return sizeof(double);
    else if (t == TYPE_LONG_DOUBLE)     return sizeof(long double);
    else if (t == TYPE_PTR)             return sizeof(void *);
    else if (t == TYPE_STRUCT_OR_UNION) return type->struct_or_union_desc->size;
    else if (t == TYPE_ARRAY)           return type->array_size * get_type_size(type->target);

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
    else if (t == TYPE_ARRAY)        return get_type_alignment(type->target);
    else if (t == TYPE_STRUCT_OR_UNION) {
        // The alignment of a struct is the max alignment of all members
        int max = 0;
        for (StructOrUnionMember **pmember = type->struct_or_union_desc->members; *pmember; pmember++) {
            int alignment = get_type_alignment((*pmember)->type);
            if (alignment > max) max = alignment;
        }
        return max;
    }

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

Type *make_struct_or_union_type(StructOrUnion *s) {
    Type *type = new_type(TYPE_STRUCT_OR_UNION);
    type->struct_or_union_desc = s;

    return type;
}

// Recurse through all struct members and determine offsets
// Bit fields are only implemented for integers (32 bits size) according to the C89 spec.
int recursive_complete_struct_or_union(StructOrUnion *s, int bit_offset, int is_root) {
    bit_offset = 0;

    int initial_bit_offset = bit_offset;
    int max_size = 0;
    int max_alignment = 0;

    for (StructOrUnionMember **pmember = s->members; *pmember; pmember++) {
        StructOrUnionMember *member = *pmember;
        int size;

        if (member->is_bit_field) {
            size = member->bit_field_size;

            // Align to the next integer boundary if
            // - It's a size zero unnamed member
            // - If the member would protrude past the next integer boundary
            if (!member->bit_field_size) bit_offset = ((bit_offset + 31) & ~31);
            else {
                int new_bit_offset = (bit_offset + member->bit_field_size - 1) & ~31;
                if (new_bit_offset != (bit_offset & ~31)) bit_offset = ((bit_offset + 31) & ~31);
            }

            // Ensure the alignment is at least 4 bytes
            if (max_alignment < 32) max_alignment = 32;
        }
        else {
            int alignment = s->is_packed ? 8 : get_type_alignment(member->type) * 8;
            if (alignment > max_alignment) max_alignment = alignment;
            bit_offset = ((bit_offset + alignment - 1) & (~(alignment - 1)));

            if (member->type->type == TYPE_STRUCT_OR_UNION) {
                // Duplicate the structs, since the bit_offset will be set and otherwise
                // would end up to a reused struct having invalid bit_offsets
                member->type->struct_or_union_desc = dup_struct_or_union(member->type->struct_or_union_desc);
                size = recursive_complete_struct_or_union(member->type->struct_or_union_desc, bit_offset, 0);
            }
            else {
                size = get_type_size(member->type) * 8;
            }
        }

        member->offset = bit_offset >> 3;
        member->bit_field_offset = bit_offset;

        if (size > max_size) max_size = size;

        if (!s->is_union) bit_offset += size;
    }

    s->is_incomplete = 0;

    int size;
    if (s->is_union)
        size = max_size;
    else {
        if (is_root)
            // Add padding to the root, but not sub struct/unions
            bit_offset = ((bit_offset + max_alignment  - 1) & (~(max_alignment - 1)));

        size = bit_offset - initial_bit_offset;
    }

    s->size = size >> 3;

    return size;
}

void complete_struct_or_union(StructOrUnion *s) {
    recursive_complete_struct_or_union(s, 0, 1);
}
