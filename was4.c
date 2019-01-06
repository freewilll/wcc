#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h

struct elf_header {
    char   ei_magic0;       // 0x7F followed by ELF(45 4c 46) in ASCII; these four bytes constitute the magic number.
    char   ei_magic1;
    char   ei_magic2;
    char   ei_magic3;
    char   ei_class;        // This byte is set to either 1 or 2 to signify 32- or 64-bit format, respectively.
    char   ei_data;         // This byte is set to either 1 or 2 to signify little or big endianness, respectively.
    char   ei_version;      // Set to 1 for the original version of ELF.
    char   ei_osabi;        // Identifies the target operating system ABI.
    char   ei_osabiversion; // Further specifies the ABI version.
    char   pad0;            // Unused
    char   pad1;            // Unused
    char   pad2;            // Unused
    char   pad3;            // Unused
    char   pad4;            // Unused
    char   pad5;            // Unused
    char   pad6;            // Unused
    short  e_type;          // File type.
    short  e_machine;       // Machine architecture.
    int    e_version;       // ELF format version.
    long   e_entry;         // Entry point.
    long   e_phoff;         // Program header file offset.
    long   e_shoff;         // Section header file offset.
    int    e_flags;         // Architecture-specific flags.
    short  e_ehsize;        // Size of ELF header in bytes.
    short  e_phentsize;     // Size of program header entry.
    short  e_phnum;         // Number of program header entries.
    short  e_shentsize;     // Size of section header entry.
    short  e_shnum;         // Number of section header entries.
    short  e_shstrndx;      // Section name strings section.
};

struct section_header {
    int  sh_name;           // An offset to a string in the .shstrtab section that represents the name of this section
    int  sh_type;           // Identifies the type of this header.
    long sh_flags;          // Identifies the attributes of the section.
    long sh_addr;           // Virtual address of the section in memory, for sections that are loaded.
    long sh_offset;         // Offset of the section in the file image.
    long sh_size;           // Size in bytes of the section in the file image. May be 0.
    int  sh_link;           // Contains the section index of an associated section.
    int  sh_info;           // Contains extra information about the section.
    long sh_addralign;      // Contains the required alignment of the section. This field must be a power of two.
    long sh_entsize;        // Contains the size, in bytes, of each entry, for sections that contain fixed-size entries. Otherwise, this field contains zero.
};

struct symbol {
    int   st_name;          // This member holds an index into the object file's symbol string table
    char  st_info;          // This member specifies the symbol's type (low 4 bits) and binding (high 4 bits) attributes
    char  st_other;         // This member currently specifies a symbol's visibility.
    short st_shndx;         // Every symbol table entry is defined in relation to some section. This member holds the relevant section header table index.
    long  st_value;         // This member gives the value of the associated symbol. Depending on the context, this may be an absolute value, an address, and so on; details appear
    long  st_size;          // Many symbols have associated sizes. For example, a data object's size is the number of bytes contained in the object. This member holds 0 if the symbol has no size or an unknown size.
};

struct relocation {
    long r_offset;          // Location to be relocated
    long r_info;            // Relocation type (low 32 bits) and symbol index (high 32 bits).
    long r_addend;          // Addend
};

struct jmp {
    long address;
    int line;
};

struct jsr {
    char *function_name;
    long offset;
};

enum {
    MAX_SYMTAB_LEN     = 10240,
    MAX_STRTAB_LEN     = 1024,
    MAX_INPUT_SIZE     = 10485760,
    MAX_TEXT_SIZE      = 10485760,
    MAX_DATA_SIZE      = 10485760,
    MAX_LINES          = 1048576,
    MAX_VM_ADDRESS     = 1048576,
    MAX_RELA_TEXT_SIZE = 10485760,
    MAX_JMPS           = 10240,
    MAX_JSRS           = 10240,

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

    // See http://refspecs.linuxbase.org/elf/x86_64-abi-0.98.pdf page 69
    R_X86_64_NONE   = 0,        // No calculation
    R_X86_64_64     = 1,        // Direct 64 bit relocation
    R_X86_64_PC32   = 2,        // truncate value to 32 bits

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

struct relocation *rela_text_data;
int num_rela_text;
char **strtab;
int strtab_len;
struct symbol *symtab_data;
int num_syms;
int data_size, text_size, strtab_size, rela_text_size;
char *data_data, *text_data, *strtab_data, *shstrtab_data, *strtab_data;
int data_size, text_size, strtab_size, rela_text_size;
int last_local_symbol, num_syms;
int shstrtab_len;
char **shstrtab;
int *shstrtab_indexes, *strtab_indexes;
int symtab_size;
int shstrtab_size;
long text_start, shdr_start, shstrtab_start, data_start, strtab_start, symtab_start, rela_text_start;
long total_size;
struct jsr* jsrs;
int jsr_count;

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
    return dst;
}

