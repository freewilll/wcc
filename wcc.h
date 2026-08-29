#ifndef WCC_H
#define WCC_H

#include <stdio.h>
#include <sys/time.h>

// Deliberately current working directory for config.h, for out-of-tree builds
#include <config.h>

#define TARGET_OPS_START 1000

#if defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

#define MAX_CPP_FILESIZE              10 * 1024 * 1024
#define MAX_CPP_INCLUDE_DEPTH         15
#define MAX_CPP_MACRO_PARAM_COUNT     1024
#define MAX_STRUCT_OR_UNION_SCALARS   1024
#define MAX_TYPEDEFS                  1024
#define MAX_STRUCT_MEMBERS            1024
#define MAX_IDENTIFIER_SIZE           1024
#define MAX_INPUT_SIZE                1 << 20 // 1 MB
#define MAX_STRING_LITERALS           20480
#define MAX_FLOATING_POINT_LITERALS   10240
#define VALUE_STACK_SIZE              10240
#define MAX_VREG_COUNT                20480
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
    int array_length;
    int bit_field_size;                                 // Used for struct members. Type is TYPE_INT
    unsigned int is_unsigned:1;
    unsigned int is_const:1;
    unsigned int is_volatile:1;
    unsigned int is_restrict:1;
    char storage_class;                                 // One of SC_*
    struct type *target;
    struct tag *tag;                                    // For structs, unions and enums
    struct struct_or_union_desc *struct_or_union_desc;
    FunctionType *function;                             // For functions
} Type;

typedef struct initializer {
    int offset;                 // Offset of the initializer
    int size;                   // Size of the initializer
    void *data;                 // Pointer to the data, or zero if it's a block of zeroes
    int is_string_literal;      //
    int string_literal_index;   // If it's a string literal, an index in string_literals
    int is_address_of;          // When set, the initializer must also be either a string literal or have a symbol
    int address_of_offset;      // Offset when used in combination with is_address_of
    int is_int128;              // Is this a 16-byte integer
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
    char definition_status;     // One of DEFINITION_STATUS_*
    char has_extern_keyword;    // Has the extern keyword been used to declare this symbol
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

typedef struct register_set {
    const int *int_registers;   // Intger registers
    const int *fp_registers;    // Floating point registers
} RegisterSet;

// functions.c
#define fpa_pl(fpa, i) (*((FunctionParamLocations *) fpa->param_locations->elements[i]))

// Details about a single scalar in a struct/union
typedef struct struct_or_union_scalar {
    Type *type;
    int offset;
} StructOrUnionScalar;

// A list of StructOrUnionScalar
typedef struct struct_or_union_scalars {
    StructOrUnionScalar **scalars;
    int count;
} StructOrUnionScalars;

extern RegisterSet arg_register_set;
extern RegisterSet function_return_value_register_set;

typedef struct function {
    char *identifier;                                   // The name of the function
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
    struct split_vreg **int128_register_mappings;       // A map from 128-bit registers to split 2*64-bit registers
    int int128_register_mappings_count;                 // The amount of allocated mappings
} Function;

// Data of the a single eight byte that's part of a struct or union function parameter or arg
typedef struct function_param_location {
    // Details of the struct/union
    int stru_offset;             // Starting offset in the case of a struct/union
    int stru_size;               // Number of bytes in the 8-byte in the case of a struct/union
    int stru_member_count;       // Amount of members in the 8-byte in the case of a struct/union
    int i128_part;               // Low or high 8-byte of a 128-bit integer

    // Details of where the function param/arg goes, either in a register or the stack
    // One of int_register/fp_register/stack_offset is not -1.
    int int_register;       // If not -1, an int register
    int fp_register;        // If not -1, an FP register
    int stack_offset;       // If not -1, the stack offset
    int stack_padding;      // If not -1, the stack padding
} FunctionParamLocation;

typedef struct function_param_locations {
    int count;
    FunctionParamLocation *locations;
} FunctionParamLocations;

typedef struct function_param_allocation {
    int single_int_register_arg_count;  // Amount of allocated integer registers
    int single_fp_register_arg_count;   // Amount of allocated floating point registers
    int biggest_alignment;              // Alignment of largest param
    int offset;                         // If on the stack, offset within the FPA
    int padding;                        // Final padding on the stack
    int size;                           // Size on the stack, including padding
    List *param_locations;
} FunctionParamAllocation;

// Physical register class
enum {
    PC_INT = 1, // Integer
    PC_FP  = 2, // Floating point
};

// Function call data related to a value
typedef struct function_call_value {
    int is_overflow_arg_area_address;                    // Set to indicate this value must point to the saved register save overflow for variadic functions
    Symbol *function_symbol;                             // Corresponding symbol in the case of a function call
    Type *function_type;                                 // Type of the function in a function call
    int function_param_original_stack_index;             // Original stack index for function parameter pushed onto the stack
    int function_call_arg_index;                         // Index of the argument (0=leftmost)
    FunctionParamLocations *function_call_arg_locations; // Destination of the arg, either a single int or FP register, or in the case of a struct, a list of locations
    int function_call_fp_register_arg_index;             // Index of the argument in integer registers going left to right (0=leftmost). Set to -1 if it's on the stack.
    int function_call_arg_stack_padding;                 // Extra initial padding needed to align the function call argument pushed arguments
    int function_call_arg_push_count;                    // Number of arguments pushed on the stack
    int function_call_fp_register_arg_count;             // Number of floaing point arguments in registers
} FunctionCallValue;

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
    char preg_class;                                     // Class of physical register, PC_INT or PC_FP
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
    FunctionCallValue function_call;                     // Function call related data
    int address_of_offset;                               // Offset when used in combination with is_address_of
    int live_range_preg;                                 // This value is bound to a physical register
    Set *return_value_live_ranges;                       // Live ranges for registers that are part of the function return values
    Symbol *global_symbol;                               // Pointer to a global symbol if the value is a global symbol
    int label;                                           // Target label in the case of jump instructions
    int ssa_subscript;                                   // Optional SSA enumeration
    int live_range;                                      // Optional SSA live range
    char preferred_live_range_preg_index;                // Preferred physical register
    int target_size;                                     // Current size while generating target code
    int non_terminal;                                    // Used in rule matching
} Value;

