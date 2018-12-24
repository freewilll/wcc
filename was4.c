#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h

enum {
    MAX_SYMTAB_LEN     = 1024 * 10,
    MAX_STRTAB_LEN     = 1024,
    MAX_INPUT_SIZE     = 1024 * 1024 * 10,
    MAX_TEXT_SIZE      = 1024 * 1024 * 10,
    MAX_DATA_SIZE      = 1024 * 1024 * 10,
    MAX_LINES          = 1024 * 1024,
    MAX_RELA_TEXT_SIZE = 1024 * 1024 *10,

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

void wstrcpy(char *dst, char *src) {
    while (*dst++ = *src++);
}

int wstrlen(char *str) {
    char *s;
    int len;
    len = 0;
    s = str;
    while (*str++) len++;
    return len;
}

char *wstrdup(char *src) {
    char *s, *dst;
    int len;
    s = src;
    len = 0;
    while (*s++) len++;
    dst = malloc(len + 1);
    wstrcpy(dst, src);
    while (*dst++ = *src++);
    return dst;
}

// Since we don't do negatives, this returns either zero or one.
// zero if the strings are equal, one otherwise.
int wmemcmp(char *str1, char *str2, int size) {
    int i;
    i = 0;
    while (i < size) {
        if (*str1++ != *str2++) return 1;
        i++;
    }
    return 0;
}

// Since we don't do negatives, this returns either zero or one.
// zero if the strings are equal, one otherwise.
int wstrcmp(char *str1, char *str2) {
    while (*str1 || *str2) {
        if (!*str1 || !*str2) return 1;
        if (*str1++ != *str2++) return 1;
    }
    return 0;
}

void witoa(char *dst, int number) {
    int i;
    i  = 0;
    while (i < 4) {
        dst[3 - i] = number % 10 + '0';
        number = number / 10;
        i++;
    }
}

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

void write_elf(char *filename, int data_size, int text_size, int strtab_size, int rela_text_size,
        char *data_data, char *text_data, char *shstrtab_data, char *symtab_data, char *strtab_data, char *rela_text_data,
        int last_local_symbol, int num_syms) {

    int shstrtab_len;
    char *s, *d, *p;
    int f, written;
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
    f = open(filename, 577, 420); // O_TRUNC=512, O_CREAT=64, O_WRONLY=1, mode=555 http://man7.org/linux/man-pages/man2/open.2.html
    if (f < 0) { printf("Unable to open write output file\n"); exit(1); }
    written = write(f, program, total_size);
    if (written < 0) { printf("Unable to write to output file %d\n", errno); exit(1); }
    close(f);
}

void add_symbol(char *symtab_data, int *num_syms, char **strtab, int *strtab_len,
        char *name, long value, int type, int binding, int section_index) {

    char *s;
    strtab[(*strtab_len)++] = name;
    if (*strtab_len == MAX_STRTAB_LEN) {
        printf("Exceeded max strtab length %d\n", MAX_STRTAB_LEN);
        exit(1);
    }

    s = (symtab_data + STE_SIZE * *num_syms);
    *((long *) &s[ST_VALUE]) = value;
    s[ST_INFO] = (type  << 4) + binding;
    *((short *) &s[ST_SHNDX]) = section_index;
    (*symtab_data) += STE_SIZE;

    (*num_syms)++;
    if (*num_syms > MAX_SYMTAB_LEN) { printf("Exceeded max symbol table size %d\n", MAX_SYMTAB_LEN); exit(1); }
}

void link_symtab_strings(char *symtab_data, char *strtab_data, int num_syms) {
    int i;
    char *s;

    i = 0;
    s = strtab_data;
    while (i < num_syms) {
        if (*s)
            symtab_data[i * STE_SIZE + ST_NAME] = s - strtab_data;
        else
            symtab_data[i * STE_SIZE + ST_NAME] = 0;
        while (*s++);
        i++;
    }
}

int add_global(char *symtab_data, int *num_syms, char **strtab, int *strtab_len, int *data_size, char *input) {
    int is_function, size, value, type, binding, section_index;
    char *name, *s;
    if (input[0] != 't' || input[1] != 'y' || input[2] != 'p' || input[3] != 'e' || input[4] != '=') {
        printf("Missing type= in global\n");
        exit(1);
    }

    is_function = input[5] == '1';
    input += 7;

    if (input[0] != 's' || input[1] != 'i' || input[2] != 'z' || input[3] != 'e' || input[4] != '=') {
        printf("Missing size= in global\n");
        exit(1);
    }
    size = input[5] - '0';
    input += 7;
    if (input[0] != '"') { printf("Missing \" in global\n"); exit(1); }
    input++;
    s = input;
    while (*input != '"') input++;
    *input = 0;
    name = strdup(s);
    *input = '"';

    // Only data is done here
    if (!is_function) {
        value = align(*data_size, size);
        if (*data_size + size > MAX_DATA_SIZE) {
            printf("Exceeded max data size %d\n", MAX_DATA_SIZE);
            exit(1);
        }
        *data_size += size;
        binding = STB_LOCAL;
        type = STT_NOTYPE;
        section_index = SEC_DATA;
        add_symbol(symtab_data, num_syms, strtab, strtab_len, name, value, binding, type, section_index);
    }
}

void add_string_literal(char *symtab_data, int *num_syms, char **strtab, int *strtab_len, char *data_data, int *data_size, char *input, int *string_literal_len) {
    char *name;

    name = malloc(8);
    name[0] = '_'; name[1] = 'S'; name[2] = 'L'; name[6] = 0;
    witoa(name + 3, (*string_literal_len)++);

    add_symbol(symtab_data, num_syms, strtab, strtab_len, name, *data_size, STB_LOCAL, STT_NOTYPE, SEC_DATA);

    input += 2;
    while (*input != '"') {
        if (*data_size == MAX_DATA_SIZE) {
            printf("Exceeded max data size %d\n", MAX_DATA_SIZE);
        }

             if (*input == '\\' && input[1] == 'n') { data_data[(*data_size)++] = '\n'; input += 1; }
        else if (*input == '\\' && input[1] == 't') { data_data[(*data_size)++] = '\t'; input += 1; }
        else if (*input == '\\' && input[1] == '\\') { data_data[(*data_size)++] = '\\'; input += 1; }
        else if (*input == '\\' && input[1] == '"') { data_data[(*data_size)++] = '"'; input += 1; }
        else
            data_data[(*data_size)++] = *input;

        input++;
    }

    if (*data_size == MAX_DATA_SIZE) {
        printf("Exceeded max data size %d\n", MAX_DATA_SIZE);
    }
    data_data[(*data_size)++] = 0;
}

void add_function_call_relocation(int symbol_index, long offset, char *rela_text_data, int *num_rela_text) {
    char *s;
    // s is the entry in the relocation table
    s = rela_text_data + *num_rela_text * RELA_SIZE;
    *num_rela_text += 1;
    *((long *) &s[R_OFFSET]) = offset;

    // R_X86_64_PC32 + a link to the .data section
    *((long *) &s[R_INFO]) = (long) R_X86_64_PC32 + ((long) symbol_index << 32);
    *((long *) &s[R_ADDEND]) = -4;
}

int symbol_index(char **strtab, int strtab_len, char *name) {
    int i;
    char **ps;
    i = 0;
    ps = strtab;
    while (i < strtab_len) {
        if (!wstrcmp(*ps, name)) return i;
        i++;
        *ps++;
    }

    printf("Unknown symbol %s\n", name);
    exit(1);
}

void prepare_function_call(char **code, int function_call_arg_count) {
    int i, v;
    char *t;
    v = function_call_arg_count;
    t = *code;

    // Read the first 6 args from the stack in right to left order
    if (v >= 6) { *t++ = 0x4c; *t++ = 0x8b; *t++ = 0x8c; *t++ = 0x24; *((int *) t) = (char) (v - 6) * 8; t += 4; } // mov x(%rsp),%r9
    if (v >= 5) { *t++ = 0x4c; *t++ = 0x8b; *t++ = 0x84; *t++ = 0x24; *((int *) t) = (char) (v - 5) * 8; t += 4; } // mov x(%rsp),%r8
    if (v >= 4) { *t++ = 0x48; *t++ = 0x8b; *t++ = 0x8c; *t++ = 0x24; *((int *) t) = (char) (v - 4) * 8; t += 4; } // mov x(%rsp),%rcx
    if (v >= 3) { *t++ = 0x48; *t++ = 0x8b; *t++ = 0x94; *t++ = 0x24; *((int *) t) = (char) (v - 3) * 8; t += 4; } // mov x(%rsp),%rdx
    if (v >= 2) { *t++ = 0x48; *t++ = 0x8b; *t++ = 0xb4; *t++ = 0x24; *((int *) t) = (char) (v - 2) * 8; t += 4; } // mov x(%rsp),%rsi
    if (v >= 1) { *t++ = 0x48; *t++ = 0x8b; *t++ = 0xbc; *t++ = 0x24; *((int *) t) = (char) (v - 1) * 8; t += 4; } // mov x(%rsp),%rdi

    // For the remaining args after the first 6, swap the opposite ends of the stack
    i = 0;
    while (i < v - 6) {
        *t++ = 0x48; *t++ = 0x8b; *t++ = 0x84; *t++ = 0x24; *((int *) t) = (char) i * 8; t += 4;           // mov x(%rsp),%rax
        *t++ = 0x48; *t++ = 0x89; *t++ = 0x84; *t++ = 0x24; *((int *) t) = (char) (v - i - 1) * 8; t += 4; // mov %rax, x(%rsp)
        i++;
    }

    // Adjust the stack for the removed args that are in registers
    *t++= 0x48; *t++= 0x81; *t++= 0xc4; // add x,%rsp
    *((int *) t) = (char) (v <= 6 ? v : 6) * 8;
    t += 4;
    *code = t;
}

void cleanup_function_call(char **code, int function_call_arg_count) {
    int i, v;
    char *t;
    v = function_call_arg_count;
    t = *code;
    // Remove the remainder of the args from the stack
    if (v > 6) {
        *t++= 0x48; *t++= 0x81; *t++= 0xc4; // add x,%rsp
        *((int *) t) = (char) (v - 6) * 8;
        t += 4;
    }
    *code = t;
}

int assemble_file(char *filename) {
    int i, f, input_size, filename_len, v, line, function_arg_count, neg, function_call_arg_count, local_stack_size, local_vars_stack_start;
    char *input, *pi, *instr, *output_filename;
    int printf_symbol, fflush_symbol;
    int data_size, text_size, strtab_size, rela_text_size, last_local_symbol, *shstrtab_indexes, *strtab_indexes, num_rela_text, num_syms;
    char *s, *t, *name, *data_data, *text_data, *strtab_data, *symtab_data, *shstrtab_data, *rela_text_data, **strtab, **ps;
    int string_literal_len, strtab_len;
    char *main_address, **line_symbols;
    int *pi1, *pi2;

    input = malloc(MAX_INPUT_SIZE);
    f  = open(filename, 0, 0); // O_RDONLY = 0
    if (f < 0) { printf("Unable to open input file\n"); exit(1); }
    input_size = read(f, input, MAX_INPUT_SIZE);
    if (input_size == MAX_INPUT_SIZE) {
        printf("Exceed max input file size %d\n", MAX_INPUT_SIZE);
        exit(1);
    }
    input[input_size] = 0;
    if (input_size < 0) { printf("Unable to read input file\n"); exit(1); }
    close(f);

    data_size = 0;
    data_data = malloc(MAX_DATA_SIZE);
    memset(data_data, 0, MAX_DATA_SIZE);

    // Symbol table
    strtab_len = 0;
    strtab = malloc(sizeof(char *) * MAX_STRTAB_LEN);
    num_syms = 0;
    symtab_data = malloc(MAX_SYMTAB_LEN * STE_SIZE);
    memset(symtab_data, MAX_SYMTAB_LEN * STE_SIZE, 0);

    s = symtab_data;
    pi1 = &num_syms;
    pi2 = &strtab_len;
    add_symbol(s, pi1, strtab, pi2, "",       0, STB_LOCAL,  STT_NOTYPE,  SHN_UNDEF); // 0 null
    add_symbol(s, pi1, strtab, pi2, filename, 0, STB_LOCAL,  STT_FILE,    SHN_ABS);   // 1 filename
    add_symbol(s, pi1, strtab, pi2, "",       0, STB_LOCAL,  STT_SECTION, SEC_DATA);  // 2 .data
    add_symbol(s, pi1, strtab, pi2, "",       0, STB_LOCAL,  STT_SECTION, SEC_TEXT);  // 3 .text

    // First pass, the .data section
    string_literal_len = 0;
    line_symbols = malloc(sizeof(char **) * MAX_LINES);
    line = 0;

    pi = input;
    while (*pi) {
        while (*pi >= '0' && *pi <= '9') pi++;
        while (*pi == ' ') pi++;
        instr = pi;
        while (*pi != ' ') pi++;
        while (*pi == ' ') pi++;

        if (!wmemcmp(instr, "GLB", 3)) {
            add_global(symtab_data, &num_syms, strtab, &strtab_len, &data_size, pi);
        }
        else if (!wmemcmp(instr, "IMM", 3) && !wmemcmp(pi, "&\"", 2)) {
            add_string_literal(symtab_data, &num_syms, strtab, &strtab_len, data_data, &data_size, pi, &string_literal_len);
            line_symbols[line] = symtab_data + STE_SIZE * (strtab_len - 1);
        }

        while (*pi != '\n') pi++;
        pi++;
        line++;
    }

    last_local_symbol = num_syms - 1;
    text_size = 0;
    rela_text_size = 0;

    printf_symbol = strtab_len;
    add_symbol(symtab_data, &num_syms, strtab, &strtab_len, "printf", 0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);

    fflush_symbol = strtab_len;
    add_symbol(symtab_data, &num_syms, strtab, &strtab_len, "fflush", 0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);

    // First pass, the .text section
    text_data = malloc(MAX_TEXT_SIZE);
    memset(text_data, 0, MAX_TEXT_SIZE);

    t = text_data;
    pi = input;
    line = 0;
    num_rela_text = 0;
    rela_text_data = malloc(MAX_RELA_TEXT_SIZE);

    while (*pi) {
        while (*pi >= '0' && *pi <= '9') pi++;
        while (*pi == ' ') pi++;
        instr = pi;
        while (*pi != ' ') pi++;
        while (*pi == ' ') pi++;

        if (!wmemcmp(instr, "GLB   type=1 size=8 \"", 21)) {
            s = instr + 21;
            name = instr + 21;
            while (*s != '"') s++;
            *s = 0;
            add_symbol(symtab_data, &num_syms, strtab, &strtab_len, name, t - text_data, STB_GLOBAL, STT_NOTYPE, SEC_TEXT);
            if (!wmemcmp(name, "main", 4)) main_address = text_data;
        }
        else if (!wmemcmp(instr, "ENT", 3)) {
            // Start a new stack frame
            *t++ = 0x55;                            // push %rbp
            *t++ = 0x48; *t++ = 0x89; *t++ = 0xe5;  // mov  %rsp, %rbp

            // Push up to the first 6 args onto the stack, so all args are on the stack with leftmost arg first.
            // Arg 7 and onwards are already pushed.

            s = instr + 6;
            function_arg_count = 0;
            while (*s >= '0' && *s <= '9') function_arg_count = 10 * function_arg_count + (*s++ - '0');
            s++;

            local_stack_size = 0;
            while (*s >= '0' && *s <= '9') local_stack_size = 10 * local_stack_size + (*s++ - '0');

            // Push the args in the registers on the stack. The order for all args is right to left.
            if (function_arg_count >= 6) { *t++ = 0x41; *t++ = 0x51; } // push %r9
            if (function_arg_count >= 5) { *t++ = 0x41; *t++ = 0x50; } // push %r8
            if (function_arg_count >= 4) { *t++ = 0x51;              } // push %rcx
            if (function_arg_count >= 3) { *t++ = 0x52;              } // push %rdx
            if (function_arg_count >= 2) { *t++ = 0x56;              } // push %rsi
            if (function_arg_count >= 1) { *t++ = 0x57;              } // push %rdi

            // Calculate stack start for locals. reduce by pushed bsp, function return, + above pushed args.
            local_vars_stack_start = -24 - 8 * (function_arg_count <= 6 ? function_arg_count : 6);

            // Allocate stack space for local variables
            if (local_stack_size > 0) {
                *t++= 0x48; *t++= 0x81; *t++= 0xec; // sub x,%rsp
                *((int *) t) = local_stack_size;
                t += 4;
            }
        }
        else if (!wmemcmp(instr, "LINE", 4)) {}
        else if (!wmemcmp(instr, "ADJ", 3)) {}
        else if (!wmemcmp(instr, "IMM   ", 6)) {
            s = instr + 6;
            if (*s >= '0' && *s <= '9') {
                v = 0;
                while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
                // movabs $0x...,%rax
                *t++ = 0x48; *t++ = 0xb8;
                *(long *) t = v;
                t += 8;
            }
            else {
                // s is the entry in the relocation table
                s = rela_text_data + num_rela_text * RELA_SIZE;
                num_rela_text += 1;
                *((long *) &s[R_OFFSET]) = t - text_data + 2;
                *t++ = 0x48; *t++ = 0xb8;
                t += 8;
                *((long *) &s[R_INFO]) = (long) R_X86_64_64 + ((long) (SEC_DATA + 1) << 32); // R_X86_64_64 + a link to the .data section
                *((long *) &s[R_ADDEND]) = *((long *) &line_symbols[line][ST_VALUE]);        // Address in .data
            }
        }
        else if (!wmemcmp(instr, "LEV", 3)) {
            *t++ = 0xc9; // leaveq
            *t++ = 0xc3; // retq
        }
        else if (!wmemcmp(instr, "JSR", 3)) {
            s = instr + 6;

            // First number is the VM address & isn't used here
            v = 0;
            while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
            s += 1;

            // Number of args
            function_call_arg_count = 0;
            while (*s >= '0' && *s <= '9') function_call_arg_count = 10 * function_call_arg_count + (*s++ - '0');
            s += 2;

            // Function name
            name = s;
            while (*s != '"') s++;
            *s = 0;

            prepare_function_call(&t, function_call_arg_count);
            *t++ = 0xe8; t += 4; // callq
            add_function_call_relocation(symbol_index(strtab, strtab_len, name), t - text_data - 4, rela_text_data, &num_rela_text);
            cleanup_function_call(&t, function_call_arg_count);
        }
        else if (!wmemcmp(instr, "PSH", 3)) {
            *t++ = 0x50; // push %rax
        }
        else if (!wmemcmp(instr, "PRTF", 4)) {
            s = instr + 6;
            function_call_arg_count = 0;
            while (*s >= '0' && *s <= '9') function_call_arg_count = 10 * function_call_arg_count + (*s++ - '0');
            prepare_function_call(&t, function_call_arg_count);
            // // The number of floating point arguments is zero
            *t++ = 0xb8; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; // mov $0x0,%eax
            *t++ = 0xe8; t += 4; // callq
            add_function_call_relocation(printf_symbol, t - text_data - 4, rela_text_data, &num_rela_text);
            cleanup_function_call(&t, function_call_arg_count);
        }

        else if (!wmemcmp(instr, "LEA", 3)) {
            neg = 0;
            s = instr + 6;
            if (*s == '-') { neg = 1; s++; }
            v = 0;
            while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
            if (neg) v = -v;
            if (v >= 2) {
                // Function argument
                v -= 2; // 0=1st arg, 1=2nd arg, etc

                *t++ = 0x48; *t++ = 0x8d; *t++ = 0x45; // lea -0x...(%rbp),%rax

                if (function_arg_count > 6) {
                    v = function_arg_count - 7 - v;
                    // Correct for split stack when there are more than 6 args
                    if (v < 0) {
                        // Read pushed arg by the callee. arg 0 is at rsp-0x30, arg 2 at rsp-0x28 etc, ... arg 5 at rsp-0x08
                        *t++ = (char) 8 * v;
                    }
                    else {
                        // Read pushed arg by the caller, arg 6 is at rsp+0x10, arg 7 at rsp+0x18, etc
                        // The +2 is to bypass the pushed rbp and return address
                        *t++ = (char) 8 * (v + 2);
                    }
                }
                else {
                    // The first arg is at v=0, second at v=1
                    // If there aree.g. 2 args:
                    // arg 0 is at mov -0x10(%rbp), %rax
                    // arg 1 is at mov -0x08(%rbp), %rax
                    *t++ = (char) -8 * (v + 1);
                }
            }
            else {
                // Local variable
                *t++ = 0x48; *t++ = 0x8d; *t++ = 0x45; // lea -0x...(%rbp), %rax
                *t++ = local_vars_stack_start - 8 * v;
            }
        }

        else if (!wmemcmp(instr, "LI", 2)) {
            *t++ = 0x48; *t++ = 0x8b; *t++ = 0x00; // mov (%rax), %rax
        }

        else if (!wmemcmp(instr, "SI", 2)) {
            *t++ = 0x5f;                           // pop %rdi
            *t++ = 0x48; *t++ = 0x89; *t++ = 0x07; // mov %rax, (%rdi)
        }

        else {
            printf("Unimplemented instruction: %.5s ", instr);
            while (*pi != '\n') { printf("%c", *pi); pi++; }
            printf("\n");
            exit(1);
        }

        while (*pi != '\n') pi++;
        pi++;
        line++;
    }

    // _start function
    add_symbol(symtab_data, &num_syms, strtab, &strtab_len, "_start", t - text_data, STB_GLOBAL, STT_NOTYPE, SEC_TEXT);

    *t++ = 0x31; *t++ = 0xed;              // xor %ebp, %ebp
    *t++ = 0x5f;                           // pop %rdi        argc
    *t++ = 0x48; *t++ = 0x89; *t++ = 0xe6; // mov %rsp, %rsi  argv

    // callq main
    *t++ = 0xe8; t += 4;
    add_function_call_relocation(symbol_index(strtab, strtab_len, "main"), t - text_data - 4, rela_text_data, &num_rela_text);

    // Flush all open output streams with fflush(0)
    *t++ = 0x50;                                                        // push %rax
    *t++ = 0xbf; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;    // mov $0, %edi
    *t++ = 0xe8; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;    // callq fflush

    add_function_call_relocation(fflush_symbol, t - text_data - 4, rela_text_data, &num_rela_text);
    *t++ = 0x58;                                                        // pop %rax

    // exit(%rax)
    *t++ = 0x48; *t++ = 0x89; *t++ = 0xc7;                              // mov    %rax, %rdi
    *t++ = 0xb8; *t++ = 0x3c; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;    // mov    $0x3c, %eax
    *t++ = 0x0f; *t++ = 0x05;                                           // syscall

    text_size = t - text_data;

    rela_text_size = num_rela_text * RELA_SIZE;

    strtab_indexes = malloc(sizeof(int) * strtab_len);
    make_string_list(strtab, strtab_len, &strtab_data, strtab_indexes, &strtab_size);
    link_symtab_strings(symtab_data, strtab_data, num_syms);

    // Write output file with .o extension from an expected source extension of .ws
    filename_len = wstrlen(filename);
    if (filename_len > 3 && filename[filename_len - 3] == '.' && filename[filename_len - 2] == 'w' && filename[filename_len - 1] == 's') {
        output_filename = malloc(filename_len + 2);
        wstrcpy(output_filename, filename);
        output_filename[filename_len - 2] = 'o';
        output_filename[filename_len - 1] = 0;
    }
    else {
        printf("Unable to determine output filename\n");
        exit(1);
    }

    write_elf(
        output_filename,
        data_size, text_size, strtab_size, rela_text_size,
        data_data, text_data, shstrtab_data, symtab_data, strtab_data, rela_text_data,
        last_local_symbol, num_syms
    );
}

int main(int argc, char **argv) {
    int help, hello;
    char *filename;

    help = 0;

    argc--;
    argv++;
    while (argc > 0 && *argv[0] == '-') {
             if (argc > 0 && !memcmp(argv[0], "-h",  3)) { help = 0;  argc--; argv++; }
        else { printf("Unknown argeter %s\n", argv[0]); exit(1); }
    }

    if (help) {
        printf("Usage: was4 [--hw] FILENAME\n\n");
        printf("Flags\n");
        printf("--hw    Make hello world\n");
        printf("-h      Help\n");
        exit(1);
    }

    assemble_file(argv[0]);
}
