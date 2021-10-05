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

typedef struct map {
    char **keys;
    void **values;
    int size;
    int used_count;
    int element_count;
} Map;

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

typedef struct type {
    int type;       // One of TYPE_*
    int is_unsigned;
    int array_size;
    int is_const;
    int is_volatile;
    struct type *target;
    struct struct_or_union_desc *struct_or_union_desc;
    struct function *function;
} Type;

typedef struct symbol {
    Type *type;                 // Type
    int size;                   // Size
    char *identifier;           // Identifier
    struct scope* scope;        // Scope
    long value;                 // Value in the case of a constant
    int local_index;            // Used by the parser for locals variables and function arguments
                                // < 0 is a local variable or tempoary, >= 2 is a function parameter
    int is_enum;                // Enums are symbols with a value
} Symbol;

typedef struct tag {
    Type *type;                 // Type
    char *identifier;           // Identifier
} Tag;

typedef struct scope {
    struct scope *parent;       // Parent scope, zero if it's the global scope
    int max_count;              // Maximum amount of symbols or tags memory is allocated for
    Symbol **symbols;           // Symbol list
    int symbol_count;           // Count of symbols
    Tag **tags;                 // Struct, union or enum tags
    int tag_count;              // Count of tags
} Scope;

typedef struct function {
    Type *return_type;                       // Type of return value
    int param_count;                         // Number of parameters
    Type **param_types;                      // Types of parameters
    int local_symbol_count;                  // Number of local symbols, used by the parser
    int vreg_count;                          // Number of virtual registers used in IR
    int stack_register_count;                // Amount of stack space needed for registers spills
    int stack_size;                          // Size of the stack
    int is_defined;                          // if a definition has been found
    int is_external;                         // Has external linkage
    int is_static;                           // Is a private function in the translation unit
    int is_variadic;                         // Set to 1 for builtin variadic functions
    Scope *scope;                            // Scope, starting with the parameters
    struct three_address_code *ir;           // Intermediate representation
    Graph *cfg;                              // Control flow graph
    Block *blocks;                           // For functions, the blocks
    Set **dominance;                         // Block dominances
    Set **uevar;                             // The upward exposed set for each block
    Set **varkill;                           // The killed var set for each block
    Set **liveout;                           // The liveout set for each block
    int *idom;                               // Immediate dominator for each block
    Set **dominance_frontiers;               // Dominance frontier for each block
    Set **var_blocks;                        // Var/block associations for vars that are written to
    Set *globals;                            // All variables that are assigned to
    Set **phi_functions;                     // All variables that need phi functions for each block
    char *interference_graph;                // The interference graph of live ranges, in a lower diagonal matrix
    struct vreg_location *vreg_locations;    // Allocated physical registers and spilled stack indexes
    int *spill_cost;                         // The estimated spill cost for each live range
    char *preferred_live_range_preg_indexes; // Preferred physical register, when possible
    char *vreg_preg_classes;                 // Preg classes for all vregs
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
    int arg_count;
    int single_int_register_arg_count;
    int single_sse_register_arg_count;
    int biggest_alignment;
    int offset;
    int padding;
    int size;
    FunctionParamLocations *params;
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
    int is_lvalue;                                       // Is the value an lvalue?
    int is_lvalue_in_register;                           // Is the value an lvalue in a register?
    int local_index;                                     // Used by parser for local variable and temporaries and function arguments
    int stack_index;                                     // stack index in case of a pushed function argument, & usage or register spill
                                                         // < 0 is for locals, temporaries and spills. >= 2 is for pushed arguments, that start at 2.
    int stack_offset;                                    // Position on the stack
    int spilled;                                         // 1 if spilled
    int is_constant;                                     // Is it a constant? If so, value is the value.
    int is_string_literal;                               // Is the value a string literal?
    int string_literal_index;                            // Index in the string_literals array in the case of a string literal
    long int_value;                                      // Value in the case of an integer constant
    long double fp_value;                                // Value in the case of a floating point constant
    int offset;                                          // For composite objects, offset from the start of the object's memory
    Symbol *function_symbol;                             // Corresponding symbol in the case of a function call
    int is_function_call_arg;                            // Is it a function call argument?
    int is_function_param;                               // Is it a function parameter?
    int live_range_preg;                                 // This value is bound to a physical register
    int function_param_original_stack_index;             // Original stack index for function parameter pushed onto the stack
    int function_call_arg_index;                         // Index of the argument (0=leftmost)
    FunctionParamLocations *function_call_arg_locations; // Destination of the arg, either a single int or sse register, or in the case of a struct, a list of locations
    int function_call_sse_register_arg_index;            // Index of the argument in integer registers going left to right (0=leftmost). Set to -1 if it's on the stack.
    int function_call_arg_stack_padding;                 // Extra initial padding needed to align the function call argument pushed arguments
    int function_call_arg_push_count;                    // Number of arguments pushed on the stack
    int function_call_sse_register_arg_count;            // Number of SSE (xmm) arguments in registers
    Symbol *global_symbol;                               // Pointer to a global symbol if the value is a global symbol
    int label;                                           // Target label in the case of jump instructions
    int ssa_subscript;                                   // Optional SSA enumeration
    int live_range;                                      // Optional SSA live range
    char preferred_live_range_preg_index;                // Preferred physical register
    int x86_size;                                        // Current size while generating x86 code
    int non_terminal;                                    // Use in rule matching
    char has_been_renamed;                               // Used in renaming and stack renumbering code
} Value;

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
} Tac;