typedef struct origin {
    char *filename;
    int line_number;
} Origin;

typedef struct clobber {
    char live_range_preg;                   // The clobbered physical registers
    unsigned int add_ig_edge_to_dst:1;      // Add interference graph edge to ...
    unsigned int add_ig_edge_to_src1:1;
    unsigned int add_ig_edge_to_src2:1;
    unsigned int clobbers_livenow:1;        // If live vregs are clobberred
} Clobber;

#define MAX_CLOBBERS 4

typedef struct operation {
    int id;                         // IR_* or target operation
    int is_move:1;                  // Is a move operation
    int is_convert_move:1;          // Is a move operation that also converts
    int is_conditional_jump:1;      // Set if the operation is a conditional jump
    int is_unconditional_jump:1;    // Set if the operation is a unconditional jump
    int is_call:1;                  // Set if the operation is a function call
    Clobber clobbers[MAX_CLOBBERS]; // Null terminated array of clobbers
} Operation;

typedef struct three_address_code {
    int index;                          // Index in a tac chain
    Operation operation;                // The operation
    int label;                          // Label if this instruction is jumped to
    Value *dst;                         // Destination
    Value *src1;                        // First rhs operand
    Value *src2;                        // Second rhs operand
    Value *phi_values;                  // For phi functions, a null terminated array of values for the args
    struct three_address_code *next;    // Next in a linked-list
    struct three_address_code *prev;    // Previous in a linked-list
    char *target_template;              // Template for rendering target instruction
    Origin *origin;                     // Filename and line number where the tac was created
} Tac;

void init_function_allocations(void);
void free_function(Function *function, int remove_from_allocations);
void free_functions(void);
Function *new_function(char *identifier);
void reverse_function_call_args_order(Function *function);
FunctionParamAllocation *initialize_function_return_value_fpa(Type *function_type);
void process_function_call_arg_allocations(Function *function);
void add_ir_call_reg_instructions(Tac *ir, Value **function_call_values, int count);
int add_arg_move_to_register(Function *function, Tac *ir, Type *type, Value *param, int preg_class, int register_index, RegisterSet *register_set);
void load_struct_scalar_into_value(Function *function, Tac *ir, Value *param, FunctionParamLocation *pl, Type *type, Value *dst, int offset);
Value *load_struct_scalar_into_new_vreg(Function *function, Tac *ir, Value *param, FunctionParamLocation *pl, Type *type);
Value *make_long_temp_vreg(Function *function);
void add_function_call_arg_move_for_struct_or_union_on_stack(Function *function, Tac *ir);
void remove_IR_ARG_instructions_that_have_been_handled(Function *function);
void add_function_call_arg_moves(Function *function);
void flatten_type(Type *type, StructOrUnionScalars *scalars, int offset);
void remap_stack_index(int *stack_index_remap, Value *v);
Value *make_function_call_value(int function_call, Type *type);
FunctionParamAllocation *init_function_param_allocaton(char *function_identifier);
void add_function_param_moves(Function *function);
Tac *make_param_move_to_register_tac(Function *function, Type *type, int single_register_arg_count, int in_register);
Tac *make_param_move_to_stack_tac(Function *function, Type *type, int single_register_arg_count);
void free_function_param_allocaton(FunctionParamAllocation *fpa);
void free_function_param_locations(FunctionParamLocations *fpl);
void finalize_function_param_allocation(FunctionParamAllocation *fpa);

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

