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
    int function_is_defined;                    // For functions, if a definition has been found
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

struct function_usages {
    int div_or_mod;
    int function_call;
    int binary_shift;
};

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
    MAX_INPUT_FILENAMES        = 1024,
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
    TOK_HASH,
    TOK_INCLUDE,
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

char *cur_filename;             // Current filename being lexed
int cur_line;                   // Current line number being lexed
int parse_struct_base_type(int parse_struct_base_type);

struct value *load_constant(struct value *cv);
void expression(int level);
void finish_parsing_header();

void panic(char *message);
void panic1d(char *fmt, int i);
void panic1s(char *fmt, char *s);
void panic2d(char *fmt, int i1, int i2);
void panic2s(char *fmt, char *s1, char *s2);