// Temporary struct for reversing function call arguments
typedef struct tac_interval {
    Tac *start;
    Tac *end;
} TacInterval;

// Struct/union member
typedef struct struct_or_union_member {
    char *identifier;
    Type *type;
    int offset;
} StructOrUnionMember;

// Struct & union description
typedef struct struct_or_union_desc {
    char *identifier;
    int size;
    int is_incomplete;          // Set to 1 if the struct has been used in a member but not yet declared
    int is_packed;
    int is_union;
    struct struct_or_union_member **members;
} StructOrUnion;

typedef struct typedef_desc {
    char *identifier;
    Type *struct_type;
} Typedef;

enum {
    MAX_STRUCTS_AND_UNIONS        = 1024,
    MAX_TYPEDEFS                  = 1024,
    MAX_STRUCT_MEMBERS            = 1024,
    MAX_INPUT_SIZE                = 10485760,
    MAX_STRING_LITERAL_SIZE       = 4095,
    MAX_STRING_LITERALS           = 10240,
    MAX_FLOATING_POINT_LITERALS   = 10240,
    MAX_FUNCTION_CALL_ARGS        = 253,
    VALUE_STACK_SIZE              = 10240,
    MAX_VREG_COUNT                = 20480,
    PHYSICAL_REGISTER_COUNT       = 32, // integer + xmm
    PHYSICAL_INT_REGISTER_COUNT   = 12, // Available registers for integers
    PHYSICAL_SSE_REGISTER_COUNT   = 14, // Available registers for floating points
    MAX_SPILLED_REGISTER_COUNT    = 1024,
    MAX_INPUT_FILENAMES           = 1024,
    MAX_BLOCKS                    = 1024,
    MAX_BLOCK_EDGES               = 1024,
    MAX_STACK_SIZE                = 10240,
    MAX_BLOCK_PREDECESSOR_COUNT   = 128,
    MAX_GRAPH_EDGE_COUNT          = 10240,
    MAX_GLOBAL_SCOPE_IDENTIFIERS  = 4095,
    MAX_LOCAL_SCOPE_IDENTIFIERS   = 511,
};

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
    TOK_VOID,               // 10
    TOK_CHAR,
    TOK_INT,
    TOK_SHORT,
    TOK_LONG,
    TOK_FLOAT,
    TOK_DOUBLE,
    TOK_STRUCT,
    TOK_UNION,
    TOK_TYPEDEF,
    TOK_TYPEDEF_TYPE,
    TOK_DO,                 // 20
    TOK_WHILE,
    TOK_FOR,
    TOK_CONTINUE,
    TOK_BREAK,
    TOK_RETURN,
    TOK_ENUM,
    TOK_SIZEOF,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_RCURLY,             // 30
    TOK_LCURLY,
    TOK_SEMI,
    TOK_COMMA,
    TOK_EQ,
    TOK_PLUS_EQ,
    TOK_MINUS_EQ,
    TOK_MULTIPLY_EQ,
    TOK_DIVIDE_EQ,
    TOK_MOD_EQ,
    TOK_BITWISE_AND_EQ,     // 40
    TOK_BITWISE_OR_EQ,
    TOK_BITWISE_XOR_EQ,
    TOK_BITWISE_RIGHT_EQ,
    TOK_BITWISE_LEFT_EQ,
    TOK_TERNARY,
    TOK_COLON,
    TOK_OR,
    TOK_AND,
    TOK_BITWISE_OR,
    TOK_XOR,                // 50
    TOK_ADDRESS_OF,
    TOK_DBL_EQ,
    TOK_NOT_EQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_BITWISE_LEFT,
    TOK_BITWISE_RIGHT,
    TOK_PLUS,               // 60
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_MOD,
    TOK_LOGICAL_NOT,
    TOK_BITWISE_NOT,
    TOK_INC,
    TOK_DEC,
    TOK_DOT,
    TOK_ARROW,              // 70
    TOK_RBRACKET,
    TOK_LBRACKET,
    TOK_ATTRIBUTE,
    TOK_PACKED,
    TOK_HASH,
    TOK_INCLUDE,
    TOK_DEFINE,
    TOK_UNDEF,
    TOK_IFDEF,
    TOK_ENDIF,              // 80
    TOK_EXTERN,
    TOK_STATIC,
    TOK_CONST,
    TOK_VOLATILE,
    TOK_ELLIPSES
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
    TYPE_FUNCTION        = 12
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
    IR_START_CALL,            // Function call
    IR_ARG,                   // Function call argument
    IR_ARG_STACK_PADDING,     // Extra padding push to align arguments pushed onto the stack
    IR_CALL_ARG_REG,          // Placeholder for fake read of a register used in function calls
    IR_CALL,                  // Start of function call
    IR_END_CALL,              // End of function call
    IR_ALLOCATE_STACK,        // Allocate stack space for a function call argument on the stack
    IR_RETURN,                // Return in function
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
    IR_BNOT,                  // Binary not ~
    IR_BOR,                   // Binary or |
    IR_BAND,                  // Binary and &
    IR_XOR,                   // Binary xor ^
    IR_BSHL,                  // Binary shift left <<
    IR_BSHR,                  // Binary shift right >>
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