#define TOKEN_LIST(TOKEN_ITEM) \
    TOKEN_ITEM(TOK_EOF,                   "EOF") \
    TOKEN_ITEM(TOK_IDENTIFIER,            "identifier") \
    TOKEN_ITEM(TOK_INTEGER,               "integer") \
    TOKEN_ITEM(TOK_FLOATING_POINT_NUMBER, "floating point number") \
    TOKEN_ITEM(TOK_STRING_LITERAL,        "string literal") \
    TOKEN_ITEM(TOK_IF,                    "if") \
    TOKEN_ITEM(TOK_ELSE,                  "else") \
    TOKEN_ITEM(TOK_SIGNED,                "signed") \
    TOKEN_ITEM(TOK_UNSIGNED,              "unsigned") \
    TOKEN_ITEM(TOK_INLINE,                "inline") \
    TOKEN_ITEM(TOK_VOID,                  "void") \
    TOKEN_ITEM(TOK_CHAR,                  "char") \
    TOKEN_ITEM(TOK_INT,                   "int") \
    TOKEN_ITEM(TOK_SHORT,                 "short") \
    TOKEN_ITEM(TOK_LONG,                  "long") \
    TOKEN_ITEM(TOK_INT128,                "__int128") \
    TOKEN_ITEM(TOK_FLOAT,                 "float") \
    TOKEN_ITEM(TOK_DOUBLE,                "double") \
    TOKEN_ITEM(TOK_STRUCT,                "struct") \
    TOKEN_ITEM(TOK_UNION,                 "union") \
    TOKEN_ITEM(TOK_TYPEDEF,               "typedef") \
    TOKEN_ITEM(TOK_TYPEDEF_TYPE,          "typedef type") \
    TOKEN_ITEM(TOK_DO,                    "do") \
    TOKEN_ITEM(TOK_WHILE,                 "while") \
    TOKEN_ITEM(TOK_FOR,                   "for") \
    TOKEN_ITEM(TOK_SWITCH,                "switch") \
    TOKEN_ITEM(TOK_CASE,                  "case") \
    TOKEN_ITEM(TOK_DEFAULT,               "default") \
    TOKEN_ITEM(TOK_CONTINUE,              "continue") \
    TOKEN_ITEM(TOK_BREAK,                 "break") \
    TOKEN_ITEM(TOK_RETURN,                "return") \
    TOKEN_ITEM(TOK_ENUM,                  "enum") \
    TOKEN_ITEM(TOK_SIZEOF,                "sizeof") \
    TOKEN_ITEM(TOK_RCURLY,                "}") \
    TOKEN_ITEM(TOK_LCURLY,                "{") \
    TOKEN_ITEM(TOK_SEMI,                  ";") \
    TOKEN_ITEM(TOK_COMMA,                 ",") \
    TOKEN_ITEM(TOK_EQ,                    "=") \
    TOKEN_ITEM(TOK_PLUS_EQ,               "+=") \
    TOKEN_ITEM(TOK_MINUS_EQ,              "-=") \
    TOKEN_ITEM(TOK_MULTIPLY_EQ,           "*=") \
    TOKEN_ITEM(TOK_DIVIDE_EQ,             "/=") \
    TOKEN_ITEM(TOK_MOD_EQ,                "%=") \
    TOKEN_ITEM(TOK_BITWISE_AND_EQ,        "&=") \
    TOKEN_ITEM(TOK_BITWISE_OR_EQ,         "|=") \
    TOKEN_ITEM(TOK_BITWISE_XOR_EQ,        "^=") \
    TOKEN_ITEM(TOK_BITWISE_RIGHT_EQ,      ">>=") \
    TOKEN_ITEM(TOK_BITWISE_LEFT_EQ,       "<<=") \
    TOKEN_ITEM(TOK_TERNARY,               "?") \
    TOKEN_ITEM(TOK_COLON,                 ":") \
    TOKEN_ITEM(TOK_OR,                    "|") \
    TOKEN_ITEM(TOK_AND,                   "&") \
    TOKEN_ITEM(TOK_BITWISE_OR,            "|") \
    TOKEN_ITEM(TOK_XOR,                   "^") \
    TOKEN_ITEM(TOK_AMPERSAND,             "&") \
    TOKEN_ITEM(TOK_DBL_EQ,                "==") \
    TOKEN_ITEM(TOK_NOT_EQ,                "!=") \
    TOKEN_ITEM(TOK_LT,                    ">") \
    TOKEN_ITEM(TOK_GT,                    ">") \
    TOKEN_ITEM(TOK_LE,                    "<=") \
    TOKEN_ITEM(TOK_GE,                    ">=") \
    TOKEN_ITEM(TOK_BITWISE_LEFT,          "<<") \
    TOKEN_ITEM(TOK_BITWISE_RIGHT,         ">>") \
    TOKEN_ITEM(TOK_PLUS,                  "+") \
    TOKEN_ITEM(TOK_MINUS,                 "-") \
    TOKEN_ITEM(TOK_MULTIPLY,              "*") \
    TOKEN_ITEM(TOK_DIVIDE,                "/") \
    TOKEN_ITEM(TOK_MOD,                   "%") \
    TOKEN_ITEM(TOK_LOGICAL_NOT,           "!") \
    TOKEN_ITEM(TOK_BITWISE_NOT,           "~") \
    TOKEN_ITEM(TOK_INC,                   "++") \
    TOKEN_ITEM(TOK_DEC,                   "--") \
    TOKEN_ITEM(TOK_DOT,                   ".") \
    TOKEN_ITEM(TOK_ARROW,                 "->") \
    TOKEN_ITEM(TOK_RBRACKET,              "]") \
    TOKEN_ITEM(TOK_LBRACKET,              "[") \
    TOKEN_ITEM(TOK_RPAREN,                ")") \
    TOKEN_ITEM(TOK_LPAREN,                "(") \
    TOKEN_ITEM(TOK_ATTRIBUTE,             "attribute") \
    TOKEN_ITEM(TOK_PACKED,                "packed") \
    TOKEN_ITEM(TOK_ALIGNED,               "aligned") \
    TOKEN_ITEM(TOK_HASH,                  "#") \
    TOKEN_ITEM(TOK_AUTO,                  "auto") \
    TOKEN_ITEM(TOK_REGISTER,              "register") \
    TOKEN_ITEM(TOK_STATIC,                "static") \
    TOKEN_ITEM(TOK_EXTERN,                "extern") \
    TOKEN_ITEM(TOK_CONST,                 "const") \
    TOKEN_ITEM(TOK_VOLATILE,              "volatile") \
    TOKEN_ITEM(TOK_RESTRICT,              "resrict") \
    TOKEN_ITEM(TOK_ELLIPSES,              "...") \
    TOKEN_ITEM(TOK_GOTO,                  "goto") \
    TOKEN_ITEM(TOK_ASM,                   "asm") \
    TOKEN_ITEM(TOK_EXTENSION,             "extension")


enum {
    TOK_NULL,
#define TOKEN_ITEM(tok, str) tok,
    TOKEN_LIST(TOKEN_ITEM)
#undef TOKEN_ITEM
};

static const char *const token_names[] = {
    NULL,
#define TOKEN_ITEM(tok, str) str,
    TOKEN_LIST(TOKEN_ITEM)
#undef TOKEN_ITEM
};

enum {
    TYPE_VOID            = 1,
    TYPE_CHAR            = 2,
    TYPE_SHORT           = 3,
    TYPE_INT             = 4,
    TYPE_LONG            = 5,
    TYPE_INT128          = 6,
    TYPE_FLOAT           = 7,
    TYPE_DOUBLE          = 8,
    TYPE_LONG_DOUBLE     = 9,
    TYPE_PTR             = 10,
    TYPE_ARRAY           = 11,
    TYPE_STRUCT_OR_UNION = 12,
    TYPE_ENUM            = 13,
    TYPE_FUNCTION        = 14,
    TYPE_TYPEDEF         = 15
};

enum {
    LINKAGE_NONE,       // Function params and block scope identifiers without a storage class
    LINKAGE_INTERNAL,
    LINKAGE_EXTERNAL,
};

