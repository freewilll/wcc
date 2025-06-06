#include <stdio.h>
#include <sys/time.h>

#include "config.h"

#define MAX_CPP_FILESIZE              10 * 1024 * 1024
#define MAX_CPP_INCLUDE_DEPTH         15
#define MAX_CPP_MACRO_PARAM_COUNT     1024
#define MAX_STRUCT_OR_UNION_SCALARS   1024
#define MAX_TYPEDEFS                  1024
#define MAX_STRUCT_MEMBERS            1024
#define MAX_IDENTIFIER_SIZE           1024
#define MAX_INPUT_SIZE                1 << 20 // 1 MB
#define MAX_STRING_LITERAL_SIZE       4095
#define MAX_STRING_LITERALS           20480
#define MAX_FLOATING_POINT_LITERALS   10240
#define VALUE_STACK_SIZE              10240
#define MAX_VREG_COUNT                20480
#define PHYSICAL_REGISTER_COUNT       32 // integer + xmm
#define PHYSICAL_INT_REGISTER_COUNT   12 // Available registers for integers
#define PHYSICAL_SSE_REGISTER_COUNT   14 // Available registers for floating points
#define MAX_SPILLED_REGISTER_COUNT    1024
#define MAX_INPUT_FILENAMES           1024
#define MAX_BLOCKS                    10240
#define MAX_BLOCK_EDGES               1024
#define MAX_STACK_SIZE                10240
#define MAX_BLOCK_PREDECESSOR_COUNT   1024
#define MAX_GRAPH_EDGE_COUNT          10240

typedef struct block {
    struct three_address_code *start, *end;
} Block;

typedef struct graph_node {
    int id;
    struct graph_edge *pred;
    struct graph_edge *succ;
} GraphNode;

typedef struct graph_edge {
    int id;
    GraphNode *from;
    GraphNode *to;
    struct graph_edge *next_pred;
    struct graph_edge *next_succ;
} GraphEdge;

typedef struct graph {
    GraphNode *nodes;
    GraphEdge *edges;
    int node_count;
    int edge_count;
    int max_edge_count;
} Graph;

typedef struct set {
    int max_value;
    char *elements;
    int cached_element_count;
    int *cached_elements;
} Set;

typedef struct stack {
    int *elements;
    int pos;
} Stack;

typedef struct strmap {
    char **keys;
    void **values;
    int size;
    int used_count;
    int element_count;
} StrMap;

typedef struct strmap_iterator {
    StrMap *map;
    int pos;
    int original_size;
} StrMapIterator;

typedef struct strset {
    StrMap *strmap;
} StrSet;

typedef struct longmap {
    long *keys;
    void **values;
    char *status;
    int size;
    int used_count;
    int element_count;
    long (*hashfunc)(long);
} LongMap;

typedef struct longmap_iterator {
    LongMap *map;
    int pos;
    int original_size;
} LongMapIterator;

typedef struct longset {
    LongMap *longmap;
} LongSet;

typedef struct longset_iterator {
    LongMapIterator longmap_iterator;
} LongSetIterator;

typedef struct circular_linked_list {
    void *target;
    struct circular_linked_list *next;
} CircularLinkedList;

// One of preg or stack_index must have a value. (preg != 0) != (stack_index < 0)
typedef struct vreg_location {
    int preg;         // Physical register starting at 0. -1 is unused.
    int stack_index;  // Stack index starting at -1. 0 is unused.
} VregLocation;

typedef struct vreg_i_graph {
    int igraph_id;
    int count;
} VregIGraph;

typedef struct i_graph_node {
    struct three_address_code *tac;
    struct value *value;
} IGraphNode;

typedef struct i_graph {
    IGraphNode *nodes;
    Graph *graph;
    int node_count;
} IGraph;

enum {
    SC_NONE,
    SC_EXTERN,
    SC_STATIC,
    SC_AUTO,
    SC_REGISTER,
};

typedef struct list {
    int length;
    int allocated;
    void **elements;
} List;

typedef struct function_type {
    struct scope *scope;                                // Scope, starting with the parameters
    int param_count;                                    // Number of parameters
    List *param_types;                                  // List of types of parameters
    List *param_identifiers;                            // List of names of parameters
    int is_variadic;                                    // Set to 1 for builtin variadic functions
    int is_paramless;                                   // No parameters are declared, it's an old style K&R function definition
    struct function_param_allocation *return_value_fpa; // function_param_allocaton for the return value if it's a struct or union
} FunctionType;

typedef struct type {
    int type;       // One of TYPE_*
    int array_size;
    unsigned int is_unsigned:1;
    unsigned int is_const:1;
    unsigned int is_volatile:1;
    unsigned int is_restrict:1;
    char storage_class;
    struct type *target;
    struct tag *tag;                                    // For structs, unions and enums
    struct struct_or_union_desc *struct_or_union_desc;
    FunctionType *function;                             // For functions
} Type;

typedef struct type_iterator {
    Type *type;                     // The top level type being iterated
    int index;                      // The index within the type. -1 if the end has been reached
    int offset;                     // Offset at the position of the iterator
    int start_offset;               // Offset when the iterator started on an array or struct/union
    int bit_field_offset;           // Bit field offset if the scalar is a bit field
    int bit_field_size;             // Bit field size if the scalar is a bit field
    struct type_iterator *parent;   // Parent to recurse back to
} TypeIterator;

typedef struct initializer {
    int offset;                 // Offset of the initializer
    int size;                   // Size of the initializer
    void *data;                 // Pointer to the data, or zero if it's a block of zeroes
    int is_string_literal;      //
    int string_literal_index;   // If it's a string literal, an index in string_literals
    int is_address_of;          // When set, the initializer must also be either a string literal or have a symbol
    int address_of_offset;      // Offset when used in combination with is_address_of
    struct symbol *symbol;      // Used together with address of.
} Initializer;

typedef struct symbol {
    Type *type;                 // Type
    struct function *function;  // Set if the symbol is a function
    int size;                   // Size
    char *identifier;           // Identifier
    char *global_identifier;    // Same as identifier for globals, autogenerated for static locals, or otherwise null
    struct scope* scope;        // Scope
    int linkage;                // One of LINKAGE_*
    long value;                 // Value in the case of a constant
    int local_index;            // Used by the parser for locals variables and function arguments
                                // < 0 is a local variable or tempoary, >= 2 is a function parameter
    int is_enum_value;          // Enums are symbols with a value
    List *initializers;         // Set when a global object is initialized;
} Symbol;

typedef struct tag {
    Type *type;                 // Type
    char *identifier;           // Identifier
} Tag;

typedef struct scope {
    struct scope *parent;       // Parent scope, zero if it's the global scope
    StrMap *symbols;            // Symbols
    List *symbol_list;          // List of symbols, in order of declaration/definition
    StrMap *tags;               // Struct, union or enum tags
} Scope;

