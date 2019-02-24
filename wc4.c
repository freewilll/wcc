#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

struct symbol {
    int type;                                   // Type
    int size;                                   // Size
    char *identifier;                           // Identifier
    int scope;                                  // Scope
    long value;                                 // Value in the case of a constant
    int stack_index;                            // For locals, index on the stack, starting with -1 and going downwards
    int is_function;                            // Is the symbol a function?
    int function_param_count;                   // For functions, number of parameters
    int function_local_symbol_count;            // For functions, number of local symbols
    int function_vreg_count;                    // For functions, number of virtual registers used in IR
    int function_spilled_register_count;        // For functions, amount of stack space needed for registers spills
    int function_call_count;                    // For functions, number of calls to other functions
    struct three_address_code *function_ir;     // For functions, intermediate representation
    int function_builtin;                       // For builtin functions, IR number of the builtin
    int function_is_variadic;                   // Set to 1 for builtin variadic functions
    int is_enum;                                // Enums are symbols with a value
};

// struct value is a value on the value stack. A value can be one of
// - global
// - local
// - constant
// - string literal
// - register
struct value {
    int type;                       // Type
    int vreg;                       // Optional vreg number
    int preg;                       // Allocated physical register
    int is_lvalue;                  // Is the value an lvalue?
    int spilled_stack_index;        // Allocated stack index in case of a register spill
    int stack_index;                // Stack index for local variable. Zero if it's not a local variable. Starts at -1 and grows downwards.
    int is_constant;                // Is it a constant? If so, value is the value.
    int is_string_literal;          // Is the value a string literal?
    int is_in_cpu_flags;            // Is the result stored in cpu flags?
    int string_literal_index;       // Index in the string_literals array in the case of a string literal
    long value;                     // Value in the case of a constant
    struct symbol *function_symbol; // Corresponding symbol in the case of a function call
    int function_call_arg_count;    // Number of arguments in the case of a function call
    struct symbol *global_symbol;   // Pointer to a global symbol if the value is a global symbol
    int label;                      // Target label in the case of jump instructions
    int original_stack_index;
    int original_is_lvalue;
    int original_vreg;
};

struct three_address_code {
    int operation;                      // IR_* operation
    int label;                          // Label if this instruction is jumped to
    struct value *dst;                  // Destination
    struct value *src1;                 // First rhs operand
    struct value *src2;                 // Second rhs operand
    struct three_address_code *next;    // Next in a linked-list
    struct three_address_code *prev;    // Previous in a linked-list
    int in_conditional;                 // Used for live range extending. True if the code is inside an if or ternary.
};

// Temporary struct for reversing function call arguments
struct tac_interval {
    struct three_address_code *start;
    struct three_address_code *end;
};

// Start and end indexes in the IR
struct liveness_interval {
    int start;
    int end;
};

// Struct member
struct struct_member {
    char *identifier;
    int type;
    int offset;
};

// Struct description
struct struct_desc {
    int type;
    char *identifier;
    struct struct_member **members;
    int size;
    int is_incomplete;          // Set to 1 if the struct has been used in a member but not yet declared
};

int verbose;                    // Print invoked program command lines
int compile;                    // Compile .c file
int run_assembler;              // Assemble .s file
int run_linker;                 // Link .o file
int target_is_object_file;
int target_is_assembly_file;
int print_spilled_register_count;
int print_ir1;                  // Print IR after parsing
int print_ir2;                  // Print IR after x84_64 arch manipulation
int print_ir3;                  // Print IR after register allocation
int fake_register_pressure;     // Simulate running out of all registers, triggering spill code
int output_inline_ir;           // Output IR inline with the assembly
int opt_enable_register_coalescing;   // Merge registers that can be reused within the same operation
int opt_use_registers_for_locals;     // Experimental. Don't use the stack for local variables.
int opt_merge_redundant_moves;        // Merge move statements that are only things between registers
int opt_spill_furthest_liveness_end;  // Prioritize spilling physical registers with furthest liveness end

char *input;                    // Input file data
int input_size;                 // Size of the input file
int ip;                         // Offset into *input, used by the lexer
int cur_line;                   // Current line number being lexed
int cur_token;                  // Current token
char *cur_identifier;           // Current identifier if the token is an identifier
long cur_long;                  // Current long if the token is a number
char *cur_string_literal;       // Current string literal if the token is a string literal
int cur_scope;                  // Current scope. 0 is global. non-zero is function. Nested scopes isn't implemented.
char **string_literals;         // Each string literal has an index in this array, with a pointer to the string literal
int string_literal_count;       // Amount of string literals

struct symbol *cur_function_symbol;     // Currently parsed function
struct value *cur_loop_continue_dst;    // Target jmp of continue statement in the current for/while loop
struct value *cur_loop_break_dst;       // Target jmp of break statement in the current for/while loop
int in_conditional;                     // Used in the parser to determine if something is in a conditional

struct symbol *symbol_table;    // Symbol table, terminated by a null symbol
struct symbol *next_symbol;     // Next free symbol in the symbol table

struct value **vs_start;        // Value stack start
struct value **vs;              // Value stack current position
struct value *vtop;             // Value at the top of the stack

struct struct_desc **all_structs; // All structs defined globally. Local struct definitions isn't implemented.
int all_structs_count;            // Number of structs, complete and incomplete

struct three_address_code *ir_start, *ir;   // intermediate representation for currently parsed function
int vreg_count;                             // Virtual register count for currently parsed function
int label_count;                            // Global label count, always growing
int function_call_count;                    // Uniquely identify a function call, always growing
int cur_loop;                               // Current loop being parsed
int loop_count;                             // Loop counter
int spilled_register_count;                 // Spilled register count for current function that's undergoing register allocation
int total_spilled_register_count;           // Spilled register count for all functions

struct liveness_interval *liveness;         // Keeps track of live vregs. Array of length vreg_count. Each element is a vreg, or zero if not live.
int *physical_registers;                    // Associated liveness interval for each in-use physical register.
int *spilled_registers;                     // Associated liveness interval for each in-use spilled register.
int *callee_saved_registers;                // Constant list of length PHYSICAL_REGISTER_COUNT. Set to 1 for registers that must be preserved in function calls.

int debug_register_allocations;

void *f; // Output file handle

enum {
    DATA_SIZE                  = 10485760,
    INSTRUCTIONS_SIZE          = 10485760,
    SYMBOL_TABLE_SIZE          = 10485760,
    MAX_STRUCTS                = 1024,
    MAX_STRUCT_MEMBERS         = 1024,
    MAX_INPUT_SIZE             = 10485760,
    MAX_STRING_LITERALS        = 10240,
    VALUE_STACK_SIZE           = 10240,
    MAX_VREG_COUNT             = 10240,
    PHYSICAL_REGISTER_COUNT    = 15,
    MAX_SPILLED_REGISTER_COUNT = 1024,
};

// Tokens in order of precedence
enum {
    TOK_EOF=1,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING_LITERAL,
    TOK_IF,
    TOK_ELSE,
    TOK_VOID,
    TOK_CHAR,
    TOK_INT,
    TOK_SHORT,          // 10
    TOK_LONG,
    TOK_STRUCT,
    TOK_WHILE,
    TOK_FOR,
    TOK_CONTINUE,
    TOK_BREAK,
    TOK_RETURN,
    TOK_ENUM,
    TOK_SIZEOF,
    TOK_RPAREN,         // 20
    TOK_LPAREN,
    TOK_RCURLY,
    TOK_LCURLY,
    TOK_SEMI,
    TOK_COMMA,
    TOK_EQ,
    TOK_PLUS_EQ,
    TOK_MINUS_EQ,       // 30
    TOK_TERNARY,
    TOK_COLON,
    TOK_OR,
    TOK_AND,
    TOK_BITWISE_OR,
    TOK_XOR,
    TOK_ADDRESS_OF,
    TOK_DBL_EQ,
    TOK_NOT_EQ,
    TOK_LT,             // 40
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_BITWISE_LEFT,
    TOK_BITWISE_RIGHT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_MOD,            // 50
    TOK_LOGICAL_NOT,
    TOK_BITWISE_NOT,
    TOK_INC,
    TOK_DEC,
    TOK_DOT,
    TOK_ARROW,
    TOK_RBRACKET,
    TOK_LBRACKET,
    TOK_ATTRIBUTE,
    TOK_PACKED,
};

// Types. All structs start at TYPE_STRUCT up to TYPE_PTR. Pointers are represented by adding TYPE_PTR to a type.
// all_structs[i] corresponds to type i - TYPE_STRUCT
enum {
    TYPE_VOID   = 1,
    TYPE_CHAR   = 2,
    TYPE_SHORT  = 3,
    TYPE_INT    = 4,
    TYPE_LONG   = 5,
    TYPE_STRUCT = 16,
    TYPE_PTR    = 1024,
};

// Intermediate representation operations
enum {
    IR_LOAD_CONSTANT=1,     // Load constant
    IR_LOAD_STRING_LITERAL, // Load string literal
    IR_LOAD_VARIABLE,       // Load global or local
    IR_INDIRECT,            // Pointer or lvalue dereference
    IR_START_CALL,          // Function call
    IR_ARG,                 // Function call argument
    IR_CALL,                // Start of function call
    IR_END_CALL,            // End of function call
    IR_RETURN,              // Return in function
    IR_START_LOOP,          // Start of a for or while loop
    IR_END_LOOP,            // End of a for or while loop
    IR_ASSIGN,              // Assignment/store. Target is either a global, local, lvalue in register or register
    IR_NOP,                 // No operation. Used for label destinations. No code is generated for this other than the label itself.
    IR_JMP,                 // Unconditional jump
    IR_JZ,                  // Jump if zero
    IR_JNZ,                 // Jump if not zero
    IR_ADD,                 // +
    IR_SUB,                 // -
    IR_RSUB,                // reverse -, used to facilitate code generation for the x86 SUB instruction
    IR_MUL,                 // *
    IR_DIV,                 // /
    IR_MOD,                 // %
    IR_EQ,                  // ==
    IR_NE,                  // !=
    IR_BNOT,                // Binary not ~
    IR_BOR,                 // Binary or |
    IR_BAND,                // Binary and &
    IR_XOR,                 // Binary xor ^
    IR_BSHL,                // Binary shift left <<
    IR_BSHR,                // Binary shift right >>
    IR_LT,                  // <
    IR_GT,                  // >
    IR_LE,                  // <=
    IR_GE,                  // >=
    IR_EXIT,                // Builtin functions
    IR_FOPEN,
    IR_FREAD,
    IR_FWRITE,
    IR_FCLOSE,
    IR_CLOSE,
    IR_STDOUT,
    IR_PRINTF,
    IR_FPRINTF,
    IR_MALLOC,
    IR_FREE,
    IR_MEMSET,
    IR_MEMCMP,
    IR_STRCMP,
    IR_STRLEN,
    IR_STRCPY,
    IR_STRRCHR,
    IR_SPRINTF,
    IR_ASPRINTF,
    IR_STRDUP,
    IR_MEMCPY,
    IR_MKTEMPS,
    IR_PERROR,
    IR_SYSTEM,
};

// Physical registers
enum {
    REG_RAX,
    REG_RBX,
    REG_RCX,
    REG_RDX,
    REG_RSI,
    REG_RDI,
    REG_RBP,
    REG_RSP,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
};

int parse_struct_base_type(int parse_struct_base_type);
struct value *load_constant(struct value *cv);
void expression(int level);

void panic(char *message) {
    printf("%d: %s\n", cur_line, message);
    exit(1);
}

void panic1d(char *fmt, int i) {
    printf("%d: ", cur_line);
    printf(fmt, i);
    printf("\n");
    exit(1);
}

void panic1s(char *fmt, char *s) {
    printf("%d: ", cur_line);
    printf(fmt, s);
    printf("\n");
    exit(1);
}

void panic2d(char *fmt, int i1, int i2) {
    printf("%d: ", cur_line);
    printf(fmt, i1, i2);
    printf("\n");
    exit(1);
}

void panic2s(char *fmt, char *s1, char *s2) {
    printf("%d: ", cur_line);
    printf(fmt, s1, s2);
    printf("\n");
    exit(1);
}

// Lexer. Lex a next token or TOK_EOF if the file is ended
void next() {
    char *i;        // Assigned to input for brevity
    int j;          // Counter
    char c1, c2;    // Next two characters in input

    i = input;

    while (ip < input_size) {
        c1 = i[ip];
        c2 = i[ip + 1];

        if (c1 == ' ' || c1 == '\t') { ip++; continue; }
        else if (c1 == '\n') { ip++; cur_line++; continue; }

        else if (c1 == '/' && input_size - ip >= 2 && c2 == '/') {
            ip += 2;
            while (i[ip++] != '\n');
            cur_line++;
            continue;
        }

        else if (                        c1 == '('                        )  { ip += 1;  cur_token = TOK_LPAREN;                     }
        else if (                        c1 == ')'                        )  { ip += 1;  cur_token = TOK_RPAREN;                     }
        else if (                        c1 == '['                        )  { ip += 1;  cur_token = TOK_LBRACKET;                   }
        else if (                        c1 == ']'                        )  { ip += 1;  cur_token = TOK_RBRACKET;                   }
        else if (                        c1 == '{'                        )  { ip += 1;  cur_token = TOK_LCURLY;                     }
        else if (                        c1 == '}'                        )  { ip += 1;  cur_token = TOK_RCURLY;                     }
        else if (                        c1 == '*'                        )  { ip += 1;  cur_token = TOK_MULTIPLY;                   }
        else if (                        c1 == '/'                        )  { ip += 1;  cur_token = TOK_DIVIDE;                     }
        else if (                        c1 == '%'                        )  { ip += 1;  cur_token = TOK_MOD;                        }
        else if (                        c1 == ','                        )  { ip += 1;  cur_token = TOK_COMMA;                      }
        else if (                        c1 == ';'                        )  { ip += 1;  cur_token = TOK_SEMI;                       }
        else if (                        c1 == '?'                        )  { ip += 1;  cur_token = TOK_TERNARY;                    }
        else if (                        c1 == ':'                        )  { ip += 1;  cur_token = TOK_COLON;                      }
        else if (                        c1 == '.'                        )  { ip += 1;  cur_token = TOK_DOT;                        }
        else if (input_size - ip >= 2 && c1 == '&' && c2 == '&'           )  { ip += 2;  cur_token = TOK_AND;                        }
        else if (input_size - ip >= 2 && c1 == '|' && c2 == '|'           )  { ip += 2;  cur_token = TOK_OR;                         }
        else if (input_size - ip >= 2 && c1 == '=' && c2 == '='           )  { ip += 2;  cur_token = TOK_DBL_EQ;                     }
        else if (input_size - ip >= 2 && c1 == '!' && c2 == '='           )  { ip += 2;  cur_token = TOK_NOT_EQ;                     }
        else if (input_size - ip >= 2 && c1 == '<' && c2 == '='           )  { ip += 2;  cur_token = TOK_LE;                         }
        else if (input_size - ip >= 2 && c1 == '>' && c2 == '='           )  { ip += 2;  cur_token = TOK_GE;                         }
        else if (input_size - ip >= 2 && c1 == '>' && c2 == '>'           )  { ip += 2;  cur_token = TOK_BITWISE_RIGHT;              }
        else if (input_size - ip >= 2 && c1 == '<' && c2 == '<'           )  { ip += 2;  cur_token = TOK_BITWISE_LEFT;               }
        else if (input_size - ip >= 2 && c1 == '+' && c2 == '+'           )  { ip += 2;  cur_token = TOK_INC;                        }
        else if (input_size - ip >= 2 && c1 == '-' && c2 == '-'           )  { ip += 2;  cur_token = TOK_DEC;                        }
        else if (input_size - ip >= 2 && c1 == '+' && c2 == '='           )  { ip += 2;  cur_token = TOK_PLUS_EQ;                    }
        else if (input_size - ip >= 2 && c1 == '-' && c2 == '='           )  { ip += 2;  cur_token = TOK_MINUS_EQ;                   }
        else if (input_size - ip >= 2 && c1 == '-' && c2 == '>'           )  { ip += 2;  cur_token = TOK_ARROW;                      }
        else if (                        c1 == '+'                        )  { ip += 1;  cur_token = TOK_PLUS;                       }
        else if (                        c1 == '-'                        )  { ip += 1;  cur_token = TOK_MINUS;                      }
        else if (                        c1 == '='                        )  { ip += 1;  cur_token = TOK_EQ;                         }
        else if (                        c1 == '<'                        )  { ip += 1;  cur_token = TOK_LT;                         }
        else if (                        c1 == '>'                        )  { ip += 1;  cur_token = TOK_GT;                         }
        else if (                        c1 == '!'                        )  { ip += 1;  cur_token = TOK_LOGICAL_NOT;                }
        else if (                        c1 == '~'                        )  { ip += 1;  cur_token = TOK_BITWISE_NOT;                }
        else if (                        c1 == '&'                        )  { ip += 1;  cur_token = TOK_ADDRESS_OF;                 }
        else if (                        c1 == '|'                        )  { ip += 1;  cur_token = TOK_BITWISE_OR;                 }
        else if (                        c1 == '^'                        )  { ip += 1;  cur_token = TOK_XOR;                        }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\t'",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\t';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\n'",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\n';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\''",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\'';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\"'", 4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\"';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\\'", 4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\\';    }

        else if (input_size - ip >= 3 && c1 == '\'' && i[ip+2] == '\'') { cur_long = i[ip+1]; ip += 3; cur_token = TOK_NUMBER; }

        // Identifier or keyword
        else if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_') {
            cur_token = TOK_IDENTIFIER;
            cur_identifier = malloc(1024);
            j = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) {
                cur_identifier[j] = i[ip];
                j++; ip++;
            }
            cur_identifier[j] = 0;

                 if (!strcmp(cur_identifier, "if"           )) { cur_token = TOK_IF;        }
            else if (!strcmp(cur_identifier, "else"         )) { cur_token = TOK_ELSE;      }
            else if (!strcmp(cur_identifier, "char"         )) { cur_token = TOK_CHAR;      }
            else if (!strcmp(cur_identifier, "short"        )) { cur_token = TOK_SHORT;     }
            else if (!strcmp(cur_identifier, "int"          )) { cur_token = TOK_INT;       }
            else if (!strcmp(cur_identifier, "long"         )) { cur_token = TOK_LONG;      }
            else if (!strcmp(cur_identifier, "void"         )) { cur_token = TOK_VOID;      }
            else if (!strcmp(cur_identifier, "struct"       )) { cur_token = TOK_STRUCT;    }
            else if (!strcmp(cur_identifier, "while"        )) { cur_token = TOK_WHILE;     }
            else if (!strcmp(cur_identifier, "for"          )) { cur_token = TOK_FOR;       }
            else if (!strcmp(cur_identifier, "continue"     )) { cur_token = TOK_CONTINUE;  }
            else if (!strcmp(cur_identifier, "break"        )) { cur_token = TOK_BREAK;     }
            else if (!strcmp(cur_identifier, "return"       )) { cur_token = TOK_RETURN;    }
            else if (!strcmp(cur_identifier, "enum"         )) { cur_token = TOK_ENUM;      }
            else if (!strcmp(cur_identifier, "sizeof"       )) { cur_token = TOK_SIZEOF;    }
            else if (!strcmp(cur_identifier, "__attribute__")) { cur_token = TOK_ATTRIBUTE; }
            else if (!strcmp(cur_identifier, "__packed__"   )) { cur_token = TOK_PACKED;    }
            else if (!strcmp(cur_identifier, "packed"       )) { cur_token = TOK_PACKED;    }
        }

        // Hex numeric literal
        else if (c1 == '0' && input_size - ip >= 2 && i[ip+1] == 'x') {
            ip += 2;
            cur_token = TOK_NUMBER;
            cur_long = 0;
            while (((i[ip] >= '0' && i[ip] <= '9')  || (i[ip] >= 'a' && i[ip] <= 'f')) && ip < input_size) {
                cur_long = cur_long * 16 + (i[ip] >= 'a' ? i[ip] - 'a' + 10 : i[ip] - '0');
                ip++;
            }
        }

        // Decimal numeric literal
        else if ((c1 >= '0' && c1 <= '9')) {
            cur_token = TOK_NUMBER;
            cur_long = 0;
            while ((i[ip] >= '0' && i[ip] <= '9') && ip < input_size) { cur_long = cur_long * 10 + (i[ip] - '0'); ip++; }
        }

        // Ignore CPP directives
        else if (c1 == '#') {
            while (i[ip++] != '\n');
            cur_line++;
            continue;
        }

        // String literal
        else if (i[ip] == '"') {
            cur_token = TOK_STRING_LITERAL;
            cur_string_literal = malloc(1024);
            j = 0;
            ip += 1;
            while (input_size - ip >= 1 && i[ip] != '"') {
                     if (i[ip] != '\\') { cur_string_literal[j] = i[ip]; j++; ip++; }
                else if (input_size - ip >= 2 && i[ip] == '\\') {
                         if (i[ip+1] == 't' ) cur_string_literal[j++] = 9;
                    else if (i[ip+1] == 'n' ) cur_string_literal[j++] = 10;
                    else if (i[ip+1] == '\\') cur_string_literal[j++] = '\\';
                    else if (i[ip+1] == '\'') cur_string_literal[j++] = '\'';
                    else if (i[ip+1] == '\"') cur_string_literal[j++] = '\"';
                    else panic("Unknown \\ escape in string literal");
                    ip += 2;
                }
            }
            ip++;
            cur_string_literal[j] = 0;
        }

        else
            panic1d("Unknown token %d", cur_token);

        return;
    }

    cur_token = TOK_EOF;
}