enum {
    DEFINITION_STATUS_NONE,         // It is a declaration
    DEFINITION_STATUS_TENTATIVE,    // A tentative definition ends up in the .bss or .comm
    DEFINITION_STATUS_DEFINED,      // Initializers are present
};

// Intermediate representation operations
enum {
    IR_MOVE=1,                // Moving of constants, string literals, variables, or registers
    IR_MOVE_TO_PTR,           // Assignment to a pointer target
    IR_MOVE_PREG_CLASS,       // Move int <-> fp without conversion
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
    IR_ADDC,                  // + with carry, used for int128
    IR_SUB,                   // -
    IR_SUBC,                  // - with carry, used for int128
    IR_MUL,                   // *
    IR_MUL128A,               // * - 128 bit multiply. It consists of two instructions that are linked together, since the result
    IR_MUL128B,               // * - is present in two 64-bit registers
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
    IR_BIT_SCAN_FWD,          // Bit scan forward
    IR_BIT_SCAN_REV,          // Bit scan reverse
    IR_PHI_FUNCTION,          // SSA phi function
};

typedef struct string_literal {
    char *data;
    int size;
    int is_wide_char;
    int allocated;
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
    CPP_TOK_ELLIPSES,
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
    char is_function;               // Is the macro an object or function macro
    int param_count;                // Amount of parameters.
    int is_variadic;                // A function-like macro has a ... at the end of the parameters definition
    const char *variadic_arg_name;  // Optionally overrides the "__VA_ARGS__" identifier
    StrMap *param_identifiers;      // Mapping of parameter identifiers => index, index starts at 1
    CppToken *tokens;               // Replacement tokens
    DirectiveRenderer renderer;     // Renderer for builtin directives
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

typedef enum {
    CP_PREPROCESSING,
    CP_PARSING,
    CP_POST_PARSING,
} CompilePhase;

extern char *cur_filename;              // Current filename being lexed
extern int cur_line;                    // Current line number being lexed

extern int print_parser_symbols;               // Print symbols after parser phase
extern int print_ir1;                          // Print IR after parsing
extern int print_ir2;                          // Print IR before register allocation
extern int print_ir3;                          // Print IR after register allocation
extern int log_compiler_phase_durations;       // Output logs of how long each compiler phase lasts
extern int sanity_check_ir;                    // Sanity check intermediate representations

extern int opt_PIC;                            // Make position independent code
extern int opt_debug_symbols;                  // Add debug symbols
extern int opt_enable_vreg_renumbering;        // Renumber vregs so the numbers are consecutive
extern int opt_enable_register_coalescing;     // Merge registers that can be reused within the same operation
extern int opt_enable_live_range_coalescing;   // Merge live ranges where possible
extern int opt_spill_furthest_liveness_end;    // Prioritize spilling physical registers with furthest liveness end
extern int opt_short_lr_infinite_spill_costs;  // Don't spill short live ranges
extern int opt_optimize_arithmetic_operations; // Optimize arithmetic operations
extern int opt_enable_preferred_pregs;         // Enable preferred preg selection in register allocator
extern int opt_enable_trigraphs;               // Enable trigraph preprocessing
extern int opt_enable_common_symbols;          // Enable .comm symbols
extern int opt_warnings_are_errors;            // Treat all warnings as errors

extern CliDirective *cli_directives;      // Linked list of directives passed on the command line with -D
extern CliIncludePath *cli_include_paths; // Linked list of include paths passed on the command line with -I
extern CliLibraryPath *cli_library_paths; // Linked list of library paths passed on the command line with -L
extern StrMap *directives;                // Map of CPP directives
extern CompilePhase compile_phase;        // One of CP_*

extern int cur_token;                     // Current token
extern char *cur_identifier;              // Current identifier if the token is an identifier
extern char *cur_type_identifier;         // Identifier of the last parsed declarator
extern Type *cur_lexer_type;              // A type determined by the lexer
extern long cur_long;                     // Current long if the token is an integral type
extern long double cur_long_double;       // Current long double if the token is a floating point type
extern StringLiteral cur_string_literal;  // Current string literal if the token is a string literal

extern Scope *cur_scope;                  // Current scope.
extern StringLiteral *string_literals;    // Each string literal has an index in this array, with a pointer to the string literal struct
extern int string_literal_count;          // Amount of string literals

extern Symbol *cur_function_symbol;     // Currently parsed function
extern Value *cur_loop_continue_dst;    // Target jmp of continue statement in the current for/while loop
extern Value *cur_loop_break_dst;       // Target jmp of break statement in the current for/while loop

extern Typedef **all_typedefs;   // All typedefs
extern int all_typedefs_count;   // Number of typedefs

extern Tac *ir_start, *ir;               // intermediate representation for currently parsed function
extern int label_count;                  // Global label count, always growing
extern int cur_loop;                     // Current loop being parsed
extern int loop_count;                   // Loop counter
extern int total_stack_register_count;   // Spilled register count for all functions

#define MAX_ARG_REGISTERS 8
extern const int int_arg_registers[MAX_ARG_REGISTERS];
extern const int fp_arg_registers[MAX_ARG_REGISTERS];

extern int error_incomptatible_pointer_type;
extern int error_int_conversion;

extern int warn_integer_constant_too_large;
extern int warn_assignment_types_incompatible;
extern int warn_extern_initializer;
extern int warn_excess_initializers;

extern int debug_function_param_allocation;
extern int debug_function_arg_mapping;
extern int debug_function_param_mapping;
extern int debug_ssa_mapping_local_stack_indexes;
extern int debug_ssa;
extern int debug_ssa_liveout;
extern int debug_ssa_cfg;
extern int debug_ssa_dominance;
extern int debug_ssa_idom;
extern int debug_ssa_dominance_frontiers;
extern int debug_ssa_phi_insertion;
extern int debug_ssa_phi_renumbering;
extern int debug_ssa_live_range;
extern int debug_ssa_vreg_renumbering;
extern int debug_ssa_interference_graph;
extern int debug_ssa_live_range_coalescing;
extern int debug_ssa_spill_cost;
extern int debug_register_allocation;
extern int debug_graph_coloring;
extern int debug_instsel_tree_merging;
extern int debug_instsel_tree_merging_deep;
extern int debug_instsel_igraph_simplification;
extern int debug_instsel_tiling;
extern int debug_instsel_cost_graph;
extern int debug_instsel_spilling;
extern int debug_int128;
extern int debug_stack_frame_layout;
extern int debug_exit_after_parser;
extern int debug_dont_compile_internals;

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
void delete_from_set(Set *s, int value);
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

NORETURN void panic(char *format, ...);

int wasprintf(char **ret, const char *format, ...);
StringBuffer *new_string_buffer(int initial_size);
void free_string_buffer(StringBuffer *sb, int free_data);
void append_to_string_buffer(StringBuffer *sb, char *str);
void terminate_string_buffer(StringBuffer *sb);

void set_debug_logging_start_time();
void debug_log(char *format, ...);

// memory.c
extern int print_heap_usage;
extern int fail_on_leaked_memory;

void *wmalloc(size_t size);
void *wrealloc(void *ptr, size_t size);
void *wcalloc(size_t nitems, size_t size);
void *wcalloc(size_t nitems, size_t size);
char *wstrdup(const char *str);
void wfree(void *ptr);
void process_memory_allocation_stats(void);

// error.c
NORETURN void panic_with_line_number(char *format, ...);
NORETURN void simple_error(char *format, ...);
NORETURN void error(char *format, ...);
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

void  print_builtin_include_paths();
void set_libc_include_paths(int use_musl);
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

extern Symbol *memcpy_symbol;
extern Symbol *memset_symbol;

Value *make_string_literal_value_from_cur_string_literal(void);
Value *new_label_dst(void);
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
void init_parser(void);
void free_parser(void);

// constexpr.c
Value* evaluate_const_binary_int_operation(int operation, Value *src1, Value *src2);
Value* evaluate_const_binary_fp_operation(int operation, Value *src1, Value *src2, Type *type);
Value *cast_constant_value(Value *src, Type *type);
Value *parse_constant_expression(int level);
Value *parse_constant_integer_expression(int all_longs);

// scopes.c
extern Scope *global_scope;

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
int is_non_128_bit_integer_type(Type *type);
int is_floating_point_type(Type *type);
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
int is_pointer_to_char(Type *type);
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

// ir.c
void init_ir(void);
void free_ir(void);

void init_value_allocations(void) ;
void free_values(void);
void init_value(Value *v);
Value *new_value(void);
Value *new_integral_constant(int type_type, long value);
Value *new_unsigned_integral_constant(int type_type, long value);
Value *new_floating_point_constant(int type_type, long double value);
Value *dup_value(Value *src);
void assign_register_to_value(Value *v, int vreg);
Value *new_value_in_stack(int type, int stack_index, int offset);
void add_tac_to_ir(Tac *tac);
Tac *new_instruction(int operation);
Tac *new_instruction_with_values(int operation, Value *dst, Value *src1, Value *src2);
Tac *assign_values_to_instruction(Tac *tac, Value *dst, Value *src1, Value *src2);
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2);
void make_instruction_a_nop(Tac *tac);
void insert_tac_before(Tac *ir, Tac *tac, int move_label);
Tac *new_tac_before(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, int move_label);
Tac *insert_tac_after(Tac *ir, Tac *tac);
Tac *new_tac_after(Tac *ir, int operation, Value *dst, Value *src1, Value *src2);
Tac *delete_instruction(Tac *tac);
void sanity_test_ir_linkage(Function *function);
void sanity_test_values(Function *function);
int make_max_function_call_value(Function *function);
int make_max_function_call_id(Function *function);
int print_value(void *f, Value *v, int is_assignment_rhs);
void print_instruction(void *f, Tac *tac, int expect_preg);
void print_ir(Function *function, int expect_preg);
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