typedef struct function {
    Type *type;                                         // Type of the function
    int local_symbol_count;                             // Number of local symbols, used by the parser
    int vreg_count;                                     // Number of virtual registers used in IR
    int stack_register_count;                           // Amount of stack space needed for registers spills
    int stack_size;                                     // Size of the stack
    int is_defined;                                     // if a definition has been found
    List *static_symbols;                               // Static symbols
    struct value *return_value_pointer;                 // Set to the register holding the memory return address if the function returns something in memory
    struct function_param_allocation *fpa;              // function_param_allocaton for the params
    struct three_address_code *ir;                      // Intermediate representation
    StrMap *labels;                                     // Map of identifiers to label ids
    CircularLinkedList *goto_backpatches;               // Gotos to labels not yet defined
    struct value *register_save_area;                   // Place where variadic functions store the arguments in registers
    Graph *cfg;                                         // Control flow graph
    Block *blocks;                                      // For functions, the blocks
    Set **dominance;                                    // Block dominances
    LongSet **uevar;                                    // The upward exposed set for each block
    LongSet **varkill;                                  // The killed var set for each block
    LongSet **liveout;                                  // The liveout set for each block
    int *idom;                                          // Immediate dominator for each block
    Set **dominance_frontiers;                          // Dominance frontier for each block
    Set **var_blocks;                                   // Var/block associations for vars that are written to
    Set *globals;                                       // All variables that are assigned to
    Set **phi_functions;                                // All variables that need phi functions for each block
    char *interference_graph;                           // The interference graph of live ranges, in a lower diagonal matrix
    struct vreg_location *vreg_locations;               // Allocated physical registers and spilled stack indexes
    int *spill_cost;                                    // The estimated spill cost for each live range
    char *preferred_live_range_preg_indexes;            // Preferred physical register, when possible
    char *vreg_preg_classes;                            // Preg classes for all vregs
} Function;

// Data of the a single eight byte that's part of a struct or union function parameter or arg
typedef struct function_param_location {
    // Details of the struct/union
    int stru_offset;             // Starting offset in the case of a struct/union
    int stru_size;               // Number of bytes in the 8-byte in the case of a struct/union
    int stru_member_count;       // Amount of members in the 8-byte in the case of a struct/union

    // Details of where the function param/arg goes, either in a register or the stack
    // One of int_register/sse_register/stack_offset is not -1.
    int int_register;       // If not -1, an int register
    int sse_register;       // If not -1, an sse register
    int stack_offset;       // If not -1 the stack offset
    int stack_padding;      // If not -1 the stack padding
} FunctionParamLocation;

typedef struct function_param_locations {
    int count;
    FunctionParamLocation *locations;
} FunctionParamLocations;

typedef struct function_param_allocation {
    int single_int_register_arg_count;
    int single_sse_register_arg_count;
    int biggest_alignment;
    int offset;
    int padding;
    int size;
    List *param_locations;
} FunctionParamAllocation;

// Physical register class
enum {
    PC_INT = 1,
    PC_SSE = 2,
};

// Value is a value on the value stack. A value can be one of
// - global
// - local
// - constant
// - string literal
// - register
typedef struct value {
    Type *type;                                          // Type
    int vreg;                                            // Optional vreg number
    int preg;                                            // Allocated physical register
    char preg_class;                                     // Class of physical register, PC_INT or PC_SSE
    unsigned int is_lvalue:1;                            // Is the value an lvalue?
    unsigned int is_lvalue_in_register:1;                // Is the value an lvalue in a register?
    unsigned int spilled:1;                              // 1 if spilled
    unsigned int is_constant:1;                          // Is it a constant? If so, value is the value.
    unsigned int is_string_literal:1;                    // Is the value a string literal?
    unsigned int has_been_renamed:1;                     // Used in renaming and stack renumbering code
    unsigned int is_address_of:1;                        // Is an address of a constant expression.
    unsigned int has_struct_or_union_return_value:1;     // Is it a function call that returns a struct/union?
    unsigned int load_from_got:1;                        // Load from Global Offset Table (GOT)
    int local_index;                                     // Used by parser for local variable and temporaries and function arguments
    int stack_index;                                     // stack index in case of a pushed function argument, & usage or register spill
                                                         // < 0 is for locals, temporaries and spills. >= 2 is for pushed arguments, that start at 2.
    int stack_offset;                                    // Position on the stack
    int string_literal_index;                            // Index in the string_literals array in the case of a string literal
    long int_value;                                      // Value in the case of an integer constant
    long double fp_value;                                // Value in the case of a floating point constant
    int offset;                                          // For composite objects, offset from the start of the object's memory
    int bit_field_offset;                                // Offset in bits for bit fields
    int bit_field_size;                                  // Size in bits for bit fields
    int is_overflow_arg_area_address;                    // Set to indicate this value must point to the saved register save overflow for variadic functions
    Symbol *function_symbol;                             // Corresponding symbol in the case of a function call
    int address_of_offset;                               // Offset when used in combination with is_address_of
    int live_range_preg;                                 // This value is bound to a physical register
    int function_param_original_stack_index;             // Original stack index for function parameter pushed onto the stack
    int function_call_arg_index;                         // Index of the argument (0=leftmost)
    FunctionParamLocations *function_call_arg_locations; // Destination of the arg, either a single int or sse register, or in the case of a struct, a list of locations
    int function_call_sse_register_arg_index;            // Index of the argument in integer registers going left to right (0=leftmost). Set to -1 if it's on the stack.
    int function_call_arg_stack_padding;                 // Extra initial padding needed to align the function call argument pushed arguments
    int function_call_arg_push_count;                    // Number of arguments pushed on the stack
    int function_call_sse_register_arg_count;            // Number of SSE (xmm) arguments in registers
    Set *return_value_live_ranges;                       // Live ranges for registers that are part of the function return values
    Symbol *global_symbol;                               // Pointer to a global symbol if the value is a global symbol
    int label;                                           // Target label in the case of jump instructions
    int ssa_subscript;                                   // Optional SSA enumeration
    int live_range;                                      // Optional SSA live range
    char preferred_live_range_preg_index;                // Preferred physical register
    int x86_size;                                        // Current size while generating x86 code
    int non_terminal;                                    // Used in rule matching
} Value;

typedef struct origin {
    char *filename;
    int line_number;
} Origin;

typedef struct three_address_code {
    int index;                          // Index in a tac chain
    int operation;                      // IR_* operation
    int label;                          // Label if this instruction is jumped to
    Value *dst;                         // Destination
    Value *src1;                        // First rhs operand
    Value *src2;                        // Second rhs operand
    Value *phi_values;                  // For phi functions, a null terminated array of values for the args
    struct three_address_code *next;    // Next in a linked-list
    struct three_address_code *prev;    // Previous in a linked-list
    char *x86_template;                 // Template for rendering x86 instruction
    Origin *origin;                     // Filename and line numberwhere the tac was created
} Tac;

// Struct/union member
typedef struct struct_or_union_member {
    char *identifier;
    Type *type;
    int offset;
    int is_bit_field;
    int bit_field_size;
    int bit_field_offset;
} StructOrUnionMember;

