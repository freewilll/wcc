int check_stack_alignment() {
    // Ensure that the stack pointer is aligned to 16 bytes

    unsigned long sp;

#if defined __x86_64__
    __asm__ __volatile__("movq %%rsp, %%rax\n" : "=&a" (sp) ::);
#elif defined __aarch64__
    __asm__ __volatile__("mov %0, sp" : "=r" (sp)
    );
#endif

    return sp % 16 != 0;
}