extern int live_range_reserved_pregs_offset;

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
void clobber_livenow(char *ig, int vreg_count, LongSet *livenow, Tac *tac, int preg_reg_index);
void blast_vregs_with_live_ranges(Function *function);
void make_interference_graph(Function *function, int include_instrsel_constraints);
void free_interference_graph(Function *function);
void coalesce_live_ranges(Function *function);
void make_preferred_live_range_preg_indexes(Function *function);
void free_preferred_live_range_preg_indexes(Function *function);

// regalloc.c
extern int *preg_map;               // Map from live range registers to physical registers
extern int *callee_saved_registers; // Set to 1 for registers that must be preserved in function calls.

extern LongSet *debug_spill_registers;

void compress_vregs(Function *function);
void init_vreg_locations(Function *function);
void free_vreg_locations(Function *function);
void allocate_registers_top_down(Function *function, int live_range_start, int physical_register_count, int preg_class);
void allocate_registers(Function *function);
void free_allocate_registers(void);

// instrsel.c
#define TARGET_NON_TERMINAL_START 0x60      // Target specific non terminals
#define TARGET_NON_TERMINAL_END   0x80
#define AUTO_NON_TERMINAL_START   0x80      // Automatically assigned custom non terminals
#define AUTO_NON_TERMINAL_END     0x300

