#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

struct symbol {
    int type;
    char *identifier;
    int scope;
    long value;
    int stack_index;
    int global_offset;
    int function_param_count;
    int function_local_symbol_count;
    int builtin;
    int size;
    int is_function;
    struct three_address_code *ir;
    int vreg_count;
};

struct value {
    int type;
    int vreg;
    int preg;
    int stack_index;
    int is_local;
    int is_string_literal;
    int is_lvalue;
    int value;
    int string_literal_index;
    struct symbol *function_symbol;
    int function_call_param_count;
    struct symbol *global_symbol;
};

struct three_address_code {
    int operation;
    struct value *dst;
    struct value *src1;
    struct value *src2;
};

struct liveness_interval {
    int start;
    int end;
};

struct str_member {
    char *name;
    int type;
    int offset;
};

struct str_desc {
    char *name;
    struct str_member **members;
    int size;
};

int print_ir_before, print_ir_after;

char *input;
int input_size;
int ip;
char **string_literals;
int string_literal_count;

int print_exit_code;
int print_cycles;
int cur_line;
int cur_token;
int cur_scope;
char *cur_identifier;
int cur_integer;
char *cur_string_literal;
struct symbol *cur_function_symbol;
int seen_return;
long *cur_loop_continue_address;
long *cur_loop_body_start;
long *cur_loop_increment_start;
int global_variable_count;

struct symbol *symbol_table;
struct symbol *next_symbol;
struct symbol *cur_symbol;

struct value **value_stack;

struct str_desc **all_structs;
int all_structs_count;

struct three_address_code *ir; // intermediate representation
int vreg_count;

struct liveness_interval *liveness;
int liveness_size;

int *physical_registers;

enum {
    DATA_SIZE               = 10 * 1024 * 1024,
    INSTRUCTIONS_SIZE       = 10 * 1024 * 1024,
    SYMBOL_TABLE_SIZE       = 10 * 1024 * 1024,
    MAX_STRUCTS             = 1024,
    MAX_STRUCT_MEMBERS      = 1024,
    MAX_INPUT_SIZE          = 10 * 1024 * 1024,
    MAX_STRING_LITERALS     = 10 * 1024,
    VALUE_STACK_SIZE        = 10 * 1024,
    MAX_THREE_ADDRESS_CODES = 1024,
    MAX_LIVENESS_SIZE       = 1024,
    PHYSICAL_REGISTER_COUNT = 15,
};

// In order of precedence
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
    TOK_RETURN,
    TOK_ENUM,
    TOK_SIZEOF,
    TOK_RPAREN,
    TOK_LPAREN,         // 20
    TOK_RBRACKET,
    TOK_LBRACKET,
    TOK_RCURLY,
    TOK_LCURLY,
    TOK_SEMI,
    TOK_COMMA,
    TOK_EQ,
    TOK_PLUS_EQ,
    TOK_MINUS_EQ,
    TOK_TERNARY,        // 30
    TOK_COLON,
    TOK_OR,
    TOK_AND,
    TOK_BITWISE_OR,
    TOK_XOR,
    TOK_ADDRESS_OF,
    TOK_DBL_EQ,
    TOK_NOT_EQ,
    TOK_LT,
    TOK_GT,             // 40
    TOK_LE,
    TOK_GE,
    TOK_BITWISE_LEFT,
    TOK_BITWISE_RIGHT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_MOD,
    TOK_LOGICAL_NOT,    // 50
    TOK_BITWISE_NOT,
    TOK_INC,
    TOK_DEC,
    TOK_DOT,
    TOK_ARROW,
    TOK_ATTRIBUTE,
    TOK_PACKED
};

// All structs start at TYPE_STRUCT up to TYPE_PTR. Pointers are represented by adding TYPE_PTR to a type.
enum { TYPE_ENUM=1, TYPE_VOID, TYPE_CHAR, TYPE_SHORT, TYPE_INT, TYPE_LONG, TYPE_STRUCT=16, TYPE_PTR=1024 };

enum { GLB_TYPE_FUNCTION=1, GLB_TYPE_VARIABLE };

enum {
    IR_LOAD_CONSTANT=1,
    IR_LOAD_STRING_LITERAL,
    IR_LOAD_VARIABLE,
    IR_INDIRECT,
    IR_PARAM,
    IR_CALL,
    IR_RETURN,
    IR_ASSIGN,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_EQ,
    IR_NE,
    IR_BNOT,
    IR_BOR,
    IR_BAND,
    IR_XOR,
    IR_BSHL,
    IR_BSHR,
    IR_LT,
    IR_GT,
    IR_LE,
    IR_GE,
    IR_EXIT,
    IR_OPEN,
    IR_READ,
    IR_WRIT,
    IR_CLOS,
    IR_PRTF,
    IR_DPRT,
    IR_MALC,
    IR_FREE,
    IR_MSET,
    IR_MCMP,
    IR_SCMP,
};

enum {IMM_NUMBER, IMM_STRING_LITERAL, IMM_GLOBAL};

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

int parse_struct_base_type();

void todo(char *what) {
    printf("TODO: %s\n", what);
    exit(1);
}

void next() {
    char *i;
    char *id;
    int idp;
    int value;
    char *sl;
    int slp;
    char c1, c2;

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
        else if (input_size - ip >= 2 && !memcmp(i+ip, "if",            2 )) { ip += 2;  cur_token = TOK_IF;                         }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "else",          4 )) { ip += 4;  cur_token = TOK_ELSE;                       }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "char",          4 )) { ip += 4;  cur_token = TOK_CHAR;                       }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "short",         5 )) { ip += 5;  cur_token = TOK_SHORT;                      }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "int",           3 )) { ip += 3;  cur_token = TOK_INT;                        }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "long",          4 )) { ip += 4;  cur_token = TOK_LONG;                       }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "void",          4 )) { ip += 4;  cur_token = TOK_VOID;                       }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "struct",        6 )) { ip += 6;  cur_token = TOK_STRUCT;                     }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "while",         5 )) { ip += 5;  cur_token = TOK_WHILE;                      }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "for",           3 )) { ip += 3;  cur_token = TOK_FOR;                        }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "continue",      8 )) { ip += 8;  cur_token = TOK_CONTINUE;                   }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "return",        6 )) { ip += 6;  cur_token = TOK_RETURN;                     }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "enum",          4 )) { ip += 4;  cur_token = TOK_ENUM;                       }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "sizeof",        6 )) { ip += 6;  cur_token = TOK_SIZEOF;                     }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "__attribute__", 13)) { ip += 13; cur_token = TOK_ATTRIBUTE;                  }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "__packed__",    10)) { ip += 10; cur_token = TOK_PACKED;                     }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "packed",        6 )) { ip += 6;  cur_token = TOK_PACKED;                     }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\t'",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_integer = '\t'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\n'",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_integer = '\n'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\''",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_integer = '\''; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\"'", 4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_integer = '\"'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\\'", 4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_integer = '\\'; }

        else if (input_size - ip >= 3 && c1 == '\'' && i[ip+2] == '\'') { cur_integer = i[ip+1]; ip += 3; cur_token = TOK_NUMBER; }

        else if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z')) {
            cur_token = TOK_IDENTIFIER;
            id = malloc(1024);
            idp = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) { id[idp] = i[ip]; idp++; ip++; }
            id[idp] = 0;
            cur_identifier = id;
        }

        else if (c1 == '0' && input_size - ip >= 2 && i[ip+1] == 'x') {
            ip += 2;
            cur_token = TOK_NUMBER;
            value = 0;
            while (((i[ip] >= '0' && i[ip] <= '9')  || (i[ip] >= 'a' && i[ip] <= 'f')) && ip < input_size) {
                value = value * 16 + (i[ip] >= 'a' ? i[ip] - 'a' + 10 : i[ip] - '0');
                ip++;
            }
            cur_integer = value;
        }

        else if ((c1 >= '0' && c1 <= '9')) {
            cur_token = TOK_NUMBER;
            value = 0;
            while ((i[ip] >= '0' && i[ip] <= '9') && ip < input_size) { value = value * 10 + (i[ip] - '0'); ip++; }
            cur_integer = value;
        }

        else if (c1 == '#') {
            // Ignore CPP directives
            while (i[ip++] != '\n');
            cur_line++;
            continue;
        }

        else if (i[ip] == '"') {
            cur_token = TOK_STRING_LITERAL;
            sl = malloc(1024);
            slp = 0;
            ip += 1;
            while (input_size - ip >= 1 && i[ip] != '"') {
                     if (i[ip] != '\\') { sl[slp] = i[ip]; slp++; ip++; }
                else if (input_size - ip >= 2 && i[ip] == '\\') {
                         if (i[ip+1] == 't' ) sl[slp++] = 9;
                    else if (i[ip+1] == 'n' ) sl[slp++] = 10;
                    else if (i[ip+1] == '\\') sl[slp++] = '\\';
                    else if (i[ip+1] == '\'') sl[slp++] = '\'';
                    else if (i[ip+1] == '\"') sl[slp++] = '\"';
                    else {
                        printf("%d: Unknown \\ escape in string literal\n", cur_line);
                        exit(1);
                    }
                    ip += 2;
                }
            }
            ip++;
            sl[slp] = 0;
            cur_string_literal = sl;
        }

        else {
            printf("%d: Unknown token %d\n", cur_line, cur_token);
            exit(1);
        }

        return;
    }

    cur_token = TOK_EOF;
}