// Struct & union description
typedef struct struct_or_union_desc {
    int size;
    int is_incomplete;          // Set to 1 if the struct has been used in a member but not yet declared
    int is_packed;
    int is_union;
    struct struct_or_union_member **members;
} StructOrUnion;

typedef struct typedef_desc {
    char *identifier;
    Type *type;
} Typedef;

typedef struct register_set {
    int *int_registers;
    int *sse_registers;
} RegisterSet;

// Tokens in order of precedence
enum {
    TOK_EOF=1,
    TOK_IDENTIFIER,
    TOK_INTEGER,
    TOK_FLOATING_POINT_NUMBER,
    TOK_STRING_LITERAL,
    TOK_IF,
    TOK_ELSE,
    TOK_SIGNED,
    TOK_UNSIGNED,
    TOK_INLINE,             // 10
    TOK_VOID,
    TOK_CHAR,
    TOK_INT,
    TOK_SHORT,
    TOK_LONG,
    TOK_FLOAT,
    TOK_DOUBLE,
    TOK_STRUCT,
    TOK_UNION,
    TOK_TYPEDEF,            // 20
    TOK_TYPEDEF_TYPE,
    TOK_DO,
    TOK_WHILE,
    TOK_FOR,
    TOK_SWITCH,
    TOK_CASE,
    TOK_DEFAULT,
    TOK_CONTINUE,
    TOK_BREAK,
    TOK_RETURN,             // 30
    TOK_ENUM,
    TOK_SIZEOF,
    TOK_RCURLY,
    TOK_LCURLY,
    TOK_SEMI,
    TOK_COMMA,
    TOK_EQ,
    TOK_PLUS_EQ,
    TOK_MINUS_EQ,
    TOK_MULTIPLY_EQ,        // 40
    TOK_DIVIDE_EQ,
    TOK_MOD_EQ,
    TOK_BITWISE_AND_EQ,
    TOK_BITWISE_OR_EQ,
    TOK_BITWISE_XOR_EQ,
    TOK_BITWISE_RIGHT_EQ,
    TOK_BITWISE_LEFT_EQ,
    TOK_TERNARY,
    TOK_COLON,
    TOK_OR,                 // 50
    TOK_AND,
    TOK_BITWISE_OR,
    TOK_XOR,
    TOK_AMPERSAND,
    TOK_DBL_EQ,
    TOK_NOT_EQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,                 // 60
    TOK_BITWISE_LEFT,
    TOK_BITWISE_RIGHT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_MOD,
    TOK_LOGICAL_NOT,
    TOK_BITWISE_NOT,
    TOK_INC,                // 70
    TOK_DEC,
    TOK_DOT,
    TOK_ARROW,
    TOK_RBRACKET,
    TOK_LBRACKET,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_ATTRIBUTE,
    TOK_PACKED,
    TOK_ALIGNED,            // 80
    TOK_HASH,
    TOK_AUTO,
    TOK_REGISTER,
    TOK_STATIC,
    TOK_EXTERN,
    TOK_CONST,
    TOK_VOLATILE,
    TOK_RESTRICT,
    TOK_ELLIPSES,
    TOK_GOTO,               // 90
    TOK_ASM,
};

enum {
    TYPE_VOID            = 1,
    TYPE_CHAR            = 2,
    TYPE_SHORT           = 3,
    TYPE_INT             = 4,
    TYPE_LONG            = 5,
    TYPE_FLOAT           = 6,
    TYPE_DOUBLE          = 7,
    TYPE_LONG_DOUBLE     = 8,
    TYPE_PTR             = 9,
    TYPE_ARRAY           = 10,
    TYPE_STRUCT_OR_UNION = 11,
    TYPE_ENUM            = 12,
    TYPE_FUNCTION        = 13,
    TYPE_TYPEDEF         = 14
};

enum {
    LINKAGE_NONE = 1,
    LINKAGE_INTERNAL,
    LINKAGE_IMPLICIT_EXTERNAL,
    // Used for a) static local symbols that need to be global, but with internal
    // linkage and b) global symbols with extern symbol This mimicks what gcc does.
    // LINKAGE_UNDECLARED_EXTERNAL is equivalent to LINKAGE_IMPLICIT_EXTERNAL in codegen, the
    // difference is only used when reporting errors in the parser.
    LINKAGE_EXPLICIT_EXTERNAL,
};

// Intermediate representation operations
enum {
    IR_MOVE=1,                // Moving of constants, string literals, variables, or registers
    IR_MOVE_TO_PTR,           // Assignment to a pointer target
    IR_MOVE_PREG_CLASS,       // Move int <-> sse without conversion
    IR_MOVE_STACK_PTR,        // Move RSP -> register
    IR_ADDRESS_OF,            // &
    IR_INDIRECT,              // Pointer or lvalue dereference
    IR_DECL_LOCAL_COMP_OBJ,   // Declare a local compound object
    IR_LOAD_BIT_FIELD,        // Load a bit field into a register
    IR_SAVE_BIT_FIELD,        // Save a bit field into a register
    IR_LOAD_FROM_GOT,         // Load object from global offset table (for PIC)
    IR_ADDRESS_OF_FROM_GOT,   // & an object from global offset table (for PIC)
    IR_START_CALL,            // Function call
    IR_ARG,                   // Function call argument
    IR_ARG_STACK_PADDING,     // Extra padding push to align arguments pushed onto the stack
    IR_CALL_ARG_REG,          // Placeholder for fake read/write of a physical register to keep a live range alive
    IR_CALL,                  // Start of function call
    IR_END_CALL,              // End of function call
    IR_VA_START,              // va_start function call
    IR_VA_ARG,                // va_arg function call
    IR_ALLOCATE_STACK,        // Allocate stack space for a function call argument on the stack
    IR_RETURN,                // Return in function
    IR_ZERO,                  // Zero a block of memory
    IR_LOAD_LONG_DOUBLE,      // Load a long double into the top of the x87 stack
    IR_START_LOOP,            // Start of a for or while loop
    IR_END_LOOP,              // End of a for or while loop
    IR_NOP,                   // No operation. Used for label destinations. No code is generated for this other than the label itself.
    IR_JMP,                   // Unconditional jump
    IR_JZ,                    // Jump if zero
    IR_JNZ,                   // Jump if not zero
    IR_ADD,                   // +
    IR_SUB,                   // -
    IR_RSUB,                  // reverse -, used to facilitate code generation for the x86 SUB instruction
    IR_MUL,                   // *
    IR_DIV,                   // /
    IR_MOD,                   // %
    IR_EQ,                    // ==
    IR_NE,                    // !=
    IR_LOR,                   // Logical or |
    IR_LAND,                  // Logical and &
    IR_LNOT,                  // Logical not
    IR_BNOT,                  // Binary not ~
    IR_BOR,                   // Binary or |
    IR_BAND,                  // Binary and &
    IR_XOR,                   // Binary xor ^
    IR_BSHL,                  // Binary shift left <<
    IR_BSHR,                  // Binary shift right >>
    IR_ASHR,                  // Arithmetic shift right >>
    IR_LT,                    // <
    IR_GT,                    // >
    IR_LE,                    // <=
    IR_GE,                    // >=
    IR_PHI_FUNCTION,          // SSA phi function
};