#define RULE_NON_TERMINAL_LIST(RULE_NON_TERMINAL_ITEM) \
    RULE_NON_TERMINAL_ITEM(STL,   4 )    /* String literal  */ \
    RULE_NON_TERMINAL_ITEM(LAB,   -1)    /* Label, i.e. a target for a (conditional) jump */ \
    RULE_NON_TERMINAL_ITEM(FUN,   -1)    /* Function, used for calls */ \
    RULE_NON_TERMINAL_ITEM(CSTV1, 1 )    /* Constant with value 1 */ \
    RULE_NON_TERMINAL_ITEM(CSTV2, 1 )    /* Constant with value 2 */ \
    RULE_NON_TERMINAL_ITEM(CSTV3, 1 )    /* Constant with value 3 */ \
    RULE_NON_TERMINAL_ITEM(CI1,   1 )    /* Constants */ \
    RULE_NON_TERMINAL_ITEM(CI2,   2 ) \
    RULE_NON_TERMINAL_ITEM(CI3,   3 ) \
    RULE_NON_TERMINAL_ITEM(CI4,   4 ) \
    RULE_NON_TERMINAL_ITEM(CU1,   1 ) \
    RULE_NON_TERMINAL_ITEM(CU2,   2 ) \
    RULE_NON_TERMINAL_ITEM(CU3,   3 ) \
    RULE_NON_TERMINAL_ITEM(CU4,   4 ) \
    RULE_NON_TERMINAL_ITEM(CO3,   3 )    /* Floating point constants */ \
    RULE_NON_TERMINAL_ITEM(CO4,   4 ) \
    RULE_NON_TERMINAL_ITEM(CO5,   5 ) \
    RULE_NON_TERMINAL_ITEM(RI1,   1 )    /* Signed registers */ \
    RULE_NON_TERMINAL_ITEM(RI2,   2 ) \
    RULE_NON_TERMINAL_ITEM(RI3,   3 ) \
    RULE_NON_TERMINAL_ITEM(RI4,   4 ) \
    RULE_NON_TERMINAL_ITEM(RU1,   1 )    /* Unsigned registers */ \
    RULE_NON_TERMINAL_ITEM(RU2,   2 ) \
    RULE_NON_TERMINAL_ITEM(RU3,   3 ) \
    RULE_NON_TERMINAL_ITEM(RU4,   4 ) \
    RULE_NON_TERMINAL_ITEM(RO3,   3 )    /* Floating point registers (O is the first letter in "float"" not already taken) */ \
    RULE_NON_TERMINAL_ITEM(RO4,   4 ) \
    RULE_NON_TERMINAL_ITEM(RO5,   5 ) \
    RULE_NON_TERMINAL_ITEM(MI1,   1 )    /* Memory, in stack or globals */ \
    RULE_NON_TERMINAL_ITEM(MI2,   2 ) \
    RULE_NON_TERMINAL_ITEM(MI3,   3 ) \
    RULE_NON_TERMINAL_ITEM(MI4,   4 ) \
    RULE_NON_TERMINAL_ITEM(MU1,   1 ) \
    RULE_NON_TERMINAL_ITEM(MU2,   2 ) \
    RULE_NON_TERMINAL_ITEM(MU3,   3 ) \
    RULE_NON_TERMINAL_ITEM(MU4,   4 ) \
    RULE_NON_TERMINAL_ITEM(MSI1,  1 )    /* Memory, in stack */ \
    RULE_NON_TERMINAL_ITEM(MSI2,  2 ) \
    RULE_NON_TERMINAL_ITEM(MSI3,  3 ) \
    RULE_NON_TERMINAL_ITEM(MSI4,  4 ) \
    RULE_NON_TERMINAL_ITEM(MSU1,  1 ) \
    RULE_NON_TERMINAL_ITEM(MSU2,  2 ) \
    RULE_NON_TERMINAL_ITEM(MSU3,  3 ) \
    RULE_NON_TERMINAL_ITEM(MSU4,  4 ) \
    RULE_NON_TERMINAL_ITEM(MGI1,  1 )    /* Memory, in globals */ \
    RULE_NON_TERMINAL_ITEM(MGI2,  2 ) \
    RULE_NON_TERMINAL_ITEM(MGI3,  3 ) \
    RULE_NON_TERMINAL_ITEM(MGI4,  4 ) \
    RULE_NON_TERMINAL_ITEM(MGU1,  1 ) \
    RULE_NON_TERMINAL_ITEM(MGU2,  2 ) \
    RULE_NON_TERMINAL_ITEM(MGU3,  3 ) \
    RULE_NON_TERMINAL_ITEM(MGU4,  4 ) \
    RULE_NON_TERMINAL_ITEM(MO3,   3 )    /* Floating point in stack or globals */ \
    RULE_NON_TERMINAL_ITEM(MO4,   4 ) \
    RULE_NON_TERMINAL_ITEM(MSO3,  3)     /* Floating point in stack */ \
    RULE_NON_TERMINAL_ITEM(MSO4,  4)  \
    RULE_NON_TERMINAL_ITEM(MSO5,  5)  \
    RULE_NON_TERMINAL_ITEM(MGO3,  3)     /* Floating point in globals */ \
    RULE_NON_TERMINAL_ITEM(MGO4,  4)  \
    RULE_NON_TERMINAL_ITEM(MGO5,  5)  \
    RULE_NON_TERMINAL_ITEM(RP1,   4 )     /* Address (aka pointer) in a register */ \
    RULE_NON_TERMINAL_ITEM(RP2,   4 ) \
    RULE_NON_TERMINAL_ITEM(RP3,   4 ) \
    RULE_NON_TERMINAL_ITEM(RP4,   4 ) \
    RULE_NON_TERMINAL_ITEM(RP5,   4 ) \
    RULE_NON_TERMINAL_ITEM(RPF,   4 )   /* Pointer to a function in a register */ \
    RULE_NON_TERMINAL_ITEM(MPF,   4 )   /* Pointer to a function in memory */ \
    RULE_NON_TERMINAL_ITEM(MSPF,  4 )   /* Pointer to a function in the stack */ \
    RULE_NON_TERMINAL_ITEM(MGPF,  4 )   /* Pointer to a function in a global */ \
    RULE_NON_TERMINAL_ITEM(MLD5,  8 )   /* 16-byte memory, for long double */ \
    RULE_NON_TERMINAL_ITEM(MPV,   4 )   /* Pointer in memory */ \
    RULE_NON_TERMINAL_ITEM(MSPV,  4 )   /* Pointer in stack */ \
    RULE_NON_TERMINAL_ITEM(MGPV,  4 )   /* Pointer in global */ \
    RULE_NON_TERMINAL_ITEM(MSA,   -1)   /* Struct or array in memory */ \
    RULE_NON_TERMINAL_ITEM(MSSA,  -1)   /* Struct or array in stack */ \
    RULE_NON_TERMINAL_ITEM(MGSA,  -1)   /* Struct or array in global */ \


