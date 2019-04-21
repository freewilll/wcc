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
} Set;

typedef struct stack {
    int *elements;
    int pos;
} Stack;

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

typedef struct symbol {
    int type;                             // Type
    int size;                             // Size
    char *identifier;                     // Identifier
    int scope;                            // Scope
    long value;                           // Value in the case of a constant
    int local_index;                      // For locals variables and function arguments
    int is_function;                      // Is the symbol a function?
    int is_enum;                          // Enums are symbols with a value
    struct function *function;            // Details specific to symbols that are functions
} Symbol;

typedef struct function {
    int param_count;                      // Number of parameters
    int local_symbol_count;               // Number of local symbols
    int vreg_count;                       // Number of virtual registers used in IR
    int spilled_register_count;           // Amount of stack space needed for registers spills
    int call_count;                       // Number of calls to other functions
    int is_defined;                       // if a definition has been found
    int builtin;                          // For builtin functions, IR number of the builtin
    int is_variadic;                      // Set to 1 for builtin variadic functions
    struct three_address_code *ir;        // Intermediate representation
    Graph *cfg;                           // Control flow graph
    Block *blocks;                        // For functions, the blocks
    Set **dominance;                      // Block dominances
    Set **uevar;                          // The upward exposed set for each block
    Set **varkill;                        // The killed var set for each block
    Set **liveout;                        // The liveout set for each block
    int *idom;                            // Immediate dominator for each block
    Set **dominance_frontiers;            // Dominance frontier for each block
    Set **var_blocks;                     // Var/block associations for vars that are written to
    Set *globals;                         // All variables that are assigned to
    Set **phi_functions;                  // All variables that need phi functions for each block
    char *interference_graph;             // The interference graph of live ranges, in a lower diagonal matrix
    struct vreg_location *vreg_locations; // Allocated physical registers and spilled stack indexes
    int *spill_cost;                      // The estimated spill cost for each live range
} Function;

