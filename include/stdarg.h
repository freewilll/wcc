#ifndef _STDARG_H
#define _STDARG_H

typedef struct __va_list {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

#define va_copy(dst, src) *dst = *src

#define va_end(ap)

typedef va_list __gnuc_va_list;

#endif /* _STDARG_H */