void expect(int token) {
    if (cur_token != token) {
        printf("%d: Expected token %d, got %d\n", cur_line, token, cur_token);
        exit(1);
    }
}

void consume(int token) {
    expect(token);
    next();
}

struct value *new_value() {
    struct value *v;

    v = malloc(sizeof(struct value));
    v->type = 0;
    v->vreg = 0;
    v->preg = -1;
    v->value = 0;

    return v;
}

void push(struct value *v) {
    *value_stack++ = v;
}

struct value *push_constant(int type, int vreg, int value) {
    struct value *v;

    v = malloc(sizeof(struct value));
    v->preg = -1;
    v->vreg = vreg;
    v->value = value;
    *value_stack++ = v;

    return v;
}

// Duplicate the top of the value stack and push it
struct value *dup_value(struct value *src) {
    struct value *dst;

    dst = new_value();
    dst->type                      = src->type;
    dst->vreg                      = src->vreg;
    dst->preg                      = src->preg;
    dst->stack_index               = src->stack_index;
    dst->is_local                  = src->is_local;
    dst->is_string_literal         = src->is_string_literal;
    dst->is_lvalue                 = src->is_lvalue;
    dst->value                     = src->value;
    dst->string_literal_index      = src->string_literal_index;
    dst->function_symbol           = src->function_symbol;
    dst->function_call_param_count = src->function_call_param_count;
    dst->global_symbol             = src->global_symbol;

    return dst;
}

struct three_address_code *add_instruction(int operation, struct value *dst, struct value *src1, struct value *src2) {
    ir->operation = operation;
    ir->dst = dst;
    ir->src1 = src1;
    ir->src2 = src2;
    ir++;
    return ir - 1;
}

// Pop a value from the stack but don't turn an lvalue into an rvalue
struct value *raw_pop() {
    struct value *result;

    result = *(value_stack - 1);
    value_stack--;

    return result;
}

// Pop a value from the stack. Load it into a register if not already done
struct value *pop() {
    struct value *dst, *src1;

    if (value_stack[-1]->vreg) {
        if (value_stack[-1]->is_lvalue) {
            // It's an lvalue in a register. Dereference it into the same register
            value_stack[-1]->is_lvalue = 0;

            src1 = raw_pop();
            dst = dup_value(src1);

            dst->is_local = 0;
            dst->global_symbol = 0;

            add_instruction(IR_INDIRECT, dst, src1, 0);
            return dst;
        }

        return raw_pop();
    }

    // Allocate a register and load it
    src1 = raw_pop();
    dst = dup_value(src1);

    dst->vreg = ++vreg_count;
    dst->is_local = 0;
    dst->global_symbol = 0;
    dst->is_lvalue = 0;

    add_instruction(IR_LOAD_VARIABLE, dst, src1, 0);

    return dst;
}

void add_ir_constant_value(int type, long value) {
    struct value *v, *cv;

    cv = new_value();
    cv->value = value;
    cv->type = type;
    v = new_value();
    v->vreg = ++vreg_count;
    v->type = TYPE_LONG;
    push(v);
    add_instruction(IR_LOAD_CONSTANT, v, cv, 0);
}

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

long lookup_function(char *name) {
    struct symbol *symbol;

    symbol = lookup_symbol(name, 0);
    if (!symbol) {
        printf("%d: Unknown function \"%s\"\n", cur_line, name);
        exit(1);
    }

    return symbol->value;
}

int operation_type(struct value *src1, struct value *src2) {
    if (src1->type >= TYPE_PTR) return src1->type;
    else if (src2->type >= TYPE_PTR) return src2->type;
    else if (src1->type == TYPE_LONG || src2->type == TYPE_LONG) return TYPE_LONG;
    else return TYPE_INT;
}

int cur_token_is_integer_type() {
    return (cur_token == TOK_CHAR || cur_token == TOK_SHORT || cur_token == TOK_INT || cur_token == TOK_LONG);
}

int get_type_alignment(int type) {
         if (type  > TYPE_PTR)   return 8;
    else if (type == TYPE_CHAR)  return 1;
    else if (type == TYPE_SHORT) return 2;
    else if (type == TYPE_INT)   return 4;
    else if (type == TYPE_LONG)  return 8;
    else if (type >= TYPE_STRUCT) {
        printf("%d: Usage of structs as struct members not implemented\n", cur_line);
        exit(1);
    }
    else {
        printf("%d: align of unknown type %d\n", cur_line, type);
        exit(1);
    }
}

int get_type_size(int type) {
         if (type == TYPE_VOID)   return sizeof(void);
    else if (type == TYPE_CHAR)   return sizeof(char);
    else if (type == TYPE_SHORT)  return sizeof(short);
    else if (type == TYPE_INT)    return sizeof(int);
    else if (type == TYPE_LONG)   return sizeof(long);
    else if (type >  TYPE_PTR)    return sizeof(void *);
    else if (type >= TYPE_STRUCT) return all_structs[type - TYPE_STRUCT]->size;
    else {
        printf("%d: sizeof unknown type %d\n", cur_line, type);
        exit(1);
    }
}

int parse_base_type() {
    int type;

         if (cur_token == TOK_VOID)   type = TYPE_VOID;
    else if (cur_token == TOK_CHAR)   type = TYPE_CHAR;
    else if (cur_token == TOK_SHORT)  type = TYPE_SHORT;
    else if (cur_token == TOK_INT)    type = TYPE_INT;
    else if (cur_token == TOK_LONG)   type = TYPE_LONG;
    else if (cur_token == TOK_STRUCT) return parse_struct_base_type();
    else {
        printf("Unable to determine type from token %d\n", cur_token);
        exit(1);
    }
    next();

    return type;
}

int parse_type() {
    int type;

    type = parse_base_type();
    while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

    return type;
}

int parse_struct_base_type() {
    struct str_desc *s;
    struct str_member **members;
    struct str_member *member;
    int member_count;
    int i, base_type, type, offset, size, result;
    char *strct_identifier;
    int alignment, biggest_alignment;
    int is_packed;

    consume(TOK_STRUCT);

    is_packed = 0;
    if (cur_token == TOK_ATTRIBUTE) {
        consume(TOK_ATTRIBUTE);
        consume(TOK_LPAREN);
        consume(TOK_LPAREN);
        consume(TOK_PACKED);
        consume(TOK_RPAREN);
        consume(TOK_RPAREN);
        is_packed = 1;
    }

    strct_identifier = cur_identifier;
    consume(TOK_IDENTIFIER);
    if (cur_token == TOK_LCURLY) {
        consume(TOK_LCURLY);

        s = malloc(sizeof(struct str_desc));
        all_structs[all_structs_count] = s;
        result = TYPE_STRUCT + all_structs_count++;
        members = malloc(sizeof(struct str_member *) * MAX_STRUCT_MEMBERS);
        memset(members, 0, sizeof(struct str_member *) * MAX_STRUCT_MEMBERS);
        member_count = 0;
        s->name = strct_identifier;
        s->members = members;
        offset = 0;
        size = 0;
        biggest_alignment = 0;
        while (cur_token != TOK_RCURLY) {
            base_type = parse_base_type();
            while (cur_token != TOK_SEMI) {
                type = base_type;

                while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

                alignment = is_packed ? 1 : get_type_alignment(type);
                if (alignment > biggest_alignment) biggest_alignment = alignment;
                offset = ((offset + alignment  - 1) & (~(alignment - 1)));

                member = malloc(sizeof(struct str_member));
                memset(member, 0, sizeof(struct str_member));
                member->name = cur_identifier;
                member->type = type;
                member->offset = offset;
                members[member_count++] = member;

                offset += get_type_size(type);

                consume(TOK_IDENTIFIER);
                if (cur_token == TOK_COMMA) next();
            }
            while (cur_token == TOK_SEMI) consume(TOK_SEMI);
        }
        offset = ((offset + biggest_alignment  - 1) & (~(biggest_alignment - 1)));
        s->size = offset;
        consume(TOK_RCURLY);
        return result;
    }
    else {
        for (i = 0; i < all_structs_count; i++)
            if (!strcmp(all_structs[i]->name, strct_identifier)) {
                return TYPE_STRUCT + i;
        }
        printf("%d: Unknown struct %s\n", cur_line, strct_identifier);
        exit(1);
    }
}

int get_type_inc_dec_size(int type) {
    // How much will the ++ operator increment a type?
    return type <= TYPE_PTR ? 1 : get_type_size(type - TYPE_PTR);
}

struct str_member *lookup_struct_member(struct str_desc *str, char *identifier) {
    struct str_member **pmember;

    pmember = str->members;