typedef struct floating_point_literal {
    int type;
    float f;
    double d;
    long double ld;
} FloatingPointLiteral;

char *cur_filename;             // Current filename being lexed
int cur_line;                   // Current line number being lexed

int print_ir1;                          // Print IR after parsing
int print_ir2;                          // Print IR after register allocation
int opt_enable_vreg_renumbering;        // Renumber vregs so the numbers are consecutive
int opt_enable_register_coalescing;     // Merge registers that can be reused within the same operation
int opt_enable_live_range_coalescing;   // Merge live ranges where possible
int opt_spill_furthest_liveness_end;    // Prioritize spilling physical registers with furthest liveness end
int opt_short_lr_infinite_spill_costs;  // Don't spill short live ranges
int opt_optimize_arithmetic_operations; // Optimize arithmetic operations
int opt_enable_preferred_pregs;         // Enable preferred preg selection in register allocator

char *input;                    // Input file data
int input_size;                 // Size of the input file
int ip;                         // Offset into *input, used by the lexer

// Copies of the above, for when a header is being parsed
char *c_input;                  // Input file data
int c_input_size;               // Size of the input file
int c_ip;                       // Offset into *input, used by the lexer
char *c_cur_filename;           // Current filename being lexed
int c_cur_line;                 // Current line number being lexed

