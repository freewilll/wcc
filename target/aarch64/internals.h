struct __wcc_register_save_area {
    unsigned long gp[8];
    long double fp[8];
};

typedef struct __va_list {
    void *stack;
    void *gr_top;
    void *vr_top;
    int gr_offs;
    int vr_offs;
} va_list[1];