void expect(int token, char *what) {
    if (cur_token != token) panic1s("Expected %s", what);
}

void consume(int token, char *what) {
    expect(token, what);
    next();
}

// A useful function for debugging
void print_value_stack() {
    struct value **lvs, *v;

    printf("%-4s %-4s %-4s %-11s %-11s %-5s\n", "type", "vreg", "preg", "global_sym", "stack_index", "is_lv");
    lvs = vs;
    while (lvs != vs_start) {
        v = *lvs;
        printf("%-4d %-4d %-4d %-11s %-11d %-5d\n",
            v->type, v->vreg, v->preg, v->global_symbol ? v->global_symbol->identifier : 0, v->stack_index, v->is_lvalue);
        lvs++;
    }
}

struct value *new_value() {
    struct value *v;

    v = malloc(sizeof(struct value));
    memset(v, 0, sizeof(struct value));
    v->preg = -1;
    v->spilled_stack_index = -1;

    return v;
}

// Push a value to the stack
void push(struct value *v) {
    *--vs = v;
    vtop = *vs;
}

// Pop a value from the stack
struct value *pop() {
    struct value *result;

    result = *vs++;
    vtop = *vs;

    return result;
}

// Duplicate a value
struct value *dup_value(struct value *src) {
    struct value *dst;

    dst = new_value();
    dst->type                      = src->type;
    dst->vreg                      = src->vreg;
    dst->preg                      = src->preg;
    dst->is_lvalue                 = src->is_lvalue;
    dst->spilled_stack_index       = src->spilled_stack_index;
    dst->stack_index               = src->stack_index;
    dst->is_constant               = src->is_constant;
    dst->is_string_literal         = src->is_string_literal;
    dst->is_in_cpu_flags           = src->is_in_cpu_flags;
    dst->string_literal_index      = src->string_literal_index;
    dst->value                     = src->value;
    dst->function_symbol           = src->function_symbol;
    dst->function_call_arg_count   = src->function_call_arg_count;
    dst->global_symbol             = src->global_symbol;
    dst->label                     = src->label;

    return dst;
}

struct three_address_code *new_instruction(int operation) {
    struct three_address_code *tac;

    tac = malloc(sizeof(struct three_address_code));
    memset(tac, 0, sizeof(struct three_address_code));
    tac->operation = operation;

    return tac;
}

// Add instruction to the global intermediate representation ir
struct three_address_code *add_instruction(int operation, struct value *dst, struct value *src1, struct value *src2) {
    struct three_address_code *tac;

    tac = new_instruction(operation);
    tac->label = 0;
    tac->dst = dst;
    tac->src1 = src1;
    tac->src2 = src2;
    tac->next = 0;
    tac->prev = 0;
    tac->in_conditional = in_conditional;

    if (!ir_start) {
        ir_start = tac;
        ir = tac;
    }
    else {
        tac->prev = ir;
        ir->next = tac;
        ir = tac;
    }

    return tac;
}

// Allocate a new virtual register
int new_vreg() {
    vreg_count++;
    if (vreg_count >= MAX_VREG_COUNT) panic1d("Exceeded max vreg count %d", MAX_VREG_COUNT);
    return vreg_count;
}

// Load a value into a register.
struct value *load(struct value *src1) {
    struct value *dst;

    dst = dup_value(src1);
    dst->vreg = new_vreg();
    dst->is_lvalue = 0;
    dst->is_in_cpu_flags = 0;

    if (src1->vreg && src1->is_lvalue) {
        // An lvalue in a register needs a dereference
        add_instruction(IR_INDIRECT, dst, src1, 0);
    }
    else if (src1->vreg)
        panic("Internal error: unexpected register");
    else if (src1->is_in_cpu_flags) {
        dst->stack_index = 0;
        dst->global_symbol = 0;
        add_instruction(IR_ASSIGN, dst, src1, 0);
    }
    else {
        // Load a value into a register. This could be a global or a local.
        dst->stack_index = 0;
        dst->global_symbol = 0;
        add_instruction(IR_LOAD_VARIABLE, dst, src1, 0);
    }

    return dst;
}

// Turn an lvalue in a register into an rvalue by dereferencing it
struct value *make_rvalue(struct value *src1) {
    struct value *dst;

    if (!src1->is_lvalue) return src1;

    if (!src1->vreg) panic("Internal error: unexpected missing register");

    // It's an lvalue in a register. Dereference it into the same register
    src1 = dup_value(src1); // Ensure no side effects on src1
    src1->is_lvalue = 0;

    dst = dup_value(src1);
    dst->vreg = new_vreg();
    dst->stack_index = 0;
    dst->global_symbol = 0;
    add_instruction(IR_INDIRECT, dst, src1, 0);

    return dst;
}

// Pop and load. Pop a value from the stack and load it into a register if not already done.
// Lvalues are converted into rvalues.
struct value *pl() {
    if (vtop->is_constant) push(load_constant(pop()));
    if (vtop->is_in_cpu_flags) push(load(pop()));

    if (vtop->vreg) {
        if (vtop->is_lvalue) return make_rvalue(pop());
        else return pop();
    }

    return load(pop());
}

struct value *load_constant(struct value *cv) {
    struct value *v;

    v = new_value();
    v->vreg = new_vreg();
    v->type = TYPE_LONG;
    add_instruction(IR_LOAD_CONSTANT, v, cv, 0);
    return v;
}

// Create a new typed constant value
struct value *new_constant(int type, long value) {
    struct value *cv;

    cv = new_value();
    cv->value = value;
    cv->type = type;
    cv->is_constant = 1;
    return cv;
}

// Create a new typed constant value and push it to the stack
void push_constant(int type, long value) {
    push(new_constant(type, value));
}

// Add an operation to the IR
struct three_address_code *add_ir_op(int operation, int type, int vreg, struct value *src1, struct value *src2) {
    struct value *v;
    struct three_address_code *result;

    v = new_value();
    v->vreg = vreg;
    v->type = type;
    result = add_instruction(operation, v, src1, src2);
    push(v);

    return result;
}

struct symbol *new_symbol() {
    struct symbol *result;

    result = next_symbol;
    next_symbol++;

    if (next_symbol - symbol_table >= SYMBOL_TABLE_SIZE) panic("Exceeded symbol table size");

    return result;
}

// Search for a symbol in a scope. Returns zero if not found
struct symbol *lookup_symbol(char *name, int scope) {
    struct symbol *s;

    s = symbol_table;
    while (s->identifier) {
        if (s->scope == scope && !strcmp((char *) s->identifier, name)) return s;
        s++;
    }

    if (scope != 0) return lookup_symbol(name, 0);

    return 0;
}

// Returns destination type of an operation with two operands
int operation_type(struct value *src1, struct value *src2) {
    if (src1->type >= TYPE_PTR) return src1->type;
    else if (src2->type >= TYPE_PTR) return src2->type;
    else if (src1->type == TYPE_LONG || src2->type == TYPE_LONG) return TYPE_LONG;
    else return TYPE_INT;
}

// Returns destination type of an operation using the top two values in the stack
int vs_operation_type() {
    return operation_type(vtop, vs[1]);
}

int cur_token_is_type() {
    return (
        cur_token == TOK_VOID ||
        cur_token == TOK_CHAR ||
        cur_token == TOK_SHORT ||
        cur_token == TOK_INT ||
        cur_token == TOK_LONG ||
        cur_token == TOK_STRUCT);
}

int get_type_alignment(int type) {
         if (type  > TYPE_PTR)    return 8;
    else if (type == TYPE_CHAR)   return 1;
    else if (type == TYPE_SHORT)  return 2;
    else if (type == TYPE_INT)    return 4;
    else if (type == TYPE_LONG)   return 8;
    else if (type >= TYPE_STRUCT) panic("Alignment of structs not implemented");
    else panic1d("align of unknown type %d", type);
}

int get_type_size(int type) {
         if (type == TYPE_VOID)   return sizeof(void);
    else if (type == TYPE_CHAR)   return sizeof(char);
    else if (type == TYPE_SHORT)  return sizeof(short);
    else if (type == TYPE_INT)    return sizeof(int);
    else if (type == TYPE_LONG)   return sizeof(long);
    else if (type >  TYPE_PTR)    return sizeof(void *);
    else if (type >= TYPE_STRUCT) return all_structs[type - TYPE_STRUCT]->size;
    else panic1d("sizeof unknown type %d", type);
}

// How much will the ++, --, +=, -= operators increment a type?
int get_type_inc_dec_size(int type) {
    return type < TYPE_PTR ? 1 : get_type_size(type - TYPE_PTR);
}

// Parse type up to the point where identifiers or * are lexed
int parse_base_type(int allow_incomplete_structs) {
    int type;

         if (cur_token == TOK_VOID)   type = TYPE_VOID;
    else if (cur_token == TOK_CHAR)   type = TYPE_CHAR;
    else if (cur_token == TOK_SHORT)  type = TYPE_SHORT;
    else if (cur_token == TOK_INT)    type = TYPE_INT;
    else if (cur_token == TOK_LONG)   type = TYPE_LONG;
    else if (cur_token == TOK_STRUCT) return parse_struct_base_type(allow_incomplete_structs);
    else panic("Unable to determine type from token %d");

    next();

    return type;
}

// Parse type, including *
int parse_type() {
    int type;

    type = parse_base_type(0);
    while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

    return type;
}

// Allocate a new struct_desc
struct struct_desc *new_struct() {
    struct struct_desc *s;

    s = malloc(sizeof(struct struct_desc));
    all_structs[all_structs_count] = s;
    s->type = TYPE_STRUCT + all_structs_count++;
    s->members = malloc(sizeof(struct struct_member *) * MAX_STRUCT_MEMBERS);
    memset(s->members, 0, sizeof(struct struct_member *) * MAX_STRUCT_MEMBERS);

    return s;
}

// Search for a struct. Returns 0 if not found.
struct struct_desc *find_struct(char *identifier) {
    struct struct_desc *s;
    int i;

    for (i = 0; i < all_structs_count; i++)
        if (!strcmp(all_structs[i]->identifier, identifier))
            return all_structs[i];
    return 0;
}

// Parse struct definitions and uses. Declarations aren't implemented.
int parse_struct_base_type(int allow_incomplete_structs) {
    char *identifier;
    struct struct_desc *s;
    struct struct_member *member;
    int member_count;
    int i, base_type, type, offset, is_packed, alignment, biggest_alignment;

    consume(TOK_STRUCT, "struct");

    // Check for packed attribute
    is_packed = 0;
    if (cur_token == TOK_ATTRIBUTE) {
        consume(TOK_ATTRIBUTE, "attribute");
        consume(TOK_LPAREN, "(");
        consume(TOK_LPAREN, "(");
        consume(TOK_PACKED, "packed");
        consume(TOK_RPAREN, ")");
        consume(TOK_RPAREN, ")");
        is_packed = 1;
    }

    identifier = cur_identifier;
    consume(TOK_IDENTIFIER, "identifier");
    if (cur_token == TOK_LCURLY) {
        // Struct definition

        consume(TOK_LCURLY, "{");

        s = find_struct(identifier);
        if (!s) s = new_struct();

        s->identifier = identifier;

        // Loop over members
        offset = 0;
        member_count = 0;
        biggest_alignment = 0;
        while (cur_token != TOK_RCURLY) {
            base_type = parse_base_type(1);
            while (cur_token != TOK_SEMI) {
                type = base_type;
                while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

                alignment = is_packed ? 1 : get_type_alignment(type);
                if (alignment > biggest_alignment) biggest_alignment = alignment;
                offset = ((offset + alignment  - 1) & (~(alignment - 1)));

                member = malloc(sizeof(struct struct_member));
                memset(member, 0, sizeof(struct struct_member));
                member->identifier = cur_identifier;
                member->type = type;
                member->offset = offset;
                s->members[member_count++] = member;

                offset += get_type_size(type);
                consume(TOK_IDENTIFIER, "identifier");
                if (cur_token == TOK_COMMA) next();
            }
            while (cur_token == TOK_SEMI) consume(TOK_SEMI, ";");
        }
        offset = ((offset + biggest_alignment  - 1) & (~(biggest_alignment - 1)));
        s->size = offset;
        s->is_incomplete = 0;
        consume(TOK_RCURLY, "}");
        return s->type;
    }
    else {
        // Struct use

        s = find_struct(identifier);
        if (s) return s->type; // Found a complete or incomplete struct

        if (allow_incomplete_structs) {
            // Didn't find a struct, but that's ok, create a incomplete one
            // to be populated later when it's defined.
            s = new_struct();
            s->identifier = identifier;
            s->is_incomplete = 1;
            return s->type;
        }
        else
            panic1s("Unknown struct %s", identifier);
    }
}

// Ensure there are no incomplete structs
void check_incomplete_structs() {
    int i;

    for (i = 0; i < all_structs_count; i++)
        if (all_structs[i]->is_incomplete)
            panic("There are incomplete structs");
}

void indirect() {
    struct value *dst, *src1;

    // The stack contains an rvalue which is a pointer. All that needs doing
    // is conversion of the rvalue into an lvalue on the stack and a type
    // change.
    src1 = pl();
    if (src1->is_lvalue) panic("Internal error: expected rvalue");
    if (!src1->vreg) panic("Internal error: expected a vreg");

    dst = new_value();
    dst->vreg = src1->vreg;
    dst->type = src1->type - TYPE_PTR;
    dst->is_lvalue = 1;
    push(dst);
}

// Search for a struct member. Panics if it doesn't exist
struct struct_member *lookup_struct_member(struct struct_desc *struct_desc, char *identifier) {
    struct struct_member **pmember;

    pmember = struct_desc->members;

    while (*pmember) {
        if (!strcmp((*pmember)->identifier, identifier)) return *pmember;
        pmember++;
    }

    panic2s("Unknown member %s in struct %s\n", identifier, struct_desc->identifier);
}

