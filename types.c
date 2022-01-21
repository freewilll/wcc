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

    switch (tt) {
        case TYPE_VOID:
            len += fprintf(f, "void");
            break;
        case TYPE_CHAR:
            len += fprintf(f, "char");
            break;
        case TYPE_INT:
            len += fprintf(f, "int");
            break;
        case TYPE_SHORT:
            len += fprintf(f, "short");
            break;
        case TYPE_LONG:
            len += fprintf(f, "long");
            break;
        case TYPE_FLOAT:
            len += fprintf(f, "float");
            break;
        case TYPE_DOUBLE:
            len += fprintf(f, "double");
            break;
        case TYPE_LONG_DOUBLE:
            len += fprintf(f, "long double");
            break;
        case TYPE_STRUCT_OR_UNION:
            if (t->struct_or_union_desc->is_union)
                len += fprintf(f, "union %s", t->tag ? t->tag->identifier : "(anonymous)");
            else
                len += fprintf(f, "struct %s", t->tag ? t->tag->identifier : "(anonymous)");
            break;
        case TYPE_ARRAY:
            len += print_type(f, t->target);
            if (t->array_size) len += fprintf(f, "[%d]", t->array_size);
            else len += fprintf(f, "[]");
            break;
        case TYPE_ENUM:
            len += fprintf(f, "enum %s", t->tag ? t->tag->identifier : "(anonymous)");
            break;
        case TYPE_FUNCTION:
            len += fprintf(f, "function");
            break;
        default:
            len += fprintf(f, "unknown tt %d", tt);
    }

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

        switch (tt) {
            case TYPE_VOID:
                buffer += sprintf(buffer, "void");
                break;
            case TYPE_CHAR:
                buffer += sprintf(buffer, "char");
                break;
            case TYPE_INT:
                buffer += sprintf(buffer, "int");
                break;
            case TYPE_SHORT:
                buffer += sprintf(buffer, "short");
                break;
            case TYPE_LONG:
                buffer += sprintf(buffer, "long");
                break;
            case TYPE_FLOAT:
                buffer += sprintf(buffer, "float");
                break;
            case TYPE_DOUBLE:
                buffer += sprintf(buffer, "double");
                break;
            case TYPE_LONG_DOUBLE:
                buffer += sprintf(buffer, "long double");
                break;
            case TYPE_PTR:
                buffer += sprintf(buffer, "pointer to ");
                break;
            case TYPE_ARRAY:
                if (type->array_size == 0) buffer += sprintf(buffer, "array of ");
                else buffer += sprintf(buffer, "array[%d] of ", type->array_size);
                break;
            case TYPE_STRUCT_OR_UNION:
                if (type->tag) {
                    if (type->struct_or_union_desc->is_union)
                        buffer += sprintf(buffer, "union %s {", type->tag->identifier);
                    else
                        buffer += sprintf(buffer, "struct %s {", type->tag->identifier);
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
                break;
            case TYPE_ENUM:
                if (type->tag)
                    buffer += sprintf(buffer, "enum %s {}", type->tag->identifier);
                else
                    buffer += sprintf(buffer, "enum {}");
                break;
            case TYPE_FUNCTION: {
                buffer += sprintf(buffer, "function(");
                int first = 1;
                for (int i = 0; i < type->function->param_count; i++) {
                    if (!first) buffer += sprintf(buffer, ", "); else first = 0;
                    buffer += sprintf(buffer, "%s", sprint_type_in_english(type->function->param_types[i]));
                }

                if (type->function->is_variadic)buffer += sprintf(buffer, ", ...");

                buffer += sprintf(buffer, ") returning ");
                break;
            }
            default:
                panic("Unknown type->type=%d", tt);
        }

        type = type->target;
    }

    return start;
}

void print_type_in_english(Type *type) {
    printf("%s\n", sprint_type_in_english(type));
}

void init_type_allocations(void) {
    allocated_types = new_list(1024);
}

void free_types(void) {
    for (int i = 0; i < allocated_types->length; i++) free(allocated_types->elements[i]);
    free(allocated_types);
}

Type *new_type(int type) {
    Type *result = malloc(sizeof(Type));
    memset(result, 0, sizeof(Type));
    result->type = type;
    append_to_list(allocated_types, result);

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
    append_to_list(allocated_types, dst);

    return dst;
}

Type *integer_promote_type(Type *type) {
    if (!is_integer_type(type)) error("Invalid operand, expected integer type");

    if (type->type >= TYPE_INT && type->type <= TYPE_LONG)
        return type;
    else
        return new_type(TYPE_INT); // An int can hold all the values
}