    while (*pmember) {
        if (!strcmp((*pmember)->name, identifier)) return *pmember;
        pmember++;
    }

    printf("%d: Unknown member %s in struct %s\n", cur_line, identifier, str->name);
    exit(1);
}

void expression(int level) {
    int org_token;
    int org_type;
    int factor;
    long *false_jmp;
    long *true_done_jmp;
    struct symbol *symbol;
    int type;
    int scope;
    long *address;
    long param_count;
    long stack_index;
    int builtin;
    struct str_desc *str;
    struct str_member *member;
    int i, size;
    struct value *v1, *v2, *cv, *dst, *src1, *src2, *function_value, *return_value;
    struct three_address_code *tac;
    char *s;

    if (cur_token == TOK_LOGICAL_NOT) {
        next();
        add_ir_constant_value(TYPE_LONG, 0);
        expression(TOK_INC);
        add_ir_op(IR_EQ, TYPE_INT, ++vreg_count, pop(), pop());
    }
    else if (cur_token == TOK_BITWISE_NOT) {
        next();
        expression(TOK_INC);
        type = value_stack[-1]->type;
        tac = add_ir_op(IR_BNOT, type, 0, pop(), 0);
        tac->dst->vreg = tac->src1->vreg;
    }
    else if (cur_token == TOK_ADDRESS_OF) {
        next();
        expression(TOK_INC);
        if (!value_stack[-1]->is_lvalue) {
            printf("%d: Cannot take an address of an rvalue\n", cur_line);
            exit(1);
        }
        value_stack[-1]->is_lvalue = 0;
        value_stack[-1]->type += TYPE_PTR;
    }
    else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
        // Prefix increment & decrement

        org_token = cur_token;
        next();

        expression(1024); // Fake highest precedence, bind nothing

        if (!value_stack[-1]->is_lvalue) {
            printf("%d: Cannot ++ or -- an rvalue\n", cur_line);
            exit(1);
        }

        v1 = raw_pop(); // Preserve original lvalue

        src1 = dup_value(v1);
        push(src1); // Make an rvalue
        src1 = pop(src1);

        add_ir_constant_value(TYPE_INT, get_type_inc_dec_size(src1->type));
        src2 = pop();
        dst = new_value();
        dst->vreg = ++vreg_count;
        tac = add_instruction(org_token == TOK_INC ? IR_ADD : IR_SUB, dst, src1, src2);
        dst->vreg = tac->src2->vreg;

        add_instruction(IR_ASSIGN, v1, dst, 0);

        push(v1); // Push the original lvalue back on the value stack
    }
    else if (cur_token == TOK_MULTIPLY) {
        next();
        expression(TOK_INC);
        if (value_stack[-1]->type <= TYPE_PTR) {
            printf("%d: Cannot dereference a non-pointer %d\n", cur_line, value_stack[-1]->type);
            exit(1);
        }

        src1 = pop();
        dst = new_value();
        dst->vreg = src1->vreg;
        dst->type = src1->type;
        dst->is_lvalue = 1;
        dst->type -= TYPE_PTR;
        dst->is_local = 0;
        dst->global_symbol = 0;
        push(dst);
    }
    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            add_ir_constant_value(TYPE_LONG, -cur_integer);
            next();
        }
        else {
            add_ir_constant_value(TYPE_LONG, -1);
            expression(TOK_INC);
            tac = add_ir_op(IR_MUL, TYPE_LONG, 0, pop(), pop());
            tac->dst->vreg = tac->src2->vreg;
        }
    }
    else if (cur_token == TOK_LPAREN) {
        next();
        if (cur_token == TOK_VOID || cur_token_is_integer_type() || cur_token == TOK_STRUCT) {
            todo("cast");
        //     // cast
        //     org_type = parse_type();
        //     consume(TOK_RPAREN);
        //     expression(TOK_INC);
        //     cur_type = org_type;
        }
        else {
            expression(TOK_COMMA);
            consume(TOK_RPAREN);
        }
    }
    else if (cur_token == TOK_NUMBER) {
        add_ir_constant_value(TYPE_LONG, cur_integer);
        next();
    }
    else if (cur_token == TOK_STRING_LITERAL) {
        dst = new_value();
        dst->vreg = ++vreg_count;
        dst->type = TYPE_CHAR + TYPE_PTR;

        src1 = new_value();
        src1->type = TYPE_CHAR + TYPE_PTR;
        src1->string_literal_index = string_literal_count;
        src1->is_string_literal = 1;
        string_literals[string_literal_count++] = cur_string_literal;
        push(dst);

        add_instruction(IR_LOAD_STRING_LITERAL, dst, src1, 0);

        next();
    }
    else if (cur_token == TOK_IDENTIFIER) {
        symbol = lookup_symbol(cur_identifier, cur_scope);

        if (!symbol) {
            printf("%d: Unknown symbol \"%s\"\n", cur_line, cur_identifier);
            exit(1);
        }

        next();
        type = symbol->type;
        scope = symbol->scope;
        if (type == TYPE_ENUM) {
            todo("enums");
            // *iptr++ = INSTR_IMM;
            // *iptr++ = symbol->value;
            // *iptr++ = IMM_NUMBER;
            // *iptr++ = 0;
            // cur_type = symbol->type;
        }
        else if (cur_token == TOK_LPAREN) {
            // Function call
            next();
            param_count = 0;
            while (cur_token != TOK_RPAREN) {
                expression(TOK_COMMA);
                add_instruction(IR_PARAM, 0, pop(), 0);
                if (cur_token == TOK_COMMA) next();
                param_count++;
            }
            consume(TOK_RPAREN);

            function_value = new_value();
            function_value->function_symbol = symbol;
            function_value->function_call_param_count = param_count;

            return_value = 0;
            if (type != TYPE_VOID) {
                return_value = new_value();
                return_value->vreg = ++vreg_count;
                return_value->type = type;
            }
            add_instruction(IR_CALL, return_value, function_value, 0);
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

            param_count = cur_function_symbol->function_param_count;

            src1 = new_value();
            src1->type = type;
            src1->is_local = 1;
            src1->is_lvalue = 1;

            if (symbol->stack_index >= 0) {
                // Step over pushed PC and BP
                src1->stack_index = param_count - symbol->stack_index + 1;
            }
            else {
                src1->stack_index = symbol->stack_index;
            }

            push(src1);
        }
    }
    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN);
        type = parse_type();
        add_ir_constant_value(TYPE_INT, get_type_size(type));
        consume(TOK_RPAREN);
    }
    else {
        printf("%d: Unexpected token %d in expression\n", cur_line, cur_token);
        exit(1);
    }

    if (cur_token == TOK_LBRACKET) {
        todo("[");
        // next();
        // TODO is this a mistake? Should this function be part of the above if/else?

        // if (cur_type <= TYPE_PTR) {
        //     printf("%d: Cannot do [] on a non-pointer for type %ld\n", cur_line, cur_type);
        //     exit(1);
        // }

        // org_type = cur_type;
        // *iptr++ = INSTR_PSH;
        // expression(TOK_COMMA);

        // factor = get_type_inc_dec_size(org_type);
        // if (factor > 1) {
        //     *iptr++ = INSTR_PSH;
        //     *iptr++ = INSTR_IMM;
        //     *iptr++ = factor;
        //     *iptr++ = IMM_NUMBER;
        //     *iptr++ = 0;
        //     *iptr++ = INSTR_MUL;
        // }

        // *iptr++ = INSTR_ADD;
        // consume(TOK_RBRACKET);
        // cur_type = org_type - TYPE_PTR;
        // load_type();
    }

    while (cur_token >= level) {
        // In order or precedence

        if (cur_token == TOK_INC || cur_token == TOK_DEC) {
            // Postfix increment & decrement

            if (!value_stack[-1]->is_lvalue) {
                printf("%d: Cannot ++ or -- an rvalue\n", cur_line);
                exit(1);
            }

            v1 = value_stack[-1]; // lvalue
            src1 = dup_value(value_stack[-1]);
            push(src1); // make an rvalue to be pushed onto the stack after the add/sub
            src1 = pop();

            // Copy the original value, since the add/sub operation destroys the original register
            v2 = new_value();
            v2->type = src1->type;
            v2->vreg = ++vreg_count;
            add_instruction(IR_ASSIGN, v2, src1, 0);

            add_ir_constant_value(TYPE_INT, get_type_inc_dec_size(src1->type));
            src2 = pop();

            dst = new_value();
            dst->vreg = ++vreg_count;
            tac = add_instruction(cur_token == TOK_INC ? IR_ADD : IR_SUB, dst, src1, src2);
            dst->vreg = tac->src2->vreg;

            add_instruction(IR_ASSIGN, v1, dst, 0);

            next();
            push(v2); // Push the original value back on the value stack
        }
        else if (cur_token == TOK_DOT || cur_token == TOK_ARROW) {
            todo("., ->");
            // if (cur_token == TOK_DOT) {
            //     // Struct member lookup. A load has already been pushed for it

            //     if (cur_type < TYPE_STRUCT || cur_type >= TYPE_PTR) {
            //         printf("%d: Cannot use . on a non-struct\n", cur_line);
            //         exit(1);
            //     }

            //     if (!is_lvalue()) {
            //         printf("%d: Expected lvalue for struct . operation.\n", cur_line);
            //         exit(1);
            //     }
            //     iptr--; // Roll back load instruction
            // }
            // else {
            //     // Pointer to struct member lookup.

            //     if (cur_type < TYPE_PTR) {
            //         printf("%d: Cannot use -> on a non-pointer\n", cur_line);
            //         exit(1);
            //     }

            //     cur_type -= TYPE_PTR;

            //     if (cur_type < TYPE_STRUCT) {
            //         printf("%d: Cannot use -> on a pointer to a non-struct\n", cur_line);
            //         exit(1);
            //     }
            // }

            // next();
            // if (cur_token != TOK_IDENTIFIER) {
            //     printf("%d: Expected identifier\n", cur_line);
            //     exit(1);
            // }

            // str = all_structs[cur_type - TYPE_STRUCT];
            // member = lookup_struct_member(str, cur_identifier);

            // if (member->offset > 0) {
            //     *iptr++ = INSTR_PSH;
            //     *iptr++ = INSTR_IMM;
            //     *iptr++ = member->offset;
            //     *iptr++ = IMM_NUMBER;
            //     *iptr++ = 0;
            //     *iptr++ = INSTR_ADD;
            // }

            // cur_type = member->type;
            // load_type();
            // consume(TOK_IDENTIFIER);
        }
        else if (cur_token == TOK_MULTIPLY) {
            next();
            expression(TOK_DOT);
            type = operation_type(value_stack[-1], value_stack[-2]);
            tac = add_ir_op(IR_MUL, type, 0, pop(), pop());
            tac->dst->vreg = tac->src2->vreg;
        }
        else if (cur_token == TOK_DIVIDE) {
            next();
            expression(TOK_INC);
            type = operation_type(value_stack[-1], value_stack[-2]);
            add_ir_op(IR_DIV, type, ++vreg_count, pop(), pop());
        }
        else if (cur_token == TOK_MOD) {
            next();
            expression(TOK_INC);
            type = operation_type(value_stack[-1], value_stack[-2]);
            add_ir_op(IR_MOD, type, ++vreg_count, pop(), pop());
        }
        else if (cur_token == TOK_PLUS || cur_token == TOK_MINUS) {
            org_token = cur_token;
            factor = get_type_inc_dec_size(value_stack[-1]->type);
            next();
            expression(TOK_MULTIPLY);

            if (factor > 1) {
                todo("arith +, - for pointers");
                // *iptr++ = INSTR_PSH;
                // *iptr++ = INSTR_IMM;
                // *iptr++ = factor;
                // *iptr++ = IMM_NUMBER;
                // *iptr++ = 0;
                // *iptr++ = INSTR_MUL;
            }

            type = operation_type(value_stack[-1], value_stack[-2]);

            if (org_token == TOK_PLUS) {
                tac = add_ir_op(IR_ADD, type, 0, pop(), pop());
                tac->dst->vreg = tac->src2->vreg;
            }
            else {
                tac = add_ir_op(IR_SUB, type, 0, pop(), pop());
                tac->dst->vreg = tac->src2->vreg;
            }
        }
        else if (cur_token == TOK_BITWISE_LEFT) {
            next();
            expression(TOK_PLUS);
            type = operation_type(value_stack[-1], value_stack[-2]);
            tac = add_ir_op(IR_BSHL, type, 0, pop(), pop());
            tac->dst->vreg = tac->src1->vreg;
        }
        else if (cur_token == TOK_BITWISE_RIGHT) {
            next();
            expression(TOK_PLUS);
            type = operation_type(value_stack[-1], value_stack[-2]);
            tac = add_ir_op(IR_BSHR, type, 0, pop(), pop());
            tac->dst->vreg = tac->src1->vreg;
        }
        else if (cur_token == TOK_LT) {
            next();
            expression(TOK_BITWISE_LEFT);
            tac = add_ir_op(IR_LT, TYPE_INT, ++vreg_count, pop(), pop());
            tac->dst->vreg = tac->src1->vreg;

        }
        else if (cur_token == TOK_GT) {
            next();
            expression(TOK_PLUS);
            tac = add_ir_op(IR_GT, TYPE_INT, ++vreg_count, pop(), pop());
            tac->dst->vreg = tac->src1->vreg;

        }
        else if (cur_token == TOK_LE) {
            next();
            expression(TOK_PLUS);
            // *iptr++ = INSTR_LE;
            tac = add_ir_op(IR_LE, TYPE_INT, ++vreg_count, pop(), pop());
            tac->dst->vreg = tac->src1->vreg;

        }
        else if (cur_token == TOK_GE) {
            next();
            expression(TOK_PLUS);
            tac = add_ir_op(IR_GE, TYPE_INT, ++vreg_count, pop(), pop());
            tac->dst->vreg = tac->src1->vreg;

        }
        else if (cur_token == TOK_DBL_EQ) {
            next();
            expression(TOK_LT);
            tac = add_ir_op(IR_EQ, TYPE_INT, ++vreg_count, pop(), pop());
            tac->dst->vreg = tac->src1->vreg;
        }
        else if (cur_token == TOK_NOT_EQ) {
            next();
            expression(TOK_LT);
            tac = add_ir_op(IR_NE, TYPE_INT, ++vreg_count, pop(), pop());
            tac->dst->vreg = tac->src2->vreg;
        }
        else if (cur_token == TOK_ADDRESS_OF) {
            next();
            expression(TOK_DBL_EQ);
            type = operation_type(value_stack[-1], value_stack[-2]);
            tac = add_ir_op(IR_BAND, type, 0, pop(), pop());
            tac->dst->vreg = tac->src2->vreg;
        }
        else if (cur_token == TOK_XOR) {
            next();
            expression(TOK_ADDRESS_OF);
            type = operation_type(value_stack[-1], value_stack[-2]);
            tac = add_ir_op(IR_XOR, type, 0, pop(), pop());
            tac->dst->vreg = tac->src2->vreg;
        }
        else if (cur_token == TOK_BITWISE_OR) {
            next();
            expression(TOK_XOR);
            type = operation_type(value_stack[-1], value_stack[-2]);
            tac = add_ir_op(IR_BOR, type, 0, pop(), pop());
            tac->dst->vreg = tac->src2->vreg;
        }
        else if (cur_token == TOK_AND) {
            todo("&&");
            // temp_iptr = iptr;
            // *iptr++ = INSTR_BZ;
            // *iptr++ = 0;
            // next();
            // *iptr++ = INSTR_PSH;
            // expression(TOK_BITWISE_OR);
            // *iptr++ = INSTR_AND;
            // *(temp_iptr + 1) = (long) iptr;
            // cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_OR) {
            todo("||");
            // temp_iptr = iptr;
            // *iptr++ = INSTR_BNZ;
            // *iptr++ = 0;
            // next();
            // *iptr++ = INSTR_PSH;
            // expression(TOK_AND);
            // *iptr++ = INSTR_OR;
            // *(temp_iptr + 1) = (long) iptr;
            // cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_TERNARY) {
            todo("?,:");
            // next();
            // false_jmp = iptr;
            // *iptr++ = INSTR_BZ;
            // *iptr++ = 0;
            // expression(TOK_OR);
            // true_done_jmp = iptr;
            // *iptr++ = INSTR_JMP;
            // *iptr++ = 0;
            // consume(TOK_COLON);
            // *(false_jmp + 1) = (long) iptr;
            // expression(TOK_OR);
            // *(true_done_jmp + 1) = (long) iptr;
        }
        else if (cur_token == TOK_EQ) {
            next();
            if (!value_stack[-1]->is_lvalue) {
                printf("%d: Cannot assign to an rvalue\n", cur_line);
                exit(1);
            }
            dst = raw_pop();
            expression(TOK_EQ);
            src1 = pop();
            dst->is_lvalue = 1;
            add_instruction(IR_ASSIGN, dst, src1, 0);
            push(dst);
        }
        else if (cur_token == TOK_PLUS_EQ || cur_token == TOK_MINUS_EQ) {
            todo("+=, -=");
            // org_token = cur_token;
            // next();
            // if (!is_lvalue()) {
            //     printf("%d: Cannot assign to an rvalue\n", cur_line);
            //     exit(1);
            // }
            // org_type = cur_type;
            // iptr--;               // Roll back load
            // *iptr++ = INSTR_PSH;  // Push address
            // load_type();          // Push value
            // *iptr++ = INSTR_PSH;

            // factor = get_type_inc_dec_size(org_type);
            // if (factor > 1) {
            //     *iptr++ = INSTR_IMM;
            //     *iptr++ = factor;
            //     *iptr++ = IMM_NUMBER;
            //     *iptr++ = 0;
            //     *iptr++ = INSTR_PSH;
            // }

            // expression(TOK_EQ);
            // if (factor > 1) *iptr++ = INSTR_MUL;
            // *iptr++ = org_token == TOK_PLUS_EQ ? INSTR_ADD : INSTR_SUB;
            // cur_type = org_type;
            // store_type(cur_type);
        }
        else
            return; // Bail once we hit something unknown

    }
}