// Allocate a new label and create a value for it, for use in a jmp
struct value *new_label_dst() {
    struct value *v;

    v = new_value();
    v->label = ++label_count;

    return v;
}

// Add a no-op instruction with a label
void add_jmp_target_instruction(struct value *v) {
    struct three_address_code *tac;

    tac = add_instruction(IR_NOP, 0, 0, 0);
    tac->label = v->label;
}

void add_conditional_jump(int operation, struct value *dst) {
    if (vtop->is_in_cpu_flags)
        add_instruction(operation, 0, pop(), dst);
    else
        // Load the result into a register
        add_instruction(operation, 0, pl(), dst);
}

// Add instructions for && and || operators
void and_or_expr(int is_and) {
    struct value *dst, *ldst1, *ldst2, *ldst3;

    next();

    ldst1 = new_label_dst(); // Store zero
    ldst2 = new_label_dst(); // Second operand test
    ldst3 = new_label_dst(); // End

    // Destination register
    dst = new_value();
    dst->vreg = new_vreg();
    dst->type = TYPE_LONG;

    // Test first operand
    add_conditional_jump(is_and ? IR_JNZ : IR_JZ, ldst2);

    // Store zero & end
    add_jmp_target_instruction(ldst1);
    push_constant(TYPE_INT, is_and ? 0 : 1);         // Store 0
    add_instruction(IR_ASSIGN, dst, pl(), 0);
    push(dst);
    add_instruction(IR_JMP, 0, ldst3, 0);

    // Test second operand
    add_jmp_target_instruction(ldst2);
    expression(TOK_BITWISE_OR);
    add_conditional_jump(is_and ? IR_JZ : IR_JNZ, ldst1); // Store zero & end
    push_constant(TYPE_INT, is_and ? 1 : 0);          // Store 1
    add_instruction(IR_ASSIGN, dst, pl(), 0);
    push(dst);

    // End
    add_jmp_target_instruction(ldst3);
}

void arithmetic_operation(int operation, int type) {
    // Pull two items from the stack and push the result. Code in the IR
    // is generated when the operands can't be valuated directly.

    struct value *src1, *src2, *t;
    struct three_address_code *tac;
    long v1, v2;
    int vreg;

    if (!type) type = vs_operation_type();

    if (    operation == IR_ADD || operation == IR_SUB || operation == IR_MUL || operation == IR_BAND || operation == IR_BOR || operation == IR_XOR ||
            operation == IR_EQ || operation == IR_NE || operation == IR_LT || operation == IR_GT || operation == IR_LE || operation == IR_GE ||
            operation == IR_BSHL || operation == IR_BSHR) {

        if ((vs[0])->is_constant && (vs[1])->is_constant) {
            v2 = pop()->value;
            v1 = pop()->value;
                 if (operation == IR_ADD)  push(new_constant(type, v1 +  v2));
            else if (operation == IR_SUB)  push(new_constant(type, v1 -  v2));
            else if (operation == IR_MUL)  push(new_constant(type, v1 *  v2));
            else if (operation == IR_BAND) push(new_constant(type, v1 &  v2));
            else if (operation == IR_BOR)  push(new_constant(type, v1 |  v2));
            else if (operation == IR_XOR)  push(new_constant(type, v1 ^  v2));
            else if (operation == IR_EQ)   push(new_constant(type, v1 == v2));
            else if (operation == IR_NE)   push(new_constant(type, v1 != v2));
            else if (operation == IR_LT)   push(new_constant(type, v1 <  v2));
            else if (operation == IR_GT)   push(new_constant(type, v1 >  v2));
            else if (operation == IR_LE)   push(new_constant(type, v1 <= v2));
            else if (operation == IR_GE)   push(new_constant(type, v1 >= v2));
            else if (operation == IR_BSHL) push(new_constant(type, v1 << v2));
            else if (operation == IR_BSHR) push(new_constant(type, v1 >> v2));
            else panic1d("Unknown constant operation: %d", operation);
            return;
        }

        if (vtop->is_constant) src2 = pop(); else src2 = pl();
        if (vtop->is_constant) src1 = pop(); else src1 = pl();

        // If the second arg is a constant, flip it to be the first for commutative operations
        if (operation != IR_SUB && operation != IR_BSHL && operation != IR_BSHR && src2->is_constant) {
            t = src1;
            src1 = src2;
            src2 = t;

            // Flip comparison operation
                 if (operation == IR_LT) operation = IR_GT;
            else if (operation == IR_GT) operation = IR_LT;
            else if (operation == IR_LE) operation = IR_GE;
            else if (operation == IR_GE) operation = IR_LE;
        }
    }
    else {
        src2 = pl();
        src1 = pl();
    }

    // Store the result in the CPU flags for comparison operations
    // It will get loaded into a register in later instructions if needed.
    if (operation == IR_GT || operation == IR_LT || operation == IR_GE || operation == IR_LE || operation == IR_EQ || operation == IR_NE)
        vreg = 0;
    else
        vreg = new_vreg();

    tac = add_ir_op(operation, type, vreg, src1, src2);

    if (!vreg) tac->dst->is_in_cpu_flags = 1;
}

void parse_arithmetic_operation(int level, int operation, int type) {
    next();
    expression(level);
    arithmetic_operation(operation, type);
}

// Parse an expression using top-down precedence climbing parsing
// https://en.cppreference.com/w/c/language/operator_precedence
// https://en.wikipedia.org/wiki/Operator-precedence_parser#Precedence_climbing_method
void expression(int level) {
    int org_token;
    int org_type;
    int factor;
    int type;
    int scope;
    int function_call, arg_count;
    int prev_in_conditional;
    struct symbol *symbol;
    struct struct_desc *str;
    struct struct_member *member;
    struct value *v1, *v2, *dst, *src1, *src2, *ldst1, *ldst2, *function_value, *return_value;
    struct three_address_code *tac;

    // Parse any tokens that can be at the start of an expression
    if (cur_token == TOK_LOGICAL_NOT) {
        next();
        expression(TOK_INC);

        if (vtop->is_constant)
            push_constant(TYPE_LONG, !pop()->value);
        else {
            push_constant(TYPE_LONG, 0);
            arithmetic_operation(IR_EQ, TYPE_INT);
        }
    }

    else if (cur_token == TOK_BITWISE_NOT) {
        next();
        expression(TOK_INC);
        if (vtop->is_constant)
            push_constant(TYPE_LONG, ~pop()->value);
        else {
            type = vtop->type;
            tac = add_ir_op(IR_BNOT, type, new_vreg(), pl(), 0);
        }
    }

    else if (cur_token == TOK_ADDRESS_OF) {
        next();
        expression(TOK_INC);
        if (!vtop->is_lvalue) panic("Cannot take an address of an rvalue");
        vtop->is_lvalue = 0;
        vtop->type += TYPE_PTR;
    }

    else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
        // Prefix increment & decrement

        org_token = cur_token;
        next();
        expression(TOK_DOT);

        if (!vtop->is_lvalue) panic("Cannot ++ or -- an rvalue");

        v1 = pop();                 // lvalue
        src1 = load(dup_value(v1)); // rvalue
        push(src1);
        push_constant(TYPE_INT, get_type_inc_dec_size(src1->type));
        arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
        add_instruction(IR_ASSIGN, v1, vtop, 0);
        push(v1); // Push the original lvalue back on the value stack
    }

    else if (cur_token == TOK_MULTIPLY) {
        next();
        expression(TOK_INC);
        if (vtop->type <= TYPE_PTR) panic1d("Cannot dereference a non-pointer %d", vtop->type);
        indirect();
    }

    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            push_constant(TYPE_LONG, -cur_long);
            next();
        }
        else {
            push_constant(TYPE_LONG, -1);
            expression(TOK_INC);
            arithmetic_operation(IR_MUL, TYPE_LONG);
        }
    }

    else if (cur_token == TOK_LPAREN) {
        next();
        if (cur_token_is_type()) {
            // cast
            org_type = parse_type();
            consume(TOK_RPAREN, ")");
            expression(TOK_INC);
            vtop->type = org_type;
        }
        else {
            expression(TOK_COMMA);
            consume(TOK_RPAREN, ")");
        }
    }

    else if (cur_token == TOK_NUMBER) {
        push_constant(TYPE_LONG, cur_long);
        next();
    }

    else if (cur_token == TOK_STRING_LITERAL) {
        dst = new_value();
        dst->vreg = new_vreg();
        dst->type = TYPE_CHAR + TYPE_PTR;

        src1 = new_value();
        src1->type = TYPE_CHAR + TYPE_PTR;
        src1->string_literal_index = string_literal_count;
        src1->is_string_literal = 1;
        string_literals[string_literal_count++] = cur_string_literal;
        if (string_literal_count >= MAX_STRING_LITERALS) panic1d("Exceeded max string literals %d", MAX_STRING_LITERALS);

        push(dst);
        add_instruction(IR_LOAD_STRING_LITERAL, dst, src1, 0);
        next();
    }

    else if (cur_token == TOK_IDENTIFIER) {
        symbol = lookup_symbol(cur_identifier, cur_scope);
        if (!symbol) panic1s("Unknown symbol \"%s\"", cur_identifier);

        next();
        type = symbol->type;
        scope = symbol->scope;
        if (symbol->is_enum)
            push_constant(TYPE_LONG, symbol->value);
        else if (cur_token == TOK_LPAREN) {
            // Function call
            function_call = function_call_count++;
            next();
            src1 = new_value();
            src1->value = function_call;
            add_instruction(IR_START_CALL, 0, src1, 0);
            arg_count = 0;
            while (1) {
                if (cur_token == TOK_RPAREN) break;
                expression(TOK_COMMA);
                add_instruction(IR_ARG, 0, src1, pl());
                arg_count++;
                if (cur_token == TOK_RPAREN) break;
                consume(TOK_COMMA, ",");
                if (cur_token == TOK_RPAREN) panic("Expected expression");
            }
            consume(TOK_RPAREN, ")");

            function_value = new_value();
            function_value->function_symbol = symbol;
            function_value->function_call_arg_count = arg_count;

            return_value = 0;
            if (type != TYPE_VOID) {
                return_value = new_value();
                return_value->vreg = new_vreg();
                return_value->type = type;
            }
            add_instruction(IR_CALL, return_value, function_value, 0);
            add_instruction(IR_END_CALL, 0, src1, 0);
            if (return_value) push(return_value);
        }

        else if (scope == 0) {
            // Global symbol
            src1 = new_value();
            src1->type = type;
            src1->is_lvalue = 1;
            src1->global_symbol = symbol;
            push(src1);
        }

        else {
            // Local symbol
            src1 = new_value();
            src1->type = type;
            src1->is_lvalue = 1;

            if (symbol->stack_index >= 0)
                // Step over pushed PC and BP
                src1->stack_index = cur_function_symbol->function_param_count - symbol->stack_index + 1;
            else
                src1->stack_index = symbol->stack_index;

            push(src1);
        }
    }

    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN, "(");
        type = parse_type();
        push_constant(TYPE_INT, get_type_size(type));
        consume(TOK_RPAREN, ")");
    }

    else
        panic1d("Unexpected token %d in expression", cur_token);

    while (cur_token >= level) {
        // In order or precedence, highest first

        if (cur_token == TOK_LBRACKET) {
            next();

            if (vtop->type < TYPE_PTR)
                panic1d("Cannot do [] on a non-pointer for type %d", vtop->type);

            factor = get_type_inc_dec_size(vtop->type);

            src1 = pl(); // Turn it into an rvalue
            push(src1);

            org_type = vtop->type;
            expression(TOK_COMMA);

            if (factor > 1) {
                push_constant(TYPE_INT, factor);
                arithmetic_operation(IR_MUL, TYPE_INT);
            }

            type = vs_operation_type();
            arithmetic_operation(IR_ADD, type);

            consume(TOK_RBRACKET, "]");
            vtop->type = org_type - TYPE_PTR;
            vtop->is_lvalue = 1;
        }

        else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
            // Postfix increment & decrement

            org_token = cur_token;
            next();

            if (!vtop->is_lvalue) panic(" Cannot ++ or -- an rvalue");

            v1 = pop();                 // lvalue
            src1 = load(dup_value(v1)); // rvalue
            push(src1);
            push_constant(TYPE_INT, get_type_inc_dec_size(src1->type));
            arithmetic_operation(org_token == TOK_INC ? IR_ADD : IR_SUB, 0);
            add_instruction(IR_ASSIGN, v1, vtop, 0);
            pop(); // Pop the lvalue of the assignment off the stack
            push(src1); // Push the original rvalue back on the value stack
        }

        else if (cur_token == TOK_DOT || cur_token == TOK_ARROW) {
            if (cur_token == TOK_DOT) {
                // Struct member lookup

                if (vtop->type < TYPE_STRUCT || vtop->type >= TYPE_PTR) panic("Cannot use . on a non-struct");
                if (!vtop->is_lvalue) panic("Expected lvalue for struct . operation.");

                // Pretend the lvalue is a pointer to a struct
                vtop->is_lvalue = 0;
                vtop->type += TYPE_PTR;
            }

            if (vtop->type < TYPE_PTR) panic("Cannot use -> on a non-pointer");
            if (vtop->type < TYPE_STRUCT + TYPE_PTR) panic("Cannot use -> on a pointer to a non-struct");

            next();
            if (cur_token != TOK_IDENTIFIER) panic("Expected identifier\n");

            str = all_structs[vtop->type - TYPE_PTR - TYPE_STRUCT];
            member = lookup_struct_member(str, cur_identifier);
            indirect();

            if (member->offset > 0) {
                // Make the lvalue an rvalue for manipulation
                vtop->is_lvalue = 0;
                push_constant(TYPE_INT, member->offset);
                arithmetic_operation(IR_ADD, 0);
            }

            vtop->type = member->type;
            vtop->is_lvalue = 1;
            consume(TOK_IDENTIFIER, "identifier");
        }

        else if (cur_token == TOK_MULTIPLY) parse_arithmetic_operation(TOK_DOT, IR_MUL, 0);
        else if (cur_token == TOK_DIVIDE)   parse_arithmetic_operation(TOK_INC, IR_DIV, 0);
        else if (cur_token == TOK_MOD)      parse_arithmetic_operation(TOK_INC, IR_MOD, 0);

        else if (cur_token == TOK_PLUS || cur_token == TOK_MINUS) {
            factor = get_type_inc_dec_size(vtop->type);

            if (factor > 1) {
                org_token = cur_token;
                next();
                expression(TOK_MULTIPLY);

                if (factor > 1) {
                    push_constant(TYPE_INT, factor);
                    arithmetic_operation(IR_MUL, TYPE_INT);
                }

                arithmetic_operation(org_token == TOK_PLUS ? IR_ADD : IR_SUB , vs_operation_type());
            }
            else
                parse_arithmetic_operation(TOK_MULTIPLY, cur_token == TOK_PLUS ? IR_ADD : IR_SUB, 0);
        }

        else if (cur_token == TOK_BITWISE_LEFT)  parse_arithmetic_operation(TOK_PLUS,         IR_BSHL, 0);
        else if (cur_token == TOK_BITWISE_RIGHT) parse_arithmetic_operation(TOK_PLUS,         IR_BSHR, 0);
        else if (cur_token == TOK_LT)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LT,   TYPE_INT);
        else if (cur_token == TOK_GT)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GT,   TYPE_INT);
        else if (cur_token == TOK_LE)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_LE,   TYPE_INT);
        else if (cur_token == TOK_GE)            parse_arithmetic_operation(TOK_BITWISE_LEFT, IR_GE,   TYPE_INT);
        else if (cur_token == TOK_DBL_EQ)        parse_arithmetic_operation(TOK_LT,           IR_EQ,   0);
        else if (cur_token == TOK_NOT_EQ)        parse_arithmetic_operation(TOK_LT,           IR_NE,   0);
        else if (cur_token == TOK_ADDRESS_OF)    parse_arithmetic_operation(TOK_DBL_EQ,       IR_BAND, 0);
        else if (cur_token == TOK_XOR)           parse_arithmetic_operation(TOK_ADDRESS_OF,   IR_XOR,  0);
        else if (cur_token == TOK_BITWISE_OR)    parse_arithmetic_operation(TOK_XOR,          IR_BOR,  0);
        else if (cur_token == TOK_AND)           and_or_expr(1);
        else if (cur_token == TOK_OR)            and_or_expr(0);

        else if (cur_token == TOK_TERNARY) {
            next();

            prev_in_conditional = in_conditional;
            in_conditional = 1;

            // Destination register
            dst = new_value();
            dst->vreg = new_vreg();

            ldst1 = new_label_dst(); // False case
            ldst2 = new_label_dst(); // End
            add_conditional_jump(IR_JZ, ldst1);
            expression(TOK_OR);
            src1 = vtop;
            add_instruction(IR_ASSIGN, dst, pl(), 0);
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of false case
            consume(TOK_COLON, ":");
            expression(TOK_OR);
            src2 = vtop;
            dst->type = operation_type(src1, src2);
            add_instruction(IR_ASSIGN, dst, pl(), 0);
            push(dst);
            add_jmp_target_instruction(ldst2); // End
            in_conditional = prev_in_conditional;
        }

        else if (cur_token == TOK_EQ) {
            next();
            if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");
            dst = pop();
            expression(TOK_EQ);
            src1 = pl();
            dst->is_lvalue = 1;
            add_instruction(IR_ASSIGN, dst, src1, 0);
            push(dst);
        }

        else if (cur_token == TOK_PLUS_EQ || cur_token == TOK_MINUS_EQ) {
            org_type = vtop->type;
            org_token = cur_token;

            next();

            if (!vtop->is_lvalue) panic("Cannot assign to an rvalue");

            v1 = vtop;                  // lvalue
            push(load(dup_value(v1)));  // rvalue

            factor = get_type_inc_dec_size(org_type);
            expression(TOK_EQ);

            if (factor > 1) {
                push_constant(TYPE_INT, factor);
                arithmetic_operation(IR_MUL, TYPE_INT);
            }

            arithmetic_operation(org_token == TOK_PLUS_EQ ? IR_ADD : IR_SUB, type);
            vtop->type = org_type;
            add_instruction(IR_ASSIGN, v1, vtop, 0);
        }
        else
            return; // Bail once we hit something unknown

    }
}