// Value is a value on the value stack. A value can be one of
// - global
// - local
// - constant
// - string literal
// - register
typedef struct value {
    int type;                       // Type
    int vreg;                       // Optional vreg number
    int preg;                       // Allocated physical register
    int is_lvalue;                  // Is the value an lvalue?
    int local_index;                // For locals variables and function arguments
    int stack_index;                // Allocated stack index in case of a spill
    int is_constant;                // Is it a constant? If so, value is the value.
    int is_string_literal;          // Is the value a string literal?
    int is_in_cpu_flags;            // Is the result stored in cpu flags?
    int string_literal_index;       // Index in the string_literals array in the case of a string literal
    long value;                     // Value in the case of a constant
    Symbol *function_symbol;        // Corresponding symbol in the case of a function call
    int function_call_arg_count;    // Number of arguments in the case of a function call
    Symbol *global_symbol;          // Pointer to a global symbol if the value is a global symbol
    int label;                      // Target label in the case of jump instructions
    int pushed_stack_aligned_quad;  // Used in code generation to remember if an additional quad was pushed to align the stack for a function call
    int ssa_subscript;              // Optional SSA enumeration
    int live_range;                 // Optional SSA live range
    int x86_size;                   // Current size while generating x86 code
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

// Struct member
typedef struct struct_member {
    char *identifier;
    int type;
    int offset;
} StructMember;

// Struct description
typedef struct struct_desc {
    int type;
    char *identifier;
    struct struct_member **members;
    int size;
    int is_incomplete;          // Set to 1 if the struct has been used in a member but not yet declared
} Struct;

typedef struct typedef_desc {
    char *identifier;
    int struct_type;
} Typedef;

enum {
    DATA_SIZE                     = 10485760,
    INSTRUCTIONS_SIZE             = 10485760,
    SYMBOL_TABLE_SIZE             = 10485760,
    MAX_STRUCTS                   = 1024,
    MAX_TYPEDEFS                  = 1024,
    MAX_STRUCT_MEMBERS            = 1024,
    MAX_INPUT_SIZE                = 10485760,
    MAX_STRING_LITERALS           = 10240,
    VALUE_STACK_SIZE              = 10240,
    MAX_VREG_COUNT                = 10240,
    PHYSICAL_REGISTER_COUNT       = 15,
    MAX_SPILLED_REGISTER_COUNT    = 1024,
    MAX_INPUT_FILENAMES           = 1024,
    MAX_BLOCKS                    = 1024,
    MAX_BLOCK_EDGES               = 1024,
    MAX_STACK_SIZE                = 10240,
    MAX_BLOCK_PREDECESSOR_COUNT   = 128,
    MAX_GRAPH_EDGE_COUNT          = 10240,
};

enum {
    DEBUG_SSA_MAPPING_LOCAL_STACK_INDEXES = 0,
    DEBUG_SSA                             = 0,
    DEBUG_SSA_LIVEOUT                     = 0,
    DEBUG_SSA_CFG                         = 0,
    DEBUG_SSA_IDOM                        = 0,
    DEBUG_SSA_PHI_INSERTION               = 0,
    DEBUG_SSA_PHI_RENUMBERING             = 0,
    DEBUG_SSA_LIVE_RANGE                  = 0,
    DEBUG_SSA_INTERFERENCE_GRAPH          = 0,
    DEBUG_SSA_LIVE_RANGE_COALESCING       = 0,
    DEBUG_SSA_SPILL_COST                  = 0,
    DEBUG_SSA_TOP_DOWN_REGISTER_ALLOCATOR = 0,
    DEBUG_INSTSEL_IGRAPHS_DEEP            = 0,
    DEBUG_INSTSEL_IGRAPHS                 = 0,
    DEBUG_INSTSEL_TILING                  = 0,
    DEBUG_SIGN_EXTENSION                  = 0,
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
    TOK_TYPEDEF,
    TOK_TYPEDEF_TYPE,
    TOK_WHILE,
    TOK_FOR,
    TOK_CONTINUE,
    TOK_BREAK,
    TOK_RETURN,
    TOK_ENUM,           // 20
    TOK_SIZEOF,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_RCURLY,
    TOK_LCURLY,
    TOK_SEMI,
    TOK_COMMA,
    TOK_EQ,
    TOK_PLUS_EQ,        // 30
    TOK_MINUS_EQ,
    TOK_TERNARY,
    TOK_COLON,
    TOK_OR,
    TOK_AND,
    TOK_BITWISE_OR,
    TOK_XOR,
    TOK_ADDRESS_OF,
    TOK_DBL_EQ,
    TOK_NOT_EQ,         // 40
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_BITWISE_LEFT,
    TOK_BITWISE_RIGHT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,         // 50
    TOK_MOD,
    TOK_LOGICAL_NOT,
    TOK_BITWISE_NOT,
    TOK_INC,
    TOK_DEC,
    TOK_DOT,
    TOK_ARROW,
    TOK_RBRACKET,
    TOK_LBRACKET,       // 60
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
    IR_LOAD_CONSTANT=1,       // Load constant
    IR_LOAD_STRING_LITERAL,   // Load string literal
    IR_LOAD_VARIABLE,         // Load global or local
    IR_INDIRECT,              // Pointer or lvalue dereference
    IR_START_CALL,            // Function call
    IR_ARG,                   // Function call argument
    IR_CALL,                  // Start of function call
    IR_END_CALL,              // End of function call
    IR_RETURN,                // Return in function
    IR_START_LOOP,            // Start of a for or while loop
    IR_END_LOOP,              // End of a for or while loop
    IR_ASSIGN,                // Assignment/store. Target is either a global, local, lvalue in register or register
    IR_ASSIGN_TO_REG_LVALUE,  // Assignment to an lvalue in a register
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
    IR_EXIT,                  // Builtin functions
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

int verbose;                    // Print invoked program command lines
int compile;                    // Compile .c file
int run_assembler;              // Assemble .s file
int run_linker;                 // Link .o file
int target_is_object_file;
int target_is_assembly_file;
int print_spilled_register_count;
int print_ir1;                          // Print IR after parsing
int print_ir2;                          // Print IR after x84_64 arch manipulation
int print_ir3;                          // Print IR after register allocation
int output_inline_ir;                   // Output IR inline with the assembly
int instruction_selection_wip;          // Experimental instruction selection
int ssa_physical_register_count;        // Experimental physical register count
int opt_enable_register_coalescing;     // Merge registers that can be reused within the same operation
int opt_enable_live_range_coalescing;   // Merge live ranges where possible
int opt_spill_furthest_liveness_end;    // Prioritize spilling physical registers with furthest liveness end
int opt_short_lr_infinite_spill_costs;  // Don't spill short live ranges
int opt_optimize_arithmetic_operations; // Optimize arithmetic operations

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

int cur_token;                  // Current token
char *cur_identifier;           // Current identifier if the token is an identifier
int cur_type;                   // Associated type if the current token is a typedef
long cur_long;                  // Current long if the token is a number
char *cur_string_literal;       // Current string literal if the token is a string literal
int cur_scope;                  // Current scope. 0 is global. non-zero is function. Nested scopes isn't implemented.
char **string_literals;         // Each string literal has an index in this array, with a pointer to the string literal
int string_literal_count;       // Amount of string literals

Symbol *cur_function_symbol;     // Currently parsed function
Value *cur_loop_continue_dst;    // Target jmp of continue statement in the current for/while loop
Value *cur_loop_break_dst;       // Target jmp of break statement in the current for/while loop

Symbol *symbol_table;    // Symbol table, terminated by a null symbol
Symbol *next_symbol;     // Next free symbol in the symbol table

Value **vs_start;        // Value stack start
Value **vs;              // Value stack current position
Value *vtop;             // Value at the top of the stack

Struct **all_structs;     // All structs defined globally. Local struct definitions isn't implemented.
int all_structs_count;    // Number of structs, complete and incomplete

Typedef **all_typedefs;   // All typedefs defined globally. Local typedef definitions isn't implemented.
int all_typedefs_count;   // Number of typedefs

Tac *ir_start, *ir;               // intermediate representation for currently parsed function
int vreg_count;                   // Virtual register count for currently parsed function
int label_count;                  // Global label count, always growing
int function_call_count;          // Uniquely identify a function call, always growing
int cur_loop;                     // Current loop being parsed
int loop_count;                   // Loop counter
int spilled_register_count;       // Spilled register count for current function that's undergoing register allocation
int total_spilled_register_count; // Spilled register count for all functions
int *callee_saved_registers;      // Constant list of length PHYSICAL_REGISTER_COUNT. Set to 1 for registers that must be preserved in function calls.
int cur_stack_push_count;         // Used in codegen to keep track of stack position

void *f; // Output file handle

// set.c
Set *new_set(int max_value);
void free_set(Set *s);
void empty_set(Set *s);
Set *copy_set(Set *s);
void copy_set_to(Set *dst, Set *src);
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

// graph.c
void init_graph(Graph *g, int node_count, int edge_count);
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
Value *load_constant(Value *cv);
int get_type_alignment(int type);
int get_type_size(int type);
int new_vreg();
Symbol *new_symbol();
int parse_struct_base_type(int parse_struct_base_type);
void check_incomplete_structs();
void expression(int level);
void finish_parsing_header();
void parse();

// ir.c
void init_value(Value *v);
Value *new_value();
Value *new_constant(int type, long value);
Value *dup_value(Value *src);
void add_tac_to_ir(Tac *tac);
Tac *new_instruction(int operation);
Tac *add_instruction(int operation, Value *dst, Value *src1, Value *src2);
void sanity_test_ir_linkage(Tac *ir);
int new_vreg();
void fprintf_escaped_string_literal(void *f, char* sl);
void print_value(void *f, Value *v, int is_assignment_rhs);
void print_instruction(void *f, Tac *tac);
void print_intermediate_representation(Function *function, char *name);
void optimize_ir(Symbol *function);

// ssa.c
enum {
    RESERVED_PHYSICAL_REGISTER_COUNT = 7,

    // Liveness interval indexes corresponding to reserved physical registers
    LIVE_RANGE_PREG_RAX_INDEX = 1,
    LIVE_RANGE_PREG_RCX_INDEX,
    LIVE_RANGE_PREG_RDX_INDEX,
    LIVE_RANGE_PREG_RSI_INDEX,
    LIVE_RANGE_PREG_RDI_INDEX,
    LIVE_RANGE_PREG_R8_INDEX,
    LIVE_RANGE_PREG_R9_INDEX,
};

// Equal to RESERVED_PHYSICAL_REGISTER_COUNT in normal usage. Set to zero in unit test for convenience
int live_range_reserved_pregs_offset;
int disable_live_ranges_coalesce;
int preg_count;
int ir_vreg_offset;

int make_vreg_count(Function *function, int starting_count);
int new_subscript(Stack **stack, int *counters, int n);
void make_uevar_and_varkill(Function *function);
void make_liveout(Function *function);
void rename_phi_function_variables(Function *function);
void make_live_ranges(Function *function);
void rename_vars(Function *function, Stack **stack, int *counters, int block_number, int vreg_count);
void make_live_ranges(Function *function);
void make_control_flow_graph(Function *function);
void make_block_dominance(Function *function);
void make_liveout(Function *function);
void make_live_range_spill_cost(Function *function);
void coalesce_live_ranges(Function *function);
void init_allocate_registers();
void add_ig_edge(char *ig, int vreg_count, int to, int from);
void allocate_registers_top_down(Function *function, int physical_register_count);
void do_oar1(Function *function);
void do_oar1a(Function *function);
void do_oar1b(Function *function);
void do_oar2(Function *function);
void do_oar3(Function *function);
void do_oar4(Function *function);
void optimize_and_allocate_registers(Function *function);

// instrsel.c
enum {
    MAX_RULE_COUNT = 2000,

    // Non terminals
    CST = 1,                     // Constant
    STL,                         // String literal
    LAB,                         // Label, i.e. a target for a (conditional) jump
    REG, REGB, REGW, REGL, REGQ, // Registers
    GLB, GLBB, GLBW, GLBL, GLBQ, // Globals
    STK, STKB, STKW, STKL, STKQ, // Stack locations

    // Operands
    DST,
    SRC1,
    SRC2,

    // x86 instructions
    X_START = 1000,
    X_RET   = 1001,
    X_ARG   = 1002,
    X_MOV   = 1003,
    X_LEA   = 1004,
    X_ADD   = 1005,
    X_MUL   = 1006,
    X_CMP   = 1007,
    X_CMPZ  = 1008,
    X_JZ    = 1009,
    X_JNZ   = 1010,
    X_JMP   = 1011,

    X_JE    = 1012,
    X_JNE   = 1013,
    X_JLT   = 1016,
    X_JGT   = 1017,
    X_JLE   = 1018,
    X_JGE   = 1019,

    X_MOVZBW  = 1020,
    X_MOVZBL  = 1021,
    X_MOVZBQ  = 1022,
    X_SETE    = 1023,
    X_SETNE   = 1024,
    X_SETLT   = 1025,
    X_SETGT   = 1026,
    X_SETLE   = 1027,
    X_SETGE   = 1028,

    X_MOVSBW   = 1029,
    X_MOVSBL   = 1030,
    X_MOVSBQ   = 1031,
    X_MOVSWL   = 1032,
    X_MOVSWQ   = 1033,
    X_MOVSLQ   = 1034,

};

typedef struct rule {
    int index;
    int non_terminal;
    int operation;
    int src1;
    int src2;
    int cost;
    struct x86_operation *x86_operations;
} Rule;

typedef struct x86_operation {
    int operation;
    int dst, v1, v2;
    char *template;
    struct x86_operation *next;
} X86Operation;

int instr_rule_count;
Rule *instr_rules;

void eis1(Function *function, int flip_jz_jnz);
void eis2(Function *function);
void experimental_instruction_selection(Symbol *function_symbol);

// instrrules.c
char size_to_x86_size(int size);
void print_rule(Rule *r);
void print_rules();
void make_value_x86_size(Value *v);
void add_x86_instruction(X86Operation *x86op, Value *dst, Value *v1, Value *v2);
void init_instruction_selection_rules();

// codegen.c
char *render_x86_operation(Tac *tac, int function_pc, int stack_start, int expect_preg);
void init_callee_saved_registers();
void output_function_body_code(Symbol *symbol);
void output_code(char *input_filename, char *output_filename);

// test-utils.c
Tac *i(int label, int operation, Value *dst, Value *src1, Value *src2);
Value *v(int vreg);
Value *vsz(int vreg, int type);
Value *l(int label);
Value *c(int value);
Value *s(int string_literal_index);
Value *S(int stack_index);
Value *Ssz(int stack_index, int type);
Value *g(int index);
Value *gsz(int index, int type);