enum rule_non_terminals {
    NT_NULL,
#define RULE_NON_TERMINAL_ITEM(non_terminal, size) non_terminal,
    RULE_NON_TERMINAL_LIST(RULE_NON_TERMINAL_ITEM)
#undef RULE_NON_TERMINAL_ITEM
};

#define MAX_RULE_COUNT 9000

// Operands
#define DST     1
#define SRC1    2
#define SRC2    3
#define SV1     11 // Slot values
#define SV2     12
#define SV3     13
#define SV4     14
#define SV5     15
#define SV6     16
#define SV7     17
#define SV8     18

// Non terminal flags
#define EXP_SIZE     0x000200
#define EXP_SIGN     0x000400
#define EXP_SIZE_1   0x000800
#define EXP_SIZE_2   0x001000
#define EXP_SIZE_3   0x001800
#define EXP_SIZE_4   0x002000
#define EXP_C        0x004000
#define EXP_R        0x008000
#define EXP_M        0x00c000
#define EXP_RP       0x010000
#define EXP_SIGNED   0x020000
#define EXP_UNSIGNED 0x040000
#define EXP_STACK    0x080000
#define EXP_GLOBAL   0x100000

// Wildcard non-terminals that are expanded by code
#define XC    (EXP_C  |                 EXP_SIZE | EXP_SIGN  )
#define XR    (EXP_R  |                 EXP_SIZE | EXP_SIGN  )
#define XM    (EXP_M  |                 EXP_SIZE | EXP_SIGN  )
#define XMS   (EXP_M  | EXP_STACK    |  EXP_SIZE | EXP_SIGN  )
#define XMG   (EXP_M  | EXP_GLOBAL   |  EXP_SIZE | EXP_SIGN  )
#define XCI   (EXP_C  | EXP_SIGNED   |  EXP_SIZE             )
#define XCU   (EXP_C  | EXP_UNSIGNED |  EXP_SIZE             )
#define XRI   (EXP_R  | EXP_SIGNED   |  EXP_SIZE             )
#define XRU   (EXP_R  | EXP_UNSIGNED |  EXP_SIZE             )
#define XMI   (EXP_M  | EXP_SIGNED   |  EXP_SIZE             )
#define XMU   (EXP_M  | EXP_UNSIGNED |  EXP_SIZE             )
#define XRP   (EXP_RP |                 EXP_SIZE             )
#define XC1   (EXP_C  |                 EXP_SIZE_1 | EXP_SIGN)
#define XC2   (EXP_C  |                 EXP_SIZE_2 | EXP_SIGN)
#define XC3   (EXP_C  |                 EXP_SIZE_3 | EXP_SIGN)
#define XC4   (EXP_C  |                 EXP_SIZE_4 | EXP_SIGN)
#define XR1   (EXP_R  |                 EXP_SIZE_1 | EXP_SIGN)
#define XR2   (EXP_R  |                 EXP_SIZE_2 | EXP_SIGN)
#define XR3   (EXP_R  |                 EXP_SIZE_3 | EXP_SIGN)
#define XR4   (EXP_R  |                 EXP_SIZE_4 | EXP_SIGN)
#define XM1   (EXP_M  |                 EXP_SIZE_1 | EXP_SIGN)
#define XM2   (EXP_M  |                 EXP_SIZE_2 | EXP_SIGN)
#define XM3   (EXP_M  |                 EXP_SIZE_3 | EXP_SIGN)
#define XM4   (EXP_M  |                 EXP_SIZE_4 | EXP_SIGN)

typedef struct rule {
    int index;
    int operation;
    int dst;
    int src1;
    int src2;
    int cost;
    struct target_operation *target_operations;
    int target_operation_count;
    long hash;
} Rule;

typedef struct target_operation {
    Operation operation;
    char dst, v1, v2;
    char *template;
    char save_value_in_slot;           // Slot number to save a value in
    char allocate_stack_index_in_slot; // Allocate a stack index and put in slot
    char allocate_register_in_slot;    // Allocate a register and put in slot
    char allocate_label_in_slot;       // Allocate a label and put in slot
    char allocated_type;               // Type to use to determine the allocated stack size
    char arg;                          // The argument (src1 or src2) to load/save
} TargetOperation;

extern int instr_rule_count;
extern Rule *instr_rules;
extern int disable_merge_constants;
extern LongMap *instr_rules_by_operation;
extern Value **saved_values;
extern char *rule_coverage_file;
extern Set *rule_coverage;

void init_instrsel();
void free_instrsel();
void check_instrsel_register_sanity(Function *function);
void select_instructions(Function *function);
void remove_vreg_self_moves(Function *function);

// instrrules-generated.c
void init_generated_instruction_selection_rules(void);

// instrutil.c
#define MAX_TARGET_OPS_PER_ROLE 32

Rule *add_rule(int dst, int operation, int src1, int src2, int cost);
TargetOperation *dup_target_operation(TargetOperation *operation);
char *non_terminal_string(int nt);
void print_rule(Rule *r, int print_operations, int indent);
void print_rules(void);
char *operation_string(int operation);
void make_value_target_size(Value *v);

#define non_terminal_for_value(v) (v->non_terminal ? v->non_terminal : uncached_non_terminal_for_value(v))
// Used to match the root node. It must be an exact match.
#define match_value_to_rule_dst(v, dst) (non_terminal_for_value(v) == dst)