// Parse a statement
void statement() {
    struct value *ldst1, *ldst2, *linit, *lcond, *lafter, *lbody, *lend, *old_loop_continue_dst, *old_loop_break_dst, *src1, *src2;
    struct three_address_code *tac;
    int loop_token;
    int prev_in_conditional;
    int prev_loop;

    vs = vs_start; // Reset value stack

    if (cur_token_is_type()) panic("Declarations must be at the top of a function");

    if (cur_token == TOK_SEMI) {
        // Empty statement
        next();
        return;
    }

    else if (cur_token == TOK_LCURLY) {
        next();
        while (cur_token != TOK_RCURLY) statement();
        consume(TOK_RCURLY, "}");
        return;
    }

    else if (cur_token == TOK_WHILE || cur_token == TOK_FOR) {
        prev_loop = cur_loop;
        cur_loop = ++loop_count;
        src1 = new_value();
        src1->value = prev_loop;
        src2 = new_value();
        src2->value = cur_loop;
        add_instruction(IR_START_LOOP, 0, src1, src2);

        loop_token = cur_token;
        next();
        consume(TOK_LPAREN, "(");

        // Preserve previous loop addresses so that we can have nested whiles/fors
        old_loop_continue_dst = cur_loop_continue_dst;
        old_loop_break_dst = cur_loop_break_dst;

        linit = lcond = lafter = 0;
        lbody  = new_label_dst();
        lend  = new_label_dst();
        cur_loop_continue_dst = 0;
        cur_loop_break_dst = lend;

        if (loop_token == TOK_FOR) {
            if (cur_token != TOK_SEMI) {
                linit  = new_label_dst();
                add_jmp_target_instruction(linit);
                expression(TOK_COMMA);
            }
            consume(TOK_SEMI, ";");

            if (cur_token != TOK_SEMI) {
                lcond  = new_label_dst();
                cur_loop_continue_dst = lcond;
                add_jmp_target_instruction(lcond);
                expression(TOK_COMMA);
                add_conditional_jump(IR_JZ, lend);
                add_instruction(IR_JMP, 0, lbody, 0);
            }
            consume(TOK_SEMI, ";");

            if (cur_token != TOK_RPAREN) {
                lafter  = new_label_dst();
                cur_loop_continue_dst = lafter;
                add_jmp_target_instruction(lafter);
                expression(TOK_COMMA);
                if (lcond) add_instruction(IR_JMP, 0, lcond, 0);
            }
        }

        else {
            lcond  = new_label_dst();
            cur_loop_continue_dst = lcond;
            add_jmp_target_instruction(lcond);
            expression(TOK_COMMA);
            add_conditional_jump(IR_JZ, lend);
        }
        consume(TOK_RPAREN, ")");

        if (!cur_loop_continue_dst) cur_loop_continue_dst = lbody;
        add_jmp_target_instruction(lbody);
        statement();

        if (lafter)
            add_instruction(IR_JMP, 0, lafter, 0);
        else if (lcond)
            add_instruction(IR_JMP, 0, lcond, 0);
        else
            add_instruction(IR_JMP, 0, lbody, 0);

        // Jump to the start of the body in a for loop like (;;i++)
        if (loop_token == TOK_FOR && !linit && !lcond && lafter)
            add_instruction(IR_JMP, 0, lbody, 0);

        add_jmp_target_instruction(lend);

        // Restore previous loop addresses
        cur_loop_continue_dst = old_loop_continue_dst;
        cur_loop_break_dst = old_loop_break_dst;

        // Restore outer loop counter
        cur_loop = prev_loop;

        add_instruction(IR_END_LOOP, 0, src1, src2);
    }

    else if (cur_token == TOK_CONTINUE) {
        next();
        add_instruction(IR_JMP, 0, cur_loop_continue_dst, 0);
        consume(TOK_SEMI, ";");
    }

    else if (cur_token == TOK_BREAK) {
        next();
        add_instruction(IR_JMP, 0, cur_loop_break_dst, 0);
        consume(TOK_SEMI, ";");
    }

    else if (cur_token == TOK_IF) {
        next();

        prev_in_conditional = in_conditional;
        in_conditional = 1;

        consume(TOK_LPAREN, "(");
        expression(TOK_COMMA);
        consume(TOK_RPAREN, ")");

        ldst1 = new_label_dst(); // False case
        ldst2 = new_label_dst(); // End
        add_conditional_jump(IR_JZ, ldst1);
        statement();

        if (cur_token == TOK_ELSE) {
            next();
            add_instruction(IR_JMP, 0, ldst2, 0); // Jump to end
            add_jmp_target_instruction(ldst1);    // Start of else case
            statement();
        }
        else {
            add_jmp_target_instruction(ldst1); // Start of else case
        }

        // End
        add_jmp_target_instruction(ldst2);

        in_conditional = prev_in_conditional;
    }

    else if (cur_token == TOK_RETURN) {
        next();
        if (cur_token == TOK_SEMI) {
            add_instruction(IR_RETURN, 0, 0, 0);
        }
        else {
            expression(TOK_COMMA);
            add_instruction(IR_RETURN, 0, pl(), 0);
        }
        consume(TOK_SEMI, ";");
    }

    else {
        expression(TOK_COMMA);
        consume(TOK_SEMI, ";");

        // Discard the destination of a function call to a non-void function
        // since it's not being assigned to anything and
        if (ir[-1].operation == IR_CALL && ir[-1].dst) {
            ir[-1].dst = 0;
            --vreg_count;
            pl();
        }
    }
}

// Parse function body
void function_body() {
    int local_symbol_count;
    int base_type, type;
    struct symbol *s;

    vreg_count = 0; // Reset global vreg_count
    local_symbol_count = 0;
    function_call_count = 0;
    cur_loop = 0;
    loop_count = 0;
    in_conditional = 0;

    consume(TOK_LCURLY, "{");

    // Parse symbols first
    while (cur_token_is_type()) {
        base_type = parse_base_type(0);

        while (cur_token != TOK_SEMI) {
            type = base_type;
            while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

            if (type >= TYPE_STRUCT && type < TYPE_PTR) panic("Direct usage of struct variables not implemented");
            if (cur_token == TOK_EQ) panic("Declarations with assignments aren't implemented");

            expect(TOK_IDENTIFIER, "identifier");
            s = new_symbol();
            s->type = type;
            s->identifier = cur_identifier;
            s->scope = cur_scope;
            s->stack_index = -1 - local_symbol_count++;
            next();
            if (cur_token != TOK_SEMI && cur_token != TOK_COMMA) panic("Expected ; or ,");
            if (cur_token == TOK_COMMA) next();
        }
        expect(TOK_SEMI, ";");
        while (cur_token == TOK_SEMI) next();
    }

    cur_function_symbol->function_local_symbol_count = local_symbol_count;
    cur_function_symbol->function_call_count = function_call_count;

    while (cur_token != TOK_RCURLY) statement();

    consume(TOK_RCURLY, "}");
}

// Parse a translation unit
void parse() {
    int base_type, type;
    long value;                // Enum value
    int param_count;            // Number of parameters to a function
    int seen_function;          // If a function has been seen, then variable declarations afterwards are forbidden
    struct symbol *param_symbol, *s;
    int i;

    cur_scope = 0;
    seen_function = 0;

    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token_is_type() ) {
            // Variable or function definition
            base_type = parse_base_type(0);

            while (cur_token != TOK_SEMI && cur_token != TOK_EOF) {
                type = base_type;
                while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

                if (type >= TYPE_STRUCT && type < TYPE_PTR) panic("Direct usage of struct variables not implemented");

                expect(TOK_IDENTIFIER, "identifier");

                s = lookup_symbol(cur_identifier, 0);
                if (!s) {
                    // Create a new symbol if it wasn't already declared. The
                    // previous declaration is left unchanged.

                    s = new_symbol();
                    s->type = type;
                    s->identifier = cur_identifier;
                    s->scope = 0;
                }

                next();

                if (cur_token == TOK_LPAREN) {
                    cur_function_symbol = s;
                    // Function declaration or definition

                    seen_function = 1;
                    cur_scope++;
                    next();

                    // Setup the intermediate representation with a dummy no operation instruction.
                    ir_start = 0;
                    ir_start = add_instruction(IR_NOP, 0, 0, 0);
                    s->function_ir = ir_start;
                    s->is_function = 1;

                    param_count = 0;
                    while (1) {
                        if (cur_token == TOK_RPAREN) break;

                        if (cur_token_is_type()) {
                            type = parse_type(0);
                            if (type >= TYPE_STRUCT && type < TYPE_PTR) panic("Direct usage of struct variables not implemented");
                        }
                        else
                            panic("Expected type or )");

                        expect(TOK_IDENTIFIER, "identifier");
                        param_symbol = new_symbol();
                        param_symbol->type = type;
                        param_symbol->identifier = cur_identifier;
                        param_symbol->scope = cur_scope;
                        param_symbol->stack_index = param_count++;
                        next();

                        if (cur_token == TOK_RPAREN) break;
                        consume(TOK_COMMA, ",");
                        if (cur_token == TOK_RPAREN) panic("Expected expression");
                    }

                    s->function_param_count = param_count;
                    cur_function_symbol = s;
                    consume(TOK_RPAREN, ")");

                    if (cur_token == TOK_LCURLY) {
                        cur_function_symbol->function_param_count = param_count;
                        function_body();
                        cur_function_symbol->function_vreg_count = vreg_count;
                    }
                    else
                        // Make it clear that this symbol will need to be backpatched if used
                        // before the definition has been processed.
                        cur_function_symbol->value = 0;

                    break;
                }
                else {
                    // Global symbol
                    if (seen_function) panic("Global variables must precede all functions");
                }

                if (cur_token == TOK_COMMA) next();
            }

            if (cur_token == TOK_SEMI) next();
        }

        else if (cur_token == TOK_ENUM) {
            consume(TOK_ENUM, "enum");
            consume(TOK_LCURLY, "{");

            value = 0;
            while (cur_token != TOK_RCURLY) {
                expect(TOK_IDENTIFIER, "identifier");
                next();
                if (cur_token == TOK_EQ) {
                    next();
                    expect(TOK_NUMBER, "number");
                    value = cur_long;
                    next();
                }

                s = new_symbol();
                s->is_enum = 1;
                s->type = TYPE_LONG;
                s->identifier = cur_identifier;
                s->scope = 0;
                s->value = value++;
                s++;

                if (cur_token == TOK_COMMA) next();
            }
            consume(TOK_RCURLY, "}");
            consume(TOK_SEMI, ";");
        }

        else if (cur_token == TOK_STRUCT) {
            parse_base_type(0); // It's a struct declaration
            consume(TOK_SEMI, ";");
        }

        else panic("Expected global declaration or function");
    }
}

void fprintf_escaped_string_literal(void *f, char* sl) {
    fprintf(f, "\"");
    while (*sl) {
             if (*sl == '\n') fprintf(f, "\\n");
        else if (*sl == '\t') fprintf(f, "\\t");
        else if (*sl == '\\') fprintf(f, "\\\\");
        else if (*sl == '"')  fprintf(f, "\\\"");
        else                  fprintf(f, "%c", *sl);
        sl++;
    }
    fprintf(f, "\"");
}

void print_value(void *f, struct value *v, int is_assignment_rhs) {
    int type;

    if (is_assignment_rhs && !v->is_lvalue && (v->global_symbol || v->stack_index)) fprintf(f, "&");
    if (!is_assignment_rhs && v->is_lvalue && !(v->global_symbol || v->stack_index)) fprintf(f, "L");

    if (v->is_constant)
        fprintf(f, "%ld", v->value);
    else if (v->is_in_cpu_flags)
        fprintf(f, "cpu");
    else if (v->preg != -1)
        fprintf(f, "p%d", v->preg);
    else if (v->spilled_stack_index != -1)
        fprintf(f, "S[%d]", v->spilled_stack_index);
    else if (v->vreg)
        fprintf(f, "r%d", v->vreg);
    else if (v->stack_index)
        fprintf(f, "s[%d]", v->stack_index);
    else if (v->global_symbol)
        fprintf(f, "%s", v->global_symbol->identifier);
    else if (v->is_string_literal) {
        fprintf_escaped_string_literal(f, string_literals[v->string_literal_index]);
    }
    else
        fprintf(f, "%ld", v->value);

    fprintf(f, ":");
    type = v->type;
    while (type >= TYPE_PTR) {
        fprintf(f, "*");
        type -= TYPE_PTR;
    }

         if (type == TYPE_VOID)   fprintf(f, "void");
    else if (type == TYPE_CHAR)   fprintf(f, "char");
    else if (type == TYPE_INT)    fprintf(f, "int");
    else if (type == TYPE_SHORT)  fprintf(f, "short");
    else if (type == TYPE_LONG)   fprintf(f, "long");
    else if (type >= TYPE_STRUCT) fprintf(f, "struct %s", all_structs[type - TYPE_STRUCT]->identifier);
    else fprintf(f, "unknown type %d", type);
}