void statement() {
    struct value *v;
    long *body_start;
    long *false_jmp;
    long *true_done_jmp;
    long *cur_loop_test_start;
    long *old_cur_loop_continue_address;
    long *old_cur_loop_body_start;
    long *old_cur_loop_increment_start;

    if (cur_token_is_integer_type() || cur_token == TOK_STRUCT)  {
        printf("%d: Declarations must be at the top of a function\n", cur_line);
        exit(1);
    }

    if (cur_token == TOK_SEMI) {
        // Empty statement
        next();
        return;
    }
    else if (cur_token == TOK_LCURLY) {
        next();
        while (cur_token != TOK_RCURLY) statement();
        consume(TOK_RCURLY);
        return;
    }
    else if (cur_token == TOK_WHILE) {
        todo("while");
        // // Preserve previous loop addresses so that we can have nested whiles/fors
        // old_cur_loop_continue_address = cur_loop_continue_address;
        // old_cur_loop_body_start = cur_loop_body_start;

        // next();
        // consume(TOK_LPAREN);

        // cur_loop_continue_address = iptr;
        // expression(TOK_COMMA);
        // consume(TOK_RPAREN);
        // *iptr++ = INSTR_BZ;
        // *iptr++ = 0;

        // cur_loop_body_start = iptr;
        // statement();

        // *iptr++ = INSTR_JMP;
        // *iptr++ = (long) cur_loop_continue_address;
        // *(cur_loop_body_start - 1) = (long) iptr;

        // // Restore previous loop addresses
        // cur_loop_continue_address = old_cur_loop_continue_address;
        // cur_loop_body_start = old_cur_loop_body_start;
    }
    else if (cur_token == TOK_FOR) {
        todo("for");
        // old_cur_loop_continue_address = cur_loop_continue_address;
        // old_cur_loop_body_start = cur_loop_body_start;

        // next();
        // consume(TOK_LPAREN);

        // expression(TOK_COMMA); // setup expression
        // consume(TOK_SEMI);

        // cur_loop_test_start = iptr;
        // expression(TOK_COMMA); // test expression
        // consume(TOK_SEMI);
        // *iptr++ = INSTR_BZ;
        // *iptr++ = 0;

        // *iptr++ = INSTR_JMP; // Jump to body start
        // *iptr++;

        // cur_loop_continue_address = iptr;
        // expression(TOK_COMMA); // increment expression
        // consume(TOK_RPAREN);
        // *iptr++ = INSTR_JMP;
        // *iptr++ = (long) cur_loop_test_start;

        // cur_loop_body_start = iptr;
        // *(cur_loop_continue_address - 1) = (long) iptr;
        // statement();
        // *iptr++ = INSTR_JMP;
        // *iptr++ = (long) cur_loop_continue_address;
        // *(cur_loop_continue_address - 3) = (long) iptr;

        // // Restore previous loop addresses
        // cur_loop_continue_address = old_cur_loop_continue_address;
        // cur_loop_body_start = old_cur_loop_body_start;
    }
    else if (cur_token == TOK_CONTINUE) {
        todo("continue");
        // next();
        // *iptr++ = INSTR_JMP;
        // *iptr++ = (long) cur_loop_continue_address;
        // consume(TOK_SEMI);
    }
    else if (cur_token == TOK_IF) {
        todo("if");
        // next();
        // consume(TOK_LPAREN);
        // expression(TOK_COMMA);
        // consume(TOK_RPAREN);
        // false_jmp = iptr;
        // *iptr++ = INSTR_BZ;
        // *iptr++ = 0;
        // statement();
        // *(false_jmp + 1) = (long) iptr;
        // if (cur_token == TOK_ELSE) {
        //     next();
        //     true_done_jmp = iptr;
        //     *iptr++ = INSTR_JMP;
        //     *iptr++ = 0;
        //     *(false_jmp + 1) = (long) iptr;
        //     statement();
        //     *(true_done_jmp + 1) = (long) iptr;
        // }
        // else
        //     *(false_jmp + 1) = (long) iptr;
    }
    else if (cur_token == TOK_RETURN) {
        next();
        if (cur_token == TOK_SEMI) todo("return void");
        expression(TOK_COMMA);
        add_instruction(IR_RETURN, 0, pop(), 0);
        consume(TOK_SEMI);
        seen_return = 1;
    }
    else {
        expression(TOK_COMMA);
        consume(TOK_SEMI);
        // Discard the destination of a function call to a non-void function
        // since it's not being assigned to anything and
        if (ir[-1].operation == IR_CALL && ir[-1].dst) {
            ir[-1].dst = 0;
            --vreg_count;
        }
    }
}