int parsing_header;             // I a header being parsed?
Map *directives;                // Map of CPP directives
int cur_token;                  // Current token
char *cur_identifier;           // Current identifier if the token is an identifier
char *cur_type_identifier;      // Identifier of the last parsed declarator
Type *cur_lexer_type;           // A type determined by the lexer
long cur_long;                  // Current long if the token is an integral type
long double cur_long_double;    // Current long double if the token is a floating point type
char *cur_string_literal;       // Current string literal if the token is a string literal
int in_ifdef;                   // In ifdef inclusion
int in_ifdef_else;              // In ifdef exclusion
Scope *cur_scope;               // Current scope.
char **string_literals;         // Each string literal has an index in this array, with a pointer to the string literal
int string_literal_count;       // Amount of string literals

FloatingPointLiteral *floating_point_literals; // Each floating point literal has an index in this array
int floating_point_literal_count;              // Amount of floating point literals

Symbol *cur_function_symbol;     // Currently parsed function
Value *cur_loop_continue_dst;    // Target jmp of continue statement in the current for/while loop
Value *cur_loop_break_dst;       // Target jmp of break statement in the current for/while loop

Value **vs_start;        // Value stack start
Value **vs;              // Value stack current position
Value *vtop;             // Value at the top of the stack

Typedef **all_typedefs;   // All typedefs defined globally. Local typedef definitions isn't implemented.
int all_typedefs_count;   // Number of typedefs

Tac *ir_start, *ir;               // intermediate representation for currently parsed function
int label_count;                  // Global label count, always growing
int cur_loop;                     // Current loop being parsed
int loop_count;                   // Loop counter
int stack_register_count;         // Spilled register count for current function that's undergoing register allocation
int total_stack_register_count;   // Spilled register count for all functions
int *callee_saved_registers;      // Constant list of length PHYSICAL_REGISTER_COUNT. Set to 1 for registers that must be preserved in function calls.
int cur_stack_push_count;         // Used in codegen to keep track of stack position

void *f; // Output file handle

int warn_integer_constant_too_large;
int debug_function_param_allocation;
int debug_function_arg_mapping;
int debug_function_param_mapping;
int debug_ssa_mapping_local_stack_indexes;
int debug_ssa;
int debug_ssa_liveout;
int debug_ssa_cfg;
int debug_ssa_idom;
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

// set.c
Set *new_set(int max_value);
void free_set(Set *s);
void empty_set(Set *s);
Set *copy_set(Set *s);
void copy_set_to(Set *dst, Set *src);
void cache_set_elements(Set *s);
int set_len(Set *s);
void print_set(Set *s);
void *add_to_set(Set *s, int value);
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
Stack *new_stack();
int stack_top(Stack *s);
void push_onto_stack(Stack *s, int v);
int pop_from_stack(Stack *s);

// map.c
Map *new_map();
void *map_get(Map *map, char *key);
void map_put(Map *map, char *key, void *value);
void map_delete(Map *map, char *key);

// graph.c
Graph *new_graph(int node_count, int edge_count);
void dump_graph(Graph *g);
GraphEdge *add_graph_edge(Graph *g, int from, int to);

// utils.c
void panic(char *message);
void panic1d(char *fmt, int i);
void panic1s(char *fmt, char *s);
void panic2d(char *fmt, int i1, int i2);
void panic2s(char *fmt, char *s1, char *s2);
Function *new_function();

// lexer.c
void init_lexer(char *filename);
void next();
void expect(int token, char *what);
void consume(int token, char *what);

// parser.c
Symbol *memcpy_symbol;
Type *operation_type(Value *src1, Value *src2, int for_ternary);
Value *load_constant(Value *cv);
int new_vreg();
Type *parse_type_name();
void finish_parsing_header();
void parse();
void dump_symbols();
void init_parser();

// scopes.c
Scope *global_scope;

