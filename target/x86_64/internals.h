struct __wcc_fp_register_save_area {
    long elements[2];
};

struct __wcc_register_save_area {
    long gp[8];
    struct __wcc_fp_register_save_area f8[8];
};

typedef struct __va_list {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];