// Physical registers
enum {
    // Integers
    REG_RAX,
    REG_RBX,
    REG_RCX,
    REG_RDX,
    REG_RSI,
    REG_RDI,
    REG_RBP,
    REG_RSP,
    REG_R08,
    REG_R09,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,

    // SSE
    REG_XMM00,
    REG_XMM14 = 30,
    REG_XMM15,
};

enum {
    OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX = 256
};

typedef struct string_literal {
    char *data;
    int size;
    int is_wide_char;
} StringLiteral;

enum {
    CPP_TOK_EOL=1,
    CPP_TOK_EOF,
    CPP_TOK_IDENTIFIER,
    CPP_TOK_STRING_LITERAL,
    CPP_TOK_HCHAR_STRING_LITERAL,
    CPP_TOK_NUMBER,
    CPP_TOK_HASH,
    CPP_TOK_PASTE,
    CPP_TOK_INCLUDE,
    CPP_TOK_DEFINE,
    CPP_TOK_UNDEF,
    CPP_TOK_IF,
    CPP_TOK_IFDEF,
    CPP_TOK_IFNDEF,
    CPP_TOK_ELIF,
    CPP_TOK_ELSE,
    CPP_TOK_ENDIF,
    CPP_TOK_LINE,
    CPP_TOK_DEFINED,
    CPP_TOK_WARNING,
    CPP_TOK_ERROR,
    CPP_TOK_PRAGMA,
    CPP_TOK_LPAREN,
    CPP_TOK_RPAREN,
    CPP_TOK_COMMA,
    CPP_TOK_INC,
    CPP_TOK_DEC,
    CPP_TOK_OTHER,
};

typedef struct cpp_token {
    int kind;               // One of CPP_TOK*
    char *whitespace;       // Preceding whitespace
    char *str;              // The token text
    int line_number;
    int line_number_offset; // Amended line number due to #line
    StrSet *hide_set;       // Hide set, when expanding macros
    struct cpp_token *next;
} CppToken;

typedef CppToken *(*DirectiveRenderer)(CppToken *);

typedef struct directive {
    char is_function;           // Is the macro an object or function macro
    int param_count;            // Amount of parameters.
    StrMap *param_identifiers;  // Mapping of parameter identifiers => index, index starts at 1
    CppToken *tokens;           // Replacement tokens
    DirectiveRenderer renderer; // Renderer for builtin directives
} Directive;

// Structure with all directives passed on the command line with -D
typedef struct cli_directive {
    char *identifier;               // Identifier of the directive
    Directive *directive;           // The actual directive
    struct cli_directive *next;
} CliDirective;

// Structure with all include paths passed on the command line with -I
typedef struct cli_include_path {
    char *path;
    struct cli_include_path *next;
} CliIncludePath;

// Structure with all include paths passed on the command line with -L
typedef struct cli_library_path {
    char *path;
    struct cli_library_path *next;
} CliLibraryPath;

// Structure with all include libraries passed on the command line with -l
typedef struct cli_library {
    char *library;
    struct cli_library *next;
} CliLibrary;

typedef enum {
    CP_PREPROCESSING,
    CP_PARSING,
    CP_POST_PARSING,
} CompilePhase;

char *cur_filename;             // Current filename being lexed
int cur_line;                   // Current line number being lexed

int print_ir1;                          // Print IR after parsing
int print_ir2;                          // Print IR after register allocation
int log_compiler_phase_durations;       // Output logs of how long each compiler phase lasts
int opt_PIC;                            // Make position independent code
int opt_debug_symbols;                  // Add debug symbols
int opt_enable_vreg_renumbering;        // Renumber vregs so the numbers are consecutive
int opt_enable_register_coalescing;     // Merge registers that can be reused within the same operation
int opt_enable_live_range_coalescing;   // Merge live ranges where possible
int opt_spill_furthest_liveness_end;    // Prioritize spilling physical registers with furthest liveness end
int opt_short_lr_infinite_spill_costs;  // Don't spill short live ranges
int opt_optimize_arithmetic_operations; // Optimize arithmetic operations
int opt_enable_preferred_pregs;         // Enable preferred preg selection in register allocator
int opt_enable_trigraphs;               // Enable trigraph preprocessing
int opt_warnings_are_errors;            // Treat all warnings as errors

CliDirective *cli_directives;      // Linked list of directives passed on the command line with -D
CliIncludePath *cli_include_paths; // Linked list of include paths passed on the command line with -I
CliLibraryPath *cli_library_paths; // Linked list of library paths passed on the command line with -L
CliLibrary *cli_libraries;         // Linked list of libraries passed on the command line with -l
StrMap *directives;                // Map of CPP directives
CompilePhase compile_phase;        // One of CP_*
int cur_token;                     // Current token
char *cur_identifier;              // Current identifier if the token is an identifier
char *cur_type_identifier;         // Identifier of the last parsed declarator
Type *cur_lexer_type;              // A type determined by the lexer
long cur_long;                     // Current long if the token is an integral type
long double cur_long_double;       // Current long double if the token is a floating point type
StringLiteral cur_string_literal;  // Current string literal if the token is a string literal
Scope *cur_scope;                  // Current scope.
StringLiteral *string_literals;    // Each string literal has an index in this array, with a pointer to the string literal struct
int string_literal_count;          // Amount of string literals

Symbol *cur_function_symbol;     // Currently parsed function
Value *cur_loop_continue_dst;    // Target jmp of continue statement in the current for/while loop
Value *cur_loop_break_dst;       // Target jmp of break statement in the current for/while loop

Typedef **all_typedefs;   // All typedefs
int all_typedefs_count;   // Number of typedefs

Tac *ir_start, *ir;               // intermediate representation for currently parsed function
int label_count;                  // Global label count, always growing
int cur_loop;                     // Current loop being parsed
int loop_count;                   // Loop counter
int stack_register_count;         // Spilled register count for current function that's undergoing register allocation
int total_stack_register_count;   // Spilled register count for all functions
int cur_stack_push_count;         // Used in codegen to keep track of stack position

int callee_saved_registers[PHYSICAL_REGISTER_COUNT + 1]; // Set to 1 for registers that must be preserved in function calls.
int int_arg_registers[6];
int sse_arg_registers[8];

FILE *f; // Output file handle

int warn_integer_constant_too_large;
int warn_assignment_types_incompatible;
int debug_function_param_allocation;
int debug_function_arg_mapping;
int debug_function_param_mapping;
int debug_ssa_mapping_local_stack_indexes;
int debug_ssa;
int debug_ssa_liveout;
int debug_ssa_cfg;
int debug_ssa_dominance;
int debug_ssa_idom;
int debug_ssa_dominance_frontiers;
int debug_ssa_phi_insertion;
int debug_ssa_phi_renumbering;
int debug_ssa_live_range;
int debug_ssa_vreg_renumbering;
int debug_ssa_interference_graph;
int debug_ssa_live_range_coalescing;
int debug_ssa_spill_cost;
int debug_register_allocation;
int debug_graph_coloring;
int debug_instsel_tree_merging;
int debug_instsel_tree_merging_deep;
int debug_instsel_igraph_simplification;
int debug_instsel_tiling;
int debug_instsel_cost_graph;
int debug_instsel_spilling;
int debug_stack_frame_layout;
int debug_exit_after_parser;
int debug_dont_compile_internals;

