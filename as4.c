#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h

enum {
    MAX_SYMTAB_LEN = 1024,
    MAX_TEXT_SIZE  = 1024 * 1024,

    // ELF header
    EI_MAGIC        = 0x00,     // 0x7F followed by ELF(45 4c 46) in ASCII; these four bytes constitute the magic number.
    EI_CLASS        = 0x04,     // This byte is set to either 1 or 2 to signify 32- or 64-bit format, respectively.
    EI_DATA         = 0x05,     // This byte is set to either 1 or 2 to signify little or big endianness, respectively.
    EI_VERSION      = 0x06,     // Set to 1 for the original version of ELF.
    EI_OSABI        = 0x07,     // Identifies the target operating system ABI.

    E_TYPE          = 0x10,     // Identifies object file type. ET_EXEC=2 is executable.
    E_MACHINE       = 0x12,     // Specifies target instruction set architecture. Some examples are, e.g. 0x3e is x86-64.
    E_VERSION       = 0x14,     // Set to 1 for the original version of ELF.
    E_ENTRY         = 0x18,     // This is the memory address of the entry point from where the process starts executing.
    E_PHOFF         = 0x20,     // Points to the start of the program header table.
    E_SHOFF         = 0x28,     // Points to the start of the section header table.
    E_FLAGS         = 0x30,     // Interpretation of this field depends on the target architecture.
    E_EHSIZE        = 0x34,     // Contains the size of the ELF header.
    E_PHENTSIZE     = 0x36,     // Contains the size of a program header table entry.
    E_PHNUM         = 0x38,     // Contains the number of entries in the program header table.
    E_SHENTSIZE     = 0x3a,     // Contains the size of a section header table entry.
    E_SHNUM         = 0x3c,     // Contains the number of entries in the section header table.
    E_SHSTRNDX      = 0x3e,     // Contains index of the section header table entry that contains the section names.

    // Program header
    P_TYPE          = 0x00,     // Identifies the type of the segment. PT_LOAD=1, PT_PHDR=6
    P_FLAGS         = 0x04,     // Segment-dependent flags. Here: read, write execute.
    P_OFFSET        = 0x08,     // Offset of the segment in the file image.
    P_VADDR         = 0x10,     // Virtual address of the segment in memory.
    P_FILESZ        = 0x20,     // Size in bytes of the segment in the file image. May be 0.
    P_MEMSZ         = 0x28,     // Size in bytes of the segment in memory. May be 0.
    P_ALIGN         = 0x30,     // 0 and 1 specify no alignment. Otherwise should be a positive, integral power of 2, with p_vaddr equating p_offset modulus p_align.

    SH_NAME         = 0x00,     // An offset to a string in the .shstrtab section that represents the name of this section
    SH_TYPE         = 0x04,     // Identifies the type of this header.
    SH_FLAGS        = 0x08,     // Identifies the attributes of the section.
    SH_ADDR         = 0x10,     // Virtual address of the section in memory, for sections that are loaded.
    SH_OFFSET       = 0x18,     // Offset of the section in the file image.
    SH_SIZE         = 0x20,     // Size in bytes of the section in the file image. May be 0.
    SH_LINK         = 0x28,     // Contains the section index of an associated section.
    SH_INFO         = 0x2c,     // Contains extra information about the section.
    SH_ADDRALIGN    = 0x30,     // Contains the required alignment of the section. This field must be a power of two.
    SH_ENTSIZE      = 0x38,     // Contains the size, in bytes, of each entry, for sections that contain fixed-size entries. Otherwise, this field contains zero.

    ST_NAME         = 0x00,     // This member holds an index into the object file's symbol string table
    ST_INFO         = 0x04,     // This member specifies the symbol's type (low 4 bits) and binding (high 4 bits) attributes
    ST_OTHER        = 0x05,     // This member currently specifies a symbol's visibility.
    ST_SHNDX        = 0x06,     // Every symbol table entry is defined in relation to some section. This member holds the relevant section header table index.
    ST_VALUE        = 0x08,     // This member gives the value of the associated symbol. Depending on the context, this may be an absolute value, an address, and so on; details appear
    ST_SIZE         = 0x10,     // Many symbols have associated sizes. For example, a data object's size is the number of bytes contained in the object. This member holds 0 if the symbol has no size or an unknown size.