void init_scopes();
void enter_scope();
void exit_scope();
Symbol *new_symbol();
Symbol *lookup_symbol(char *name, Scope *scope, int recurse);
Tag *new_tag();
Tag *lookup_tag(char *name, Scope *scope, int recurse);

// types.c
int print_type(void *f, Type *type);
char *sprint_type_in_english(Type *type);
Type *new_type(int type);
StructOrUnion *dup_struct_or_union(StructOrUnion *src);
Type *dup_type(Type *src);
Type *make_pointer(Type *src);
Type *make_pointer_to_void();
Type *deref_pointer(Type *type);
int is_integer_type(Type *type);
int is_floating_point_type(Type *type);
int is_sse_floating_point_type(Type *type);
int is_arithmetic_type(Type *type);
int is_scalar_type(Type *type);
int is_object_type(Type *type);
int is_incomplete_type(Type *type);
int is_pointer_type(Type *type);
int is_pointer_to_object_type(Type *type);
int is_null_pointer(Value *v);
int is_pointer_to_void(Type *type);
int type_fits_in_single_int_register(Type *type);
int get_type_size(Type *type);
int get_type_alignment(Type *type);
int type_eq(Type *type1, Type *type2);
int types_are_compabible(Type *type1, Type *type2);
int is_integer_operation_result_unsigned(Type *src1, Type *src2);
Type *make_struct_or_union_type(StructOrUnion *s);
void complete_struct_or_union(StructOrUnion *s);

// ir.c
void init_value(Value *v);
Value *new_value();
Value *new_integral_constant(int type_type, long value);
Value *new_floating_point_constant(int type_type, long double value);
Value *dup_value(Value *src);
void add_tac_to_ir(Tac *tac);
Tac *new_instruction(int operation);
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2);
void insert_instruction(Tac *ir, Tac *tac, int move_label);
void insert_instruction_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2, int move_label);
Tac *insert_instruction_after(Tac *ir, Tac *tac);
Tac *insert_instruction_after_from_operation(Tac *ir, int operation, Value *dst, Value *src1, Value *src2);
Tac *delete_instruction(Tac *tac);
void sanity_test_ir_linkage(Function *function);
int make_function_call_count(Function *function);
int new_vreg();
int fprintf_escaped_string_literal(void *f, char* sl);
int print_value(void *f, Value *v, int is_assignment_rhs);
char *operation_string(int operation);
void print_instruction(void *f, Tac *tac, int expect_preg);
void print_ir(Function *function, char* name, int expect_preg);
void reverse_function_argument_order(Function *function);
void merge_consecutive_labels(Function *function);
void renumber_labels(Function *function);
void allocate_value_vregs(Function *function);
void convert_long_doubles_jz_and_jnz(Function *function);
void move_long_doubles_to_the_stack(Function *function);
void make_stack_register_count(Function *function);
void allocate_value_stack_indexes(Function *function);
void remove_unused_function_call_results(Function *function);
void process_struct_and_union_copies(Function *function);
void add_memory_copy(Function *function, Tac *ir, Value *dst, Value *src1, int size);

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
};

int live_range_reserved_pregs_offset;

void optimize_arithmetic_operations(Function *function);
void rewrite_lvalue_reg_assignments(Function *function);
void make_control_flow_graph(Function *function);
void make_block_dominance(Function *function);
void analyze_dominance(Function *function);
int make_vreg_count(Function *function, int starting_count);
void make_uevar_and_varkill(Function *function);
void make_liveout(Function *function);
void make_globals_and_var_blocks(Function *function);
void insert_phi_functions(Function *function);
void rename_phi_function_variables(Function *function);
void make_live_ranges(Function *function);
void blast_vregs_with_live_ranges(Function *function);
void add_ig_edge(char *ig, int vreg_count, int to, int from);
void coalesce_live_ranges(Function *function, int check_register_constraints);
void make_preferred_live_range_preg_indexes(Function *function);

