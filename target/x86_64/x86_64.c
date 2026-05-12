#include "../../wcc.h"

char *target_op_name(int operation) {
    switch (operation) {
        case 0:                      return "";
        case X86_OP_MOV:             return "mov";
        case X86_OP_ADD:             return "add";
        case X86_OP_ADDC:            return "addc";
        case X86_OP_SUB:             return "sub";
        case X86_OP_SUBC:            return "subc";
        case X86_OP_MUL:             return "mul";
        case X86_OP_MUL128A:         return "mul128A";
        case X86_OP_MUL128B:         return "mul128B";
        case X86_OP_IDIV:            return "idiv";
        case X86_OP_CQTO:            return "cqto";
        case X86_OP_CMP:             return "cmp";
        case X86_OP_COMIS:           return "comis";
        case X86_OP_TEST:            return "test";
        case X86_OP_CMPZ:            return "cmpz";
        case X86_OP_JMP:             return "jmp";
        case X86_OP_JZ:              return "jz";
        case X86_OP_JNZ:             return "jnz";
        case X86_OP_JE:              return "je";
        case X86_OP_JNE:             return "jne";
        case X86_OP_JLT:             return "jlt";
        case X86_OP_JGT:             return "jgt";
        case X86_OP_JLE:             return "jle";
        case X86_OP_JGE:             return "jge";
        case X86_OP_JB:              return "jb";
        case X86_OP_JA:              return "ja";
        case X86_OP_JBE:             return "jbe";
        case X86_OP_JAE:             return "jae";
        case X86_OP_SETE:            return "sete";
        case X86_OP_SETNE:           return "setne";
        case X86_OP_SETP:            return "setp";
        case X86_OP_SETNP:           return "setnp";
        case X86_OP_SETLT:           return "setlt";
        case X86_OP_SETGT:           return "setgt";
        case X86_OP_SETLE:           return "setle";
        case X86_OP_SETGE:           return "setge";
        case X86_OP_MOVS:            return "movs";
        case X86_OP_MOVZ:            return "movz";
        case X86_OP_MOVC:            return "movc";
        default:                     panic("Unknown x86 operation %d", operation);
    }
}

void print_target_instruction(void *f, Tac *tac) {
    int o = tac->operation.id;

    if (o == X86_OP_ARG) {
        fprintf(f, "arg for call %ld ", tac->src1->int_value);
        print_value(f, tac->src2, 1);
    }

    else if (o == X86_OP_CALL)      { fprintf(f, "call "  ); print_value(f, tac->src1, 1); if (tac->dst) { printf(" -> "); print_value(f, tac->dst, 1); } }
    else if (o == X86_OP_LEA)       { fprintf(f, "lea "   ); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_MOV)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_MOVS)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_MOVZ)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_MOVC)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_ADD)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_ADDC)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_SUB)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_SUBC)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_MUL)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_MUL128A)   { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_MUL128B)   { fprintf(f, "%-6s", operation_string(o));                                                 print_value(f, tac->dst,  1); }
    else if (o == X86_OP_IDIV)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_CQTO)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == X86_OP_CMP)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->src2, 1); }
    else if (o == X86_OP_CMPZ)      { fprintf(f, "%-6s", operation_string(o)); fprintf(f, "0");              fprintf(f, ", "); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JMP)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JZ)        { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JNZ)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JE)        { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JNE)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JLT)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JGT)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JLE)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JGE)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JB)        { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JA)        { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JBE)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_JAE)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_SETE)      { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_SETNE)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_SETLT)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_SETGT)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_SETLE)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else if (o == X86_OP_SETGE)     { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); }
    else
        panic("print_instruction(): Unknown operation: %d", tac->operation.id);

}

void print_physical_register_name_for_lr_reg_index(int preg_reg_index) {
    switch(preg_reg_index) {
        case LIVE_RANGE_PREG_RAX_INDEX:         printf("rax");   break;
        case LIVE_RANGE_PREG_RBX_INDEX:         printf("rbx");   break;
        case LIVE_RANGE_PREG_RCX_INDEX:         printf("rcx");   break;
        case LIVE_RANGE_PREG_RDX_INDEX:         printf("rdx");   break;
        case LIVE_RANGE_PREG_RSI_INDEX:         printf("rsi");   break;
        case LIVE_RANGE_PREG_RDI_INDEX:         printf("rdi");   break;
        case LIVE_RANGE_PREG_R08_INDEX:         printf("r8");    break;
        case LIVE_RANGE_PREG_R09_INDEX:         printf("r9");    break;
        case LIVE_RANGE_PREG_R12_INDEX:         printf("r12");   break;
        case LIVE_RANGE_PREG_R13_INDEX:         printf("r13");   break;
        case LIVE_RANGE_PREG_R14_INDEX:         printf("r14");   break;
        case LIVE_RANGE_PREG_R15_INDEX:         printf("r15");   break;
        case LIVE_RANGE_PREG_XMM00_INDEX:       printf("xmm0");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 1:   printf("xmm1");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 2:   printf("xmm2");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 3:   printf("xmm3");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 4:   printf("xmm4");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 5:   printf("xmm5");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 6:   printf("xmm6");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 7:   printf("xmm7");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 8:   printf("xmm8");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 9:   printf("xmm9");  break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 10:  printf("xmm10"); break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 11:  printf("xmm11"); break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 12:  printf("xmm12"); break;
        case LIVE_RANGE_PREG_XMM00_INDEX + 13:  printf("xmm13"); break;
        default: printf("Unknown LR preg index %d", preg_reg_index);
    }
}