void function_body() {
    int is_main;
    int local_symbol_count;
    int base_type, type;

    vreg_count = 0;

    seen_return = 0;
    is_main = !strcmp(cur_function_symbol->identifier, "main");
    local_symbol_count = 0;

    consume(TOK_LCURLY);

    // Parse symbols first
    while (cur_token_is_integer_type() || cur_token == TOK_STRUCT) {
        base_type = parse_base_type();

        while (cur_token != TOK_SEMI) {
            type = base_type;
            while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

            if (type >= TYPE_STRUCT && type < TYPE_PTR) {
                printf("%d: Direct usage of struct variables not implemented\n", cur_line);
                exit(1);
            }

            if (cur_token == TOK_EQ) {
                printf("%d: Declarations with assignments aren't implemented\n", cur_line);
                exit(1);
            }

            expect(TOK_IDENTIFIER);
            cur_symbol = next_symbol;
            next_symbol->type = type;
            next_symbol->identifier = cur_identifier;
            next_symbol->scope = cur_scope;
            next_symbol->stack_index = -1 - local_symbol_count++;
            next_symbol++;
            next();
            if (cur_token == TOK_COMMA) next();
        }
        expect(TOK_SEMI);
        while (cur_token == TOK_SEMI) next();
    }

    cur_function_symbol->function_local_symbol_count = local_symbol_count;

    while (cur_token != TOK_RCURLY) statement();

    consume(TOK_RCURLY);
}

void parse() {
    int base_type, type;
    long number;
    int param_count;
    int seen_function;
    int doing_var_declaration;
    char *cur_function_name;
    struct symbol *existing_symbol;
    int i;

    cur_scope = 0;
    seen_function = 0;

    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token == TOK_VOID || cur_token_is_integer_type() || cur_token == TOK_STRUCT) {
            doing_var_declaration = 1;
            base_type = parse_base_type();

            while (doing_var_declaration && cur_token != TOK_SEMI && cur_token != TOK_EOF) {
                type = base_type;
                while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

                if (type >= TYPE_STRUCT && type < TYPE_PTR) {
                    printf("%d: Direct usage of struct variables not implemented\n", cur_line);
                    exit(1);
                }

                expect(TOK_IDENTIFIER);
                cur_function_name = cur_identifier;

                existing_symbol = lookup_symbol(cur_function_name, 0);
                if (!existing_symbol) {
                    // Create a new symbol if it wasn't already declared. The
                    // previous declaration is left unchanged.

                    cur_symbol = next_symbol;
                    next_symbol->type = type;
                    next_symbol->identifier = cur_identifier;
                    next_symbol->stack_index = global_variable_count++;
                    next_symbol->scope = 0;
                    next_symbol++;
                }
                else
                    cur_symbol = existing_symbol;

                next();

                if (cur_token == TOK_LPAREN) {
                    cur_function_symbol = cur_symbol;
                    // Function declaration or definition

                    seen_function = 1;
                    cur_scope++;
                    next();

                    ir = malloc(sizeof(struct three_address_code) * MAX_THREE_ADDRESS_CODES);
                    memset(ir, 0, sizeof(struct three_address_code) * MAX_THREE_ADDRESS_CODES);
                    cur_symbol->ir = ir;
                    cur_symbol->is_function = 1;

                    param_count = 0;
                    while (cur_token != TOK_RPAREN) {
                        if (cur_token_is_integer_type() || cur_token == TOK_STRUCT) {
                            type = parse_base_type();
                            while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

                            if (type >= TYPE_STRUCT && type < TYPE_PTR) {
                                printf("%d: Direct usage of struct variables not implemented\n", cur_line);
                                exit(1);
                            }
                        }
                        else {
                            printf("%d: Unknown type token in function def %d\n", cur_line, cur_token);
                            exit(1);
                        }

                        consume(TOK_IDENTIFIER);
                        next_symbol->type = type;
                        next_symbol->identifier = cur_identifier;
                        next_symbol->scope = cur_scope;
                        next_symbol->stack_index = param_count++;
                        next_symbol++;
                        if (cur_token == TOK_COMMA) next();
                    }
                    cur_symbol->function_param_count = param_count;
                    consume(TOK_RPAREN);
                    cur_function_symbol = cur_symbol;

                    if (cur_token == TOK_LCURLY) {
                        cur_function_symbol->function_param_count = param_count;
                        function_body();
                        cur_function_symbol->vreg_count = vreg_count;
                    }
                    else
                        // Make it clear that this symbol will need to be backpatched if used
                        // before the definition has been processed.
                        cur_function_symbol->value = 0;

                    doing_var_declaration = 0;
                }
                else {
                    // Global symbol
                    if (seen_function) {
                        printf("%d: Global variables must precede all functions\n", cur_line);
                        exit(1);
                    }
                }

                if (cur_token == TOK_COMMA) next();
            }

            if (cur_token == TOK_SEMI) next();
        }

        else if (cur_token == TOK_ENUM) {
            consume(TOK_ENUM);
            consume(TOK_LCURLY);
            number = 0;
            while (cur_token != TOK_RCURLY) {
                expect(TOK_IDENTIFIER);
                next();
                if (cur_token == TOK_EQ) {
                    next();
                    expect(TOK_NUMBER);
                    number = cur_integer;
                    next();
                }

                next_symbol->type = TYPE_ENUM;
                next_symbol->identifier = cur_identifier;
                next_symbol->scope = 0;
                next_symbol->value = number++;
                next_symbol++;

                if (cur_token == TOK_COMMA) next();
            }
            consume(TOK_RCURLY);
            consume(TOK_SEMI);
        }

        else if (cur_token == TOK_STRUCT) {
            parse_base_type(); // It's a struct declaration
            consume(TOK_SEMI);
        }

        else {
            printf("%d: Expected global declaration or function\n", cur_line);
            exit(1);
        }
    }
}