void print_instruction(void *f, struct three_address_code *tac) {
    if (tac->label)
        fprintf(f, "l%-5d", tac->label);
    else
        fprintf(f, "      ");

    if (tac->dst) {
        print_value(f, tac->dst, tac->operation != IR_ASSIGN);
        fprintf(f, " = ");
    }

    if (tac->operation == IR_LOAD_CONSTANT || tac->operation == IR_LOAD_VARIABLE || tac->operation == IR_LOAD_STRING_LITERAL) {
        print_value(f, tac->src1, 1);
    }

    else if (tac->operation == IR_START_CALL) fprintf(f, "start call %ld", tac->src1->value);
    else if (tac->operation == IR_END_CALL) fprintf(f, "end call %ld", tac->src1->value);

    else if (tac->operation == IR_ARG) {
        fprintf(f, "arg for call %ld ", tac->src1->value);
        print_value(f, tac->src2, 1);
    }

    else if (tac->operation == IR_CALL) {
        fprintf(f, "call \"%s\" with %d params", tac->src1->function_symbol->identifier, tac->src1->function_call_arg_count);
    }

    else if (tac->operation == IR_RETURN) {
        if (tac->src1) {
            fprintf(f, "return ");
            print_value(f, tac->src1, 1);
        }
        else
            fprintf(f, "return");
    }

    else if (tac->operation == IR_START_LOOP) fprintf(f, "start loop par=%ld loop=%ld", tac->src1->value, tac->src2->value);
    else if (tac->operation == IR_END_LOOP)   fprintf(f, "end loop par=%ld loop=%ld",   tac->src1->value, tac->src2->value);

    else if (tac->operation == IR_ASSIGN)
        print_value(f, tac->src1, 1);

    else if (tac->operation == IR_NOP)
        fprintf(f, "nop");

    else if (tac->operation == IR_JZ || tac->operation == IR_JNZ) {
             if (tac->operation == IR_JZ)  fprintf(f, "jz ");
        else if (tac->operation == IR_JNZ) fprintf(f, "jnz ");
        print_value(f, tac->src1, 1);
        fprintf(f, " l%d", tac->src2->label);
    }

    else if (tac->operation == IR_JMP)
        fprintf(f, "jmp l%d", tac->src1->label);

    else if (tac->operation == IR_INDIRECT)      {                               fprintf(f, "*");    print_value(f, tac->src1, 1); }
    else if (tac->operation == IR_ADD)           { print_value(f, tac->src1, 1); fprintf(f, " + ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_SUB)           { print_value(f, tac->src1, 1); fprintf(f, " - ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_RSUB)          { print_value(f, tac->src2, 1); fprintf(f, " - ");  print_value(f, tac->src1, 1); fprintf(f, " [reverse]"); }
    else if (tac->operation == IR_MUL)           { print_value(f, tac->src1, 1); fprintf(f, " * ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_DIV)           { print_value(f, tac->src1, 1); fprintf(f, " / ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_MOD)           { print_value(f, tac->src1, 1); fprintf(f, " %% "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BNOT)          {                               fprintf(f, "~");    print_value(f, tac->src1, 1); }
    else if (tac->operation == IR_EQ)            { print_value(f, tac->src1, 1); fprintf(f, " == "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_NE)            { print_value(f, tac->src1, 1); fprintf(f, " != "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_GT)            { print_value(f, tac->src1, 1); fprintf(f, " > ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_LT)            { print_value(f, tac->src1, 1); fprintf(f, " < ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_GE)            { print_value(f, tac->src1, 1); fprintf(f, " >= "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_LE)            { print_value(f, tac->src1, 1); fprintf(f, " <= "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BAND)          { print_value(f, tac->src1, 1); fprintf(f, " & ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BOR)           { print_value(f, tac->src1, 1); fprintf(f, " | ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_XOR)           { print_value(f, tac->src1, 1); fprintf(f, " ^ ");  print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BSHL)          { print_value(f, tac->src1, 1); fprintf(f, " << "); print_value(f, tac->src2, 1); }
    else if (tac->operation == IR_BSHR)          { print_value(f, tac->src1, 1); fprintf(f, " >> "); print_value(f, tac->src2, 1); }

    else
        panic1d("print_instruction(): Unknown operation: %d", tac->operation);

    fprintf(f, "\n");
}

void print_intermediate_representation(struct symbol *function) {
    struct three_address_code *tac;
    int i;

    fprintf(stdout, "%s:\n", function->identifier);
    i = 0;
    tac = function->function_ir;
    while (tac) {
        fprintf(stdout, "%-4d > ", i++);
        print_instruction(stdout, tac);
        tac = tac->next;
    }
    fprintf(stdout, "\n");
}

// We're not quite entirely SSA but a step in the right direction.
void ensure_must_be_ssa_ish(struct three_address_code *ir) {
    struct three_address_code *tac1, *tac2;

    int i, j, dst, src1, src2;

    i = 0;
    tac1 = ir;
    while (tac1) {
        dst = src1 = src2 = 0;

        // Ensure dst isn't the same as src1 nor src2
        if (tac1-> dst && tac1-> dst->vreg != -1) dst  = tac1-> dst->vreg;
        if (tac1->src1 && tac1->src1->vreg != -1) src1 = tac1->src1->vreg;
        if (tac1->src2 && tac1->src2->vreg != -1) src2 = tac1->src2->vreg;

        if (dst && ((dst == src1 || dst == src2))) {
            print_instruction(stdout, tac1);
            printf("Not SSA in IR instruction %d\n", i);
            exit(1);
        }

        // FIXME the ternary generates code with 2 register assigns, which violates SSA
        // if (ir->operation != IR_TERNARY && dst) {
        //     tac2 = tac1 + 1;
        //     j = 0;
        //     while (tac2->operation) {
        //         if (tac2->dst && tac2->dst->vreg == dst) {
        //             printf("IR instructions not SSA\n");
        //             fprintf(stdout, "%-4d > ", i); print_instruction(stdout, tac1);
        //             fprintf(stdout, "%-4d > ", j); print_instruction(stdout, tac2);
        //             exit(1);
        //         }
        //         tac2++;
        //         j++;
        //     }
        // }

        i++;
        tac1 = tac1->next;
    }
}

void update_register_liveness(int vreg, int instruction_position) {
    if (liveness[vreg].start == -1 || instruction_position < liveness[vreg].start) liveness[vreg].start = instruction_position;
    if (liveness[vreg].end == -1 || instruction_position > liveness[vreg].end) liveness[vreg].end = instruction_position;
}

int get_instruction_for_label(struct symbol *function, int label) {
    struct three_address_code *tac;
    int i;

    i = 0;
    tac = function->function_ir;
    while (tac) {
        if (tac->label == label) return i;
        tac = tac->next;
        i++;
    }

    panic1d("Unknown label %d", label);
}

// Debug function
void print_liveness(struct symbol *function) {
    int i;

    for (i = 1; i <= function->function_vreg_count; i++)
        printf("r%-4d %4d - %4d\n", i, liveness[i].start, liveness[i].end);
}

// Make a liveness_interval for each three address code in the IR. The liveness
// interval, if set, corresponds with the most outer for-loop.
struct liveness_interval **make_outer_loops(struct symbol *function) {
    struct three_address_code *tac, *end;
    struct liveness_interval **result, *l;
    int i, ir_len, loop;

    // Allocate result, one liveness_interval per three address code in the IR
    ir_len = 1;
    tac = function->function_ir;
    while (tac = tac->next) ir_len++;
    result = malloc(sizeof(struct liveness_interval *) * ir_len);

    i = 0;
    l = 0;
    tac = function->function_ir;
    while (tac) {
        if (!l && tac->operation == IR_START_LOOP) {
            l = malloc(sizeof(struct liveness_interval));
            l->start = i;
            loop = tac->src2->value;
        }
        else if (tac->operation == IR_END_LOOP && tac->src2->value == loop) {
            l->end = i;
            l = 0;
        }

        result[i] = l;
        tac = tac->next;
        i++;
    }

    return result;
}

void analyze_liveness(struct symbol *function) {
    int i, j, k, l;
    struct three_address_code *tac;
    struct liveness_interval *liveness_interval, **outer_loops;

    for (i = 1; i <= function->function_vreg_count; i++) {
        liveness[i].start = -1;
        liveness[i].end = -1;
    }

    // Update liveness based on usage in IR
    i = 0;
    tac = function->function_ir;
    while (tac) {
        if (tac->dst  && tac->dst->vreg)  update_register_liveness(tac->dst->vreg,  i);
        if (tac->src1 && tac->src1->vreg) update_register_liveness(tac->src1->vreg, i);
        if (tac->src2 && tac->src2->vreg) update_register_liveness(tac->src2->vreg, i);
        tac = tac->next;
        i++;
    }

    if (opt_use_registers_for_locals) {
        // Deal with backwards jumps. Any backwards jump targets that are in a live
        // range need the live range extending to include the jump instruction
        // since the value might have changed just before the jump.
        // Update liveness based on usage in IR.
        i = 0;
        tac = function->function_ir;
        while (tac) {
            if (tac->operation == IR_JMP || tac->operation == IR_JZ || tac->operation == IR_JNZ) {
                l = get_instruction_for_label(function, tac->src1->label);
                if (i > l)
                    for (j = 1; j <= function->function_vreg_count; j++)
                        if (liveness[j].start <= l && liveness[j].end >= l && i > liveness[j].end) liveness[j].end = i;
            }
            tac = tac->next;
            i++;
        }

        // Look for assignments in a conditional in a loop. The liveness needs extending.
        // to include the outmost loop.
        outer_loops = make_outer_loops(function);

        i = 0;
        tac = function->function_ir;
        while (tac) {
            if (tac->dst && tac->dst->vreg && tac->in_conditional) {
                // Only extend liveness for variables, not temporaries.
                if (tac->dst->stack_index || tac->dst->original_stack_index) {
                    if (debug_register_allocations) printf("extending liveness due to if/for for vreg=%d\n", tac->dst->vreg);
                    liveness_interval = outer_loops[i];
                    if (liveness_interval) {
                        if (liveness_interval->start < liveness[tac->dst->vreg].start) liveness[tac->dst->vreg].start = liveness_interval->start;
                        if (liveness_interval->end > liveness[tac->dst->vreg].end) liveness[tac->dst->vreg].end = liveness_interval->end;
                    }
                }
            }

            tac = tac->next;
            i++;
        }
    }
}

// Merge tac with the instruction after it. The next instruction is removed from the chain.
void merge_instructions(struct three_address_code *tac, int ir_index, int allow_labelled_next) {
    int i;
    struct three_address_code *next;

    if (!tac->next) panic("merge_instructions called on a tac without next\n");
    if (tac->next->label && !allow_labelled_next) panic("merge_instructions called on a tac with a label");

    next = tac->next;
    tac->next = next->next;
    if (tac->next) tac->next->prev = tac;

    // Nuke the references to the removed node to prevent confusion
    next->prev = 0;
    next->next = 0;

    for (i = 0; i < vreg_count; i++) {
        if (liveness[i].start > ir_index) liveness[i].start--;
        if (liveness[i].end > ir_index) liveness[i].end--;
    }
}

// Replace
// 1    >       r1:int = 1
// 2    >       r7:int = r1:int
// with
// 1    >       r7:int = 1
// 2    >       nop
//
// And also combinations such as
// 1    >       r1:int = 1
// 2    >       r7:int = r1:int
// 3    >       r8:int = r7:int
// with
// 1    >       r8:int = 1
// 2    >       nop
//
// Also
// 4    >       r1:int = r4:int
// 5    >       arg for call 0 r1:int
// with
// 4    >       arg for call 0 r4:int
void merge_redundant_moves(struct symbol *function) {
    struct three_address_code *tac;
    int i, vreg, changed;

    changed = 1;
    while (changed) {
        changed = 0;
        tac = function->function_ir;
        i = 0;
        while (tac) {
            if (
                    ((tac->operation == IR_LOAD_CONSTANT || tac->operation == IR_LOAD_VARIABLE || tac->operation == IR_ASSIGN)) &&
                    tac->dst->vreg &&
                    tac->next &&
                    tac->next->operation == IR_ASSIGN && tac->next->dst->vreg &&
                    ((tac->next->src1 && tac->next->src1->vreg == tac->dst->vreg) || (tac->next->src2 && tac->next->src2->vreg == tac->dst->vreg))) {

                vreg = tac->dst->vreg;
                if (liveness[vreg].start == i && liveness[vreg].end == i + 1 && !tac->next->dst->is_lvalue) {
                    liveness[tac->dst->vreg].start = -1;
                    liveness[tac->dst->vreg].end = -1;
                    tac->dst = tac->next->dst;
                    merge_instructions(tac, i, 0);
                    changed = 1;
                }
            }

            if (
                    tac->operation == IR_ASSIGN &&
                    tac->dst->vreg && !tac->src1->is_in_cpu_flags &&
                    tac->next && tac->next->operation == IR_ARG &&
                    tac->dst->vreg == tac->next->src2->vreg) {

                vreg = tac->dst->vreg;
                if (liveness[vreg].start == i && liveness[vreg].end == i + 1) {
                    liveness[tac->dst->vreg].start = -1;
                    liveness[tac->dst->vreg].end = -1;
                    tac->operation = IR_ARG;
                    tac->src2 = tac->src1;
                    tac->src1 = tac->next->src1;
                    tac->dst = 0;
                    merge_instructions(tac, i, 0);
                    changed = 1;
                }
            }

            tac = tac->next;
            i++;
        }
    }
}

// The arguments are pushed onto the stack right to left, but the ABI requries
// the seventh arg and later to be pushed in reverse order. Easiest is to flip
// all args backwards, so they are pushed left to right. This nukes the
// liveness which will need regenerating.
void reverse_function_argument_order(struct symbol *function) {
    struct three_address_code *tac, *call_start, *call;
    int i, j, arg_count, function_call_count;

    struct tac_interval *args;

    ir = function->function_ir;
    args = malloc(sizeof(struct tac_interval *) * 256);

    // Need to count this IR's function_call_count
    function_call_count = 0;
    tac = function->function_ir;
    while (tac) {
        if (tac->operation == IR_START_CALL) function_call_count++;
        tac = tac->next;
    }

    for (i = 0; i < function_call_count; i++) {
        tac = function->function_ir;
        arg_count = 0;
        call_start = 0;
        call = 0;
        while (tac) {
            if (tac->operation == IR_START_CALL && tac->src1->value == i) {
                call_start = tac;
                tac = tac->next;
                args[arg_count].start = tac;
            }
            else if (tac->operation == IR_END_CALL && tac->src1->value == i) {
                call = tac->prev;
                tac = tac->next;
            }
            else if (tac->operation == IR_ARG && tac->src1->value == i) {
                args[arg_count].end = tac;
                arg_count++;
                tac = tac->next;
                if (tac->operation != IR_END_CALL) args[arg_count].start = tac;
            }
            else
                tac = tac->next;
        }

        if (arg_count > 1) {
            call_start->next = args[arg_count - 1].start;
            args[arg_count - 1].start->prev = call_start;
            args[0].end->next = call;
            call->prev = args[0].end;

            for (j = 0; j < arg_count; j++) {
                // Rearrange args backwards from this IR
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> cs.end
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> p2.start -> p2.end -> cs.end
                if (j < arg_count - 1) {
                    args[j + 1].end->next = args[j].start;
                    args[j].start->prev = args[j + 1].end;
                }
            }
        }

    }

    // Remove start & end function markers since they aren't needed any more and
    // will interfere with the redundant move optimization code.
    tac = function->function_ir;
    i = 0;
    while (tac) {
        if (tac->next && (tac->operation == IR_START_CALL || tac->operation == IR_END_CALL)) {
            tac = tac->prev;
            i--;
            merge_instructions(tac, i, 0);
        }
        tac = tac->next;
        i++;
    }

    analyze_liveness(function);
    free(args);
}

void assign_local_to_register(struct value *v, int vreg) {
    v->original_stack_index = v->stack_index;
    v->original_is_lvalue = v->is_lvalue;
    v->original_vreg = v->vreg;
    v->stack_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

void spill_local_value_back_to_stack(struct value *v) {
    if (v->stack_index < 0) return; // already done
    if (debug_register_allocations) printf("spill_local_value_back_to_stack preg=%d, vreg=%d, ovreg=%d, s=%d\n", v->preg, v->vreg, v->original_vreg, v->original_stack_index);
    v->vreg = v->original_vreg;
    v->stack_index = v->original_stack_index;
    v->is_lvalue = v->original_is_lvalue;
    v->preg = -1;
}

void assign_locals_to_registers(struct symbol *function) {
    struct three_address_code *tac;
    int i, vreg;

    int *has_address_of;

    has_address_of = malloc(sizeof(int) * (function->function_local_symbol_count + 1));
    memset(has_address_of, 0, sizeof(int) * (function->function_local_symbol_count + 1));

    tac = function->function_ir;
    while (tac) {
        if (tac->dst  && !tac->dst ->is_lvalue && tac->dst ->stack_index < 0) has_address_of[-tac->dst ->stack_index] = 1;
        if (tac->src1 && !tac->src1->is_lvalue && tac->src1->stack_index < 0) has_address_of[-tac->src1->stack_index] = 1;
        if (tac->src2 && !tac->src2->is_lvalue && tac->src2->stack_index < 0) has_address_of[-tac->src2->stack_index] = 1;
        tac = tac ->next;
    }

    for (i = 1; i <= function->function_local_symbol_count; i++) {
        if (has_address_of[i]) continue;
        vreg = ++vreg_count;

        tac = function->function_ir;
        while (tac) {
            if (tac->dst  && tac->dst ->stack_index == -i) assign_local_to_register(tac->dst , vreg);
            if (tac->src1 && tac->src1->stack_index == -i) assign_local_to_register(tac->src1, vreg);
            if (tac->src2 && tac->src2->stack_index == -i) assign_local_to_register(tac->src2, vreg);
            if (tac->operation == IR_LOAD_VARIABLE && tac->src1->vreg == vreg)
                tac->operation = IR_ASSIGN;

            tac = tac ->next;
        }
    }

    function->function_vreg_count = vreg_count;
    analyze_liveness(function);
}

void renumber_ir_vreg(struct three_address_code *ir, int src, int dst) {
    struct three_address_code *tac;

    if (src == 0 || dst == 0) panic("Unexpected zero reg renumber");

    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg == src) tac->dst ->vreg = dst;
        if (tac->src1 && tac->src1->vreg == src) tac->src1->vreg = dst;
        if (tac->src2 && tac->src2->vreg == src) tac->src2->vreg = dst;
        tac = tac->next;
    }

    if (src == -2) src == 0;
    if (dst == -2) dst == 0;
    liveness[dst] = liveness[src];
}

void swap_ir_registers(struct three_address_code *ir, int vreg1, int vreg2) {
    renumber_ir_vreg(ir, vreg1, -2);
    renumber_ir_vreg(ir, vreg2, vreg1);
    renumber_ir_vreg(ir, -2, vreg2);
}

struct three_address_code *insert_instruction(struct three_address_code *ir, int ir_index, struct three_address_code *tac) {
    int i;
    struct three_address_code *prev;

    prev = ir->prev;
    tac->prev = prev;
    tac->next = ir;
    prev->next = tac;

    for (i = 0; i < vreg_count; i++) {
        if (liveness[i].start >= ir_index) liveness[i].start++;
        if (liveness[i].end >= ir_index) liveness[i].end++;
    }
}

void renumber_label(struct three_address_code *ir, int l1, int l2) {
    struct three_address_code *t;

    t = ir;
    while (t) {
        if (t->src1 && t->src1->label == l1) t->src1->label = l2;
        if (t->src2 && t->src2->label == l1) t->src2->label = l2;
        if (t->label == l1) t->label = l2;
        t = t->next;
    }
}

void merge_labels(struct three_address_code *ir, struct three_address_code *tac, int ir_index) {
    struct three_address_code *deleted_tac, *t;
    int l;

    while(1) {
        if (!tac->label || !tac->next || !tac->next->label) return;

        deleted_tac = tac->next;
        merge_instructions(tac, ir_index, 1);
        renumber_label(ir, deleted_tac->label, tac->label);
    }
}

// Renumber all labels so they are consecutive. Uses label_count global.
void renumber_labels(struct three_address_code *ir) {
    struct three_address_code *t;
    int temp_labels;

    temp_labels = -2;

    t = ir;
    while (t) {
        if (t->label) {
            // Rename target label out of the way
            renumber_label(ir, label_count + 1, temp_labels);
            temp_labels--;

            // Replace t->label with label_count + 1
            renumber_label(ir, t->label, -1);
            t->label = ++label_count;
            renumber_label(ir, -1, t->label);
        }
        t = t->next;
    }
}

void rearrange_reverse_sub_operation(struct three_address_code *ir, struct three_address_code *tac) {
    struct value *src1, *src2;
    int vreg1, vreg2;

    if (tac->operation == IR_SUB) {
        tac->operation = IR_RSUB;

        if (tac->src2->is_constant) {
            // Flip the operands
            src1 = dup_value(tac->src1);
            src2 = dup_value(tac->src2);
            tac->src1 = src2;
            tac->src2 = src1;
        }
        else {
            // Convert sub to reverse sub. Swap registers everywhere except
            // this tac and convert the operation to RSUB.
            vreg1 = tac->src1->vreg;
            vreg2 = tac->src2->vreg;
            swap_ir_registers(ir, tac->src1->vreg, tac->src2->vreg);
            tac->src1 = dup_value(tac->src1);
            tac->src2 = dup_value(tac->src2);
            tac->src1->vreg = vreg1;
            tac->src2->vreg = vreg2;
        }
    }
}

void coalesce_operation_registers(struct three_address_code *ir, struct three_address_code *tac, int i) {
    struct value *src1, *src2;

    // If src2 isn't live after this instruction, then the operation
    // is allowed to change it. dst->vreg can be replaced with src2->vreg
    // which saves a vreg.
    if ((   tac->operation == IR_ADD || tac->operation == IR_RSUB ||
            tac->operation == IR_MUL || tac->operation == IR_DIV || tac->operation == IR_MOD ||
            tac->operation == IR_BOR || tac->operation == IR_BAND || tac->operation == IR_XOR) && i == liveness[tac->src2->vreg].end)
        renumber_ir_vreg(ir, tac->dst->vreg, tac->src2->vreg);

    // Free either src1 or src2's vreg if possible
    if (tac->operation == IR_LT || tac->operation == IR_GT || tac->operation == IR_LE || tac->operation == IR_GE || tac->operation == IR_EQ || tac->operation == IR_NE) {
        if (tac->dst->vreg && tac->src1->vreg && i == liveness[tac->src1->vreg].end)
            renumber_ir_vreg(ir, tac->dst->vreg, tac->src1->vreg);
        else if (tac->dst->vreg && tac->src2->vreg && i == liveness[tac->src2->vreg].end)
            renumber_ir_vreg(ir, tac->dst->vreg, tac->src2->vreg);
    }

    // The same applies to the one-operand opcodes
    if ((tac->operation == IR_INDIRECT || tac->operation == IR_BNOT || tac->operation == IR_BSHL || tac->operation == IR_BSHR) && i == liveness[tac->src1->vreg].end)
        renumber_ir_vreg(ir, tac->dst->vreg, tac->src1->vreg);
}

void preload_src1_constant_into_register(struct three_address_code *tac, int *i) {
    struct value *dst, *src1;
    struct three_address_code *load_tac;

    load_tac = new_instruction(IR_LOAD_CONSTANT);

    load_tac->src1 = tac->src1;

    dst = new_value();
    dst->vreg = new_vreg();
    dst->type = TYPE_LONG;

    load_tac->dst = dst;
    insert_instruction(tac, *i, load_tac);
    tac->src1 = dst;

    liveness[dst->vreg].start = *i;
    liveness[dst->vreg].end = *i + 1;
    (*i)++;
}

void allocate_registers_for_constants(struct three_address_code *tac, int *i) {
    // Some instructions can't handle one of the operands being a constant. Allocate a vreg for it
    // and load the constant into it.

    // 1 - i case can't be done in x86_64 and needs to be done with registers
    if (tac->operation == IR_SUB && tac->src1->is_constant)
        preload_src1_constant_into_register(tac, i);
}

void optimize_ir(struct symbol *function) {
    struct three_address_code *ir, *tac;
    int i;

    ir = function->function_ir;

    reverse_function_argument_order(function);

    if (opt_use_registers_for_locals) assign_locals_to_registers(function);
    if (opt_merge_redundant_moves) merge_redundant_moves(function);

    tac = ir;
    i = 0;
    while (tac) {
        merge_labels(ir, tac, i);
        allocate_registers_for_constants(tac, &i);
        rearrange_reverse_sub_operation(ir, tac);
        if (opt_enable_register_coalescing) coalesce_operation_registers(ir, tac, i);
        tac = tac->next;
        i++;
    }

    renumber_labels(ir);
}

void spill_local_in_register_back_to_stack(struct three_address_code *ir, int original_stack_index) {
    struct three_address_code *tac;

    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->original_stack_index == original_stack_index) spill_local_value_back_to_stack(tac->dst);
        if (tac->src1 && tac->src1->original_stack_index == original_stack_index) spill_local_value_back_to_stack(tac->src1);
        if (tac->src2 && tac->src2->original_stack_index == original_stack_index) spill_local_value_back_to_stack(tac->src2);
        if (tac->operation == IR_ASSIGN && tac->src1->stack_index == original_stack_index)
            tac->operation = IR_LOAD_VARIABLE;
        tac = tac->next;
    }
}

int get_spilled_register_stack_index() {
    int i, result;

    // First try and reuse a previously used spilled stack register
    for (i = 0; i < spilled_register_count; i++)
        if (!spilled_registers[i]) return i;

    // All spilled register stack locations are in use. Allocate a new one
    result = spilled_register_count++;

    if (spilled_register_count >= MAX_SPILLED_REGISTER_COUNT)
        panic1d("Exceeded max spilled register count %d", MAX_SPILLED_REGISTER_COUNT);
    return result;
}

void spill_vreg_for_value(struct three_address_code *ir, struct value *v) {
    struct three_address_code *tac;
    int i, spilled_register_stack_index, original_stack_index, original_vreg;

    original_stack_index = v->original_stack_index;
    original_vreg = v->vreg;

    if (v->original_stack_index < 0) {
        if (debug_register_allocations) printf("spill_vreg_for_value spill_local_in_register_back_to_stack to s=%d for vreg=%d\n", original_stack_index, v->vreg);
        // We have a stack index already, use that
        spill_local_in_register_back_to_stack(ir, original_stack_index);
        return;
    }

    spilled_register_stack_index = get_spilled_register_stack_index();
    v->spilled_stack_index = spilled_register_stack_index;
    spilled_registers[spilled_register_stack_index] = v->vreg;
    if (debug_register_allocations) printf("spilling to S=%d for vreg=%d\n", spilled_register_stack_index, v->vreg);
}

// Spill a previously allocated physical register to the stack
void spill_preg(struct three_address_code *ir, struct value *v, int preg) {
    struct three_address_code *tac;
    int vreg, spilled_register_stack_index, original_stack_index;

    // Check if preg belongs to a local variable. Keep looping until the latest TAC,
    // since the preg could have been resued may times.
    original_stack_index = 0;
    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->preg == preg) { original_stack_index = tac->dst ->original_stack_index; }
        if (tac->src1 && tac->src1->preg == preg) { original_stack_index = tac->src1->original_stack_index; }
        if (tac->src2 && tac->src2->preg == preg) { original_stack_index = tac->src2->original_stack_index; }
        tac = tac->next;
    }

    // It's already allocated a stack index, use that
    if (original_stack_index < 0) {
        if (debug_register_allocations) printf("spill_preg preg=%d to s=%d\n", preg, original_stack_index);
        spill_local_in_register_back_to_stack(ir, original_stack_index);
    }

    else {
        // Always allocate a new stack index. It must be new since this stack index
        // is applied to the entire IR, so it must not overlap with a previously
        // used stack index.
        spilled_register_stack_index = get_spilled_register_stack_index();
        vreg = physical_registers[preg];
        spilled_registers[spilled_register_stack_index] = vreg;
        if (debug_register_allocations) printf("spill_preg preg=%d for vreg=%d to S=%d\n", preg, vreg, original_stack_index);

        tac = ir;
        while (tac) {
            if (tac->dst  && tac->dst ->vreg == vreg) { tac->dst ->preg = -1; tac->dst ->spilled_stack_index = spilled_register_stack_index; }
            if (tac->src1 && tac->src1->vreg == vreg) { tac->src1->preg = -1; tac->src1->spilled_stack_index = spilled_register_stack_index; }
            if (tac->src2 && tac->src2->vreg == vreg) { tac->src2->preg = -1; tac->src2->spilled_stack_index = spilled_register_stack_index; }
            tac = tac->next;
        }
    }

    if (debug_register_allocations) {
        printf("spill_preg repurposed preg=%d for vreg=%d\n", preg, v->vreg);
        tac = ir;
        while (tac) {
            print_instruction(stdout, tac);
            tac = tac->next;
        }
    }

    physical_registers[preg] = v->vreg;
    v->preg = preg;
}

void allocate_register(struct three_address_code *ir, struct value *v) {
    int i;
    int existing_preg, existing_preg_liveness_end;
    struct three_address_code *tac;

    // Return if already allocated. This can happen if values share memory
    // between different instructions
    if (v->preg != -1) return;
    if (v->spilled_stack_index != -1) return;

    // Check for already spilled registers
    for (i = 0; i < spilled_register_count; i++) {
        if (spilled_registers[i] == v->vreg) {
            v->spilled_stack_index = i;
            return;
        }
    }

    // Check for already allocated registers
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (physical_registers[i] == v->vreg) {
            v->preg = i;
            return;
        }
    }

    // Find a free register
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (!physical_registers[i]) {
            if (debug_register_allocations) printf("allocated preg=%d for vreg=%d\n", i, v->vreg);
            physical_registers[i] = v->vreg;
            v->preg = i;

            return;
        }
    }

    // Failed to allocate a register, something needs to be spilled.
    existing_preg = -1;
    existing_preg_liveness_end = -1;
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (physical_registers[i] > 0) {
            if (liveness[physical_registers[i]].end > existing_preg_liveness_end) {
                existing_preg = i;
                existing_preg_liveness_end = liveness[physical_registers[i]].end;
            }
        }
    }

    if (opt_spill_furthest_liveness_end && existing_preg_liveness_end > liveness[v->vreg].end) {
        spill_preg(ir, v, existing_preg);
        return;
    }

    spill_vreg_for_value(ir, v);
}

void allocate_registers(struct three_address_code *ir) {
    int line, i, j, vreg;
    struct three_address_code *tac, *tac2;

    physical_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(physical_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    spilled_registers = malloc(sizeof(int) * MAX_SPILLED_REGISTER_COUNT);
    memset(spilled_registers, 0, sizeof(int) * MAX_SPILLED_REGISTER_COUNT);
    spilled_register_count = 0;

    // Don't allocate these registers. The approach is quite crude and it
    // doesn't leave a lot of registers to play with. But it'll do for a first
    // approach.
    physical_registers[REG_RAX] = -1; // RAX and RDX are used for division & modulo. RAX is also a function return value
    physical_registers[REG_RDX] = -1;
    physical_registers[REG_RCX] = -1; // RCX is used for shifts
    physical_registers[REG_RSP] = -1;
    physical_registers[REG_RBP] = -1;
    physical_registers[REG_RSI] = -1; // Used in function calls
    physical_registers[REG_RDI] = -1; // Used in function calls
    physical_registers[REG_R8]  = -1; // Used in function calls
    physical_registers[REG_R9]  = -1; // Used in function calls
    physical_registers[REG_R10] = -1; // Not preserved in function calls
    physical_registers[REG_R11] = -1; // Not preserved in function calls

    if (fake_register_pressure) {
        // Allocate all registers, forcing all temporaries into the stack
        physical_registers[REG_RBX] = -1;
        physical_registers[REG_R12] = -1;
        physical_registers[REG_R13] = -1;
        physical_registers[REG_R14] = -1;
        physical_registers[REG_R15] = -1;
    }

    line = 0;
    tac = ir;
    while (tac) {
        if (debug_register_allocations) {
            printf("%d ", line);
            print_instruction(stdout, tac);
        }

        // Allocate registers
        if (tac->dst  && tac->dst->vreg)  allocate_register(ir, tac->dst);
        if (tac->src1 && tac->src1->vreg) allocate_register(ir, tac->src1);
        if (tac->src2 && tac->src2->vreg) allocate_register(ir, tac->src2);

        // Free registers
        for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
            if (physical_registers[i] > 0 && liveness[physical_registers[i]].end == line) {
                if (debug_register_allocations) printf("freeing preg=%d for vreg=%d\n", i, physical_registers[i]);
                physical_registers[i] = 0;
            }
        }

        // Free spilled stack locations
        for (i = 0; i < spilled_register_count; i++) {
            if (spilled_registers[i] > 0 && liveness[spilled_registers[i]].end == line)
                spilled_registers[i] = 0;
        }

        tac = tac->next;
        line++;
    }

    free(physical_registers);
    free(spilled_registers);

    total_spilled_register_count += spilled_register_count;
}

void check_preg(int preg) {
    if (preg == -1) panic("Illegal attempt to output -1 preg");
    if (preg < 0 || preg >= PHYSICAL_REGISTER_COUNT) panic1d("Illegal preg %d", preg);
}

void output_byte_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "al   bl   cl   dl   sil  dil  bpl  spl  r8b  r9b  r10b r11b r12b r13b r14b r15b";
         if (preg < 4)  fprintf(f, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) fprintf(f, "%%%.3s", &names[preg * 5]);
    else                fprintf(f, "%%%.4s", &names[preg * 5]);
}