// functions.c
void add_function_call_result_moves(Function *function);
void add_function_return_moves(Function *function);
void add_function_call_arg_moves(Function *function);
void add_function_param_moves(Function *function, char *identifier);
Value *make_function_call_value(int function_call);
FunctionParamAllocation *init_function_param_allocaton(char *function_identifier);
void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type);
void finalize_function_param_allocation(FunctionParamAllocation *fpa);

// regalloc.c
int *int_arg_registers;
int *sse_arg_registers;

void compress_vregs(Function *function);
void init_vreg_locations(Function *function);
void allocate_registers_top_down(Function *function, int live_range_start, int physical_register_count, int preg_class);
void allocate_registers(Function *function);
void init_allocate_registers();

// instrsel.c
enum {
    MAX_RULE_COUNT = 5000,

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
    MLD5,                        // 16-byte memory, for long double
    MPV,                         // Pointer in memory
    STR,                         // Struct

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
    X_SHL,
    X_SAR,
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
    long hash;
    int operation;
    int dst;
    int src1;
    int src2;
    int cost;
    struct x86_operation *x86_operations;
} Rule;

typedef struct x86_operation {
    int operation;
    int dst, v1, v2;
    char *template;
    int save_value_in_slot;           // Slot number to save a value in
    int allocate_stack_index_in_slot; // Allocate a stack index and put in slot
    int allocate_register_in_slot;    // Allocate a register and put in slot
    int allocate_label_in_slot;       // Allocate a label and put in slot
    int allocated_type;               // Type to use to determine the allocated stack size
    int arg;                          // The argument (src1 or src2) to load/save
    struct x86_operation *next;
} X86Operation;


int instr_rule_count;
int disable_merge_constants;
Rule *instr_rules;
Value **saved_values;
char *rule_coverage_file;
Set *rule_coverage;

void select_instructions(Function *function);
void remove_vreg_self_moves(Function *function);
void remove_stack_self_moves(Function *function);
void add_spill_code(Function *function);

// instrutil.c
X86Operation *dup_x86_operation(X86Operation *operation);
char size_to_x86_size(int size);
char *non_terminal_string(int nt);
void print_rule(Rule *r, int print_operations, int indent);
void print_rules();
void make_value_x86_size(Value *v);
int match_value_to_rule_src(Value *v, int src);
int match_value_to_rule_dst(Value *v, int dst);
int match_value_type_to_rule_dst(Value *v, int dst);
char *value_to_non_terminal_string(Value *v);
int make_x86_size_from_non_terminal(int non_terminal);
Tac *add_x86_instruction(X86Operation *x86op, Value *dst, Value *v1, Value *v2);
void check_rules_dont_decrease_precision();
Rule *add_rule(int dst, int operation, int src1, int src2, int cost);
X86Operation *add_op(Rule *r, int operation, int dst, int v1, int v2, char *template);
void add_save_value(Rule *r, int arg, int slot);
void add_allocate_stack_index_in_slot(Rule *r, int slot, int type);
void add_allocate_register_in_slot(Rule *r, int slot, int type);
void add_allocate_label_in_slot(Rule *r, int slot);
void fin_rule(Rule *r);
void check_for_duplicate_rules();
void write_rule_coverage_file();

// instrules.c
int disable_check_for_duplicate_rules;

void init_instruction_selection_rules();

// codegen.c
char *register_name(int preg);
char *render_x86_operation(Tac *tac, int function_pc, int expect_preg);
void make_stack_offsets(Function *function, char *function_name);
void add_final_x86_instructions(Function *function, char *function_name);
void remove_nops(Function *function);
void merge_rsp_func_call_add_subs(Function *function);
void output_code(char *input_filename, char *output_filename);

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

char *make_temp_filename(char *template);
void run_compiler_phases(Function *function, char *function_name, int start_at, int stop_at);
void compile(char *compiler_input_filename, char *compiler_output_filename);

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
Value *make_arg_src1();

void start_ir();
void finish_register_allocation_ir(Function *function);
void finish_ir(Function *function);
void finish_spill_ir(Function *function);

// Autogenerated externals.c
char *externals();