void printf_escaped_string_literal(char* sl) {
    printf("\"");
    while (*sl) {
             if (*sl == '\n') printf("\\n");
        else if (*sl == '\t') printf("\\t");
        else if (*sl == '\\') printf("\\");
        else if (*sl == '"')  printf("\\\"");
        else                  printf("%c", *sl);
        sl++;
    }
    printf("\"");
}

void dprintf_escaped_string_literal(int f, char* sl) {
    dprintf(f, "\"");
    while (*sl) {
             if (*sl == '\n') dprintf(f, "\\n");
        else if (*sl == '\t') dprintf(f, "\\t");
        else if (*sl == '\\') dprintf(f, "\\");
        else if (*sl == '"')  dprintf(f, "\\\"");
        else                  dprintf(f, "%c", *sl);
        sl++;
    }
    dprintf(f, "\"");
}

void print_value(struct value *v, int is_assignment_rhs) {
    if (is_assignment_rhs && !v->is_lvalue && (v->global_symbol || v->is_local)) printf("&");
    if (!is_assignment_rhs && v->is_lvalue && !(v->global_symbol || v->is_local)) printf("*");

    if (v->preg != -1)
        printf("p%d", v->preg);
    else if (v->vreg)
        printf("r%d", v->vreg);
    else if (v->is_local)
        printf("s[%d]", v->stack_index);
    else if (v->global_symbol)
        printf("%s", v->global_symbol->identifier);
    else if (v->is_string_literal) {
        printf_escaped_string_literal(string_literals[v->string_literal_index]);
    }
    else
        printf("%d", v->value);
}