void output_word_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "ax   bx   cx   dx   si   di   bp   sp   r8w  r9w  r10w r11w r12w r13w r14w r15w";
         if (preg < 8)  fprintf(f, "%%%.2s", &names[preg * 5]);
    else if (preg < 10) fprintf(f, "%%%.3s", &names[preg * 5]);
    else                fprintf(f, "%%%.4s", &names[preg * 5]);
}

void output_long_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "eax  ebx  ecx  edx  esi  edi  ebp  esp  r8d  r9d  r10d r11d r12d r13d r14d r15d";
    if (preg < 10) fprintf(f, "%%%.3s", &names[preg * 5]);
    else           fprintf(f, "%%%.4s", &names[preg * 5]);
}

void output_quad_register_name(int preg) {
    char *names;

    check_preg(preg);
    names = "rax rbx rcx rdx rsi rdi rbp rsp r8  r9  r10 r11 r12 r13 r14 r15";
    if (preg == 8 || preg == 9) fprintf(f, "%%%.2s", &names[preg * 4]);
    else                        fprintf(f, "%%%.3s", &names[preg * 4]);
}

void move_quad_register_to_register(int preg1, int preg2) {
    if (preg1 != preg2) {
        fprintf(f, "\tmovq\t");
        output_quad_register_name(preg1);
        fprintf(f, ", ");
        output_quad_register_name(preg2);
        fprintf(f, "\n");
    }
}

void output_type_specific_register_name(int type, int preg) {
         if (type == TYPE_CHAR)  output_byte_register_name(preg);
    else if (type == TYPE_SHORT) output_word_register_name(preg);
    else if (type == TYPE_INT)   output_long_register_name(preg);
    else                         output_quad_register_name(preg);
}

void _output_op(char *instruction, struct three_address_code *tac) {
    move_quad_register_to_register(tac->src2->preg, tac->dst->preg);
    fprintf(f, "\t%s\t", instruction);
    output_quad_register_name(tac->src1->preg);
    fprintf(f, ", ");
    output_quad_register_name(tac->dst->preg);
    fprintf(f, "\n");
}

void output_constant_operation(char *instruction, struct three_address_code *tac) {
    if (tac->src1->is_constant) {
        move_quad_register_to_register(tac->src2->preg, tac->dst->preg);
        fprintf(f, "\t%s\t$%ld", instruction, tac->src1->value);
        fprintf(f, ", ");
        output_quad_register_name(tac->dst->preg);
        fprintf(f, "\n");
    }
    else
        _output_op(instruction, tac);
}

void output_op(char *instruction, struct three_address_code *tac) {
    if (tac->operation == IR_ADD || tac->operation == IR_RSUB || tac->operation == IR_MUL || tac->operation == IR_BAND || tac->operation == IR_BOR || tac->operation == IR_XOR) {
        output_constant_operation(instruction, tac);
        return;
    }
    else
        _output_op(instruction, tac);
}

// src1 and src2 are swapped, to facilitate treating the first operand as a potential
// constant. src1 is output first, which is backwards to src1 <op> src2.
// Therefore, this function is called reverse.
void output_reverse_cmp(struct three_address_code *tac) {
    fprintf(f, "\tcmpq\t");

    if (tac->src1->is_constant)
        fprintf(f, "$%ld", tac->src1->value);
    else
        output_quad_register_name(tac->src1->preg);

    fprintf(f, ", ");
    output_quad_register_name(tac->src2->preg);
    fprintf(f, "\n");
}

void output_cmp_result_instruction(struct three_address_code *ir, char *instruction) {
    fprintf(f, "\t%s\t", instruction);
    output_byte_register_name(ir->dst->preg);
    fprintf(f, "\n");
}

void output_movzbq(struct three_address_code *ir) {
    fprintf(f, "\tmovzbq\t");
    output_byte_register_name(ir->dst->preg);
    fprintf(f, ", ");
    output_quad_register_name(ir->dst->preg);
    fprintf(f, "\n");
}

void output_type_specific_mov(int type) {
         if (type == TYPE_CHAR)  fprintf(f, "\tmovb\t");
    else if (type == TYPE_SHORT) fprintf(f, "\tmovw\t");
    else if (type == TYPE_INT)   fprintf(f, "\tmovl\t");
    else                         fprintf(f, "\tmovq\t");
}

void output_type_specific_sign_extend_mov(int type) {
         if (type == TYPE_CHAR)  fprintf(f, "\tmovsbq\t");
    else if (type == TYPE_SHORT) fprintf(f, "\tmovswq\t");
    else if (type == TYPE_INT)   fprintf(f, "\tmovslq\t");
    else                         fprintf(f, "\tmovq\t");
}

void output_type_specific_lea(int type) {
         if (type == TYPE_CHAR)  fprintf(f, "\tleasbq\t");
    else if (type == TYPE_SHORT) fprintf(f, "\tleaswq\t");
    else if (type == TYPE_INT)   fprintf(f, "\tleaslq\t");
    else                         fprintf(f, "\tleaq\t");
}

void output_reverse_cmp_operation(struct three_address_code *tac, char *instruction) {
    output_reverse_cmp(tac);
    if (tac->dst->is_in_cpu_flags) return;
    output_cmp_result_instruction(tac, instruction);
    output_movzbq(tac);
}