    STB_LOCAL       = 0,
    STB_GLOBAL      = 1,
    STB_WEAK        = 2,

    STT_NOTYPE      = 0,
    STT_OBJECT      = 1,
    STT_FUNC        = 2,
    STT_SECTION     = 3,
    STT_FILE        = 4,
    STT_COMMON      = 5,
    STT_TLS         = 6,
    STT_LOOS        = 10,
    STT_HIOS        = 12,
    STT_LOPROC      = 13,
    STT_HIPROC      = 15,

    SHN_UNDEF       = 0,        // This section table index means the symbol is undefined. When the link editor combines this object file with another that defines the indicated symbol, this file's references to the symbol will be linked to the actual definition.
    SHN_ABS         = 65521,    // The symbol has an absolute value that will not change because of relocation.

    SHF_WRITE       = 1,
    SHF_ALLOC       = 2,
    SHF_EXECINSTR   = 4,

    R_OFFSET        = 0x00,     // Location to be relocated
    R_INFO          = 0x08,     // Relocation type (low 32 bits) and symbol index (high 32 bits).
    R_ADDEND        = 0x10,     // Addend

    // See http://refspecs.linuxbase.org/elf/x86_64-abi-0.98.pdf page 69
    R_X86_64_NONE   = 0,        // No calculation
    R_X86_64_64     = 1,        // Direct 64 bit relocation
    R_X86_64_PC32   = 2,        // truncate value to 32 bits

    // Sizes of the headers
    EHDR_SIZE       = 0x40,     // Elf header size
    PHDR_SIZE       = 0x38,     // Program header size
    SHDR_SIZE       = 0x40,     // Section header size
    STE_SIZE        = 0x18,     // Symbol table entry size
    RELA_SIZE       = 0x18,     // Relocation section entry size

    SEC_NULL      = 0,
    SEC_DATA      = 1,
    SEC_TEXT      = 2,
    SEC_SHSTRTAB  = 3,
    SEC_SYMTAB    = 4,
    SEC_STRTAB    = 5,
    SEC_RELA_TEXT = 6,

    SHT_PROGBITS  = 0x1,
    SHT_SYMTAB    = 0x2,
    SHT_STRTAB    = 0x3,
    SHT_RELA      = 0x4,

    E_MACHINE_TYPE_X86_64 = 0x3e,

    ET_REL = 1,                         // relocatable
};

int align(long address, int alignment) {
    return (address + alignment - 1) & (~(alignment-1));
}

void make_string_list(char** strings, int len, char **string_list, int *indexes, int *size) {
    char *result;
    long i;
    char *src, *dst, start;

    *size = 0;
    i = 0;
    while (i < len) {
        src = strings[i];
        while (*src++);
        *size += (src - strings[i]);
        i++;
    }

    result = malloc(*size);

    dst = result;
    i = 0;
    while (i < len) {
        src = strings[i];
        indexes[i] = dst - result;
        while (*dst++ = *src++);
        i++;
    }

    *string_list = result;
}

void add_section_header(char *headers, int *shstrtab_indexes, int index, int type, long flags,
        long offset, long size, int link, int info, long align, long ent_size) {

    headers[SHDR_SIZE * index + SH_NAME]      = shstrtab_indexes[index];
    headers[SHDR_SIZE * index + SH_TYPE]      = type;
    headers[SHDR_SIZE * index + SH_FLAGS]     = flags;
    headers[SHDR_SIZE * index + SH_LINK]      = link;
    headers[SHDR_SIZE * index + SH_INFO]      = info;
    headers[SHDR_SIZE * index + SH_ADDRALIGN] = align;
    headers[SHDR_SIZE * index + SH_ENTSIZE]   = ent_size;

    *((long *) &headers[SHDR_SIZE * index + SH_OFFSET]) = offset;
    *((long *) &headers[SHDR_SIZE * index + SH_SIZE])   = size;
}