Type *make_pointer(Type *src) {
    Type *dst = new_type(TYPE_PTR);
    dst->target = dup_type(src);

    return dst;
}

Type *make_pointer_to_void(void) {
    return make_pointer(new_type(TYPE_VOID));
}

Type *deref_pointer(Type *src) {
    if (src->type != TYPE_PTR && src->type != TYPE_ARRAY) error("Cannot dereference a non-pointer");

    return dup_type(src->target);
}

Type *make_array(Type *src, int size) {
    Type *dst = new_type(TYPE_ARRAY);
    dst->target = dup_type(src);
    dst->array_size = size;

    return dst;
}

Type *decay_array_to_pointer(Type *src) {
    return make_pointer(src->target);
}

// Integral and floating types are collectively called arithmetic types.
// Arithmetic types and pointer types are collectively called scalar types.
// Array and structure types are collectively called aggregate types.
int is_integer_type(Type *type) {
    return ((type->type >= TYPE_CHAR && type->type <= TYPE_LONG) || type->type == TYPE_ENUM);
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
    return (type->type == TYPE_ENUM || (type->type >= TYPE_CHAR && type->type <= TYPE_PTR));
}

int is_object_type(Type *type) {
    return (type->type != TYPE_FUNCTION);
}

int is_incomplete_type(Type *type) {
    if (type->type == TYPE_STRUCT_OR_UNION && type->struct_or_union_desc->is_incomplete) return 1;
    if (type->type == TYPE_ARRAY && type->array_size == 0) return 1;
    return 0;
}

int is_pointer_type(Type *type) {
    return type->type == TYPE_PTR || type->type == TYPE_ARRAY;
}

int is_pointer_to_object_type(Type *type) {
    return (type->type == TYPE_PTR || type->type == TYPE_ARRAY) && is_object_type(type->target);
}