int match_value_type_to_rule_dst(Value *v, int dst);
char *value_to_non_terminal_string(Value *v);
int value_ptr_target_target_size(Value *v);
int make_target_size_from_non_terminal(int non_terminal);
void init_rules_by_operation(void);
void free_rules_by_operation(void);
void check_for_duplicate_rules(void);
TargetOperation *add_target_op_to_rule(Rule *r, TargetOperation *target_op);
void fin_rule(Rule *r);
void add_save_value(Rule *r, int arg, int slot);
void add_allocate_stack_index_in_slot(Rule *r, int slot, int type);
void add_allocate_register_in_slot(Rule *r, int slot, int type);
void add_allocate_label_in_slot(Rule *r, int slot);
void write_rule_coverage_file(void);

// codegen.c
#define OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX 0x10000

typedef struct floating_point_literal {
    int type;
    float f;
    double d;
    long double ld;
} FloatingPointLiteral;

typedef struct saved_register {
    int preg;
    int size;
} SavedRegister;

typedef enum elf_section {
    SEC_NONE,
    SEC_TEXT,
    SEC_DATA,
    SEC_BSS,
} ElfSection;

typedef struct  {
    List *saved_registers[6]; // A list of saved registers, keyed by size
} SizedSavedRegisters;

extern FILE *output_file; // Output file handle

extern FloatingPointLiteral *floating_point_literals; // Each floating point literal has an index in this array
extern int floating_point_literal_count;              // Amount of floating point literals

extern StrMap *debug_strings;                      // Map of debug strings to identifiers
extern int debug_string_counter;                   // Counter to uniquely identify a debug string
extern int last_outputted_filename_id;             // Keep track of last printed .loc filename id
extern int last_outputted_filename_line_number;    // Keep track of last printed .loc line number

extern List *allocated_strings;

extern int elf_section;

int fprintf_escaped_char(void *f, unsigned char c);
int fprintf_octal_char(void *f, char c);
int fprintf_escaped_string_literal(void *f, StringLiteral *sl, int for_assembly);
Tac *insert_target_instruction(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, char *target_template);
Value *new_preg_value(int preg);
void remove_nops(Function *function);
int function_is_main(Function *function);
int open_output_file(char *input_filename, char *output_filename);
void output_object_symbols(void);
SizedSavedRegisters *make_saved_registers(Function *function, int preg_class);
void free_sized_saved_registers(SizedSavedRegisters *ssr);
void check_floating_point_literal_max(void);
int add_float_literal(Value *value);
int add_double_literal(Value *value);
int add_long_double_literal(Value *value);
void init_codegen(void);
void free_codegen(void);

// wcc.c
typedef enum compiler_phase_tags {
    PH_NONE,
    PH_BEGIN,
    PH_SSA,
    PH_PARAM,
    PH_DOM,
    PH_PHI,
    PH_LIVE,
    PH_SPILL,
    PH_INSTR,
    PH_PLPR,
    PH_END,
} CompilerPhaseTag;

void init_instruction_selection_rules(void);
void free_instruction_selection_rules(void);

void init_memory_management_for_translation_unit(void);
void free_memory_for_translation_unit(void);

char *make_temp_filename(char *template);
void run_compiler_phases(Function *function, char *function_name, int start_at, int stop_at);
void compile(char *input, char *original_input_filename, char *output_filename);

// test-utils.c
extern int failures;
extern int remove_reserved_physical_registers;

void assert_target_op(char *expected);
void assert_tac(Tac *tac, int operation, Value *dst, Value *src1, Value *src2);

Tac *i(int label, int operation, Value *dst, Value *src1, Value *src2);
Value *p(Value *v);
Value *v(int vreg);
Value *uv(int vreg);
Value *vsz(int vreg, int type);
Value *vusz(int vreg, int type);
Value *pvsz(int vreg, int type);
Value *a(int vreg);
Value *asz(int vreg, int type);
Value *ausz(int vreg, int type);
Value *d(int vreg);
Value *l(int label);
Value *ci(int value);
Value *c(long value);
Value *cld(long double value);
Value *cd(long double value);
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

// int128.c
// Vreg numbers for a 128-bit vreg split into a low and high vregs
typedef struct split_vreg {
    int low;
    int high;
} SplitVreg;


void transform_int128_instructions(Function *function);

// Target specific code
// ------------------------------------------------
extern int char_is_unsigned_by_default;

extern int physical_register_count;
extern int physical_int_register_count;
extern int physical_fp_register_count;

extern int total_function_stack_size_alignment; // The amount to align the total allocated stack for a function

extern int long_doubles_are_in_the_stack;

char *target_op_name(int operation);
void print_target_instruction(void *f, Tac *tac);
void print_physical_register_name_for_lr_reg_index(int preg_reg_index);
int get_preg_class_for_scalar_type(Type *type);
void perform_peephole_optimization(Function *function);

// Target functions related code
Set *allocate_return_value_live_ranges(void);
int prepend_function_params(Function *function);
void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type);
int add_struct_or_union_param_move(Function *function, Tac *ir, Type *type, FunctionParamLocations *pl, RegisterSet *register_set);
void add_function_vararg_param_moves(Function *function, FunctionParamAllocation *fpa);
int *make_original_stack_indexes(Function *function);
int make_struct_or_union_arg_move_instructions(Function *function, Tac *ir, Value *param, int preg_class, int register_index, FunctionParamLocation *location, RegisterSet *register_set);
void process_target_functions(Function *function);
void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac);

// Target instruction rules related code
char *add_size_to_template(char *template, int size);
int uncached_non_terminal_for_value(Value *v);
int match_value_to_rule_src(Value *v, int src);
void define_rules(void);
char *target_non_terminal_string(int nt);

// Target registers related code
void init_allocate_registers(void);
void remove_vreg_self_moves(Function *function);
void add_spill_code(Function *function);

// Codegen
char *register_name(int preg);
char *render_target_operation(Tac *tac, int function_pc, int expect_preg);
void make_stack_offsets(Function *function);
void add_final_instructions(Function *function);
void optimize_final_instructions(Function *function);
void merge_rsp_func_call_add_subs(Function *function);
void output_defined_object_symbol(Symbol *symbol);
void output_code(char *input_filename, char *output_filename);

#endif