#define free_and_null(object) do { wfree(object); object = NULL; } while(0);

// set.c
#define add_to_set(s, value) do { \
    if (value > s->max_value) panic("Max set value of %d exceeded with %d in add_to_set", s->max_value, value); \
    s->elements[value] = 1; \
} while (0)

Set *new_set(int max_value);
void free_set(Set *s);
void empty_set(Set *s);
Set *copy_set(Set *s);
void copy_set_to(Set *dst, Set *src);
void cache_set_elements(Set *s);
int set_len(Set *s);
void print_set(Set *s);

int in_set(Set *s, int value);
int set_eq(Set *s1, Set *s2);
void *delete_from_set(Set *s, int value);
Set *set_intersection(Set *s1, Set *s2);
void set_intersection_to(Set *dst, Set *s1, Set *s2);
Set *set_union(Set *s1, Set *s2);
void set_union_to(Set *dst, Set *s1, Set *s2);
Set *set_difference(Set *s1, Set *s2);
void set_difference_to(Set *dst, Set *s1, Set *s2);

// stack.c
Stack *new_stack(void);
void free_stack(Stack *s);
int stack_top(Stack *s);
void push_onto_stack(Stack *s, int v);
int pop_from_stack(Stack *s);

// strmap.c
StrMap *new_strmap(void);
void free_strmap(StrMap *map);
void *strmap_get(StrMap *strmap, char *key);
void strmap_put(StrMap *strmap, char *key, void *value);
void strmap_delete(StrMap *strmap, char *key);
StrMapIterator strmap_iterator(StrMap *map);
int strmap_iterator_finished(StrMapIterator *iterator);
void strmap_iterator_next(StrMapIterator *iterator);
char *strmap_iterator_key(StrMapIterator *iterator);
#define strmap_foreach(ls, it) for (StrMapIterator it = strmap_iterator(ls); !strmap_iterator_finished(&it); strmap_iterator_next(&it))

// strset.c
StrSet *new_strset(void);
void free_strset(StrSet *ss);
void strset_add(StrSet *ss, char *element);
int strset_in(StrSet *ss, char *element);
StrSet *dup_strset(StrSet *ss);
StrSet *strset_union(StrSet *ss1, StrSet *ss2);
StrSet *strset_intersection(StrSet *ss1, StrSet *ss2);
void print_strset(StrSet *s);

// longset.c
LongSet *new_longset(void);
void free_longset(LongSet *ss);
void longset_add(LongSet *ss, long element);
void longset_delete(LongSet *ls, long element);
int longset_in(LongSet *ss, long element);
void longset_empty(LongSet *ls);
LongSet *longset_copy(LongSet *ls);
int longset_eq(LongSet *ss1, LongSet *ss2);
int longset_len(LongSet *ls);
LongSet *longset_union(LongSet *ss1, LongSet *ss2);
LongSet *longset_intersection(LongSet *ss1, LongSet *ss2);
#define longset_iterator_finished(iterator) longmap_iterator_finished(&(iterator)->longmap_iterator)
void longset_iterator_next(LongSetIterator *iterator);
#define longset_iterator_element(iterator) longmap_iterator_key(&(iterator)->longmap_iterator)
LongSetIterator longset_iterator(LongSet *ls);
void print_longset(LongSet *s);
#define longset_foreach(ls, it) for (LongSetIterator it = longset_iterator(ls); !longset_iterator_finished(&it); longset_iterator_next(&it))

// longmap.c
LongMap *new_longmap(void);
void free_longmap(LongMap *map);
void *longmap_get(LongMap *longmap, long key);
void longmap_put(LongMap *longmap, long key, void *value);
void longmap_delete(LongMap *longmap, long key);
int longmap_keys(LongMap *map, int **keys);
void longmap_empty(LongMap *map);
LongMap *longmap_copy(LongMap *map);
LongMapIterator longmap_iterator(LongMap *map);
#define longmap_iterator_finished(iterator) ((iterator)->pos == -1)
void longmap_iterator_next(LongMapIterator *iterator);
#define longmap_iterator_key(iterator) (iterator)->map->keys[(iterator)->pos]
#define longmap_foreach(map, it) for (LongMapIterator it = longmap_iterator(map); !longmap_iterator_finished(&it); longmap_iterator_next(&it))

// list.c
// Append to circular linked list of allocated tokens
#define append_to_cll(cll, new_target) \
    do { \
        if (cll) { \
            CircularLinkedList *next = wmalloc(sizeof(CircularLinkedList)); \
            next->target = new_target; \
            next->next = cll->next; \
            cll->next = next; \
            cll = next; \
        } \
        else { \
            cll = wmalloc(sizeof(CircularLinkedList)); \
            cll->target = new_target; \
            cll->next = cll; \
        } \
    } while(0)

void free_circular_linked_list(CircularLinkedList *cll);

List *new_list(int length);
void free_list(List *l);
void append_to_list(List *l, void *element);

// graph.c
Graph *new_graph(int node_count, int edge_count);
void free_graph(Graph *g);
void dump_graph(Graph *g);
GraphEdge *add_graph_edge(Graph *g, int from, int to);

// utils.c
// A struct for appending strings to a buffer without knowing the size beforehand
typedef struct string_buffer {
    char *data;
    int position;
    int allocated;
} StringBuffer;

void panic(char *format, ...);

char *base_path(char *path);
int wasprintf(char **ret, const char *format, ...);
StringBuffer *new_string_buffer(int initial_size);
void free_string_buffer(StringBuffer *sb, int free_data);
void append_to_string_buffer(StringBuffer *sb, char *str);
void terminate_string_buffer(StringBuffer *sb);

int set_debug_logging_start_time();
int debug_log(char *format, ...);


// memory.c
int print_heap_usage;
int fail_on_leaked_memory;

void *wmalloc(size_t size);
void *wrealloc(void *ptr, size_t size);
void *wcalloc(size_t nitems, size_t size);
void *wcalloc(size_t nitems, size_t size);
char *wstrdup(const char *str);
void wfree(void *ptr);
void process_memory_allocation_stats(void);

// error.c
void panic_with_line_number(char *format, ...);
void simple_error(char *format, ...);
void error(char *format, ...);
void warning(char *format, ...);

// lexer.c
void init_lexer_from_filename(char *filename);
void init_lexer_from_string(char *string);
void free_lexer(void);
void next(void);
void rewind_lexer(void);
void expect(int token, char *what);
void consume(int token, char *what);

// cpp.c
typedef struct line_map {
    int position;
    int line_number;
    struct line_map *next;
} LineMap;