int is_pointer_to_function_type(Type *type) {
    return type->type == TYPE_PTR && type->target->type == TYPE_FUNCTION;
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

int is_pointer_or_array_type(Type *type) {
    return type->type == TYPE_PTR || type->type == TYPE_ARRAY;
}

int type_fits_in_single_int_register(Type *type) {
    return ((type->type >= TYPE_CHAR && type->type <= TYPE_LONG) || (type->type == TYPE_PTR));
}

int get_type_size(Type *type) {
    int t;

    t = type->type;
    switch(t) {
        case TYPE_VOID:
            return sizeof(void);
        case TYPE_CHAR:
            return sizeof(char);
        case TYPE_SHORT:
            return sizeof(short);
        case TYPE_INT:
            return sizeof(int);
        case TYPE_LONG:
            return sizeof(long);
        case TYPE_ENUM:
            return sizeof(int);
        case TYPE_FLOAT:
            return sizeof(float);
        case TYPE_DOUBLE:
            return sizeof(double);
        case TYPE_LONG_DOUBLE:
            return sizeof(long double);
        case TYPE_PTR:
            return sizeof(void *);
        case TYPE_STRUCT_OR_UNION:
            return type->struct_or_union_desc->size;
        case TYPE_ARRAY:
            return type->array_size * get_type_size(type->target);
        case TYPE_FUNCTION:
            return 1;
        default:
            panic("sizeof unknown type %d", t);
    }
}

int get_type_alignment(Type *type) {
    int t;

    t = type->type;

    switch (t) {
        case TYPE_PTR:
            return 8;
        case TYPE_CHAR:
            return 1;
        case TYPE_SHORT:
            return 2;
        case TYPE_INT:
            return 4;
        case TYPE_ENUM:
            return 4;
        case TYPE_LONG:
            return 8;
        case TYPE_FLOAT:
            return 4;
        case TYPE_DOUBLE:
            return 8;
        case TYPE_LONG_DOUBLE:
            return 16;
        case TYPE_ARRAY:
            return get_type_alignment(type->target);
        case TYPE_STRUCT_OR_UNION: {
            // The alignment of a struct is the max alignment of all members
            int max = 0;
            for (StructOrUnionMember **pmember = type->struct_or_union_desc->members; *pmember; pmember++) {
                int alignment = get_type_alignment((*pmember)->type);
                if (alignment > max) max = alignment;
            }
            return max;
        }
        case TYPE_FUNCTION: // Is really a pointer to a function
            return 8;
        default:
            panic("align of unknown type %d", t);
    }
}

// Apply default argument promotions & decay arrays
Type *apply_default_function_call_argument_promotions(Type *type) {
    if (type->type < TYPE_INT) {
        type = new_type(TYPE_INT);
        if (type->is_unsigned) type->is_unsigned = 1;
    }
    else if (type->type == TYPE_FLOAT)
        type = new_type(TYPE_DOUBLE);
    else if (type->type == TYPE_ARRAY)
        type = decay_array_to_pointer(type);

    return type;
}

int type_eq(Type *type1, Type *type2) {
    if (type1->type != type2->type) return 0;
    if (type1->type == TYPE_PTR) return type_eq(type1->target, type2->target);
    return (type1->is_unsigned == type2->is_unsigned);
}

static int types_tags_are_compatible(Type *type1, Type *type2) {
    if ((type1->tag == 0) != (type2->tag == 0))
        return 0;

    // If defined, the tags must match
    if (type1->tag && strcmp(type1->tag->identifier, type2->tag->identifier))
        return 0;

    return 1;
}

static int struct_or_union_member_count(StructOrUnion *s) {
    int count = 0;
    for (StructOrUnionMember **pmember = s->members; *pmember; pmember++) count++;
    return count;
}

static int struct_members_cmpfunc(const void *a, const void *b) {
    char *a_identifier = (*(StructOrUnionMember **) a)->identifier;
    char *b_identifier = (*(StructOrUnionMember **) b)->identifier;
    if (!a_identifier) a_identifier = "";
    if (!b_identifier) b_identifier = "";

   return strcmp(a_identifier, b_identifier);
}

StructOrUnionMember **sort_struct_or_union_members(StructOrUnionMember **members, int count) {
    // Make a shallow copy of the members (i.e. only the pointers)
    StructOrUnionMember **result = malloc(sizeof(StructOrUnionMember *) * count);
    for (int i = 0; i < count; i++) result[i] = members[i];

    qsort(result, count, sizeof(StructOrUnionMember *), struct_members_cmpfunc);

    return result;
}

static int recursive_types_are_compatible(Type *type1, Type *type2, StrMap *seen_tags, int check_qualifiers);

static int struct_or_unions_are_compatible(StructOrUnion *s1, StructOrUnion *s2, StrMap *seen_tags) {
    if (s1->is_incomplete || s2->is_incomplete) return 1;

    int count = struct_or_union_member_count(s1);
    if (count != struct_or_union_member_count(s2)) return 0;

    StructOrUnionMember **members1;
    StructOrUnionMember **members2;

    if (s1->is_union) {
        // Members in unions can be out of order
        members1 = sort_struct_or_union_members(s1->members, count);
        members2 = sort_struct_or_union_members(s2->members, count);
    }
    else {
        members1 = s1->members;
        members2 = s2->members;
    }

    // The names must match and types must be compatible
    for (int i = 0; i < count; i++) {
        StructOrUnionMember *member1 = members1[i];
        StructOrUnionMember *member2 = members2[i];

        // Check for matching name
        if ((member1->identifier == 0) != (member2->identifier == 0)) return 0;
        if (strcmp(member1->identifier, member2->identifier)) return 0;

        // Check bit field sizes match
        if (member1->bit_field_size != member2->bit_field_size) return 0;

        // Check types are compatible
        if (!recursive_types_are_compatible(member1->type, member2->type, seen_tags, 0)) return 0;
    }

    return 1;
}

static int functions_are_compatible(Type *type1, Type *type2, StrMap *seen_tags) {
    if (!recursive_types_are_compatible(type1->target, type2->target, seen_tags, 1)) return 0;

    // Both are non variadic and one of them has an old style parameter list
    if (!type1->function->is_variadic && !type2->function->is_variadic && type1->function->is_paramless != type2->function->is_paramless) {
        // Swap so that type1 is paramless, type2 is not
        if (type2->function->is_paramless) {
            Type *temp = type1;
            type1 = type2;
            type2 = temp;
        }

        // Ensure all types on type1 are compatible with type2 types after default argument promotions
        for (int i = 0; i < type2->function->param_count; i++) {
            if (i < type1->function->param_count) {
                // Check params match
                Type *type = apply_default_function_call_argument_promotions(type2->function->param_types[i]);
                if (!types_are_compatible(type1->function->param_types[i], type))
                    return 0;
            }
            else {
                // Check default promotions don't affect params
                Type *type = apply_default_function_call_argument_promotions(type2->function->param_types[i]);
                if (!types_are_compatible(type2->function->param_types[i], type))
                    return 0;
            }
        }

        return 1;
    }

    // Check they both are or aren't variadic
    if (type1->function->is_variadic != type2->function->is_variadic) return 0;

    // Check param counts match
    if (type1->function->param_count != type2->function->param_count) return 0;

    // Check params match
    for (int i = 0; i < type1->function->param_count; i++) {
        if (!types_are_compatible(type1->function->param_types[i], type2->function->param_types[i]))
            return 0;
    }

    return 1;
}

// Check two types are compatible. Prevent infinite loops by keeping a map of seen tags
static int recursive_types_are_compatible(Type *type1, Type *type2, StrMap *seen_tags, int check_qualifiers) {
    // Following https://en.cppreference.com/w/c/language/type

    if (type1->type == TYPE_STRUCT_OR_UNION && type1->tag) {
        if (strmap_get(seen_tags, type1->tag->identifier)) return 1;
        strmap_put(seen_tags, type1->tag->identifier, type1);
    }

    if (type2->type == TYPE_STRUCT_OR_UNION && type2->tag) {
        if (strmap_get(seen_tags, type2->tag->identifier)) return 1;
        strmap_put(seen_tags, type2->tag->identifier, type2);
    }

    // They are identically qualified versions of compatible unqualified types
    if (check_qualifiers && type1->is_const != type2->is_const) return 0;

    // They are pointer types and are pointing to compatible types
    if (type1->type == TYPE_PTR && type2->type == TYPE_PTR)
        return recursive_types_are_compatible(type1->target, type2->target, seen_tags, 0);

    // They are array types, and if both have constant size, that size is the same
    // and element types are compatible
    if (type1->type == TYPE_ARRAY && type2->type == TYPE_ARRAY) {
        if (!recursive_types_are_compatible(type1->target, type2->target, seen_tags, 0)) return 0;
        if (type1->array_size && type2->array_size && type1->array_size != type2->array_size) return 0;
        return 1;
    }

    if (type1->type == TYPE_STRUCT_OR_UNION && type2->type == TYPE_STRUCT_OR_UNION) {
        if (type1->tag) strmap_put(seen_tags, type1->tag->identifier, type1);
        if (type2->tag) strmap_put(seen_tags, type2->tag->identifier, type2);

        if (!types_tags_are_compatible(type1, type2)) return 0;
    }

    if (type1->type == TYPE_ENUM && type2->type == TYPE_ENUM) {
        if (!types_tags_are_compatible(type1, type2)) return 0;

      // Note: enum members aren't checked since it's not possible to declare two
      // anonymous enums with the same member names since they would have clashing
      // symbols Enum values checks are useless, since if the names don't match, they
      // are incompatible. Therefore, it suffices to only check the tag.
      return 1;
    }

    // One is an enumerated type and the other is that enumeration's underlying type
    if (type1->type == TYPE_ENUM && type2->type == TYPE_INT) return 1;
    if (type2->type == TYPE_ENUM && type1->type == TYPE_INT) return 1;

    if (type1->type == TYPE_STRUCT_OR_UNION && type2->type == TYPE_STRUCT_OR_UNION)
        return struct_or_unions_are_compatible(type1->struct_or_union_desc, type2->struct_or_union_desc, seen_tags);

    if (type1->type == TYPE_FUNCTION && type2->type == TYPE_FUNCTION)
        return functions_are_compatible(type1, type2, seen_tags);

    return (type1->type == type2->type);
}

int types_are_compatible(Type *type1, Type *type2) {
    StrMap *seen_tags = new_strmap();
    return recursive_types_are_compatible(type1, type2, seen_tags, 1);
}

// Ignore qualifiers at the top level
int types_are_compatible_ignore_qualifiers(Type *type1, Type *type2) {
    StrMap *seen_tags = new_strmap();
    return recursive_types_are_compatible(type1, type2, seen_tags, 0);
}

// Compatibility checks must already have been done before calling this
Type *composite_type(Type *type1, Type *type2) {
    // Implicit else, the type->type matches
    if (type1->type == TYPE_ARRAY) {
        if (type1->array_size) return type1;
        else if (type2->array_size) return type2;
        else return type1;
    }

    if (type1->type == TYPE_PTR)
        return make_pointer(composite_type(type1->target, type2->target));

    if (type1->type == TYPE_FUNCTION) {
        if (type1->function->param_count == 0 && type2->function->param_count != 0)
            return type2;
        else if (type2->function->param_count == 0 && type1->function->param_count != 0)
            return type1;
        else {
            if (type1->function->param_count != type2->function->param_count)
                error("Incompatible types");

            Type *result = new_type(TYPE_FUNCTION);
            result->target = type1->target;
            Function *function = malloc(sizeof(Function));
            *function = *type1->function;
            function->return_type = type1->function->return_type;
            result->function = function;

            for (int i = 0; i < function->param_count; i++) {
                Type *type = composite_type(type1->function->param_types[i], type2->function->param_types[i]);
                function->param_types[i] = type;
            }

            return result;
        }
    }

    return type1;
}

// Make the composite type for two pointers in a ternary operation If both the second
//
// From 3.3.15:
// and third operands are pointers or one is a null pointer constant and the other is a
// pointer, the result type is a pointer to a type qualified with all the type
// qualifiers of the types pointed-to by both operands.
Type *ternary_pointer_composite_type(Type *type1, Type *type2) {
    // If the qualifiers of the target are the same, the composite type can be made directly
    if (type1->target->is_const == type2->target->is_const)
        return composite_type(type1, type2);

    int is_const = type1->target->is_const || type2->target->is_const ;

    // Make copies and discard the const qualifier
    Type *dup_type1 = dup_type(type1);
    Type *dup_type2 = dup_type(type2);
    dup_type1->target->is_const = 0;
    dup_type2->target->is_const = 0;
    Type *result = composite_type(dup_type1, dup_type2);

    // If either is a pointer to a const, the result is a pointer to a const
    result->target->is_const = is_const;

    return result;
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
int recursive_complete_struct_or_union(StructOrUnion *s, int bit_offset) {
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
                size = recursive_complete_struct_or_union(member->type->struct_or_union_desc, bit_offset);
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
        bit_offset = ((bit_offset + max_alignment  - 1) & (~(max_alignment - 1)));
        size = bit_offset - initial_bit_offset;
    }

    s->size = size >> 3;

    return size;
}

// Recursively flatten anonymous structs, .e.g.
// struct { struct { int i, j; }; int k; } => struct { int i, j, k; }
void flatten_anonymous_structs(StructOrUnion *s) {
    // Determine member count of the outer struct
    int member_count = 0;
    for (StructOrUnionMember **pmember = s->members; *pmember; pmember++)
        member_count++;

    StrMap *member_map = new_strmap();

    StructOrUnionMember **pmember = s->members;
    int member_index = 0;
    while (*pmember) {
        StructOrUnionMember *member = *pmember;
        Type *type = member->type;

        if (member->identifier)  {
            if (strmap_get(member_map, member->identifier))
                error("Duplicate struct/union member %s", member->identifier);
            strmap_put(member_map, member->identifier, (void *) 1);
        }

        // A struct is anonymous if the struct has no tag & no identifier
        if (type->type == TYPE_STRUCT_OR_UNION && !type->tag && !member->identifier) {
            StructOrUnion *sub = type->struct_or_union_desc;

            // Determine member count of the inner struct
            int sub_member_count = 0;
            for (StructOrUnionMember **sub_pmember = sub->members; *sub_pmember; sub_pmember++)
                sub_member_count++;

            if (member_count + sub_member_count == MAX_STRUCT_MEMBERS)
                panic("Exceeded max struct/union members %d", MAX_STRUCT_MEMBERS);

            // Move the trailing members forward
            for (int i = 0; i < member_count - member_index - 1; i++)
                s->members[member_count - i + sub_member_count - 2] = s->members[member_count - i - 1];

            // Copy the sub struct members
            for (int i = 0; i < sub_member_count; i++) {
                sub->members[i]->offset += member->offset;
                s->members[member_index + i] = sub->members[i];
            }

            member_count += sub_member_count;
            continue; // Keep going with current pmember & member_index
        }

        pmember++;
        member_index++;
    }
}

void complete_struct_or_union(StructOrUnion *s) {
    recursive_complete_struct_or_union(s, 0);
    flatten_anonymous_structs(s);
}

int type_is_modifiable(Type *type) {
    if (type->is_const) return 0;

    if (type->type == TYPE_STRUCT_OR_UNION) {
        int is_modifiable = 1;
        StructOrUnion *s = type->struct_or_union_desc;
        for (StructOrUnionMember **pmember = s->members; *pmember; pmember++) {
            StructOrUnionMember *member = *pmember;

            if (member->type->is_const) {
                is_modifiable = 0;
                break;
            }

            if (member->type->type == TYPE_STRUCT_OR_UNION) {
                is_modifiable = type_is_modifiable(member->type);
                if (is_modifiable) break;
            }
        }

        return is_modifiable;
    }

    return 1;
}

// Create a new type iterator instance. A type iterator can be used to recurse
// through all scalars either depth first, or by iterating at a single level.
TypeIterator *type_iterator(Type *type) {
    TypeIterator *it = malloc(sizeof(TypeIterator));
    memset(it, 0, sizeof(TypeIterator));
    it->type = type;

    return it;
}

// Are there any scalars left?
int type_iterator_done(TypeIterator *it) {
    return (it->index == -1);
}

// A child iterator calls this when it has run out of scalars
static TypeIterator *type_iterator_recurse_upwards(TypeIterator *it) {
    it->index = -1;
    if (!it->parent) return it;
    return type_iterator_next(it->parent);
}

// Go to the next element, staying at the same level if there are any left elements and
// going back up if the elements run out.
TypeIterator *type_iterator_next(TypeIterator *it) {
    if (type_iterator_done(it)) panic("Attempt to call next on a done type iterator");

    if (is_scalar_type(it->type)) {
        it->offset += get_type_size(it->type);
        return type_iterator_recurse_upwards(it);
    }
    else if (it->type->type == TYPE_ARRAY) {
        it->offset += get_type_size(it->type->target);
        it->index++;
        if (it->index == it->type->array_size)
            return type_iterator_recurse_upwards(it);
        return it;
    }
    else if (it->type->type == TYPE_STRUCT_OR_UNION) {
        it->index++;
        int is_union = it->type->struct_or_union_desc->is_union;
        if (is_union || !it->type->struct_or_union_desc->members[it->index]) {
            it->offset = it->start_offset + get_type_size(it->type);
            return type_iterator_recurse_upwards(it);
        }

        it->offset = it->start_offset + it->type->struct_or_union_desc->members[it->index]->offset;

        // Skip over zero size bit fields
        StructOrUnionMember *member = it->type->struct_or_union_desc->members[it->index];
        if (member->is_bit_field && !member->bit_field_size) return type_iterator_next(it);

        return it;
    }
    else
        panic("Unhandled type in type_iterator_next");
}

// Go down one level into a struct/union/array and return an new iterator
TypeIterator *type_iterator_descend(TypeIterator *it) {
    if (type_iterator_done(it))
        panic("Attempt to call descend on a done type iterator");

    if (is_scalar_type(it->type)) {
        TypeIterator *child = type_iterator(it->type);
        child->offset = it->offset;
        child->parent = it;
        return child;
    }
    if (it->type->type == TYPE_ARRAY) {
        TypeIterator *child = type_iterator(it->type->target);
        child->offset = it->offset;
        child->start_offset = it->offset;
        child->parent = it;
        return child;;
    }
    else if (it->type->type == TYPE_STRUCT_OR_UNION) {
        TypeIterator *child = type_iterator(it->type->struct_or_union_desc->members[it->index]->type);
        child->bit_field_offset = it->type->struct_or_union_desc->members[it->index]->bit_field_offset;
        child->bit_field_size = it->type->struct_or_union_desc->members[it->index]->bit_field_size;
        child->offset = it->offset;
        child->start_offset = it->offset;
        child->parent = it;
        return child;
    }
    else
        panic("Unhandled type in type_iterator_descend");
}

// Go to the first scalar in a depth first walk.
TypeIterator *type_iterator_dig(TypeIterator *it) {
    if (type_iterator_done(it))
        panic("Attempt to call dig on a done type iterator");
    else if (is_scalar_type(it->type))
        return it;
    else if (it->type->type == TYPE_ARRAY || it->type->type == TYPE_STRUCT_OR_UNION)
        return type_iterator_dig(type_iterator_descend(it));
    else
        panic("Unhandled type in type_iterator_dig");

}

// Go to the first scalar in a depth first walk.
TypeIterator *type_iterator_dig_for_string_literal(TypeIterator *it) {
    if (type_iterator_done(it))
        panic("Attempt to call dig on a done type iterator");
    else if (is_scalar_type(it->type))
        return it;
    else if (it->type->type == TYPE_ARRAY && (it->type->target->type == TYPE_CHAR || it->type->target->type == TYPE_INT))
        return it;
    else if (it->type->type == TYPE_ARRAY || it->type->type == TYPE_STRUCT_OR_UNION)
        return type_iterator_dig_for_string_literal(type_iterator_descend(it));
    else
        panic("Unhandled type in type_iterator_dig");

}