// Since we don't do negatives, this returns either zero or one.
// zero if the strings are equal, one otherwise.
int wmemcmp(char *str1, char *str2, int size) {
    int i;
    for (i = 0; i < size; i++)
        if (*str1++ != *str2++) return 1;
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

int align(long address, int alignment) {
    return (address + alignment - 1) & (~(alignment-1));
}

void make_string_list(char** strings, int len, char **string_list, int *indexes, int *size) {
    char *result;
    long i;
    char *src, *dst, start;

    *size = 0;
    for (i = 0; i < len; i++) {
        src = strings[i];
        while (*src++);
        *size += (src - strings[i]);
    }

    result = malloc(*size);

    dst = result;
    for (i = 0; i < len; i++) {
        src = strings[i];
        indexes[i] = dst - result;
        while (*dst++ = *src++);
    }

    *string_list = result;
}

void add_section_header(char *headers, int *shstrtab_indexes, int index, int type, long flags,
        long offset, long size, int link, int info, long align, long ent_size) {

    struct section_header *sh;
    sh = (struct section_header *) (headers + sizeof(struct section_header) * index);

    sh->sh_name      = shstrtab_indexes[index];
    sh->sh_type      = type;
    sh->sh_flags     = flags;
    sh->sh_offset    = offset;
    sh->sh_size      = size;
    sh->sh_link      = link;
    sh->sh_info      = info;
    sh->sh_addralign = align;
    sh->sh_entsize   = ent_size;
}

void plan_elf_sections() {
    int shdr_size;
    shdr_size = sizeof(struct section_header);

    // Add section header strings
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

    symtab_size = num_syms * sizeof(struct symbol);

    // Determine section offsets
    shdr_start      = sizeof(struct elf_header);
    data_start      = align(shdr_start      + shdr_size * shstrtab_len, 16);
    text_start      = align(data_start      + data_size,                16);
    shstrtab_start  = align(text_start      + text_size,                16);
    strtab_start    = align(shstrtab_start  + shstrtab_size,            16);
    symtab_start    = align(strtab_start    + strtab_size,              16);
    rela_text_start = align(symtab_start    + symtab_size,              16);
    total_size      = align(rela_text_start + rela_text_size,           16);
}

void write_elf(char *filename) {
    char *s, *d, *p;
    int f, written;
    int *si;
    char *program, *h;
    int data_flags, text_flags;
    struct elf_header *elf_header;

    program = malloc(total_size);
    memset(program, 0, total_size);

    elf_header = (struct elf_header *) program;

    // ELF header
    elf_header->ei_magic0 = 0x7f;                           // Magic
    elf_header->ei_magic1 = 'E';
    elf_header->ei_magic2 = 'L';
    elf_header->ei_magic3 = 'F';
    elf_header->ei_class    = 2;                            // 64-bit
    elf_header->ei_data     = 1;                            // LSB
    elf_header->ei_version  = 1;                            // Original ELF version
    elf_header->ei_osabi    = 0;                            // Unix System V
    elf_header->e_type      = ET_REL;                       // ET_REL Relocatable
    elf_header->e_machine   = E_MACHINE_TYPE_X86_64;        // x86-64
    elf_header->e_version   = 1;                            // EV_CURRENT Current version of ELF
    elf_header->e_phoff     = 0;                            // Offset to program header table
    elf_header->e_shoff     = shdr_start;                   // Offset to section header table
    elf_header->e_ehsize    = sizeof(struct elf_header);    // The size of this header, 0x40 for 64-bit
    elf_header->e_phentsize = 0;                            // The size of the program header
    elf_header->e_phnum     = 0;                            // Number of program header entries
    elf_header->e_shentsize = sizeof(struct section_header);// The size of the section header
    elf_header->e_shnum     = shstrtab_len;                 // Number of section header entries
    elf_header->e_shstrndx  = SEC_SHSTRTAB;                 // The string table index is the fourth section

    // Section headers
    h = program + shdr_start;
    si = shstrtab_indexes;
    data_flags = SHF_WRITE + SHF_ALLOC;
    text_flags = SHF_ALLOC + SHF_EXECINSTR;

    //                        index          type,         flags       offset           size            link        info                   align ent_size
    add_section_header(h, si, SEC_DATA,      SHT_PROGBITS, data_flags, data_start,      data_size,      0,          0,                     0x04, 0);
    add_section_header(h, si, SEC_TEXT,      SHT_PROGBITS, text_flags, text_start,      text_size,      0,          0,                     0x10, 0);
    add_section_header(h, si, SEC_SHSTRTAB,  SHT_STRTAB,   0,          shstrtab_start,  shstrtab_size,  0,          0,                     0x1,  0);
    add_section_header(h, si, SEC_SYMTAB,    SHT_SYMTAB,   0,          symtab_start,    symtab_size,    SEC_STRTAB, last_local_symbol + 1, 0x8,  sizeof(struct symbol));
    add_section_header(h, si, SEC_STRTAB,    SHT_STRTAB,   0,          strtab_start,    strtab_size,    0,          0,                     0x1,  0);
    add_section_header(h, si, SEC_RELA_TEXT, SHT_RELA,     0,          rela_text_start, rela_text_size, SEC_SYMTAB, SEC_TEXT,              0x8,  sizeof(struct relocation));

    // Data
    s =          data_data;      d = &program[data_start];       while (s -          data_data < data_size)           *d++ = *s++;
    s =          text_data;      d = &program[text_start];       while (s -          text_data < text_size)           *d++ = *s++;
    s =          shstrtab_data;  d = &program[shstrtab_start];   while (s -          shstrtab_data < shstrtab_size)   *d++ = *s++;
    s = (char *) symtab_data;    d = &program[symtab_start];     while (s - (char *) symtab_data < symtab_size)       *d++ = *s++;
    s =          strtab_data;    d = &program[strtab_start];     while (s -          strtab_data < strtab_size)       *d++ = *s++;
    s = (char *) rela_text_data; d = &program[rela_text_start];  while (s - (char *) rela_text_data < rela_text_size) *d++ = *s++;

    // Write file
    if (!strcmp(filename, "-"))
        f = 1;
    else {
        f = open(filename, 577, 420); // O_TRUNC=512, O_CREAT=64, O_WRONLY=1, mode=555 http://man7.org/linux/man-pages/man2/open.2.html
        if (f < 0) { printf("Unable to open write output file\n"); exit(1); }
    }
    written = write(f, program, total_size);
    if (written < 0) { printf("Unable to write to output file\n"); exit(1); }
    close(f);
}

void add_symbol(char *name, long value, int type, int binding, int section_index) {
    struct symbol *s;

    if (strtab_len == MAX_STRTAB_LEN) {
        printf("Exceeded max strtab length %d\n", MAX_STRTAB_LEN);
        exit(1);
    }
    strtab[strtab_len++] = name;

    s = &symtab_data[num_syms];
    s->st_value = value;
    s->st_info = (type << 4) + binding;
    s->st_shndx = section_index;
    num_syms++;
    if (num_syms > MAX_SYMTAB_LEN) { printf("Exceeded max symbol table size %d\n", MAX_SYMTAB_LEN); exit(1); }
}

void link_symtab_strings(struct symbol *symtab_data, char *strtab_data, int num_syms) {
    int i;
    char *s;

    s = strtab_data;
    for (i = 0; i < num_syms; i++) {
        if (*s)
            symtab_data[i].st_name = s - strtab_data;
        else
            symtab_data[i].st_name = 0;
        while (*s++);
    }
}

int add_global(int *data_size, char *input) {
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
    name = wstrdup(s);
    *input = '"';

    // Only data is done here
    if (!is_function) {
        value = align(*data_size, size);
        if (*data_size + size > MAX_DATA_SIZE) {
            printf("Exceeded max data size %d\n", MAX_DATA_SIZE);
            exit(1);
        }
        *data_size = value + size;
        binding = STB_LOCAL;
        type = STT_NOTYPE;
        section_index = SEC_DATA;
        add_symbol(name, value, binding, type, section_index);
    }
}

void add_string_literal(char *input) {
    input += 2;
    while (*input != '"') {
        if (data_size == MAX_DATA_SIZE) {
            printf("Exceeded max data size %d\n", MAX_DATA_SIZE);
        }

             if (*input == '\\' && input[1] == 'n') { data_data[(data_size)++] = '\n'; input += 1; }
        else if (*input == '\\' && input[1] == 't') { data_data[(data_size)++] = '\t'; input += 1; }
        else if (*input == '\\' && input[1] == '\\') { data_data[(data_size)++] = '\\'; input += 1; }
        else if (*input == '\\' && input[1] == '"') { data_data[(data_size)++] = '"'; input += 1; }
        else
            data_data[data_size++] = *input;

        input++;
    }

    if (data_size == MAX_DATA_SIZE) {
        printf("Exceeded max data size %d\n", MAX_DATA_SIZE);
    }
    data_data[data_size++] = 0;
}

void add_function_call_relocation(int symbol_index, long offset) {
    struct relocation *r;
    // s is the entry in the relocation table
    r = &rela_text_data[num_rela_text];
    num_rela_text += 1;
    r->r_offset = offset;

    // R_X86_64_PC32 + a link to the .data section
    r->r_info = R_X86_64_PC32 + ((long) symbol_index << 32);
    r->r_addend = -4;
}

int symbol_index(char *name) {
    int i;
    char **ps;

    ps = strtab;
    for (i = 0; i < strtab_len; i++) {
        if (!wstrcmp(*ps, name)) return i;
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
    for (i = 0; i < v - 6; i++) {
        *t++ = 0x48; *t++ = 0x8b; *t++ = 0x84; *t++ = 0x24; *((int *) t) = (char) i * 8; t += 4;           // mov x(%rsp),%rax
        *t++ = 0x48; *t++ = 0x89; *t++ = 0x84; *t++ = 0x24; *((int *) t) = (char) (v - i - 1) * 8; t += 4; // mov %rax, x(%rsp)
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

void builtin_function_call(char **t, char *name, int num_args, int extend_to_long) {
    prepare_function_call(t, num_args);
    *(*t)++ = 0xe8; *t += 4; // callq
    add_function_call_relocation(symbol_index(name), *t - text_data - 4);
    cleanup_function_call(t, num_args);
    if (extend_to_long) { *(*t)++ = 0x48; *(*t)++ = 0x98; } // cltq
}

void backpatch_jsrs() {
    int i;

    for (i = 0; i < jsr_count; i++)
        add_function_call_relocation(symbol_index(jsrs[i].function_name), jsrs[i].offset);
}

int assemble_file(char *input_filename, char *output_filename) {
    int i, f, input_size, filename_len, v, line, function_arg_count, neg, function_call_arg_count, local_stack_size, local_vars_stack_start;
    char *input, *pi, *instr;
    int vm_address;
    int *shstrtab_indexes, *strtab_indexes;
    char *s, *t, *name, **ps;
    struct symbol *symbol;
    struct relocation *r;
    char *main_address;
    long *line_string_literals;
    char **vm_address_map; // map VM address as indicated by the first number in the compiler output to its address in .text
    struct jmp *jmp, *jmps_start, *jmps;
    int *pi1, *pi2;
    int string_literal_len;

    input = malloc(MAX_INPUT_SIZE);
    f  = open(input_filename, 0, 0); // O_RDONLY = 0
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
    symtab_data = malloc(MAX_SYMTAB_LEN * sizeof(struct symbol));
    memset(symtab_data, MAX_SYMTAB_LEN * sizeof(struct symbol), 0);

    add_symbol("",             0, STB_LOCAL,  STT_NOTYPE,  SHN_UNDEF); // 0 null
    add_symbol(input_filename, 0, STB_LOCAL,  STT_FILE,    SHN_ABS);   // 1 filename
    add_symbol("",             0, STB_LOCAL,  STT_SECTION, SEC_DATA);  // 2 .data
    add_symbol("",             0, STB_LOCAL,  STT_SECTION, SEC_TEXT);  // 3 .text

    // First pass, the .data section
    string_literal_len = 0;
    line_string_literals = malloc(sizeof(long) * MAX_LINES);
    vm_address_map = malloc(sizeof(long) * MAX_VM_ADDRESS);
    jmps = malloc(sizeof(struct jmp) * MAX_JMPS);
    memset(jmps, 0, sizeof(struct jmp) * MAX_JMPS);
    jmps_start = jmps;

    jsrs = malloc(sizeof(struct jsr) * MAX_JSRS);
    memset(jsrs, 0,sizeof(struct jsr) * MAX_JSRS);
    jsr_count = 0;

    line = 0;

    pi = input;
    while (*pi) {
        while (*pi >= '0' && *pi <= '9') pi++;
        while (*pi == ' ') pi++;
        instr = pi;
        while (*pi != ' ') pi++;
        while (*pi == ' ') pi++;

        if (!wmemcmp(instr, "GLB", 3)) {
            add_global(&data_size, pi);
        }
        else if (!wmemcmp(instr, "IMM", 3) && !wmemcmp(pi, "&\"", 2)) {
            line_string_literals[line] = data_size;
            add_string_literal(pi);
        }

        while (*pi != '\n') pi++;
        pi++;
        line++;
    }

    last_local_symbol = num_syms - 1;
    text_size = 0;
    rela_text_size = 0;

    add_symbol("printf",  0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("dprintf", 0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("fflush",  0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("malloc",  0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("free",    0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("memset",  0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("memcmp",  0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("strcmp",  0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("open",    0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("read",    0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("write",   0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("close",   0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);
    add_symbol("exit",    0, STB_GLOBAL, STT_NOTYPE, SHN_UNDEF);

    // First pass, the .text section
    text_data = malloc(MAX_TEXT_SIZE);
    memset(text_data, 0, MAX_TEXT_SIZE);

    t = text_data;
    pi = input;
    line = 0;
    num_rela_text = 0;
    rela_text_data = malloc(MAX_RELA_TEXT_SIZE);

    while (*pi) {
        vm_address = 0;
        while (*pi >= '0' && *pi <= '9') vm_address = 10 * vm_address + (*pi++ - '0');
        vm_address_map[vm_address] = t;

        while (*pi == ' ') pi++;
        instr = pi;
        while (*pi != ' ') pi++;
        while (*pi == ' ') pi++;

        if (!wmemcmp(instr, "ADJ", 3)) {}
        else if (!wmemcmp(instr, "LINE", 4)) {}

        else if (!wmemcmp(instr, "GLB   type=1", 12)) {
            s = instr + 21;
            name = instr + 21;
            while (*s != '"') s++;
            *s = 0;
            add_symbol(name, t - text_data, STB_GLOBAL, STT_NOTYPE, SEC_TEXT);
            if (!wmemcmp(name, "main", 4)) main_address = text_data;
        }

        else if (!wmemcmp(instr, "GLB", 3)) {}

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

            // Calculate stack start for locals. reduce by pushed bsp and  above pushed args.
            local_vars_stack_start = -8 - 8 * (function_arg_count <= 6 ? function_arg_count : 6);

            // Allocate stack space for local variables
            if (local_stack_size > 0) {
                *t++= 0x48; *t++= 0x81; *t++= 0xec; // sub x,%rsp
                *((int *) t) = local_stack_size;
                t += 4;
            }
        }

        else if (!wmemcmp(instr, "IMM   global", 12)) {
            s = instr + 13;
            while (*s != ' ') s++;
            s += 2;
            name = s;
            while (*s != '"') s++;
            *s = 0;

            // s is the entry in the relocation table
            r = &rela_text_data[num_rela_text];
            num_rela_text += 1;
            r->r_offset = t - text_data + 2;
            *t++ = 0x48; *t++ = 0xb8; // movabs $0x...,%rax
            t += 8;
            r->r_info = (long) R_X86_64_64 + ((long) (SEC_DATA + 1) << 32); // R_X86_64_64 + a link to the .data section
            r->r_addend = symtab_data[symbol_index(name)].st_value; // Address in .data
        }

        else if (!wmemcmp(instr, "IMM   ", 6)) {
            s = instr + 6;
            if ((*s >= '0' && *s <= '9') || *s == '-') {
                // A number
                neg = 0;
                s = instr + 6;
                if (*s == '-') { neg = 1; s++; }
                v = 0;
                while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
                if (neg) v = -v;
                *t++ = 0x48; *t++ = 0xb8; // movabs $0x...,%rax
                *(long *) t = v;
                t += 8;
            }
            else {
                // A string literal. s is the entry in the relocation table
                r = &rela_text_data[num_rela_text];
                num_rela_text += 1;
                r->r_offset = t - text_data + 3;
                *t++ = 0x48; *t++ = 0x8d; *t++ = 0x05; // lea 0x...(%rip), %rax
                t += 4;
                r->r_info = R_X86_64_PC32 + ((long) (SEC_DATA + 1) << 32); // R_X86_64_PC32 + a link to the .data section
                r->r_addend = line_string_literals[line] - 4;              // Address in .data - 4
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

            jsrs[jsr_count].function_name = name;
            jsrs[jsr_count].offset = t - text_data - 4;
            jsr_count++;

            if (jsr_count == MAX_JSRS) {
                printf("Exceeded max JSRs %d\n", MAX_JSRS);
                exit(1);
            }
            cleanup_function_call(&t, function_call_arg_count);
        }

        else if (!wmemcmp(instr, "BZ", 2)) {
            s = instr + 6;
            v = 0;
            while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
            *t++ = 0x48; *t++ = 0x83; *t++ = 0xf8; *t++ = 0x00; //cmp $0x0,% rax
            jmps->address = (long) t + 2;
            jmps->line = v;
            jmps++;
            *t++ = 0x0f; *t++ = 0x84; // je ...
            t += 4;
        }

        else if (!wmemcmp(instr, "BNZ", 3)) {
            s = instr + 6;
            v = 0;
            while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
            *t++ = 0x48; *t++ = 0x83; *t++ = 0xf8; *t++ = 0x00; //cmp $0x0,% rax
            jmps->address = (long) t + 2;
            jmps->line = v;
            jmps++;
            *t++ = 0x0f; *t++ = 0x85; // jne ...
            t += 4;
        }
        else if (!wmemcmp(instr, "JMP", 3)) {
            s = instr + 6;
            v = 0;
            while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
            jmps->address = (long) t + 1;
            jmps->line = v;
            jmps++;
            *t++ = 0xe9; // jmpq ...
            t += 4;
        }

        else if (!wmemcmp(instr, "PSH", 3)) {
            *t++ = 0x50; // push %rax
        }

        else if (!wmemcmp(instr, "LEA", 3)) {
            neg = 0;
            s = instr + 6;
            if (*s == '-') { neg = 1; s++; }
            v = 0;
            while (*s >= '0' && *s <= '9') v = 10 * v + (*s++ - '0');
            if (neg) v = -v;

            *t++ = 0x48; *t++ = 0x8d; *t++ = 0x85; // lea 0x...(%rbp), %rax

            if (v >= 2) {
                // Function argument
                v -= 2; // 0=1st arg, 1=2nd arg, etc

                if (function_arg_count > 6) {
                    v = function_arg_count - 7 - v;
                    // Correct for split stack when there are more than 6 args
                    if (v < 0) {
                        // Read pushed arg by the callee. arg 0 is at rsp-0x30, arg 2 at rsp-0x28 etc, ... arg 5 at rsp-0x08
                        *((int *) t) =  (char) 8 * v;
                    }
                    else {
                        // Read pushed arg by the caller, arg 6 is at rsp+0x10, arg 7 at rsp+0x18, etc
                        // The +2 is to bypass the pushed rbp and return address
                        *((int *) t) =  (char) 8 * (v + 2);
                    }
                }
                else {
                    // The first arg is at v=0, second at v=1
                    // If there aree.g. 2 args:
                    // arg 0 is at mov -0x10(%rbp), %rax
                    // arg 1 is at mov -0x08(%rbp), %rax
                    *((int *) t) =  (char) -8 * (v + 1);
                }
            }
            else {
                // Local variable. v=-1 is the first, v=-2 the second, etc
                *((int *) t) =  local_vars_stack_start + 8 * (v + 1);
            }

            t+= 4;
        }

        else if (!wmemcmp(instr, "LC", 2)) { *t++ = 0x48; *t++ = 0x0f; *t++ = 0xbe; *t++ = 0x00; } // movsbq (%rax), %rax ; move byte to quad with sign extension
        else if (!wmemcmp(instr, "LS", 2)) { *t++ = 0x48; *t++ = 0x0f; *t++ = 0xbf; *t++ = 0x00; } // movswq (%rax), %rax ; move word to quad with sign extension
        else if (!wmemcmp(instr, "LI", 2)) { *t++ = 0x48; *t++ = 0x63; *t++ = 0x00;              } // movslq (%rax), %rax ; move long to quad with sign extension
        else if (!wmemcmp(instr, "LL", 2)) { *t++ = 0x48; *t++ = 0x8b; *t++ = 0x00;              } // mov (%rax), %rax

        else if (!wmemcmp(instr, "SC ", 3)) {
            *t++ = 0x5f;                                            // pop %rdi
            *t++ = 0x88; *t++ = 0x07;                               // mov %al, (%rdi)
        }

        else if (!wmemcmp(instr, "SS ", 3)) {
            *t++ = 0x5f;                                            // pop %rdi
            *t++ = 0x66; *t++ = 0x89; *t++ = 0x07;                  // mov %ax, (%rdi)
        }

        else if (!wmemcmp(instr, "SI ", 3)) {
            *t++ = 0x5f;                                            // pop %rdi
            *t++ = 0x89; *t++ = 0x07;                               // mov %eax, (%rdi)
        }

        else if (!wmemcmp(instr, "SL ", 3)) {
            *t++ = 0x5f;                                            // pop %rdi
            *t++ = 0x48; *t++ = 0x89; *t++ = 0x07;                  // mov %rax, (%rdi)
        }

        else if (!memcmp(instr, "BNOT", 4)) {
            *t++ = 0x48; *t++ = 0xf7; *t++ = 0xd0;              // not %rax
        }

        else if (!memcmp(instr, "BOR", 3)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x09; *t++ = 0xd0;              // or %rax, %rdx
        }

        else if (!memcmp(instr, "BAND", 4)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x21; *t++ = 0xd0;              // and %rax, %rdx
        }

        else if (!memcmp(instr, "BWL", 3)) {
            *t++ = 0x48; *t++ = 0x89; *t++ = 0xc1; // mov    %rax,%rcx
            *t++ = 0x58;                           // pop    %rax
            // *t++ = 0X59;                                        // pop %rcx
            *t++ = 0x48; *t++ = 0xd3; *t++ = 0xe0;              // shl %cl, %rax
        }

        else if (!memcmp(instr, "BWR", 3)) {
            *t++ = 0x48; *t++ = 0x89; *t++ = 0xc1; // mov    %rax,%rcx
            *t++ = 0x58;                           // pop    %rax
            // *t++ = 0X59;                                        // pop %rcx
            *t++ = 0x48; *t++ = 0xd3; *t++ = 0xf8;              // sar %cl, %rax
        }

        else if (!memcmp(instr, "XOR", 3)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x31; *t++ = 0xd0;              // xor %rax, %rdx
        }

        else if (!memcmp(instr, "OR",  2)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x83; *t++ = 0xfa; *t++ = 0x00; // cmp $0x0, %rdx
            *t++ = 0x0f; *t++ = 0x84;                           // jne end
            *t++ = 0x03; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;
            *t++ = 0x48; *t++ = 0x89;  *t++ = 0xd0;             // mov %rdx, %rax
        }

        else if (!memcmp(instr, "AND", 3)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x83; *t++ = 0xfa; *t++ = 0x00; // cmp $0x0, %rdx
            *t++ = 0x0f; *t++ = 0x85;                           // je end
            *t++ = 0x03; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;
            *t++ = 0x48; *t++ = 0x89;  *t++ = 0xd0;             // mov %rdx, %rax
        }

        else if (!memcmp(instr, "EQ",  2)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x39; *t++ = 0xc2;              // cmp %rdx, %rax
            *t++ = 0x0f; *t++ = 0x94; *t++ = 0xc0;              // sete %al
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xb6; *t++ = 0xc0; // movzbl %al, %rax
        }

        else if (!memcmp(instr, "NE",  2)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x39; *t++ = 0xc2;              // cmp %rdx, %rax
            *t++ = 0x0f; *t++ = 0x95; *t++ = 0xc0;              // setne %al
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xb6; *t++ = 0xc0; // movzbl %al, %rax
        }

        else if (!memcmp(instr, "LT",  2)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x39; *t++ = 0xc2;              // cmp %rdx, %rax
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xb6; *t++ = 0xc0; // movzbl %al, %rax
            *t++ = 0x0f; *t++ = 0x9c; *t++ = 0xc0;              // setl %al
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xb6; *t++ = 0xc0; // movzbl %al, %rax
        }

        else if (!memcmp(instr, "GT",  2)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x39; *t++ = 0xc2;              // cmp %rdx, %rax
            *t++ = 0x0f; *t++ = 0x9f; *t++ = 0xc0;              // setg %al
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xb6; *t++ = 0xc0; // movzbl %al, %rax
        }

        else if (!memcmp(instr, "LE",  2)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x39; *t++ = 0xc2;              // cmp %rdx, %rax
            *t++ = 0x0f; *t++ = 0x9e; *t++ = 0xc0;              // setle %al
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xb6; *t++ = 0xc0; // movzbl %al, %rax
        }

        else if (!memcmp(instr, "GE",  2)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x39; *t++ = 0xc2;              // cmp %rdx, %rax
            *t++ = 0x0f; *t++ = 0x9d; *t++ = 0xc0;              // setge %al
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xb6; *t++ = 0xc0; // movzbl %al, %rax
        }

        else if (!memcmp(instr, "ADD", 3)) {
            *t++ = 0x5a;                            // pop %rdx
            *t++ = 0x48; *t++ = 0x01; *t++ = 0xd0;  // add %rdx, %rax
        }

        else if (!memcmp(instr, "SUB", 3)) {
            *t++ = 0x48; *t++ = 0x89; *t++ = 0xc3; // mov %rax, %rbx
            *t++ = 0x58;                           // pop %rax
            *t++ = 0x48; *t++ = 0x29; *t++ = 0xd8; // sub %rbx, %rax
        }

        else if (!memcmp(instr, "MUL", 3)) {
            *t++ = 0x5a;                                        // pop %rdx
            *t++ = 0x48; *t++ = 0x0f; *t++ = 0xaf; *t++ = 0xc2; // imul %rdx, %rax
        }

        else if (!memcmp(instr, "DIV", 3)) {
            *t++ = 0x48; *t++ = 0x89; *t++ = 0xc3; // mov %rax, %rbx
            *t++ = 0x58;                           // pop %rax
            *t++ = 0x48; *t++ = 0x99;              // sign extend rax to rdx:rax
            *t++ = 0x48; *t++ = 0xf7; *t++ = 0xfb; // idiv %rbx
        }

        else if (!memcmp(instr, "MOD", 3)) {
            // Same as DIV, but with rdx (the remainder) shifted to rax
            *t++ = 0x48; *t++ = 0x89; *t++ = 0xc3; // mov %rax, %rbx
            *t++ = 0x58;                           // pop %rax
            *t++ = 0x48; *t++ = 0x99;              // sign extend rax to rdx:rax
            *t++ = 0x48; *t++ = 0xf7; *t++ = 0xfb; // idiv %rbx
            *t++ = 0x48; *t++ = 0x89; *t++ = 0xd0; // mov %rdx, %rax
        }

        else if (!wmemcmp(instr, "PRTF", 4) || !wmemcmp(instr, "DPRT", 4)) {
            s = instr + 6;
            function_call_arg_count = 0;
            while (*s >= '0' && *s <= '9') function_call_arg_count = 10 * function_call_arg_count + (*s++ - '0');
            prepare_function_call(&t, function_call_arg_count);
            // The number of floating point arguments is zero
            *t++ = 0xb8; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; // mov $0x0,%eax
            *t++ = 0xe8; t += 4;                                             // callq
            name = wmemcmp(instr, "PRTF", 4) ? "dprintf" : "printf";
            add_function_call_relocation(symbol_index(name), t - text_data - 4);
            cleanup_function_call(&t, function_call_arg_count);
        }

        else if (!memcmp(instr, "OPEN", 4)) { builtin_function_call(&t, "open",    3, 1); }
        else if (!memcmp(instr, "READ", 4)) { builtin_function_call(&t, "read",    3, 1); }
        else if (!memcmp(instr, "WRIT", 4)) { builtin_function_call(&t, "write",   3, 1); }
        else if (!memcmp(instr, "CLOS", 4)) { builtin_function_call(&t, "close",   1, 1); }
        else if (!memcmp(instr, "MALC", 4)) { builtin_function_call(&t, "malloc",  1, 0); }
        else if (!memcmp(instr, "FREE", 4)) { builtin_function_call(&t, "free",    1, 1); }
        else if (!memcmp(instr, "MSET", 4)) { builtin_function_call(&t, "memset",  3, 1); }
        else if (!memcmp(instr, "MCMP", 4)) { builtin_function_call(&t, "memcmp",  3, 1); }
        else if (!memcmp(instr, "SCMP", 4)) { builtin_function_call(&t, "strcmp",  2, 1); }
        else if (!memcmp(instr, "EXIT", 4)) { builtin_function_call(&t, "exit",    1, 1); }

        else {
            printf("Unimplemented instruction: %.5s ", instr);
            while (*pi != '\n') { printf("%c", *pi); pi++; }
            printf("\n");
            exit(1);
        }

        while (*pi != '\n') pi++;
        pi++;

        line++;
        if (line > MAX_LINES) {
            printf("Exceeded max lines %d\n", MAX_LINES);
            exit(1);
        }
    }

    // Fixup jmps
    jmp = jmps_start;
    while (jmp->line) {
        *(int *) jmp->address = ((long) vm_address_map[jmp->line] - (long) jmp->address - 4);
        jmp++;
    }

    // _start function
    add_symbol("_start", t - text_data, STB_GLOBAL, STT_NOTYPE, SEC_TEXT);

    *t++ = 0x31; *t++ = 0xed;              // xor %ebp, %ebp
    *t++ = 0x5f;                           // pop %rdi        argc
    *t++ = 0x48; *t++ = 0x89; *t++ = 0xe6; // mov %rsp, %rsi  argv

    // callq main
    *t++ = 0xe8; t += 4;
    add_function_call_relocation(symbol_index("main"), t - text_data - 4);

    // Flush all open output streams with fflush(0)
    *t++ = 0x50;                                                        // push %rax
    *t++ = 0xbf; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;    // mov $0, %edi
    *t++ = 0xe8; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;    // callq fflush

    add_function_call_relocation(symbol_index("fflush"), t - text_data - 4);
    *t++ = 0x58;                                                        // pop %rax

    // exit(%rax)
    *t++ = 0x48; *t++ = 0x89; *t++ = 0xc7;                              // mov    %rax, %rdi
    *t++ = 0xb8; *t++ = 0x3c; *t++ = 0x00; *t++ = 0x00; *t++ = 0x00;    // mov    $0x3c, %eax
    *t++ = 0x0f; *t++ = 0x05;                                           // syscall

    text_size = t - text_data;

    backpatch_jsrs();

    rela_text_size = num_rela_text * sizeof(struct relocation);

    strtab_indexes = malloc(sizeof(int) * strtab_len);
    make_string_list(strtab, strtab_len, &strtab_data, strtab_indexes, &strtab_size);
    link_symtab_strings(symtab_data, strtab_data, num_syms);

    plan_elf_sections();
    write_elf(output_filename);
}

int main(int argc, char **argv) {
    int help, filename_len;
    char *input_filename, *output_filename;

    help = 0;
    output_filename = 0;

    argc--;
    argv++;
    while (argc > 0 && *argv[0] == '-') {
             if (argc > 0 && !memcmp(argv[0], "-h",  3)) { help = 0;  argc--; argv++; }
        else if (argc > 1 && !memcmp(argv[0], "-o",  2)) {
            output_filename = argv[1];
            argc -= 2;
            argv += 2;
        }
        else { printf("Unknown argument %s\n", argv[0]); exit(1); }
    }

    if (help || argc < 1) {
        printf("Usage: was4 [--hw] FILENAME\n\n");
        printf("Flags\n");
        printf("-o Output filename. Use - for stdout.\n");
        printf("-h Help\n");
        exit(1);
    }

    input_filename = argv[0];

    if (!output_filename) {
        // Write output file with .o extension from an expected source extension of .ws
        filename_len = wstrlen(input_filename);
        if (filename_len > 3 && input_filename[filename_len - 3] == '.' && input_filename[filename_len - 2] == 'w' && input_filename[filename_len - 1] == 's') {
            output_filename = malloc(filename_len + 2);
            wstrcpy(output_filename, input_filename);
            output_filename[filename_len - 2] = 'o';
            output_filename[filename_len - 1] = 0;
        }
        else {
            printf("Unable to determine output filename\n");
            exit(1);
        }
    }

    assemble_file(input_filename, output_filename);
}