void init_cpp(void);
void get_cpp_filename_and_line();
void init_cpp_from_string(char *string);
char *get_cpp_input(void);
void init_directives(void);
void free_directives(void);
LineMap *get_cpp_linemap(void);
void transform_trigraphs(void);
void strip_backslash_newlines(void);
Directive *parse_cli_define(char *string);
char *preprocess(char *filename, List *cli_directive_strings);
void preprocess_to_file(char *input_filename, char *output_filename, List *cli_directive_strings);
void free_cpp_allocated_garbage();

// parser.c
typedef Value *parse_expression_function_type(int);

Value *make_string_literal_value_from_cur_string_literal(void);
Value *new_label_dst(void);
Symbol *memcpy_symbol;
Symbol *memset_symbol;
int cur_token_is_type(void);
Type *operation_type(Value *src1, Value *src2, int for_ternary);
void check_unary_operation_type(int operation, Value *value);
void check_binary_operation_types(int operation, Value *src1, Value *src2);
void check_plus_operation_type(Value *src1, Value *src2, int for_array_subscript);
void check_minus_operation_type(Value *src1, Value *src2);
void check_ternary_operation_types(Value *switcher, Value *src1, Value *src2);
Value *load_constant(Value *cv);
Type *parse_type_name(void);
void parse_typedef(void);
Type *find_struct_or_union(char *identifier, int is_union, int recurse);
StructOrUnionMember *lookup_struct_or_union_member(Type *type, char *identifier);
Value *make_symbol_value(Symbol *symbol);
int parse_sizeof(parse_expression_function_type expr);
void parse(void);
void dump_symbols(void);
void init_parser(void);
void free_parser(void);

// constexpr.c
Value* evaluate_const_unary_int_operation(int operation, Value *value);
Value* evaluate_const_binary_int_operation(int operation, Value *src1, Value *src2);
Value* evaluate_const_binary_fp_operation(int operation, Value *src1, Value *src2, Type *type);
Value *cast_constant_value(Value *src, Type *type);
Value *parse_constant_expression(int level);
Value *parse_constant_integer_expression(int all_longs);

// scopes.c
Scope *global_scope;

void init_scopes(void);
void free_scopes(void);
void enter_scope(void);
void exit_scope(void);
Symbol *new_symbol(char *identifier);
Symbol *lookup_symbol(char *name, Scope *scope, int recurse);
Tag *new_tag(char *identifier);
Tag *lookup_tag(char *name, Scope *scope, int recurse);

// types.c
int print_type(void *f, Type *type);
char *sprint_type_in_english(Type *type);
void print_type_in_english(Type *type);
void init_type_allocations(void);
void free_types(void);
Type *new_type(int type);
Type *dup_type(Type *src);
Type *new_struct_or_union(char *tag_identifier);
StructOrUnionMember *new_struct_member(void);
Type *integer_promote_type(Type *type);
Type *make_pointer(Type *src);
Type *make_pointer_to_void(void);
Type *deref_pointer(Type *type);
Type *make_array(Type *src, int size);
Type *decay_array_to_pointer(Type *src);
int is_integer_type(Type *type);
int is_floating_point_type(Type *type);
int is_sse_floating_point_type(Type *type);
int is_arithmetic_type(Type *type);
int is_scalar_type(Type *type);
int is_object_type(Type *type);
int is_incomplete_type(Type *type);
int is_pointer_type(Type *type);
int is_pointer_or_array_type(Type *type);
int is_pointer_to_object_type(Type *type);
int is_pointer_to_function_type(Type *type);
int is_null_pointer(Value *v);
int is_pointer_to_void(Type *type);
int type_fits_in_single_int_register(Type *type);
int get_type_size(Type *type);
int get_type_alignment(Type *type);
int type_eq(Type *type1, Type *type2);
Type *apply_default_function_call_argument_promotions(Type *type);
int types_are_compatible(Type *type1, Type *type2);
int types_are_compatible_ignore_qualifiers(Type *type1, Type *type2);
Type *composite_type(Type *type1, Type *type2);
Type *ternary_pointer_composite_type(Type *type1, Type *type2);
int is_integer_operation_result_unsigned(Type *src1, Type *src2);
Type *make_struct_or_union_type(StructOrUnion *s);
void complete_struct_or_union(StructOrUnion *s);
int type_is_modifiable(Type *type);
TypeIterator *type_iterator(Type *type);
int type_iterator_done(TypeIterator *it);
TypeIterator *type_iterator_next(TypeIterator *it);
TypeIterator *type_iterator_dig(TypeIterator *it);
TypeIterator *type_iterator_dig_for_string_literal(TypeIterator *it);
TypeIterator *type_iterator_descend(TypeIterator *it);

// ir.c
List *allocated_values;

void init_ir(void);
void free_ir(void);

void init_value_allocations(void) ;
void free_values(void);
void init_value(Value *v);
Value *new_value(void);
Value *new_integral_constant(int type_type, long value);
Value *new_floating_point_constant(int type_type, long double value);
Value *dup_value(Value *src);
void add_tac_to_ir(Tac *tac);
Tac *new_instruction(int operation);
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2);
void insert_instruction(Tac *ir, Tac *tac, int move_label);
Tac *insert_instruction_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, int move_label);
Tac *insert_instruction_after(Tac *ir, Tac *tac);
Tac *insert_instruction_after_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2);
Tac *delete_instruction(Tac *tac);
void sanity_test_ir_linkage(Function *function);
int make_function_call_count(Function *function);
int print_value(void *f, Value *v, int is_assignment_rhs);
void print_instruction(void *f, Tac *tac, int expect_preg);
void print_ir(Function *function, char* name, int expect_preg);
void reverse_function_argument_order(Function *function);
void merge_consecutive_labels(Function *function);
void renumber_labels(Function *function);
void allocate_value_vregs(Function *function);
void convert_long_doubles_jz_and_jnz(Function *function);
void add_zero_memory_instructions(Function *function);
void move_long_doubles_to_the_stack(Function *function);
void make_stack_register_count(Function *function);
void allocate_value_stack_indexes(Function *function);
void determine_bit_field_params(Value *v, int *offset, int *bit_offset, int *bit_size);
void process_bit_fields(Function *function);
void remove_unused_function_call_results(Function *function);
void process_struct_and_union_copies(Function *function);
Tac *add_memory_copy_with_registers(Function *function, Tac *ir, Value *dst, Value *src1, int size, int splay_offsets);
Tac *add_memory_copy(Function *function, Tac *ir, Value *dst, Value *src1, int size);
void convert_enums(Function *function);
void add_PIC_load_and_saves(Function *function);
void convert_functions_address_of(Function *function);

// ssa.c
enum {
    // Liveness interval indexes corresponding to reserved physical registers
    LIVE_RANGE_PREG_RAX_INDEX = 1,
    LIVE_RANGE_PREG_RBX_INDEX,
    LIVE_RANGE_PREG_RCX_INDEX,
    LIVE_RANGE_PREG_RDX_INDEX,
    LIVE_RANGE_PREG_RSI_INDEX,
    LIVE_RANGE_PREG_RDI_INDEX,
    LIVE_RANGE_PREG_R08_INDEX,
    LIVE_RANGE_PREG_R09_INDEX,
    LIVE_RANGE_PREG_R12_INDEX,
    LIVE_RANGE_PREG_R13_INDEX,
    LIVE_RANGE_PREG_R14_INDEX,
    LIVE_RANGE_PREG_R15_INDEX,      // 12