// Get offset from the stack in bytes, from a stack_index. This is a hairy function
// since it has to take the amount of params (pc) into account as well as the
// position where local variables start.
//
// The stack layout
// stack index  offset  var
// 1            +3     first passed arg
// 0            +2     second passed arg
//              +1     return address
//              +0     BP
//                     saved registers
// -1           -1     first local
// -2           -2     second local
int get_stack_offset_from_index(int function_pc, int local_vars_stack_start, int stack_index) {
    int stack_offset;

    if (stack_index >= 2) {
        // Function argument
        stack_index -= 2; // 0=1st arg, 1=2nd arg, etc

        if (function_pc > 6) {
            stack_index = function_pc - 7 - stack_index;
            // Correct for split stack when there are more than 6 args
            if (stack_index < 0) {
                // Read pushed arg by the callee. arg 0 is at rsp-0x30, arg 2 at rsp-0x28 etc, ... arg 5 at rsp-0x08
                stack_offset = 8 * stack_index;
            }
            else {
                // Read pushed arg by the caller, arg 6 is at rsp+0x10, arg 7 at rsp+0x18, etc
                // The +2 is to bypass the pushed rbp and return address
                stack_offset = 8 * (stack_index + 2);
            }
        }
        else {
            // The first arg is at stack_index=0, second at stack_index=1
            // If there are e.g. 2 args:
            // arg 0 is at mov -0x10(%rbp), %rax
            // arg 1 is at mov -0x08(%rbp), %rax
            stack_offset = -8 * (stack_index + 1);
        }
    }
    else {
        // Local variable. v=-1 is the first, v=-2 the second, etc
        stack_offset = local_vars_stack_start + 8 * (stack_index + 1);
    }

    return stack_offset;
}

// Called once at startup to indicate which registers are preserved across function calls
void init_callee_saved_registers() {
    callee_saved_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(callee_saved_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    callee_saved_registers[REG_RBX] = 1;
    callee_saved_registers[REG_R12] = 1;
    callee_saved_registers[REG_R13] = 1;
    callee_saved_registers[REG_R14] = 1;
    callee_saved_registers[REG_R15] = 1;
}

// Determine which registers are used in a function, push them onto the stack and return the list
int *push_callee_saved_registers(struct three_address_code *tac) {
    int *saved_registers;
    int i;

    saved_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(saved_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    while (tac) {
        if (tac->dst  && tac->dst ->preg != -1 && callee_saved_registers[tac->dst ->preg]) saved_registers[tac->dst ->preg] = 1;
        if (tac->src1 && tac->src1->preg != -1 && callee_saved_registers[tac->src1->preg]) saved_registers[tac->src1->preg] = 1;
        if (tac->src2 && tac->src2->preg != -1 && callee_saved_registers[tac->src2->preg]) saved_registers[tac->src2->preg] = 1;
        tac = tac->next;
    }

    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
        if (saved_registers[i]) {
            fprintf(f, "\tpushq\t");
            output_quad_register_name(i);
            fprintf(f, "\n");
        }
    }

    return saved_registers;
}

// Pop the callee saved registers back
void pop_callee_saved_registers(int *saved_registers) {
    int i;

    for (i = PHYSICAL_REGISTER_COUNT - 1; i >= 0; i--) {
        if (saved_registers[i]) {
            fprintf(f, "\tpopq\t");
            output_quad_register_name(i);
            fprintf(f, "\n");
        }
    }
}

void pre_instruction_local_load(struct three_address_code *ir, int function_pc, int local_vars_stack_start) {
    int stack_offset;

    // Load src1 into r10
    if (ir->src1 && ir->src1->preg == -1 && ir->src1->stack_index < 0) {
        stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, ir->src1->stack_index);
        output_type_specific_sign_extend_mov(ir->src1->type);
        fprintf(f, "%d(%%rbp), %%r10\n", stack_offset);
        ir->src1 = dup_value(ir->src1); // Ensure no side effects
        ir->src1->preg = REG_R10;
    }

    // Load src2 into r11
    if (ir->src2 && ir->src2->preg == -1 && ir->src2->stack_index < 0) {
        stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, ir->src2->stack_index);
        output_type_specific_sign_extend_mov(ir->src2->type);
        fprintf(f, "%d(%%rbp), %%r11\n", stack_offset);
        ir->src2 = dup_value(ir->src2); // Ensure no side effects
        ir->src2->preg = REG_R11;
    }

    if (ir->dst && ir->dst->preg == -1 && ir->dst->stack_index < 0) {
        // Set the dst preg to r10 or r11 depending on what the operation type has set

        ir->dst = dup_value(ir->dst); // Ensure no side effects
        if (ir->operation == IR_BSHR || ir->operation == IR_BSHL)
            ir->dst->preg = REG_R10;
        else
            ir->dst->preg = REG_R11;

        // If the operation is an assignment and If there is an lvalue on the stack, move it into r11.
        // The assign code will use that to store the result of the assignment.
        if (ir->operation == IR_ASSIGN && ir->dst->vreg && ir->dst->is_lvalue) {
            stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, ir->dst->stack_index);
            fprintf(f, "\tmovq\t%d(%%rbp), %%r11\n", stack_offset);
        }
    }
}

void post_instruction_local_store(struct three_address_code *ir, int function_pc, int local_vars_stack_start) {
    int stack_offset;

    if (ir->dst && ir->dst->preg != -1 && ir->dst->stack_index < 0) {
        // Output a mov for assignments that are a register copy.
        if (ir->operation == IR_ASSIGN && (ir->dst->stack_index || ir->dst->global_symbol || ir->dst->is_lvalue || ir->dst->is_in_cpu_flags)) return;

        stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, ir->dst->stack_index);
        fprintf(f, "\tmovq\t");
        output_quad_register_name(ir->dst->preg);
        fprintf(f, ", %d(%%rbp)\n", stack_offset);
    }
}

// If any of the operands are spilled, output code to read the stack locations into registers r10 and r11
// and set the preg accordingly. Also set the dst preg.
void pre_instruction_spill(struct three_address_code *ir, int function_pc, int local_vars_stack_start, int spilled_registers_stack_start) {
    pre_instruction_local_load(ir, function_pc, local_vars_stack_start);

    // Load src1 into r10
    if (ir->src1 && ir->src1->spilled_stack_index != -1) {
        fprintf(f, "\tmovq\t%d(%%rbp), %%r10\n", spilled_registers_stack_start - ir->src1->spilled_stack_index * 8);
        ir->src1 = dup_value(ir->src1); // Ensure no side effects
        ir->src1->preg = REG_R10;
    }

    // Load src2 into r11
    if (ir->src2 && ir->src2->spilled_stack_index != -1) {
        fprintf(f, "\tmovq\t%d(%%rbp), %%r11\n", spilled_registers_stack_start - ir->src2->spilled_stack_index * 8);
        ir->src2 = dup_value(ir->src2); // Ensure no side effects
        ir->src2->preg = REG_R11;
    }

    if (ir->dst && ir->dst->spilled_stack_index != -1) {
        // Set the dst preg to r10 or r11 depending on what the operation type has set

        ir->dst = dup_value(ir->dst); // Ensure no side effects
        if (ir->operation == IR_BSHR || ir->operation == IR_BSHL)
            ir->dst->preg = REG_R10;
        else
            ir->dst->preg = REG_R11;

        // If the operation is an assignment and If there is an lvalue on the stack, move it into r11.
        // The assign code will use that to store the result of the assignment.
        if (ir->operation == IR_ASSIGN && ir->dst->vreg && ir->dst->is_lvalue)
            fprintf(f, "\tmovq\t%d(%%rbp), %%r11\n", spilled_registers_stack_start - ir->dst->spilled_stack_index * 8);
    }
}

void post_instruction_spill(struct three_address_code *ir, int function_pc, int local_vars_stack_start, int spilled_registers_stack_start) {
    post_instruction_local_store(ir, function_pc, local_vars_stack_start);

    if (ir->dst && ir->dst->spilled_stack_index != -1) {
        // Output a mov for assignments that are a register copy.
        if (ir->operation == IR_ASSIGN && (ir->dst->stack_index || ir->dst->global_symbol || ir->dst->is_lvalue || ir->dst->is_in_cpu_flags)) return;

        fprintf(f, "\tmovq\t");
        output_quad_register_name(ir->dst->preg);
        fprintf(f, ", %d(%%rbp)\n", spilled_registers_stack_start - ir->dst->spilled_stack_index * 8);
    }
}

// Output code from the IR of a function
void output_function_body_code(struct symbol *symbol) {
    int i, stack_offset;
    struct three_address_code *tac;
    int function_pc;                    // The Function's param count
    int ac;                             // A function call's arg count
    int local_stack_size;               // Size of the stack containing local variables and spilled registers
    int local_vars_stack_start;         // Stack start for the local variables
    int spilled_registers_stack_start;  // Stack start for the spilled registers
    int *saved_registers;               // Callee saved registers
    char *s;

    fprintf(f, "\tpush\t%%rbp\n");
    fprintf(f, "\tmovq\t%%rsp, %%rbp\n");

    // Push up to the first 6 args onto the stack, so all args are on the stack with leftmost arg first.
    // Arg 7 and onwards are already pushed.

    function_pc = symbol->function_param_count;

    // Push the args in the registers on the stack. The order for all args is right to left.
    if (function_pc >= 6) fprintf(f, "\tpush\t%%r9\n");
    if (function_pc >= 5) fprintf(f, "\tpush\t%%r8\n");
    if (function_pc >= 4) fprintf(f, "\tpush\t%%rcx\n");
    if (function_pc >= 3) fprintf(f, "\tpush\t%%rdx\n");
    if (function_pc >= 2) fprintf(f, "\tpush\t%%rsi\n");
    if (function_pc >= 1) fprintf(f, "\tpush\t%%rdi\n");

    // Calculate stack start for locals. reduce by pushed bsp and  above pushed args.
    local_vars_stack_start = -8 - 8 * (function_pc <= 6 ? function_pc : 6);
    spilled_registers_stack_start = local_vars_stack_start - 8 * symbol->function_local_symbol_count;

    // Allocate stack space for local variables and spilled registers
    local_stack_size = 8 * (symbol->function_local_symbol_count + symbol->function_spilled_register_count);

    // Allocate local stack
    if (local_stack_size > 0)
        fprintf(f, "\tsubq\t$%d, %%rsp\n", local_stack_size);

    tac = symbol->function_ir;
    saved_registers = push_callee_saved_registers(tac);

    while (tac) {
        if (output_inline_ir) {
            fprintf(f, "\t// ------------------------------------- ");
            print_instruction(f, tac);
        }

        pre_instruction_spill(tac, function_pc, local_vars_stack_start, spilled_registers_stack_start);

        if (tac->label) fprintf(f, ".l%d:\n", tac->label);

        if (tac->operation == IR_NOP || tac->operation == IR_START_LOOP || tac->operation == IR_END_LOOP);

        else if (tac->operation == IR_LOAD_CONSTANT) {
            fprintf(f, "\tmovq\t$%ld, ", tac->src1->value);
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_LOAD_STRING_LITERAL) {
            fprintf(f, "\tleaq\t.SL%d(%%rip), ", tac->src1->string_literal_index);
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_LOAD_VARIABLE) {
            if (tac->src1->global_symbol) {
                if (!tac->src1->is_lvalue) {
                    output_type_specific_lea(tac->src1->type);
                    fprintf(f, "%s(%%rip), ", tac->src1->global_symbol->identifier);
                }
                else {
                    output_type_specific_sign_extend_mov(tac->src1->type);
                    fprintf(f, "%s(%%rip), ", tac->src1->global_symbol->identifier);
                }
            }
            else {
                stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, tac->src1->stack_index);
                if (!tac->src1->is_lvalue) {
                    output_type_specific_lea(tac->src1->type);
                    fprintf(f, "%d(%%rbp), ", stack_offset);
                }
                else {
                    output_type_specific_sign_extend_mov(tac->src1->type);
                    fprintf(f, "%d(%%rbp), ", stack_offset);
                }
            }

            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_INDIRECT) {
            output_type_specific_sign_extend_mov(tac->dst->type);
            fprintf(f, "(");
            output_quad_register_name(tac->src1->preg);
            fprintf(f, "), ");
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_START_CALL);
        else if (tac->operation == IR_END_CALL);

        else if (tac->operation == IR_ARG) {
            fprintf(f, "\tpushq\t");
            output_quad_register_name(tac->src2->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_CALL) {
            // Read the first 6 args from the stack in right to left order
            ac = tac->src1->function_call_arg_count;

            if (ac >= 1) fprintf(f, "\tpopq\t%%rdi\n");
            if (ac >= 2) fprintf(f, "\tpopq\t%%rsi\n");
            if (ac >= 3) fprintf(f, "\tpopq\t%%rdx\n");
            if (ac >= 4) fprintf(f, "\tpopq\t%%rcx\n");
            if (ac >= 5) fprintf(f, "\tpopq\t%%r8\n");
            if (ac >= 6) fprintf(f, "\tpopq\t%%r9\n");

            // Variadic functions have the number of floating point arguments passed in al.
            // Since floating point numbers isn't implemented, this is zero.
            if (tac->src1->function_symbol->function_is_variadic)
                fprintf(f, "\tmovb\t$0, %%al\n");

            if (tac->src1->function_symbol->function_builtin)
                fprintf(f, "\tcallq\t%s@PLT\n", tac->src1->function_symbol->identifier);
            else
                fprintf(f, "\tcallq\t%s\n", tac->src1->function_symbol->identifier);

            // For all builtins that return something smaller an int, extend it to a quad
            if (tac->dst) {
                if (tac->dst->type <= TYPE_CHAR)  fprintf(f, "\tcbtw\n");
                if (tac->dst->type <= TYPE_SHORT) fprintf(f, "\tcwtl\n");
                if (tac->dst->type <= TYPE_INT)   fprintf(f, "\tcltq\n");

                fprintf(f, "\tmovq\t%%rax, ");
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }

            // Adjust the stack for any args that are on in stack
            if (ac > 6) fprintf(f, "\taddq\t$%d, %%rsp\n", (ac - 6) * 8);
        }

        else if (tac->operation == IR_RETURN) {
            if (tac->src1) {
                if (tac->src1->preg != REG_RAX) {
                    fprintf(f, "\tmovq\t");
                    output_quad_register_name(tac->src1->preg);
                    fprintf(f, ", ");
                    output_quad_register_name(REG_RAX);
                    fprintf(f, "\n");
                }
            }
            pop_callee_saved_registers(saved_registers);
            fprintf(f, "\tleaveq\n");
            fprintf(f, "\tretq\n");
        }

        else if (tac->operation == IR_ASSIGN) {
            if (tac->dst->is_in_cpu_flags); // Do nothing
            else if (tac->src1->is_in_cpu_flags) {
                     if (tac->prev->operation == IR_EQ) s = "sete";
                else if (tac->prev->operation == IR_NE) s = "setne";
                else if (tac->prev->operation == IR_LT) s = "setg";
                else if (tac->prev->operation == IR_GT) s = "setl";
                else if (tac->prev->operation == IR_LE) s = "setge";
                else if (tac->prev->operation == IR_GE) s = "setle";
                else panic1d("Internal error: unknown comparison operation %d", tac->prev->operation);
                output_cmp_result_instruction(tac, s);
                output_movzbq(tac);
            }
            else if (tac->dst->global_symbol) {
                // dst a global
                if (tac->dst->vreg) panic("Unexpected vreg in assign for global");
                output_type_specific_mov(tac->dst->type);
                output_type_specific_register_name(tac->dst->type, tac->src1->preg);
                fprintf(f, ", ");
                fprintf(f, "%s(%%rip)\n", tac->dst->global_symbol->identifier);
            }
            else if (tac->dst->stack_index) {
                // dst is a local variable on the stack
                if (tac->dst->vreg) panic("Unexpected vreg in assign for local");
                output_type_specific_mov(tac->dst->type);
                output_type_specific_register_name(tac->dst->type, tac->src1->preg);
                fprintf(f, ", ");
                stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, tac->dst->stack_index);
                fprintf(f, "%d(%%rbp)\n", stack_offset);
            }
            else if (tac->dst->is_lvalue) {
                // dst is an lvalue in a register
                output_type_specific_mov(tac->dst->type);
                output_type_specific_register_name(tac->dst->type, tac->src1->preg);
                fprintf(f, ", ");
                fprintf(f, "(");
                output_quad_register_name(tac->dst->preg);
                fprintf(f, ")\n");
            }
            else {
                // Register copy
                fprintf(f, "\tmovq\t");
                output_quad_register_name(tac->src1->preg);
                fprintf(f, ", ");
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }
        }

        else if (tac->operation == IR_JZ || tac->operation == IR_JNZ) {
            if (tac->src1->is_in_cpu_flags) {
                // src1 and src2 is backwards to facilitate constant handling,
                // hence the flipping of the comparision direction.
                     if (tac->prev->operation == IR_GT && tac->operation == IR_JNZ) s = "jl";
                else if (tac->prev->operation == IR_GT && tac->operation == IR_JZ)  s = "jnl";
                else if (tac->prev->operation == IR_LT && tac->operation == IR_JNZ) s = "jg";
                else if (tac->prev->operation == IR_LT && tac->operation == IR_JZ)  s = "jng";
                else if (tac->prev->operation == IR_GE && tac->operation == IR_JNZ) s = "jle";
                else if (tac->prev->operation == IR_GE && tac->operation == IR_JZ)  s = "jnle";
                else if (tac->prev->operation == IR_LE && tac->operation == IR_JNZ) s = "jge";
                else if (tac->prev->operation == IR_LE && tac->operation == IR_JZ)  s = "jnge";
                else if (tac->prev->operation == IR_EQ && tac->operation == IR_JNZ) s = "je";
                else if (tac->prev->operation == IR_EQ && tac->operation == IR_JZ)  s = "jne";
                else if (tac->prev->operation == IR_NE && tac->operation == IR_JNZ) s = "jne";
                else if (tac->prev->operation == IR_NE && tac->operation == IR_JZ)  s = "je";

                else {
                    panic1d("Unknown comparison operator %d", tac->prev->operation);
                    exit(1);
                }

                fprintf(f, "\t%s\t.l%d\n", s, tac->src2->label);
            }

            else  {
                // The condition is in a register. Check if it's zero.
                fprintf(f, "\tcmpq\t$0x0, ");
                output_quad_register_name(tac->src1->preg);
                fprintf(f, "\n");
                fprintf(f, "\t%s\t.l%d\n", tac->operation == IR_JZ ? "je" : "jne", tac->src2->label);
            }
        }

        else if (tac->operation == IR_JMP)
            fprintf(f, "\tjmp\t.l%d\n", tac->src1->label);

        else if (tac->operation == IR_EQ) output_reverse_cmp_operation(tac, "sete");
        else if (tac->operation == IR_NE) output_reverse_cmp_operation(tac, "setne");
        else if (tac->operation == IR_LT) output_reverse_cmp_operation(tac, "setg");
        else if (tac->operation == IR_GT) output_reverse_cmp_operation(tac, "setl");
        else if (tac->operation == IR_LE) output_reverse_cmp_operation(tac, "setge");
        else if (tac->operation == IR_GE) output_reverse_cmp_operation(tac, "setle");

        else if (tac->operation == IR_BOR)  output_op("orq",   tac);
        else if (tac->operation == IR_BAND) output_op("andq",  tac);
        else if (tac->operation == IR_XOR)  output_op("xorq",  tac);
        else if (tac->operation == IR_ADD)  output_op("addq",  tac);
        else if (tac->operation == IR_RSUB) output_op("subq",  tac);
        else if (tac->operation == IR_MUL)  output_op("imulq", tac);

        else if (tac->operation == IR_DIV || tac->operation == IR_MOD) {
            // This is slightly ugly. src1 is the dividend and needs doubling in size and placing in the RDX register.
            // It could have been put in the RDX register in the first place.
            // The quotient is stored in RAX and remainder in RDX, but is then copied
            // to whatever register is allocated for the dst, which might as well have been RAX or RDX for the respective quotient and remainders.

            move_quad_register_to_register(tac->src2->preg, tac->dst->preg);
            fprintf(f, "\tmovq\t");
            output_quad_register_name(tac->src1->preg);
            fprintf(f, ", %%rax\n");
            fprintf(f, "\tcqto\n");
            fprintf(f, "\tidivq\t");
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");

            if (tac->operation == IR_DIV)
                move_quad_register_to_register(REG_RAX, tac->dst->preg);
            else
                move_quad_register_to_register(REG_RDX, tac->dst->preg);
        }

        else if (tac->operation == IR_BNOT)  {
            move_quad_register_to_register(tac->src1->preg, tac->dst->preg);
            fprintf(f, "\tnot\t");
            output_quad_register_name(tac->dst->preg);
            fprintf(f, "\n");
        }

        else if (tac->operation == IR_BSHL || tac->operation == IR_BSHR) {
            // Ugly, this means rcx is permamently allocated
            if (tac->src2->is_constant) {
                // Shift a non-constant by a constant amount
                move_quad_register_to_register(tac->src1->preg, tac->dst->preg);
                fprintf(f, "\t%s\t$%ld, ", tac->operation == IR_BSHL ? "shl" : "sar", tac->src2->value);
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }
            else {
                if (tac->src1->is_constant) {
                    // Shift a constant by a non-constant amount
                    fprintf(f, "\tmovq\t$%ld, ", tac->src1->value);
                    output_quad_register_name(tac->dst->preg);
                    fprintf(f, "\n");
                }
                else
                    // Shift a non-constant by a non-constant amount
                    move_quad_register_to_register(tac->src1->preg, tac->dst->preg);

                fprintf(f, "\tmovq\t");
                output_quad_register_name(tac->src2->preg);
                fprintf(f, ", %%rcx\n");
                fprintf(f, "\t%s\t%%cl, ", tac->operation == IR_BSHL ? "shl" : "sar");
                output_quad_register_name(tac->dst->preg);
                fprintf(f, "\n");
            }
        }

        else
            panic1d("output_function_body_code(): Unknown operation: %d", tac->operation);

        post_instruction_spill(tac, function_pc, local_vars_stack_start, spilled_registers_stack_start);

        tac = tac->next;
    }

    // Special case for main, return 0 if no return statement is present
    if (!strcmp(symbol->identifier, "main")) fprintf(f, "\tmovq\t$0, %%rax\n");

    pop_callee_saved_registers(saved_registers);
    fprintf(f, "\tleaveq\n");
    fprintf(f, "\tretq\n");
}