void print_instruction(struct three_address_code *tac) {
    if (tac->dst) {
        print_value(tac->dst, tac->operation != IR_ASSIGN);
        printf(" = ");
    }

    if (tac->operation == IR_LOAD_CONSTANT || tac->operation == IR_LOAD_VARIABLE || tac->operation == IR_LOAD_STRING_LITERAL) {
        print_value(tac->src1, 1);
    }
    else if (tac->operation == IR_PARAM) {
        printf("param ");
        print_value(tac->src1, 1);
    }
    else if (tac->operation == IR_CALL) {
        printf("call \"%s\" with %d params", tac->src1->function_symbol->identifier, tac->src1->function_call_param_count);
    }
    else if (tac->operation == IR_RETURN) {
        printf("return ");
        print_value(tac->src1, 1);
    }
    else if (tac->operation == IR_ASSIGN) {
        print_value(tac->src1, 1);
    }

    else if (tac->operation == IR_INDIRECT)      {                            printf("*");    print_value(tac->src1, 1); }
    else if (tac->operation == IR_ADD)           { print_value(tac->src1, 1); printf(" + ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_SUB)           { print_value(tac->src1, 1); printf(" - ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_MUL)           { print_value(tac->src1, 1); printf(" * ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_DIV)           { print_value(tac->src1, 1); printf(" / ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_MOD)           { print_value(tac->src1, 1); printf(" %% "); print_value(tac->src2, 1); }
    else if (tac->operation == IR_BNOT)          {                            printf("!");    print_value(tac->src1, 1); }
    else if (tac->operation == IR_EQ)            { print_value(tac->src1, 1); printf(" == "); print_value(tac->src2, 1); }
    else if (tac->operation == IR_NE)            { print_value(tac->src1, 1); printf(" != "); print_value(tac->src2, 1); }
    else if (tac->operation == IR_GT)            { print_value(tac->src1, 1); printf(" > ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_LT)            { print_value(tac->src1, 1); printf(" < ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_GE)            { print_value(tac->src1, 1); printf(" >= "); print_value(tac->src2, 1); }
    else if (tac->operation == IR_LE)            { print_value(tac->src1, 1); printf(" <= "); print_value(tac->src2, 1); }
    else if (tac->operation == IR_BAND)          { print_value(tac->src1, 1); printf(" & ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_BOR)           { print_value(tac->src1, 1); printf(" | ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_XOR)           { print_value(tac->src1, 1); printf(" ^ ");  print_value(tac->src2, 1); }
    else if (tac->operation == IR_BSHL)          { print_value(tac->src1, 1); printf(" << "); print_value(tac->src2, 1); }
    else if (tac->operation == IR_BSHR)          { print_value(tac->src1, 1); printf(" >> "); print_value(tac->src2, 1); }

    else {
        printf("print_instruction(): Unknown operation: %d\n", tac->operation);
        exit(1);
    }

    printf("\n");
}

void print_intermediate_representation(char *function_name, struct three_address_code *ir) {
    struct three_address_code *tac;
    struct symbol *s;
    int i;

    printf("%s:\n", function_name);
    i = 0;
    while (ir->operation) {
        printf("%d > ", i++);
        print_instruction(ir);
        ir++;
    }
    printf("\n");
}

void update_register_liveness(int reg, int instruction_position) {
    if (liveness[reg].start == -1 || instruction_position < liveness[reg].start) liveness[reg].start = instruction_position;
    if (liveness[reg].end == -1 || instruction_position > liveness[reg].end) liveness[reg].end = instruction_position;
}

void analyze_liveness(struct three_address_code *ir, int vreg_count) {
    int i;

    for (i = 1; i <= vreg_count; i++) {
        liveness[i].start = -1;
        liveness[i].end = -1;
    }

    i = 0;
    while (ir->operation) {
        if (ir->dst  && ir->dst->vreg)  update_register_liveness(ir->dst->vreg,  i);
        if (ir->src1 && ir->src1->vreg) update_register_liveness(ir->src1->vreg, i);
        if (ir->src2 && ir->src2->vreg) update_register_liveness(ir->src2->vreg, i);
        ir++;
        i++;
    }
}

void print_liveness(int vreg_count) {
    int i;

    for (i = 1; i <= vreg_count; i++)
        printf("r%d %d %d\n", i, liveness[i].start, liveness[i].end);
}

void allocate_register(struct value *v) {
    int i;

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
            physical_registers[i] = v->vreg;
            v->preg = i;
            return;
        }
    }

    printf("Ran out of registers\n");
    exit(1);
}

void allocate_registers(struct three_address_code *ir) {
    int line, i, j;

    physical_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(physical_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

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

    line = 0;
    while (ir->operation) {
        if (ir->dst  && ir->dst->vreg)  allocate_register(ir->dst);
        if (ir->src1 && ir->src1->vreg) allocate_register(ir->src1);
        if (ir->src2 && ir->src2->vreg) allocate_register(ir->src2);

        // Free registers
        for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++) {
            if (physical_registers[i] > 0 && liveness[physical_registers[i]].end == line) {
                physical_registers[i] = 0;
            }
        }
        ir++;
        line++;
    }

    free(physical_registers);
}

void output_load_param(int f, int position, char *register_name) {
    dprintf(f, "\tmovq\t%d(%%rsp), %%%s\n", position * 8, register_name);
}

void output_register_name(int f, int preg) {
    char *names;

    names = "rax rbx rcx rdx rsi rdi rbp rsp r8  r9  r10 r11 r12 r13 r14 r15";
    if (preg == 8 || preg == 9) dprintf(f, "%%%.2s", &names[preg * 4]);
    else dprintf(f, "%%%.3s", &names[preg * 4]);
}

void output_8bit_register_name(int f, int preg) {
    char *names;

    names = "al   bl   cl   dl   sil  dil  bpl  spl  r8b  r9b  r10b r11b r12b r13b r14b r15b";
    if (preg < 4)
        dprintf(f, "%%%.2s", &names[preg * 5]);
    else if (preg < 10)
        dprintf(f, "%%%.3s", &names[preg * 5]);
    else
        dprintf(f, "%%%.4s", &names[preg * 5]);
}

void output_op(int f, char *instruction, int reg1, int reg2) {
    dprintf(f, "\t%s\t", instruction);
    output_register_name(f, reg1);
    dprintf(f, ", ");
    output_register_name(f, reg2);
    dprintf(f, "\n");
}

void output_cmp(int f, struct three_address_code *ir) {
    dprintf(f, "\tcmpq\t");
    output_register_name(f, ir->src2->preg);
    dprintf(f, ", ");
    output_register_name(f, ir->src1->preg);
    dprintf(f, "\n");
}

void output_cmp_result_instruction(int f, struct three_address_code *ir, char *instruction) {
    dprintf(f, "\t%s\t", instruction);
    output_8bit_register_name(f, ir->src2->preg);
    dprintf(f, "\n");
}

void output_movzbl(int f, struct three_address_code *ir) {
    dprintf(f, "\tmovzbl\t");
    output_8bit_register_name(f, ir->src2->preg);
    dprintf(f, ", ");
    output_register_name(f, ir->dst->preg);
    dprintf(f, "\n");
}

void output_cmp_operation(int f, struct three_address_code *ir, char *instruction) {
    output_cmp(f, ir);
    output_cmp_result_instruction(f, ir, instruction);
    output_movzbl(f, ir);
}

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
            // If there aree.g. 2 args:
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

void output_function_body_code(int f, struct symbol *symbol) {
    struct three_address_code *ir;
    int i;
    int function_pc, pc; // Function param count
    int stack_index;
    int stack_offset;
    int local_stack_size;
    int local_vars_stack_start;

    ir = symbol->ir;

    dprintf(f, "\tpush\t%%rbp\n");
    dprintf(f, "\tmovq\t%%rsp, %%rbp\n");

    // Push up to the first 6 args onto the stack, so all args are on the stack with leftmost arg first.
    // Arg 7 and onwards are already pushed.

    function_pc = symbol->function_param_count;

    // Push the args in the registers on the stack. The order for all args is right to left.
    if (function_pc >= 6) dprintf(f, "\tpush\t%%r9\n");
    if (function_pc >= 5) dprintf(f, "\tpush\t%%r8\n");
    if (function_pc >= 4) dprintf(f, "\tpush\t%%rcx\n");
    if (function_pc >= 3) dprintf(f, "\tpush\t%%rdx\n");
    if (function_pc >= 2) dprintf(f, "\tpush\t%%rsi\n");
    if (function_pc >= 1) dprintf(f, "\tpush\t%%rdi\n");

    // Calculate stack start for locals. reduce by pushed bsp and  above pushed args.
    local_vars_stack_start = -8 - 8 * (function_pc <= 6 ? function_pc : 6);

    // Allocate stack space for local variables
    local_stack_size = symbol->function_local_symbol_count * 8;
    if (local_stack_size > 0)
        dprintf(f, "\tsubq\t$%d, %%rsp\n", local_stack_size);

    while (ir->operation) {
        if (ir->operation == IR_LOAD_CONSTANT) {
            dprintf(f, "\tmovq\t$%d, ", ir->src1->value);
            output_register_name(f, ir->dst->preg);
            dprintf(f, "\n");

        }

        else if (ir->operation == IR_LOAD_STRING_LITERAL) {
            dprintf(f, "\tleaq\t.SL%d(%%rip), ", ir->src1->string_literal_index);
            output_register_name(f, ir->dst->preg);
            dprintf(f, "\n");
        }

        else if (ir->operation == IR_LOAD_VARIABLE) {
            if (ir->src1->global_symbol) {
                if (!ir->src1->is_lvalue)
                    dprintf(f, "\tleaq\t%s(%%rip), ", ir->src1->global_symbol->identifier);
                else
                    dprintf(f, "\tmovq\t%s(%%rip), ", ir->src1->global_symbol->identifier);
            }
            else {
                stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, ir->src1->stack_index);
                if (!ir->src1->is_lvalue)
                    dprintf(f, "\tleaq\t%d(%%rbp), ", stack_offset);
                else
                    dprintf(f, "\tmovq\t%d(%%rbp), ", stack_offset);
            }

            output_register_name(f, ir->dst->preg);
            dprintf(f, "\n");
        }

        else if (ir->operation == IR_INDIRECT) {
            dprintf(f, "\tmovq\t(");
            output_register_name(f, ir->src1->preg);
            dprintf(f, "), ");
            output_register_name(f, ir->dst->preg);
            dprintf(f, "\n");
        }

        else if (ir->operation == IR_PARAM) {
            dprintf(f, "\tpushq\t");
            output_register_name(f, ir->src1->preg);
            dprintf(f, "\n");
        }

        else if (ir->operation == IR_CALL) {
            // Read the first 6 args from the stack in right to left order
            pc = ir->src1->function_call_param_count;
            if (pc >= 6) output_load_param(f, pc - 6, "r9");
            if (pc >= 5) output_load_param(f, pc - 5, "r8");
            if (pc >= 4) output_load_param(f, pc - 4, "rcx");
            if (pc >= 3) output_load_param(f, pc - 3, "rdx");
            if (pc >= 2) output_load_param(f, pc - 2, "rsi");
            if (pc >= 1) output_load_param(f, pc - 1, "rdi");

            // For the remaining args after the first 6, swap the opposite ends of the stack
            for (i = 0; i < pc - 6; i++) {
                dprintf(f, "\tmovq\t%d(%%rsp), %%rax\n", i * 8);
                dprintf(f, "\tmovq\t%%rax, %d(%%rsp)\n", (pc - i - 1) * 8);
            }

            // Adjust the stack for the removed args that are in registers
            if (pc > 0) dprintf(f, "\taddq\t$%d, %%rsp\n", (pc <= 6 ? pc : 6) * 8);

            if (ir->src1->function_symbol->builtin == IR_PRTF || ir->src1->function_symbol->builtin == IR_DPRT) {
                dprintf(f, "\tmovl\t$0, %%eax\n");
            }

            if (ir->src1->function_symbol->builtin)
                dprintf(f, "\tcallq\t%s@PLT\n", ir->src1->function_symbol->identifier);
            else
                dprintf(f, "\tcallq\t%s\n", ir->src1->function_symbol->identifier);

            // For all builtins that return something smaller an int, extend it to a quad
            if (ir->dst) {
                if (ir->dst->type <= TYPE_CHAR)  dprintf(f, "\tcbtw\n");
                if (ir->dst->type <= TYPE_SHORT) dprintf(f, "\tcwtl\n");
                if (ir->dst->type <= TYPE_INT)   dprintf(f, "\tcltq\n");

                dprintf(f, "\tmovq\t%%rax, ");
                output_register_name(f, ir->dst->preg);
                dprintf(f, "\n");
            }
        }

        else if (ir->operation == IR_RETURN) {
            if (ir->src1->preg != REG_RAX) {
                dprintf(f, "\tmovq\t");
                output_register_name(f, ir->src1->preg);
                dprintf(f, ", ");
                output_register_name(f, REG_RAX);
                dprintf(f, "\n");
            }
            dprintf(f, "\tleaveq\n");
            dprintf(f, "\tretq\n");
        }

        else if (ir->operation == IR_ASSIGN) {
            dprintf(f, "\tmovq\t");
            output_register_name(f, ir->src1->preg);
            dprintf(f, ", ");

            if (ir->dst->global_symbol) {
                // dst a global
                dprintf(f, "%s(%%rip)\n", ir->dst->global_symbol->identifier);
            }
            else if (ir->dst->is_local) {
                // dst is on the stack
                stack_offset = get_stack_offset_from_index(function_pc, local_vars_stack_start, ir->dst->stack_index);
                dprintf(f, "%d(%%rbp)\n", stack_offset);
            }
            else if (!ir->dst->is_lvalue) {
                // Register copy
                output_register_name(f, ir->dst->preg);
                dprintf(f, "\n");
            }
            else {
                // dst is an lvalue in a register
                dprintf(f, "(");
                output_register_name(f, ir->dst->preg);
                dprintf(f, ")\n");
            }
        }

        else if (ir->operation == IR_ADD)
            output_op(f, "addq",  ir->src1->preg, ir->src2->preg);

        else if (ir->operation == IR_SUB) {
            dprintf(f, "\txchg\t");
            output_register_name(f, ir->src1->preg);
            dprintf(f, ", ");
            output_register_name(f, ir->src2->preg);
            dprintf(f, "\n");
            output_op(f, "subq",  ir->src1->preg, ir->src2->preg);
        }

        else if (ir->operation == IR_MUL)
            output_op(f, "imulq", ir->src1->preg, ir->src2->preg);

        else if (ir->operation == IR_DIV || ir->operation == IR_MOD) {
            // This is slightly ugly. src1 is the dividend and needs doubling in size and placing in the RDX register.
            // It could have been put in the RDX register in the first place.
            // The quotient is stored in RAX and remainder in RDX, but is then copied
            // to whatever register is allocated for the dst, which might as well have been RAX or RDX for the respective quotient and remainders.

            dprintf(f, "\tmovq\t");
            output_register_name(f, ir->src1->preg);
            dprintf(f, ", %%rax\n");
            dprintf(f, "\tcqto\n");
            dprintf(f, "\tidivq\t");
            output_register_name(f, ir->src2->preg);
            dprintf(f, "\n");
            if (ir->operation == IR_DIV)
                dprintf(f, "\tmovq\t%%rax, ");
            else
                dprintf(f, "\tmovq\t%%rdx, ");
            output_register_name(f, ir->dst->preg);
            dprintf(f, "\n");
        }

        else if (ir->operation == IR_BNOT)  {
            dprintf(f, "\tnot\t");
            output_register_name(f, ir->src1->preg);
            dprintf(f, "\n");
        }

        else if (ir->operation == IR_EQ)   output_cmp_operation(f, ir, "sete");
        else if (ir->operation == IR_NE)   output_cmp_operation(f, ir, "setne");
        else if (ir->operation == IR_LT)   output_cmp_operation(f, ir, "setl");
        else if (ir->operation == IR_GT)   output_cmp_operation(f, ir, "setg");
        else if (ir->operation == IR_LE)   output_cmp_operation(f, ir, "setle");
        else if (ir->operation == IR_GE)   output_cmp_operation(f, ir, "setge");
        else if (ir->operation == IR_BOR)  output_op(f, "orq",  ir->src1->preg, ir->src2->preg);
        else if (ir->operation == IR_BAND) output_op(f, "andq", ir->src1->preg, ir->src2->preg);
        else if (ir->operation == IR_XOR)  output_op(f, "xorq", ir->src1->preg, ir->src2->preg);

        else if (ir->operation == IR_BSHL || ir->operation == IR_BSHR) {
            // Ugly, this means rcx is permamently allocated

            dprintf(f, "\tmovq\t");
            output_register_name(f, ir->src2->preg);
            dprintf(f, ", %%rcx\n");
            dprintf(f, "\t%s\t%%cl, ", ir->operation == IR_BSHL ? "shl" : "sar");
            output_register_name(f, ir->src1->preg);
            dprintf(f, "\n");
        }

        else {
            printf("output_function_body_code(): Unknown operation: %d\n", ir->operation);
            exit(1);
        }
        ir++;
    }

    if (!strcmp(symbol->identifier, "main")) dprintf(f, "\tmovq\t$0, %%rax\n");

    dprintf(f, "\tleaveq\n");
    dprintf(f, "\tretq\n");
}

void output_code(char *input_filename, char *output_filename) {
    int i, f;
    struct three_address_code *tac;
    struct symbol *s;
    char *sl;

    // Write file
    if (!strcmp(output_filename, "-"))
        f = 1;
    else {
        f = open(output_filename, 577, 420); // O_TRUNC=512, O_CREAT=64, O_WRONLY=1, mode=555 http://man7.org/linux/man-pages/man2/open.2.html
        if (f < 0) { printf("Unable to open write output file\n"); exit(1); }
    }

    dprintf(f, "\t.file\t\"%s\"\n", input_filename);

    dprintf(f, "\t.text\n");
    s = symbol_table;
    while (s->identifier) {
        if (s->scope || s->is_function || s->builtin) { s++; continue; };
        dprintf(f, "\t.comm %s,%d,%d\n", s->identifier, get_type_size(s->type), get_type_alignment(s->type));
        s++;
    }

    if (string_literal_count > 0) {
        dprintf(f, "\n\t.section\t.rodata\n");
        for (i = 0; i < string_literal_count; i++) {
            sl = string_literals[i];
            dprintf(f, ".SL%d:\n", i);
            dprintf(f, "\t.string ");
            dprintf_escaped_string_literal(f, sl);
            dprintf(f, "\n");
        }
        dprintf(f, "\n");
    }

    dprintf(f, "\t.text\n");

    s = symbol_table;
    while (s->identifier) {
        if (s->is_function) dprintf(f, "\t.globl\t%s\n", s->identifier);
        s++;
    }

    dprintf(f, "\n");

    s = symbol_table;
    while (s->identifier) {
        if (!s->is_function) { s++; continue; }

        dprintf(f, "%s:\n", s->identifier);

        // registers start at 1
        liveness_size = 0;
        liveness = malloc(sizeof(struct liveness_interval) * (MAX_LIVENESS_SIZE + 1));

        analyze_liveness(s->ir, s->vreg_count);
        if (print_ir_before) print_intermediate_representation(s->identifier, s->ir);
        allocate_registers(s->ir);
        if (print_ir_after) print_intermediate_representation(s->identifier, s->ir);
        output_function_body_code(f, s);
        dprintf(f, "\n");
        s++;
    }

    close(f);
}

void add_builtin(char *identifier, int instruction, int type) {
    struct symbol *symbol;
    symbol = next_symbol;
    symbol->type = type;
    symbol->identifier = identifier;
    symbol->builtin = instruction;
    next_symbol++;
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

int main(int argc, char **argv) {
    char *input_filename, *output_filename;
    int f;
    int help, debug, print_symbols, print_code;
    int filename_len;

    help = 0;
    print_ir_before = 0;
    print_ir_after = 0;
    print_exit_code = 1;
    print_cycles = 1;
    print_symbols = 0;
    output_filename = 0;

    argc--;
    argv++;
    while (argc > 0 && *argv[0] == '-') {
             if (argc > 0 && !memcmp(argv[0], "-h",  3)) { help = 0;               argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-d",  2)) { debug = 1;              argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-rb", 3)) { print_ir_before = 1;    argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-ra", 3)) { print_ir_after = 1;     argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-s",  2)) { print_symbols = 1;      argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-ne", 3)) { print_exit_code = 0;    argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-nc", 3)) { print_cycles = 0;       argc--; argv++; }
        else if (argc > 1 && !memcmp(argv[0], "-o",  2)) {
            output_filename = argv[1];
            argc -= 2;
            argv += 2;
        }
        else { printf("Unknown parameter %s\n", argv[0]); exit(1); }
    }

    if (help || argc < 1) {
        printf("Usage: wc4 [-d -rb -ra -s -c-ne -nc] INPUT-FILE\n\n");
        printf("Flags\n");
        printf("-d      Debug output\n");
        printf("-s      Output symbol table\n");
        printf("-o      Output code without executing it. Use - for stdout.\n");
        printf("-ne     Don't print exit code\n");
        printf("-nc     Don't print cycles\n");
        printf("-h      Help\n");
        exit(1);
    }

    input_filename = argv[0];

    input = malloc(10 * 1024 * 1024);
    symbol_table = malloc(SYMBOL_TABLE_SIZE);
    memset(symbol_table, 0, SYMBOL_TABLE_SIZE);
    next_symbol = symbol_table;

    string_literals = malloc(MAX_STRING_LITERALS);
    string_literal_count = 0;
    global_variable_count = 0;

    all_structs = malloc(sizeof(struct str_desc *) * MAX_STRUCTS);
    all_structs_count = 0;

    add_builtin("exit",    IR_EXIT, TYPE_VOID);
    add_builtin("open",    IR_OPEN, TYPE_INT);
    add_builtin("read",    IR_READ, TYPE_INT);
    add_builtin("write",   IR_WRIT, TYPE_INT);
    add_builtin("close",   IR_CLOS, TYPE_INT);
    add_builtin("printf",  IR_PRTF, TYPE_INT);
    add_builtin("dprintf", IR_DPRT, TYPE_INT);
    add_builtin("malloc",  IR_MALC, TYPE_VOID + TYPE_PTR);
    add_builtin("free",    IR_FREE, TYPE_INT);
    add_builtin("memset",  IR_MSET, TYPE_INT);
    add_builtin("memcmp",  IR_MCMP, TYPE_INT);
    add_builtin("strcmp",  IR_SCMP, TYPE_INT);

    f  = open(input_filename, 0, 0); // O_RDONLY = 0
    if (f < 0) { printf("Unable to open input file\n"); exit(1); }
    input_size = read(f, input, 10 * 1024 * 1024);
    input[input_size] = 0;
    if (input_size < 0) { printf("Unable to read input file\n"); exit(1); }
    close(f);

    cur_line = 1;
    next();

    value_stack = malloc(sizeof(struct value *) * VALUE_STACK_SIZE);
    parse();

    if (print_symbols) do_print_symbols();

    if (!output_filename) {
        // Write output file with .s extension from an expected source extension of .c
        filename_len = strlen(input_filename);
        if (filename_len > 2 && input_filename[filename_len - 2] == '.' && input_filename[filename_len - 1] == 'c') {
            output_filename = malloc(filename_len + 1);
            strcpy(output_filename, input_filename);
            output_filename[filename_len - 1] = 's';
        }
        else {
            printf("Unable to determine output filename\n");
            exit(1);
        }
    }

    output_code(input_filename, output_filename);

    exit(0);
}