    LIVE_RANGE_PREG_XMM00_INDEX,    // 13
    LIVE_RANGE_PREG_XMM01_INDEX,
};

int live_range_reserved_pregs_offset;

// Interference graph indexing for a lower triangular matrix with size vreg_count
#define ig_lookup(ig, vreg_count, i1, i2) ig[i1 > i2 ? i2 * vreg_count + i1 : i1 * vreg_count + i2]

#define add_ig_edge(ig, vreg_count, to, from) \
    do { \
        if (debug_ssa_interference_graph) printf("Adding edge %d <-> %d\n", (int) to, (int) from); \
        ig_lookup(ig, vreg_count, to, from) = 1; \
    } while(0)

void optimize_arithmetic_operations(Function *function);
void rewrite_lvalue_reg_assignments(Function *function);
void make_control_flow_graph(Function *function);
void make_block_dominance(Function *function);
void analyze_dominance(Function *function);
void free_dominance(Function *function);
int make_vreg_count(Function *function, int starting_count);
void make_uevar_and_varkill(Function *function);
void free_uevar_and_varkill(Function *function);
void make_liveout(Function *function);
void free_liveout(Function *function);
void make_globals_and_var_blocks(Function *function);
void free_globals_and_var_blocks(Function *function);
void insert_phi_functions(Function *function);
void free_phi_functions(Function *function);
void rename_phi_function_variables(Function *function);
void make_live_ranges(Function *function);
void free_live_range_spill_cost(Function *function);
void free_vreg_preg_classes(Function *function);
void blast_vregs_with_live_ranges(Function *function);
void make_interference_graph(Function *function, int include_clobbers, int include_instrsel_constraints);
void free_interference_graph(Function *function);
void coalesce_live_ranges(Function *function, int check_register_constraints);
void make_preferred_live_range_preg_indexes(Function *function);
void free_preferred_live_range_preg_indexes(Function *function);

// functions.c
void init_function_allocations(void);
void free_function(Function *function, int remove_from_allocations);
void free_functions(void);
Function *new_function(void);
void add_function_call_result_moves(Function *function);
void add_function_return_moves(Function *function, char *identifier);
void add_function_call_arg_moves(Function *function);
void process_function_varargs(Function *function);
void add_function_param_moves(Function *function, char *identifier);
Value *make_function_call_value(int function_call);
FunctionParamAllocation *init_function_param_allocaton(char *function_identifier);
void free_function_param_allocaton(FunctionParamAllocation *fpa);
void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type);
void finalize_function_param_allocation(FunctionParamAllocation *fpa);

RegisterSet arg_register_set;
RegisterSet function_return_value_register_set;

void compress_vregs(Function *function);
void init_vreg_locations(Function *function);
void free_vreg_locations(Function *function);
void allocate_registers_top_down(Function *function, int live_range_start, int physical_register_count, int preg_class);
void allocate_registers(Function *function);
void init_allocate_registers(void);

// instrsel.c
enum {
    MAX_RULE_COUNT = 9000,

    // Non terminals
    STL = 1,                     // String literal
    LAB,                         // Label, i.e. a target for a (conditional) jump
    FUN,                         // Function, used for calls
    CSTV1,                       // Constant with value 1
    CSTV2,                       // Constant with value 2
    CSTV3,                       // Constant with value 3
    CI1, CI2, CI3, CI4,          // Constants
    CU1, CU2, CU3, CU4,          // Constants
    CLD,                         // Long double constant
    CS3, CS4,                    // SSE floating point constants
    RI1, RI2, RI3, RI4,          // Signed registers
    RU1, RU2, RU3, RU4,          // Unsigned registers
    RS3, RS4,                    // SSE (xmm) registers
    MI1, MI2, MI3, MI4,          // Memory, in stack or globals
    MU1, MU2, MU3, MU4,          // Memory, in stack or globals
    MS3, MS4,                    // SSE (xmm) in stack or globals
    RP1, RP2, RP3, RP4, RP5,     // Address (aka pointer) in a register
    RPF,                         // Pointer to a function in a register
    MPF,                         // Pointer to a function in memory
    MLD5,                        // 16-byte memory, for long double
    MPV,                         // Pointer in memory
    MSA,                         // Struct or array in memory

    AUTO_NON_TERMINAL_START,
    AUTO_NON_TERMINAL_END = 0x200, // Must match next line

    EXP_SIZE     = 0x00200,
    EXP_SIGN     = 0x00400,
    EXP_SIZE_1   = 0x00800,
    EXP_SIZE_2   = 0x01000,
    EXP_SIZE_3   = 0x01800,
    EXP_SIZE_4   = 0x02000,
    EXP_C        = 0x04000,
    EXP_R        = 0x08000,
    EXP_M        = 0x0c000,
    EXP_RP       = 0x10000,
    EXP_SIGNED   = 0x20000,
    EXP_UNSIGNED = 0x40000,

    XC           = 0x04600, // EXP_C  + EXP_SIZE + EXP_SIGN
    XR           = 0x08600, // EXP_R  + EXP_SIZE + EXP_SIGN
    XM           = 0x0c600, // EXP_M  + EXP_SIZE + EXP_SIGN
    XCI          = 0x24200, // EXP_C  + EXP_SIGNED   + EXP_SIZE
    XCU          = 0x44200, // EXP_C  + EXP_UNSIGNED + EXP_SIZE
    XRI          = 0x28200, // EXP_R  + EXP_SIGNED   + EXP_SIZE
    XRU          = 0x48200, // EXP_R  + EXP_UNSIGNED + EXP_SIZE
    XMI          = 0x2c200, // EXP_M  + EXP_SIGNED   + EXP_SIZE
    XMU          = 0x4c200, // EXP_M  + EXP_UNSIGNED + EXP_SIZE
    XRP          = 0x10200, // EXP_RP + EXP_SIZE
    XC1          = 0x04c00, // EXP_C  + EXP_SIZE_1 + EXP_SIGN
    XC2          = 0x05400, // EXP_C  + EXP_SIZE_2 + EXP_SIGN
    XC3          = 0x05c00, // EXP_C  + EXP_SIZE_3 + EXP_SIGN
    XC4          = 0x06400, // EXP_C  + EXP_SIZE_4 + EXP_SIGN
    XR1          = 0x08c00, // EXP_R  + EXP_SIZE_1 + EXP_SIGN
    XR2          = 0x09400, // EXP_R  + EXP_SIZE_2 + EXP_SIGN
    XR3          = 0x09c00, // EXP_R  + EXP_SIZE_3 + EXP_SIGN
    XR4          = 0x0a400, // EXP_R  + EXP_SIZE_4 + EXP_SIGN
    XM1          = 0x0cc00, // EXP_M  + EXP_SIZE_1 + EXP_SIGN
    XM2          = 0x0d400, // EXP_M  + EXP_SIZE_2 + EXP_SIGN
    XM3          = 0x0dc00, // EXP_M  + EXP_SIZE_3 + EXP_SIGN
    XM4          = 0x0e400, // EXP_M  + EXP_SIZE_4 + EXP_SIGN