// Output code for the translation unit
void output_code(char *input_filename, char *output_filename) {
    int i;
    struct three_address_code *tac;
    struct symbol *s;
    char *sl;

    if (!strcmp(output_filename, "-"))
        f = stdout;
    else {
        // Open output file for writing
        f = fopen(output_filename, "w");
        if (f == 0) {
            perror(output_filename);
            exit(1);
        }
    }

    fprintf(f, "\t.file\t\"%s\"\n", input_filename);

    // Output symbols
    fprintf(f, "\t.text\n");
    s = symbol_table;
    while (s->identifier) {
        if (s->scope || s->is_function || s->function_builtin) { s++; continue; };
        fprintf(f, "\t.comm %s,%d,%d\n", s->identifier, get_type_size(s->type), get_type_alignment(s->type));
        s++;
    }

    // Output string literals
    if (string_literal_count > 0) {
        fprintf(f, "\n\t.section\t.rodata\n");
        for (i = 0; i < string_literal_count; i++) {
            sl = string_literals[i];
            fprintf(f, ".SL%d:\n", i);
            fprintf(f, "\t.string ");
            fprintf_escaped_string_literal(f, sl);
            fprintf(f, "\n");
        }
        fprintf(f, "\n");
    }

    // Output code
    fprintf(f, "\t.text\n");

    // Output symbols for all functions
    s = symbol_table;
    while (s->identifier) {
        if (s->is_function) fprintf(f, "\t.globl\t%s\n", s->identifier);
        s++;
    }
    fprintf(f, "\n");

    label_count = 0; // Used in label renumbering

    // Generate body code for all functions
    s = symbol_table;
    while (s->identifier) {
        if (!s->is_function) { s++; continue; }

        ensure_must_be_ssa_ish(s->function_ir);

        fprintf(f, "%s:\n", s->identifier);

        // registers start at 1
        liveness = malloc(sizeof(struct liveness_interval) * (MAX_VREG_COUNT + 1));

        if (print_ir1) print_intermediate_representation(s);

        analyze_liveness(s);

        vreg_count = s->function_vreg_count;
        optimize_ir(s);
        s->function_vreg_count = vreg_count;

        if (print_ir2) print_intermediate_representation(s);

        if (debug_register_allocations) print_liveness(s);
        allocate_registers(s->function_ir);
        s->function_spilled_register_count = spilled_register_count;
        if (print_ir3) print_intermediate_representation(s);

        output_function_body_code(s);
        fprintf(f, "\n");
        s++;
    }

    fclose(f);
}

// Add a builtin symbol
void add_builtin(char *identifier, int instruction, int type, int is_variadic) {
    struct symbol *s;

    s = new_symbol();
    s->type = type;
    s->identifier = identifier;
    s->function_builtin = instruction;
    s->function_is_variadic = is_variadic;
}

void do_print_symbols() {
    long type, scope, value, stack_index;
    struct symbol *s;
    char *identifier;

    printf("Symbols:\n");
    s = symbol_table;
    while (s->identifier) {
        type = s->type;
        identifier = (char *) s->identifier;
        scope = s->scope;
        value = s->value;
        stack_index = s->stack_index;
        printf("%-20ld %-5ld %-3ld %-3ld %-20ld %s\n", (long) s, type, scope, stack_index, value, identifier);
        s++;
    }
    printf("\n");
}

char *replace_extension(char *input, char *ext) {
    char *p;
    char *result;
    int len;

    p  = strrchr(input,'.');
    if (!p) {
        result = malloc(strlen(input) + strlen(ext) + 2);
        sprintf(result, "%s.%s", input, ext);
        return result;
    }
    else {
        len = p - input + strlen(ext) + 1;
        result = malloc(len + 1);
        memcpy(result, input, p - input);
        result[p  - input] = '.';
        memcpy(result + (p - input + 1), ext, strlen(ext));
        result[len] = 0;
        return result;
    }
}

char *make_temp_filename(char *template) {
    int fd;

    template = strdup(template);
    fd = mkstemps(template, 2);
    if (fd == -1) {
        perror("in make_temp_filename");
        exit(1);
    }
    close(fd);

    return strdup(template);
}

int main(int argc, char **argv) {
    int help, debug, print_symbols, print_code;
    char *input_filename, *output_filename;
    char *compiler_input_filename, *compiler_output_filename;
    char *assembler_input_filename, *assembler_output_filename;
    char *linker_input_filename, *linker_output_filename;
    int filename_len;
    char *command;
    int result;

    verbose = 0;
    compile = 1;
    run_assembler = 1;
    run_linker = 1;
    target_is_object_file = 0;
    target_is_assembly_file = 0;
    help = 0;
    print_spilled_register_count = 0;
    print_ir1 = 0;
    print_ir2 = 0;
    print_ir3 = 0;
    print_symbols = 0;
    fake_register_pressure = 0;
    opt_enable_register_coalescing = 1;
    opt_use_registers_for_locals = 0;
    opt_merge_redundant_moves = 0;
    opt_spill_furthest_liveness_end = 0;
    output_inline_ir = 0;
    output_filename = 0;
    input_filename = 0;

    debug_register_allocations = 0;

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"                            )) { help = 1;                            argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-v"                            )) { verbose = 1;                         argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-d"                            )) { debug = 1;                           argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-s"                            )) { print_symbols = 1;                   argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--prc"                         )) { print_spilled_register_count = 1;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir1"                         )) { print_ir1 = 1;                       argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir2"                         )) { print_ir2 = 1;                       argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--ir3"                         )) { print_ir3 = 1;                       argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--frp"                         )) { fake_register_pressure = 1;          argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "--iir"                         )) { output_inline_ir = 1;                argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fno-coalesce-registers"       )) { opt_enable_register_coalescing = 0;  argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fuse-registers-for-locals"    )) { opt_use_registers_for_locals = 1;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fmerge-redundant-moves"       )) { opt_merge_redundant_moves = 1;       argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-fspill-furthest-liveness-end" )) { opt_spill_furthest_liveness_end = 1; argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-S"                            )) {
                run_assembler = 0;
                run_linker = 0;
                target_is_assembly_file = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-O1",   2)) {
                opt_enable_register_coalescing = 1;
                opt_use_registers_for_locals = 1;
                opt_merge_redundant_moves = 1;
                opt_spill_furthest_liveness_end = 1;
                argc--;
                argv++;
            }
            else if (argc > 0 && !memcmp(argv[0], "-c",   2)) {
                run_linker = 0;
                target_is_object_file = 1;
                argc--;
                argv++;
            }
            else if (argc > 1 && !memcmp(argv[0], "-o",   2)) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            else {
                printf("Unknown parameter %s\n", argv[0]);
                exit(1);
            }
        }
        else {
            if (input_filename) {
                printf("Duplicate input filename\n");
                exit(1);
            }
            input_filename = argv[0];
            argc--;
            argv++;
        }
    }

    if (help) {
        printf("Usage: wc4 [-S -c -v -d -ir1 -ir2 -ir3 -s -frp -iir -h] [-o OUTPUT-FILE] INPUT-FILE\n\n");
        printf("Flags\n");
        printf("-S                             Compile only; do not assemble or link\n");
        printf("-c                             Compile and assemble, but do not link\n");
        printf("-o <file>                      Output file. Use - for stdout. Defaults to the source file with extension .s\n");
        printf("-v                             Display the programs invoked by the compiler\n");
        printf("-d                             Debug output\n");
        printf("-s                             Output symbol table\n");
        printf("-frp                           Fake register pressure, for testing spilling code\n");
        printf("-iir                           Output inline intermediate representation\n");
        printf("--prc                          Output spilled register count\n");
        printf("--ir1                          Output intermediate representation after parsing\n");
        printf("--ir2                          Output intermediate representation after x86_64 rearrangements\n");
        printf("--ir3                          Output intermediate representation after register allocation\n");
        printf("-h                             Help\n");
        printf("\n");
        printf("Optimization options:\n");
        printf("-fno-coalesce-registers        Disable register coalescing\n");
        printf("-fuse-registers-for-locals     Allocate registers for locals instead of using the stack by default\n");
        printf("-fmerge-redundant-moves        Merge redundant register moves\n");
        printf("-fspill-furthest-liveness-end  Spill liveness intervals that have the greatest end liveness interval\n");
        exit(1);
    }

    if (!input_filename) {
        printf("Missing input filename\n");
        exit(1);
    }

    filename_len = strlen(input_filename);

    if (filename_len > 2 && input_filename[filename_len - 2] == '.') {
        if (input_filename[filename_len - 1] == 'o') {
            compile = 0;
            run_assembler = 0;
            run_linker = 1;
        }
        else if (input_filename[filename_len - 1] == 's') {
            compile = 0;
            run_assembler = 1;
        }
    }

    if (!output_filename) {
        if (run_linker)
            output_filename = "a.out";
        else if (run_assembler)
            output_filename = replace_extension(input_filename, "o");
        else
            output_filename = replace_extension(input_filename, "s");
    }

    if (compile) {
        compiler_input_filename = input_filename;
        if (run_assembler)
            compiler_output_filename = make_temp_filename("/tmp/XXXXXX.s");
        else
            compiler_output_filename = strdup(output_filename);
    }

    if (run_assembler) {
        if (compile)
            assembler_input_filename = compiler_output_filename;
        else
            assembler_input_filename = input_filename;

        if (run_linker)
            assembler_output_filename = make_temp_filename("/tmp/XXXXXX.o");
        else
            assembler_output_filename = strdup(output_filename);
    }

    init_callee_saved_registers();

    command = malloc(1024);

    if (compile) {
        input = malloc(10 * 1024 * 1024);
        symbol_table = malloc(SYMBOL_TABLE_SIZE);
        memset(symbol_table, 0, SYMBOL_TABLE_SIZE);
        next_symbol = symbol_table;

        string_literals = malloc(MAX_STRING_LITERALS);
        string_literal_count = 0;

        all_structs = malloc(sizeof(struct struct_desc *) * MAX_STRUCTS);
        all_structs_count = 0;

        add_builtin("exit",     IR_EXIT,     TYPE_VOID,            0);
        add_builtin("fopen",    IR_FOPEN,    TYPE_VOID + TYPE_PTR, 0);
        add_builtin("fread",    IR_FREAD,    TYPE_INT,             0);
        add_builtin("fwrite",   IR_FWRITE,   TYPE_INT,             0);
        add_builtin("fclose",   IR_FCLOSE,   TYPE_INT,             0);
        add_builtin("close",    IR_CLOSE,    TYPE_INT,             0);
        add_builtin("stdout",   IR_STDOUT,   TYPE_LONG,            0);
        add_builtin("printf",   IR_PRINTF,   TYPE_INT,             1);
        add_builtin("fprintf",  IR_FPRINTF,  TYPE_INT,             1);
        add_builtin("malloc",   IR_MALLOC,   TYPE_VOID + TYPE_PTR, 0);
        add_builtin("free",     IR_FREE,     TYPE_INT,             0);
        add_builtin("memset",   IR_MEMSET,   TYPE_INT,             0);
        add_builtin("memcmp",   IR_MEMCMP,   TYPE_INT,             0);
        add_builtin("strcmp",   IR_STRCMP,   TYPE_INT,             0);
        add_builtin("strlen",   IR_STRLEN,   TYPE_INT,             0);
        add_builtin("strcpy",   IR_STRCPY,   TYPE_INT,             0);
        add_builtin("strrchr",  IR_STRRCHR,  TYPE_CHAR + TYPE_PTR, 0);
        add_builtin("sprintf",  IR_SPRINTF,  TYPE_INT,             1);
        add_builtin("asprintf", IR_ASPRINTF, TYPE_INT,             1);
        add_builtin("strdup",   IR_STRDUP,   TYPE_CHAR + TYPE_PTR, 0);
        add_builtin("memcpy",   IR_MEMCPY,   TYPE_VOID + TYPE_PTR, 0);
        add_builtin("mkstemps", IR_MKTEMPS,  TYPE_INT,             0);
        add_builtin("perror",   IR_PERROR,   TYPE_VOID,            0);
        add_builtin("system",   IR_SYSTEM,   TYPE_INT,             0);

        f  = fopen(compiler_input_filename, "r");
        if (f == 0) {
            perror(compiler_input_filename);
            exit(1);
        }
        input_size = fread(input, 1, 10 * 1024 * 1024, f);
        if (input_size < 0) {
            printf("Unable to read input file\n");
            exit(1);
        }
        input[input_size] = 0;
        fclose(f);

        cur_line = 1;
        next();

        vs_start = malloc(sizeof(struct value *) * VALUE_STACK_SIZE);
        vs_start += VALUE_STACK_SIZE; // The stack traditionally grows downwards
        label_count = 0;
        total_spilled_register_count;
        parse();
        check_incomplete_structs();

        if (print_symbols) do_print_symbols();

        output_code(compiler_input_filename, compiler_output_filename);

        if (print_spilled_register_count) printf("spilled_register_count=%d\n", total_spilled_register_count);

    }

    if (run_assembler) {
        sprintf(command, "as -64 %s -o %s", assembler_input_filename, assembler_output_filename);
        if (verbose) {
            sprintf(command, "%s %s\n", command, "-v");
            printf("%s\n", command);
        }
        result = system(command);
        if (result != 0) exit(result);
    }

    if (run_linker) {
        if (run_assembler)
            linker_input_filename = assembler_output_filename;
        else
            linker_input_filename = input_filename;

        linker_output_filename = output_filename;

        sprintf(command, "gcc %s -o %s", linker_input_filename, linker_output_filename);
        if (verbose) {
            sprintf(command, "%s %s\n", command, "-v");
            printf("%s\n", command);
        }
        result = system(command);
        if (result != 0) exit(result);
    }

    exit(0);
}
