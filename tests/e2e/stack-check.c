int check_stack_alignment() {
    // Ensure that the stack pointer is aligned to 16 bytes

    long r0;

    __asm__ __volatile__("movq %%rsp, %%rax\n" : "=&a" (r0) ::);

    return r0 % 16 != 0;
}