    // Operands
    DST = 1,
    SRC1,
    SRC2,

    SV1, // Slot values
    SV2,
    SV3,
    SV4,
    SV5,
    SV6,
    SV7,
    SV8,

    // x86 instructions
    X_START = 1000,
    X_ARG,             // A function call argument, pushed into the stack
    X_ALLOCATE_STACK,
    X_MOVE_STACK_PTR,
    X_CALL,

    X_MOV,
    X_MOVZ, // Zero extend
    X_MOVS, // Sign extend
    X_MOVC, // Move, but not allowed to be coalesced

    X_MOV_FROM_IND,
    X_MOV_FROM_SCALED_IND,
    X_MOV_TO_IND,

    X_LEA,
    X_LEA_FROM_SCALED_IND,
    X_ADD,
    X_SUB,
    X_MUL,
    X_MOD,
    X_IDIV,
    X_CQTO,
    X_CLTD,
    X_SHC, // Shift with a constant
    X_SHR, // Shift with a register
    X_BOR,
    X_BAND,
    X_XOR,
    X_BNOT,

    X_FADD,
    X_FSUB,
    X_FMUL,
    X_FDIV,
    X_LD_EQ_CMP, // Long double == and != comparisons

    X_CMP,
    X_CMPZ,
    X_COMIS,
    X_TEST,
    X_JZ,
    X_JNZ,
    X_JMP,

    X_JE,
    X_JNE,

    X_JLT,
    X_JGT,
    X_JLE,
    X_JGE,

    X_JB,
    X_JA,
    X_JBE,
    X_JAE,

    X_SETE,
    X_SETNE,

    X_SETP,
    X_SETNP,

    X_SETLT,
    X_SETGT,
    X_SETLE,
    X_SETGE,

    X_SETB,
    X_SETA,
    X_SETBE,
    X_SETAE,

    X_PUSH,
    X_POP,
    X_LEAVE,
    X_RET_FROM_FUNC,
    X_CALL_FROM_FUNC,
};

typedef struct rule {
    int index;
    int operation;
    int dst;
    int src1;
    int src2;
    int cost;
    struct x86_operation *x86_operations;
    int x86_operation_count;
    long hash;
} Rule;

typedef struct x86_operation {
    int operation;
    char dst, v1, v2;
    char *template;
    char save_value_in_slot;           // Slot number to save a value in
    char allocate_stack_index_in_slot; // Allocate a stack index and put in slot
    char allocate_register_in_slot;    // Allocate a register and put in slot
    char allocate_label_in_slot;       // Allocate a label and put in slot
    char allocated_type;               // Type to use to determine the allocated stack size
    char arg;                          // The argument (src1 or src2) to load/save
} X86Operation;

int instr_rule_count;
Rule *instr_rules;
int disable_merge_constants;
LongMap *instr_rules_by_operation;
Value **saved_values;
char *rule_coverage_file;
Set *rule_coverage;

void select_instructions(Function *function);
void remove_vreg_self_moves(Function *function);
void remove_stack_self_moves(Function *function);
void add_spill_code(Function *function);

// instrrules-generated.c
void init_generated_instruction_selection_rules(void);

// instrutil.c
X86Operation *dup_x86_operation(X86Operation *operation);
char size_to_x86_size(int size);
char *non_terminal_string(int nt);
void print_rule(Rule *r, int print_operations, int indent);
void print_rules(void);
char *operation_string(int operation);
void make_value_x86_size(Value *v);
int match_value_to_rule_src(Value *v, int src);

#define non_terminal_for_value(v) (v->non_terminal ? v->non_terminal : uncached_non_terminal_for_value(v))
int uncached_non_terminal_for_value(Value *v);
// Used to match the root node. It must be an exact match.
#define match_value_to_rule_dst(v, dst) (non_terminal_for_value(v) == dst)

int match_value_type_to_rule_dst(Value *v, int dst);
char *value_to_non_terminal_string(Value *v);
int make_x86_size_from_non_terminal(int non_terminal);
void init_rules_by_operation(void);
void free_rules_by_operation(void);
void check_for_duplicate_rules(void);
void write_rule_coverage_file(void);

// instrules.c
void define_rules(void);

// codegen.c
void init_instrsel();
void free_instrsel();

char *register_name(int preg);
char *render_x86_operation(Tac *tac, int function_pc, int expect_preg);
void make_stack_offsets(Function *function, char *function_name);
void add_final_x86_instructions(Function *function, char *function_name);
void remove_nops(Function *function);
void merge_rsp_func_call_add_subs(Function *function);
int fprintf_escaped_string_literal(void *f, StringLiteral *sl, int for_assembly);
void output_code(char *input_filename, char *output_filename);
void init_codegen(void);
void free_codegen(void);

// wcc.c
enum {
    COMPILE_START_AT_BEGINNING,
    COMPILE_START_AT_ARITHMETIC_MANPULATION,
    COMPILE_STOP_AFTER_ANALYZE_DOMINANCE,
    COMPILE_STOP_AFTER_INSERT_PHI_FUNCTIONS,
    COMPILE_STOP_AFTER_LIVE_RANGES,
    COMPILE_STOP_AFTER_INSTRUCTION_SELECTION,
    COMPILE_STOP_AFTER_ADD_SPILL_CODE,
    COMPILE_STOP_AT_END,
};

void init_instruction_selection_rules(void);
void free_instruction_selection_rules(void);

char init_memory_management_for_translation_unit(void);
char free_memory_for_translation_unit(void);

char *make_temp_filename(char *template);
void run_compiler_phases(Function *function, char *function_name, int start_at, int stop_at);
void compile(char *input, char *original_input_filename, char *output_filename);

// test-utils.c
int failures;
int remove_reserved_physical_registers;

void assert_x86_op(char *expected);
void assert_tac(Tac *tac, int operation, Value *dst, Value *src1, Value *src2);

Tac *i(int label, int operation, Value *dst, Value *src1, Value *src2);
Value *p(Value *v);
Value *v(int vreg);
Value *uv(int vreg);
Value *vsz(int vreg, int type);
Value *vusz(int vreg, int type);
Value *a(int vreg);
Value *asz(int vreg, int type);
Value *ausz(int vreg, int type);
Value *l(int label);
Value *ci(int value);
Value *c(long value);
Value *cld(long double value);
Value *uc(long value);
Value *csz(long value, int type);
Value *s(int string_literal_index);
Value *S(int stack_index);
Value *Ssz(int stack_index, int type);
Value *g(int index);
Value *gsz(int index, int type);
Value *fu(int index);
Value *pfu(int index);
Value *make_arg_src1(void);

void start_ir(void);
void finish_register_allocation_ir(Function *function);
void finish_ir(Function *function);
void finish_spill_ir(Function *function);

// Autogenerated internals.c
char *internals(void);