void write_elf(int data_size, int text_size, int strtab_size, int rela_text_size,
        char *data_data, char *text_data, char *shstrtab_data, char *symtab_data, char *strtab_data, char *rela_text_data,
        int last_local_symbol, int num_syms) {

    int shstrtab_len;
    char *s, *d, *p;
    int f, written;
    char *filename;
    char **shstrtab;
    int *shstrtab_indexes, *strtab_indexes, *si;
    char *program, *h;
    long text_start, shdr_start, shstrtab_start, data_start, strtab_start, symtab_start, rela_text_start;
    long total_size;
    int symtab_size;
    int shstrtab_size;
    int data_flags, text_flags;

    // Section header strings
    shstrtab_len = 7;
    shstrtab = malloc(sizeof(char *) * shstrtab_len);
    shstrtab_indexes = malloc(sizeof(int) * shstrtab_len);
    shstrtab[0] = "";           // SEC_NULL
    shstrtab[1] = ".data";      // SEC_DATA
    shstrtab[2] = ".text";      // SEC_TEXT
    shstrtab[3] = ".shstrtab";  // SEC_SHSTRTAB
    shstrtab[4] = ".symtab";    // SEC_SYMTAB
    shstrtab[5] = ".strtab";    // SEC_STRTAB
    shstrtab[6] = ".rela.text"; // SEC_RELA_TEXT
    make_string_list(shstrtab, shstrtab_len, &shstrtab_data, shstrtab_indexes, &shstrtab_size);

    symtab_size = num_syms * STE_SIZE;

    shdr_start      = EHDR_SIZE;
    data_start      = align(shdr_start      + SHDR_SIZE * shstrtab_len, 16);
    text_start      = align(data_start      + data_size,                16);
    shstrtab_start  = align(text_start      + text_size,                16);
    strtab_start    = align(shstrtab_start  + shstrtab_size,            16);
    symtab_start    = align(strtab_start    + strtab_size,              16);
    rela_text_start = align(symtab_start    + symtab_size,              16);
    total_size      = align(rela_text_start + rela_text_size,           16);

    program = malloc(total_size);
    memset(program, 0, total_size);

    // ELF header
    program[EI_MAGIC+ 0] = 0x7f;                  // Magic
    program[EI_MAGIC+ 1] = 'E';
    program[EI_MAGIC+ 2] = 'L';
    program[EI_MAGIC+ 3] = 'F';
    program[EI_CLASS]    = 2;                     // 64-bit
    program[EI_DATA]     = 1;                     // LSB
    program[EI_VERSION]  = 1;                     // Original ELF version
    program[EI_OSABI]    = 0;                     // Unix System V
    program[E_TYPE]      = ET_REL;                // ET_REL Relocatable
    program[E_MACHINE]   = E_MACHINE_TYPE_X86_64; // x86-64
    program[E_VERSION]   = 1;                     // EV_CURRENT Current version of ELF
    program[E_PHOFF]     = 0;                     // Offset to program header table
    program[E_SHOFF]     = shdr_start;            // Offset to section header table
    program[E_EHSIZE]    = EHDR_SIZE;             // The size of this header, 0x40 for 64-bit
    program[E_PHENTSIZE] = 0;                     // The size of the program header
    program[E_PHNUM]     = 0;                     // Number of program header entries
    program[E_SHENTSIZE] = SHDR_SIZE;             // The size of the section header
    program[E_SHNUM]     = shstrtab_len;          // Number of section header entries
    program[E_SHSTRNDX]  = SEC_SHSTRTAB;          // The string table index is the fourth section

    // Section headers
    h = program + shdr_start;
    si = shstrtab_indexes;
    data_flags = SHF_WRITE + SHF_ALLOC;
    text_flags = SHF_ALLOC + SHF_EXECINSTR;

    //                        index          type,         flags       offset           size            link        info                   align ent_size
    add_section_header(h, si, SEC_DATA,      SHT_PROGBITS, data_flags, data_start,      data_size,      0,          0,                     0x04, 0);
    add_section_header(h, si, SEC_TEXT,      SHT_PROGBITS, text_flags, text_start,      text_size,      0,          0,                     0x10, 0);
    add_section_header(h, si, SEC_SHSTRTAB,  SHT_STRTAB,   0,          shstrtab_start,  shstrtab_size,  0,          0,                     0x1,  0);
    add_section_header(h, si, SEC_SYMTAB,    SHT_SYMTAB,   0,          symtab_start,    symtab_size,    SEC_STRTAB, last_local_symbol + 1, 0x8,  STE_SIZE);
    add_section_header(h, si, SEC_STRTAB,    SHT_STRTAB,   0,          strtab_start,    strtab_size,    0,          0,                     0x1,  0);
    add_section_header(h, si, SEC_RELA_TEXT, SHT_RELA,     0,          rela_text_start, rela_text_size, SEC_SYMTAB, SEC_TEXT,              0x8,  RELA_SIZE);

    // Data
    s = data_data;      d = &program[data_start];       while (s - data_data < data_size)           *d++ = *s++;
    s = text_data;      d = &program[text_start];       while (s - text_data < text_size)           *d++ = *s++;
    s = shstrtab_data;  d = &program[shstrtab_start];   while (s - shstrtab_data < shstrtab_size)   *d++ = *s++;
    s = symtab_data;    d = &program[symtab_start];     while (s - symtab_data < symtab_size)       *d++ = *s++;
    s = strtab_data;    d = &program[strtab_start];     while (s - strtab_data < strtab_size)       *d++ = *s++;
    s = rela_text_data; d = &program[rela_text_start];  while (s - rela_text_data < rela_text_size) *d++ = *s++;

    // Write file
    filename = "as4-test.o";
    f = open(filename, 65, 420); // O_CREAT=64, O_WRONLY=1, mode=555 http://man7.org/linux/man-pages/man2/open.2.html
    if (f < 0) { printf("Unable to open write output file\n"); exit(1); }
    written = write(f, program, total_size);
    if (written < 0) { printf("Unable to write to output file %d\n", errno); exit(1); }
    close(f);
}

