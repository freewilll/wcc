int check_stack_alignment() {
    // Ensure that the stack pointer is aligned to 16 bytes

    int r0;
    __asm__ __volatile__("movl %%esp, %%eax\n" : "=&a" (r0) ::);
    return r0 % 16 != 0;
}