void add_symbol(char *symtab_data, int *num_syms, char **strtab, int *strtab_len,
        char *name, long value, int type, int binding, int section_index) {

    char *s;
    strtab[(*strtab_len)++] = name;

    s = (symtab_data + STE_SIZE * *num_syms);
    s[ST_VALUE] = value;
    s[ST_INFO] = (type  << 4) + binding;
    *((short *) &s[ST_SHNDX]) = section_index;
    (*symtab_data) += STE_SIZE;

    (*num_syms)++;
    if (*num_syms > MAX_SYMTAB_LEN) { printf("Exceeded max symbol table size %d\n", MAX_SYMTAB_LEN); exit(1); }
}

int main(int argc, char **argv) {
    int text_size, data_size, strtab_size, rela_text_size, last_local_symbol, i, num_rela_text,
        *shstrtab_indexes, *strtab_indexes, num_syms;
    char *s, *d, *data_data, *text_data, *shstrtab_data, *strtab_data, *rela_text_data, *symtab_data, *org_symtab_data;
    char **strtab;
    int strtab_len;
    char *_start_addr, main_addr;
    int *pi1, *pi2;

    // Data segment
    data_data = "%s\n\0Hello World!\0"; // Both strings are aligned to 4 bytes
    data_size = 17;

    // Text segment
    text_data = malloc(MAX_TEXT_SIZE);
    d = text_data;
    main_addr = 0;

    // mov    %rdi,%rax
    *d++ = 0x48; *d++ = 0x89; *d++ = 0xf8;

    // retq
    *d++ = 0xc3;

    // _start:
    _start_addr = (char *) (d - text_data);

    // movabs $0x0,%rsi, relocation is at offset 0x06 "fmt"
    *d++ = 0x48; *d++ = 0xbe; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;
    *d++ = 0x00; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;

    // movabs $0x0,%rdi, relocation is at offset 0x10 "msg"
    *d++ = 0x48; *d++ = 0xbf; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;
    *d++ = 0x00; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;

    // mov    $0x0,%eax
    *d++ = 0xb8; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;

    // callq  ... <...>, relocation is at offset 0x1e "printf"
    *d++ = 0xe8; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;

    // mov    $0x3c,%eax  0x3c = exit syscall
    *d++ = 0xb8; *d++ = 0x3c; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;

    // mov    $0x0,%edi
    *d++ = 0xbf; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00; *d++ = 0x00;

    // syscall
    *d++ = 0x0f; *d++ = 0x05;

    text_size = d - text_data;

    // Symbol table
    strtab_len = 0;
    strtab = malloc(sizeof(char *) * 1024);
    num_syms = 0;
    symtab_data = malloc(MAX_SYMTAB_LEN * STE_SIZE);
    memset(symtab_data, MAX_SYMTAB_LEN * STE_SIZE, 0);

    s = symtab_data;
    pi1 = &num_syms;
    pi2 = &strtab_len;
    add_symbol(s, pi1, strtab, pi2, "",                 0,                  STB_LOCAL,  STT_NOTYPE,  SHN_UNDEF); // 0 null
    add_symbol(s, pi1, strtab, pi2, "hello-printf.asm", 0,                  STB_LOCAL,  STT_FILE,    SHN_ABS);   // 1 hello-printf.asm
    add_symbol(s, pi1, strtab, pi2, "",                 0,                  STB_LOCAL,  STT_SECTION, SEC_DATA);  // 2                   .data
    add_symbol(s, pi1, strtab, pi2, "",                 0,                  STB_LOCAL,  STT_SECTION, SEC_TEXT);  // 3                   .text
    add_symbol(s, pi1, strtab, pi2, "fmt",              0,                  STB_LOCAL,  STT_NOTYPE,  SEC_DATA);  // 4 fmt               .data
    add_symbol(s, pi1, strtab, pi2, "msg",              4,                  STB_LOCAL,  STT_NOTYPE,  SEC_DATA);  // 5 msg               .data + 4
    add_symbol(s, pi1, strtab, pi2, "printf",           (long) main_addr,   STB_GLOBAL, STT_NOTYPE,  SHN_UNDEF); // 6 printf
    add_symbol(s, pi1, strtab, pi2, "_start",           (long) _start_addr, STB_GLOBAL, STT_NOTYPE,  SEC_TEXT);  // 7 _start            .text + 4
    add_symbol(s, pi1, strtab, pi2, "main",             0,                  STB_GLOBAL, STT_NOTYPE,  SEC_TEXT);  // 8 main              .text

    strtab_indexes = malloc(sizeof(int) * strtab_len);
    make_string_list(strtab, strtab_len, &strtab_data, strtab_indexes, &strtab_size);

    i = 0;
    s = strtab_data;
    while (i < num_syms) {
        if (*s) symtab_data[i * STE_SIZE + ST_NAME] = s - strtab_data;
        while (*s++);
        i++;
    }

    last_local_symbol = 5;


    // Relocation segment
    num_rela_text = 3;
    rela_text_size = num_rela_text * RELA_SIZE;
    rela_text_data = malloc(rela_text_size);
    d = rela_text_data;
    //            Offset in .text                                 type                    symbol                            addend
    d[R_OFFSET] = 0x000000000006; *((long *) &d[R_INFO]) = (long) R_X86_64_64   + ((long) 2 << 32); *((long *) &d[R_ADDEND]) =  4; d += STE_SIZE; // .data + 4
    d[R_OFFSET] = 0x000000000010; *((long *) &d[R_INFO]) = (long) R_X86_64_64   + ((long) 2 << 32); *((long *) &d[R_ADDEND]) =  0; d += STE_SIZE; // .data + 0
    d[R_OFFSET] = 0x00000000001e; *((long *) &d[R_INFO]) = (long) R_X86_64_PC32 + ((long) 6 << 32); *((long *) &d[R_ADDEND]) = -4; d += STE_SIZE; // .printf - 4

    // That's it, let's write it
    write_elf(
        data_size, text_size, strtab_size, rela_text_size,
        data_data, text_data, shstrtab_data, symtab_data, strtab_data, rela_text_data,
        last_local_symbol, num_syms
    );
}
