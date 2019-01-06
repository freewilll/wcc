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
    int function_param_count;
    int builtin;
    int size;
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

struct fwd_function_backpatch {
    long *iptr;
    struct symbol *symbol;
};

char *input;
int input_size;
int ip;
long *instructions;
char *data;
char *data_start;
long *iptr;

int print_instructions;
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
long cur_type;

struct str_desc **all_structs;
int all_structs_count;

long DATA_SIZE;
long INSTRUCTIONS_SIZE;
long SYMBOL_TABLE_SIZE;

struct fwd_function_backpatch *fwd_function_backpatches;

enum {
    MAX_STRUCTS=1024,
    MAX_STRUCT_MEMBERS=1024,
    MAX_FWD_FUNCTION_BACKPATCHES=1024,
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
    INSTR_LINE=1,
    INSTR_GLB,
    INSTR_LEA,
    INSTR_IMM,
    INSTR_JMP,
    INSTR_JSR,
    INSTR_BZ,
    INSTR_BNZ,
    INSTR_ENT,
    INSTR_ADJ,
    INSTR_BNOT,
    INSTR_LEV,
    INSTR_LC,
    INSTR_LS,
    INSTR_LI,
    INSTR_LL,
    INSTR_SC,
    INSTR_SS,
    INSTR_SI,
    INSTR_SL,
    INSTR_OR,
    INSTR_AND,
    INSTR_BITWISE_OR,
    INSTR_BITWISE_AND,
    INSTR_XOR,
    INSTR_EQ,
    INSTR_NE,
    INSTR_LT,
    INSTR_GT,
    INSTR_LE,
    INSTR_GE,
    INSTR_BWL,
    INSTR_BWR,
    INSTR_ADD,
    INSTR_SUB,
    INSTR_MUL,
    INSTR_DIV,
    INSTR_MOD,
    INSTR_PSH,
    INSTR_OPEN,
    INSTR_READ,
    INSTR_WRIT,
    INSTR_CLOS,
    INSTR_PRTF,
    INSTR_DPRT,
    INSTR_MALC,
    INSTR_FREE,
    INSTR_MSET,
    INSTR_MCMP,
    INSTR_SCMP,
    INSTR_EXIT,
};

enum {IMM_NUMBER, IMM_STRING_LITERAL, IMM_GLOBAL};

int parse_struct_base_type();

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

int get_type_sizeof(int type) {
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

                offset += get_type_sizeof(type);

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
        i = 0;
        while (i < all_structs_count) {
            if (!strcmp(all_structs[i]->name, strct_identifier)) {
                return TYPE_STRUCT + i;
            }
            i++;
        }
        printf("%d: Unknown struct %s\n", cur_line, strct_identifier);
        exit(1);
    }
}

int get_type_inc_dec_size(int type) {
    // How much will the ++ operator increment a type?
    return type <= TYPE_PTR ? 1 : get_type_sizeof(type - TYPE_PTR);
}

int is_lvalue() {
    return *(iptr - 1) == INSTR_LC || *(iptr -1) == INSTR_LS || *(iptr -1) == INSTR_LI || *(iptr -1) == INSTR_LL;
}

int load_type() {
         if (cur_type == TYPE_CHAR)  *iptr++ = INSTR_LC;
    else if (cur_type == TYPE_SHORT) *iptr++ = INSTR_LS;
    else if (cur_type == TYPE_INT)   *iptr++ = INSTR_LI;
    else if (cur_type == TYPE_LONG)  *iptr++ = INSTR_LL;
    else if (cur_type >  TYPE_LONG)  *iptr++ = INSTR_LL;
    else {
        printf("%d: load for unknown type %ld\n", cur_line, cur_type);
        exit(1);
    }
}

int store_type(int type) {
         if (type == TYPE_CHAR)  *iptr++ = INSTR_SC;
    else if (type == TYPE_SHORT) *iptr++ = INSTR_SS;
    else if (type == TYPE_INT)   *iptr++ = INSTR_SI;
    else if (type == TYPE_LONG)  *iptr++ = INSTR_SL;
    else if (type >  TYPE_PTR)   *iptr++ = INSTR_SL;
    else {
        printf("%d: load for unknown type %d\n", cur_line, type);
        exit(1);
    }
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
    long *temp_iptr;
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
    int i;

    if (cur_token == TOK_LOGICAL_NOT) {
        next();
        *iptr++ = INSTR_IMM;
        *iptr++ = 0;
        *iptr++ = IMM_NUMBER;
        *iptr++ = 0;
        *iptr++ = INSTR_PSH;
        expression(TOK_INC);
        *iptr++ = INSTR_EQ;
        cur_type = TYPE_INT;
    }
    else if (cur_token == TOK_BITWISE_NOT) {
        next();
        expression(TOK_INC);
        *iptr++ = INSTR_BNOT;
        cur_type = TYPE_INT;
    }
    else if (cur_token == TOK_ADDRESS_OF) {
        next();
        expression(TOK_INC);
        if (!is_lvalue()) {
            printf("%d: Cannot take an address of an rvalue on line\n", cur_line);
            exit(1);
        }
        iptr--;                                                // Roll back load
        cur_type = cur_type + TYPE_PTR;
    }
    else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
        // Prefix increment & decrement
        org_token = cur_token;
        next();
        expression(1024); // Fake highest precedence, bind nothing
        org_type = cur_type;
        if (!is_lvalue()) {
            printf("%d: Cannot prefix increment/decrement an rvalue on line\n", cur_line);
            exit(1);
        }
        iptr--;              // Roll back load
        *iptr++ = INSTR_PSH; // Push address
        load_type();         // Push value
        *iptr++ = INSTR_PSH;
        *iptr++ = INSTR_IMM;
        *iptr++ = get_type_inc_dec_size(org_type);
        *iptr++ = IMM_NUMBER;
        *iptr++ = 0;
        *iptr++ = org_token == TOK_INC ? INSTR_ADD : INSTR_SUB;
        cur_type = org_type;
        store_type(cur_type);
    }
    else if (cur_token == TOK_MULTIPLY) {
        next();
        expression(TOK_INC);
        if (cur_type <= TYPE_PTR) {
            printf("%d: Cannot derefence a non-pointer %ld\n", cur_line, cur_type);
            exit(1);
        }
        cur_type = cur_type - TYPE_PTR;
        load_type();
    }
    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            *iptr++ = INSTR_IMM;
            *iptr++ = -cur_integer;
            *iptr++ = IMM_NUMBER;
            *iptr++ = 0;
            next();
            cur_type = TYPE_INT;
        }
        else {
            *iptr++ = INSTR_IMM;
            *iptr++ = -1;
            *iptr++ = IMM_NUMBER;
            *iptr++ = 0;
            *iptr++ = INSTR_PSH;
            expression(TOK_INC);
            *iptr++ = INSTR_MUL;
        }
    }
    else if (cur_token == TOK_LPAREN) {
        next();
        if (cur_token == TOK_VOID || cur_token_is_integer_type() || cur_token == TOK_STRUCT) {
            // cast
            org_type = parse_type();
            consume(TOK_RPAREN);
            expression(TOK_INC);
            cur_type = org_type;
        }
        else {
            expression(TOK_COMMA);
            consume(TOK_RPAREN);
        }
    }
    else if (cur_token == TOK_NUMBER) {
        *iptr++ = INSTR_IMM;
        *iptr++ = cur_integer;
        *iptr++ = IMM_NUMBER;
        *iptr++ = 0;
        cur_type = TYPE_INT;
        next();
    }
    else if (cur_token == TOK_STRING_LITERAL) {
        *iptr++ = INSTR_IMM;
        *iptr++ = (long) data;
        *iptr++ = IMM_STRING_LITERAL;
        *iptr++ = 0;
        while (*data++ = *cur_string_literal++);
        cur_type = TYPE_CHAR + TYPE_PTR;
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
            *iptr++ = INSTR_IMM;
            *iptr++ = symbol->value;
            *iptr++ = IMM_NUMBER;
            *iptr++ = 0;
            cur_type = symbol->type;
        }
        else if (cur_token == TOK_LPAREN) {
            // Function call
            next();
            param_count = 0;
            while (cur_token != TOK_RPAREN) {
                expression(TOK_COMMA);
                *iptr++ = INSTR_PSH;
                if (cur_token == TOK_COMMA) next();
                param_count++;
            }
            consume(TOK_RPAREN);
            builtin = symbol->builtin;
            if (builtin) {
                *iptr++ = builtin;
                if (!strcmp("printf", (char *) symbol->identifier) || !strcmp("dprintf", (char *) symbol->identifier)) {
                    if (param_count >  10) {
                        printf("printf can't handle more than 10 args\n");
                        exit(1);
                    }
                    *iptr++ = param_count;
                }
            }
            else {
                *iptr++ = INSTR_JSR;
                if (!symbol->value) {
                    // The function hasn't been defined yet, add a backpatch to it.
                    i = 0;
                    while (i < MAX_FWD_FUNCTION_BACKPATCHES) {
                        if (!fwd_function_backpatches[i].iptr) {
                            fwd_function_backpatches[i].iptr = iptr;
                            fwd_function_backpatches[i].symbol = symbol;
                            i = MAX_FWD_FUNCTION_BACKPATCHES; // break
                        }
                        i++;
                    }
                }

                *iptr++ = symbol->value;
                *iptr++ = (long) symbol->identifier;
                *iptr++ = param_count;
                *iptr++ = INSTR_ADJ;
                *iptr++ = param_count;
            }
            cur_type = symbol->type;
        }
        else if (scope == 0) {
            // Global symbol
            address = (long *) symbol->value;
            *iptr++ = INSTR_IMM;
            *iptr++ = (long) address;
            cur_type = symbol->type;
            *iptr++ = IMM_GLOBAL;
            *iptr++ = (long) symbol;
            load_type();
        }
        else {
            // Local symbol
            param_count = cur_function_symbol->function_param_count;
            *iptr++ = INSTR_LEA;
            if (symbol->stack_index >= 0) {
                stack_index = param_count - symbol->stack_index - 1;
                *iptr++ = stack_index + 2; // Step over pushed PC and BP
            }
            else {
                *iptr++ = symbol->stack_index;
            }
            cur_type = symbol->type;
            load_type();
        }
    }
    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN);
        cur_type = parse_type();
        *iptr++ = INSTR_IMM;
        *iptr++ = get_type_sizeof(cur_type);
        *iptr++ = IMM_NUMBER;
        *iptr++ = 0;
        consume(TOK_RPAREN);
        cur_type = TYPE_INT;
    }
    else {
        printf("%d: Unexpected token %d in expression\n", cur_line, cur_token);
        exit(1);
    }

    if (cur_token == TOK_LBRACKET) {
        next();

        if (cur_type <= TYPE_PTR) {
            printf("%d: Cannot do [] on a non-pointer for type %ld\n", cur_line, cur_type);
            exit(1);
        }

        org_type = cur_type;
        *iptr++ = INSTR_PSH;
        expression(TOK_COMMA);

        factor = get_type_inc_dec_size(org_type);
        if (factor > 1) {
            *iptr++ = INSTR_PSH;
            *iptr++ = INSTR_IMM;
            *iptr++ = factor;
            *iptr++ = IMM_NUMBER;
            *iptr++ = 0;
            *iptr++ = INSTR_MUL;
        }

        *iptr++ = INSTR_ADD;
        consume(TOK_RBRACKET);
        cur_type = org_type - TYPE_PTR;
        load_type();
    }

    while (cur_token >= level) {
        // In order or precedence

        if (cur_token == TOK_INC || cur_token == TOK_DEC) {
            // Postfix increment and decrement
            iptr--;              // Roll back load
            *iptr++ = INSTR_PSH; // Push address
            load_type();         // Push value
            *iptr++ = INSTR_PSH;
            *iptr++ = INSTR_IMM;
            *iptr++ = get_type_inc_dec_size(cur_type);
            *iptr++ = IMM_NUMBER;
            *iptr++ = 0;
            *iptr++ = cur_token == TOK_INC ? INSTR_ADD : INSTR_SUB;
            store_type(cur_type);

            // Dirty!
            *iptr++ = INSTR_PSH;
            *iptr++ = INSTR_IMM;
            *iptr++ = get_type_inc_dec_size(cur_type);
            *iptr++ = IMM_NUMBER;
            *iptr++ = 0;
            *iptr++ = cur_token == TOK_INC ? INSTR_SUB : INSTR_ADD;

            next();
        }
        else if (cur_token == TOK_DOT || cur_token == TOK_ARROW) {

            if (cur_token == TOK_DOT) {
                // Struct member lookup. A load has already been pushed for it

                if (cur_type < TYPE_STRUCT || cur_type >= TYPE_PTR) {
                    printf("%d: Cannot use . on a non-struct\n", cur_line);
                    exit(1);
                }

                if (!is_lvalue()) {
                    printf("%d: Expected lvalue for struct . operation.\n", cur_line);
                    exit(1);
                }
                iptr--; // Roll back load instruction
            }
            else {
                // Pointer to struct member lookup.

                if (cur_type < TYPE_PTR) {
                    printf("%d: Cannot use -> on a non-pointer\n", cur_line);
                    exit(1);
                }

                cur_type -= TYPE_PTR;

                if (cur_type < TYPE_STRUCT) {
                    printf("%d: Cannot use -> on a pointer to a non-struct\n", cur_line);
                    exit(1);
                }
            }

            next();
            if (cur_token != TOK_IDENTIFIER) {
                printf("%d: Expected identifier\n", cur_line);
                exit(1);
            }

            str = all_structs[cur_type - TYPE_STRUCT];
            member = lookup_struct_member(str, cur_identifier);

            if (member->offset > 0) {
                *iptr++ = INSTR_PSH;
                *iptr++ = INSTR_IMM;
                *iptr++ = member->offset;
                *iptr++ = IMM_NUMBER;
                *iptr++ = 0;
                *iptr++ = INSTR_ADD;
            }

            cur_type = member->type;
            load_type();
            consume(TOK_IDENTIFIER);
        }
        else if (cur_token == TOK_MULTIPLY) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_DOT);
            *iptr++ = INSTR_MUL;
        }
        else if (cur_token == TOK_DIVIDE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_INC);
            *iptr++ = INSTR_DIV;
        }
        else if (cur_token == TOK_MOD) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_INC);
            *iptr++ = INSTR_MOD;
        }
        else if (cur_token == TOK_PLUS || cur_token == TOK_MINUS) {
            org_token = cur_token;
            org_type = cur_type;
            factor = get_type_inc_dec_size(cur_type);
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);

            if (factor > 1) {
                *iptr++ = INSTR_PSH;
                *iptr++ = INSTR_IMM;
                *iptr++ = factor;
                *iptr++ = IMM_NUMBER;
                *iptr++ = 0;
                *iptr++ = INSTR_MUL;
            }

            *iptr++ = org_token == TOK_PLUS ? INSTR_ADD : INSTR_SUB;
            cur_type = org_type;
        }
        else if (cur_token == TOK_BITWISE_LEFT) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_BWL;
        }
        else if (cur_token == TOK_BITWISE_RIGHT) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_BWR;
        }
        else if (cur_token == TOK_LT) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_BITWISE_LEFT);
            *iptr++ = INSTR_LT;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_GT) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_GT;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_LE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_LE;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_GE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_GE;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_DBL_EQ) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_LT);
            *iptr++ = INSTR_EQ;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_NOT_EQ) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_LT);
            *iptr++ = INSTR_NE;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_ADDRESS_OF) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_DBL_EQ);
            *iptr++ = INSTR_BITWISE_AND;
            cur_type += TYPE_PTR;
        }
        else if (cur_token == TOK_XOR) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_ADDRESS_OF);
            *iptr++ = INSTR_XOR;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_BITWISE_OR) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_XOR);
            *iptr++ = INSTR_BITWISE_OR;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_AND) {
            temp_iptr = iptr;
            *iptr++ = INSTR_BZ;
            *iptr++ = 0;
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_BITWISE_OR);
            *iptr++ = INSTR_AND;
            *(temp_iptr + 1) = (long) iptr;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_OR) {
            temp_iptr = iptr;
            *iptr++ = INSTR_BNZ;
            *iptr++ = 0;
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_AND);
            *iptr++ = INSTR_OR;
            *(temp_iptr + 1) = (long) iptr;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_TERNARY) {
            next();
            false_jmp = iptr;
            *iptr++ = INSTR_BZ;
            *iptr++ = 0;
            expression(TOK_OR);
            true_done_jmp = iptr;
            *iptr++ = INSTR_JMP;
            *iptr++ = 0;
            consume(TOK_COLON);
            *(false_jmp + 1) = (long) iptr;
            expression(TOK_OR);
            *(true_done_jmp + 1) = (long) iptr;
        }
        else if (cur_token == TOK_EQ) {
            next();
            if (!is_lvalue()) {
                printf("%d: Cannot assign to an rvalue\n", cur_line);
                exit(1);
            }
            iptr--;
            org_type = cur_type;
            *iptr++ = INSTR_PSH;
            expression(TOK_EQ);
            store_type(org_type);
            type = org_type;
        }
        else if (cur_token == TOK_PLUS_EQ || cur_token == TOK_MINUS_EQ) {
            org_token = cur_token;
            next();
            if (!is_lvalue()) {
                printf("%d: Cannot assign to an rvalue\n", cur_line);
                exit(1);
            }
            org_type = cur_type;
            iptr--;               // Roll back load
            *iptr++ = INSTR_PSH;  // Push address
            load_type();          // Push value
            *iptr++ = INSTR_PSH;

            factor = get_type_inc_dec_size(org_type);
            if (factor > 1) {
                *iptr++ = INSTR_IMM;
                *iptr++ = factor;
                *iptr++ = IMM_NUMBER;
                *iptr++ = 0;
                *iptr++ = INSTR_PSH;
            }

            expression(TOK_EQ);
            if (factor > 1) *iptr++ = INSTR_MUL;
            *iptr++ = org_token == TOK_PLUS_EQ ? INSTR_ADD : INSTR_SUB;
            cur_type = org_type;
            store_type(cur_type);
        }
        else
            return; // Bail once we hit something unknown

    }
}

void statement() {
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

    if (print_instructions) {
        *iptr++ = INSTR_LINE;
        *iptr++ = cur_line;
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
        // Preserve previous loop addresses so that we can have nested whiles/fors
        old_cur_loop_continue_address = cur_loop_continue_address;
        old_cur_loop_body_start = cur_loop_body_start;

        next();
        consume(TOK_LPAREN);

        cur_loop_continue_address = iptr;
        expression(TOK_COMMA);
        consume(TOK_RPAREN);
        *iptr++ = INSTR_BZ;
        *iptr++ = 0;

        cur_loop_body_start = iptr;
        statement();

        *iptr++ = INSTR_JMP;
        *iptr++ = (long) cur_loop_continue_address;
        *(cur_loop_body_start - 1) = (long) iptr;

        // Restore previous loop addresses
        cur_loop_continue_address = old_cur_loop_continue_address;
        cur_loop_body_start = old_cur_loop_body_start;
    }
    else if (cur_token == TOK_FOR) {
        old_cur_loop_continue_address = cur_loop_continue_address;
        old_cur_loop_body_start = cur_loop_body_start;

        next();
        consume(TOK_LPAREN);

        expression(TOK_COMMA); // setup expression
        consume(TOK_SEMI);

        cur_loop_test_start = iptr;
        expression(TOK_COMMA); // test expression
        consume(TOK_SEMI);
        *iptr++ = INSTR_BZ;
        *iptr++ = 0;

        *iptr++ = INSTR_JMP; // Jump to body start
        *iptr++;

        cur_loop_continue_address = iptr;
        expression(TOK_COMMA); // increment expression
        consume(TOK_RPAREN);
        *iptr++ = INSTR_JMP;
        *iptr++ = (long) cur_loop_test_start;

        cur_loop_body_start = iptr;
        *(cur_loop_continue_address - 1) = (long) iptr;
        statement();
        *iptr++ = INSTR_JMP;
        *iptr++ = (long) cur_loop_continue_address;
        *(cur_loop_continue_address - 3) = (long) iptr;

        // Restore previous loop addresses
        cur_loop_continue_address = old_cur_loop_continue_address;
        cur_loop_body_start = old_cur_loop_body_start;
    }
    else if (cur_token == TOK_CONTINUE) {
        next();
        *iptr++ = INSTR_JMP;
        *iptr++ = (long) cur_loop_continue_address;
        consume(TOK_SEMI);
    }
    else if (cur_token == TOK_IF) {
        next();
        consume(TOK_LPAREN);
        expression(TOK_COMMA);
        consume(TOK_RPAREN);
        false_jmp = iptr;
        *iptr++ = INSTR_BZ;
        *iptr++ = 0;
        statement();
        *(false_jmp + 1) = (long) iptr;
        if (cur_token == TOK_ELSE) {
            next();
            true_done_jmp = iptr;
            *iptr++ = INSTR_JMP;
            *iptr++ = 0;
            *(false_jmp + 1) = (long) iptr;
            statement();
            *(true_done_jmp + 1) = (long) iptr;
        }
        else
            *(false_jmp + 1) = (long) iptr;
    }
    else if (cur_token == TOK_RETURN) {
        next();
        if (cur_token != TOK_SEMI) expression(TOK_COMMA);
        *iptr++ = INSTR_LEV;
        consume(TOK_SEMI);
        seen_return = 1;
    }
    else {
        expression(TOK_COMMA);
        consume(TOK_SEMI);
    }
}

void function_body(char *func_name, int param_count) {
    int is_main;
    int local_symbol_count;
    int base_type, type;

    seen_return = 0;
    is_main = !strcmp(func_name, "main");
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

    *iptr++ = INSTR_ENT;
    *iptr++ = local_symbol_count * sizeof(long); // allocate stack space for locals
    *iptr++ = param_count;

    while (cur_token != TOK_RCURLY) statement();

    if (is_main && !seen_return) {
        *iptr++ = INSTR_IMM;
        *iptr++ = 0;
        *iptr++ = IMM_NUMBER;
        *iptr++ = 0;
        cur_type = TYPE_INT;
    }

    if (*(iptr - 1) != INSTR_LEV) *iptr++ = INSTR_LEV;

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
                    seen_function = 1;
                    cur_scope++;
                    next();

                    // Function declaration or definition
                    cur_symbol->value = (long) iptr;
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
                        // Add global definition for use by the assembler
                        *iptr++ = INSTR_GLB;
                        *iptr++ = (long) cur_function_name;
                        *iptr++ = get_type_sizeof(type);
                        *iptr++ = GLB_TYPE_FUNCTION;

                        function_body((char *) cur_symbol->identifier, param_count);

                        // Now that this function is defined, handle any backpatches to it
                        i = 0;
                        while (i < MAX_FWD_FUNCTION_BACKPATCHES) {
                            if (fwd_function_backpatches[i].symbol == cur_function_symbol) {
                                *fwd_function_backpatches[i].iptr = cur_function_symbol->value;
                                fwd_function_backpatches[i].iptr = 0;
                                fwd_function_backpatches[i].symbol = 0;
                            }
                            i++;
                        }
                    }
                    else
                        // Make it clear that this symbol will need to be backpatched if used
                        // before the definition has been processed.
                        cur_function_symbol->value = 0;

                    doing_var_declaration = 0;
                }
                else {
                    // Global symbol
                    *iptr++ = INSTR_GLB;
                    *iptr++ = (long) cur_identifier;
                    *iptr++ = get_type_sizeof(type);
                    *iptr++ = GLB_TYPE_VARIABLE;

                    if (seen_function) {
                        printf("%d: Global variables must precede all functions\n", cur_line);
                        exit(1);
                    }

                    cur_symbol->value = (long) data;
                    data += sizeof(long);
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

void print_instruction(int f, long *pc, int relative, int print_pc) {
    int instr;
    long operand, imm_type;
    char *s;
    struct symbol *symbol;
    instr = *pc;

    if (print_pc) dprintf(f, "%-15ld ", (long) pc - (long) instructions);
    dprintf(f, "%.5s", &"LINE GLB  LEA  IMM  JMP  JSR  BZ   BNZ  ENT  ADJ  BNOT LEV  LC   LS   LI   LL   SC   SS   SI   SL   OR   AND  BOR  BAND XOR  EQ   NE   LT   GT   LE   GE   BWL  BWR  ADD  SUB  MUL  DIV  MOD  PSH  OPEN READ WRIT CLOS PRTF DPRT MALC FREE MSET MCMP SCMP EXIT "[instr * 5 - 5]);
    if (instr <= INSTR_ADJ) {
        operand = *(pc + 1);
        symbol = (struct symbol *) *(pc + 3);
        if (relative) {
            if ((instr == INSTR_IMM)) {
                imm_type = *(pc + 2);
                if (imm_type == IMM_NUMBER)
                    dprintf(f, " %ld", operand);
                else if (imm_type == IMM_STRING_LITERAL) {
                    dprintf(f, " &\"");
                    s = (char *) operand;
                    while (*s) {
                        if (*s == '\n') dprintf(f, "\\n");
                        else if (*s == '\t') dprintf(f, "\\t");
                        else if (*s == '\\') dprintf(f, "\\\\");
                        else if (*s == '"') dprintf(f, "\\\"");
                        else dprintf(f, "%c", *s);
                        s++;
                    }
                    dprintf(f, "\"");
                }
                else if (imm_type == IMM_GLOBAL)
                    dprintf(f, " global %d \"%s\"", symbol->stack_index, (char *) symbol->identifier);
                else {
                    dprintf(f, "unknown imm %ld\n", imm_type);
                    exit(1);
                }
            }
            else if (instr == INSTR_ENT) {
                dprintf(f, " %ld %ld", *(pc + 2), operand); // # of params, local stack size
                pc += 2;
            }
            else if (instr == INSTR_GLB) {
                dprintf(f, " type=%ld size=%ld \"%s\"", *(pc + 3), *(pc + 2), (char *) operand);
                pc += 2;
            }
            else if (instr == INSTR_JSR)
                dprintf(f, " %ld %ld \"%s\"", operand - (long) instructions, *(pc + 3), (char *) *(pc + 2));
            else if ((instr == INSTR_JMP) || (instr == INSTR_BZ) || (instr == INSTR_BNZ)) {
                dprintf(f, " %ld", operand - (long) instructions);
            }
            else
                dprintf(f, " %ld", operand);
        }
        else
            dprintf(f, " %ld", operand);
    }

    else if (instr == INSTR_PRTF || instr == INSTR_DPRT) {
        operand = *(pc + 1);
        dprintf(f, " %ld", operand);
    }

    dprintf(f, "\n");
}

void output_code(char *filename) {
    long *pc;
    int f, instr;

    if (filename[0] == '-' && filename[1] == 0)
        f = 1;
    else {
        f = open(filename, 577, 420); // O_TRUNC=512, O_CREAT=64, O_WRONLY=1, mode=555 http://man7.org/linux/man-pages/man2/open.2.html
        if (f < 0) { printf("Unable to open write output file\n"); exit(1); }
    }

    pc = instructions;
    while (*pc) {
        instr = *pc;
        print_instruction(f, pc, 1, 1);
        if (instr == INSTR_IMM) pc += 4;
        else if (instr == INSTR_GLB) pc += 4;
        else if (instr == INSTR_ENT) pc += 3;
        else if (instr == INSTR_JSR) pc += 4;
        else if (instr <= INSTR_ADJ) pc += 2;
        else if (instr == INSTR_PRTF) pc += 2;
        else if (instr == INSTR_DPRT) pc += 2;
        else pc++;
    }

    close(f);
}

long run(long argc, char **argv, int print_instructions) {
    long *stack;
    long a;
    long *pc;
    long *sp, *bp;
    long *t;
    long instr;
    long cycle;

    stack = malloc(sizeof(long) * 1024 * 1024);

    sp = stack + 1024 * 1024;
    bp = sp;

    *--sp = INSTR_EXIT; // call exit if main returns
    *--sp = INSTR_PSH;
    t = sp;
    *--sp = argc;
    *--sp = (long) argv;
    *--sp = (long) t;

    cycle = 0;
    a = 0;

    pc = (long *) lookup_function("main");

    while (*pc) {
        if (sp < stack) {
            printf("Stack overflow\n");
            exit(1);
        }

        cycle++;

        if (print_instructions) {
            dprintf(1, "%-5ld> ", cycle);
            dprintf(1, "pc = %-15ld ", (long) pc - 8);
            dprintf(1, "a = %-15ld ", a);
            dprintf(1, "sp = %-15ld ", (long) sp);
            print_instruction(1, pc, 0, 0);
        }

        instr = *pc++;

             if (instr == INSTR_LINE) pc++;                                                 // No-op, print line number
        else if (instr == INSTR_GLB) pc += 3;                                               // Global
        else if (instr == INSTR_LEA) a = (long) (bp + *pc++);                               // load local address
        else if (instr == INSTR_IMM) {a = *pc++; pc += 2; }                                 // load global address or immediate
        else if (instr == INSTR_JMP) pc = (long *) *pc;                                     // jump
        else if (instr == INSTR_JSR) { *--sp = (long) (pc + 3); pc = (long *)*pc; }         // jump to subroutine
        else if (instr == INSTR_BZ)  pc = a ? pc + 1 : (long *) *pc;                        // branch if zero
        else if (instr == INSTR_BNZ) pc = a ? (long *) *pc : pc + 1;                        // branch if not zero
        else if (instr == INSTR_ENT) { *--sp = (long) bp; bp = sp; sp = sp - *pc++; pc++; } // enter subroutine
        else if (instr == INSTR_ADJ) sp = sp + *pc++;                                       // stack adjust
        else if (instr == INSTR_LEV) { sp = bp; bp = (long *) *sp++; pc = (long *) *sp++; } // leave subroutine
        else if (instr == INSTR_LC) a = *(char *)a;                                         // load char
        else if (instr == INSTR_LS) a = *(short *)a;                                        // load short
        else if (instr == INSTR_LI) a = *(int *)a;                                          // load int
        else if (instr == INSTR_LL) a = *(long *)a;                                         // load long
        else if (instr == INSTR_SC) a = *(char *)*sp++ = a;                                 // store char
        else if (instr == INSTR_SS) a = *(short  *) *sp++ = a;                              // store short
        else if (instr == INSTR_SI) a = *(int *)*sp++ = a;                                  // store int
        else if (instr == INSTR_SL) *(long *)*sp++ = a;                                     // store long
        else if (instr == INSTR_PSH) *--sp = a;
        else if (instr == INSTR_OR ) a = *sp++ || a;
        else if (instr == INSTR_AND) a = *sp++ && a;
        else if (instr == INSTR_BITWISE_OR) a = *sp++ | a;
        else if (instr == INSTR_BITWISE_AND) a = *sp++ & a;
        else if (instr == INSTR_XOR) a = *sp++ ^ a;
        else if (instr == INSTR_BNOT) a = ~ a;
        else if (instr == INSTR_EQ ) a = *sp++ == a;
        else if (instr == INSTR_NE ) a = *sp++ != a;
        else if (instr == INSTR_LT ) a = *sp++ < a;
        else if (instr == INSTR_GT ) a = *sp++ > a;
        else if (instr == INSTR_LE ) a = *sp++ <= a;
        else if (instr == INSTR_GE ) a = *sp++ >= a;
        else if (instr == INSTR_BWL ) a = *sp++ << a;
        else if (instr == INSTR_BWR ) a = *sp++ >> a;
        else if (instr == INSTR_ADD) a = *sp++ + a;
        else if (instr == INSTR_SUB) a = *sp++ - a;
        else if (instr == INSTR_MUL) a = *sp++ * a;
        else if (instr == INSTR_DIV) a = *sp++ / a;
        else if (instr == INSTR_MOD) a = *sp++ % a;
        else if (instr == INSTR_OPEN) { a = open((char *) sp[2], sp[1], *sp); sp += 3; }
        else if (instr == INSTR_READ) { a = read(sp[2], (char *) sp[1], *sp); sp += 3; }
        else if (instr == INSTR_WRIT) { a = write(sp[2], (char *) sp[1], *sp); sp += 3; }
        else if (instr == INSTR_CLOS) a = close(*sp++);
        else if (instr == INSTR_PRTF) { t = sp + *pc++; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6], t[-7], t[-8], t[-9], t[-10]); sp += *(pc - 1); }
        else if (instr == INSTR_DPRT) { t = sp + *pc++; a = dprintf(t[-1], (char *)t[-2], t[-3], t[-4], t[-5], t[-6], t[-7], t[-8], t[-9], t[-10]); sp += *(pc - 1); }
        else if (instr == INSTR_MALC) a = (long) malloc(*sp++);
        else if (instr == INSTR_FREE) free((void *) *sp++);
        else if (instr == INSTR_MSET) { a = (long) memset((char *) sp[2], sp[1], *sp); sp += 3; }
        else if (instr == INSTR_MCMP) { a = (long) memcmp((char *) sp[2], (char *) sp[1], *sp); sp += 3; }
        else if (instr == INSTR_SCMP) { a = (long) strcmp((char *) sp[1], (char *) *sp); sp += 2; }
        else if (instr == INSTR_EXIT) {
            if (print_cycles && print_exit_code) printf("exit %ld in %ld cycles\n", *sp, cycle);
            else {
                if (print_cycles) printf("%ld cycles\n", cycle);
                if (print_exit_code) printf("exit %ld\n", *sp);
            }
            return *sp;
        }

        else {
            printf("Internal error: unknown instruction %ld\n", instr);
            exit(1);
        }
    }

    // Should never get here
    return 0;
}

void add_builtin(char *identifier, int instruction) {
    struct symbol *symbol;
    symbol = next_symbol;
    symbol->type = TYPE_VOID;
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

    DATA_SIZE = 10 * 1024 * 1024;
    INSTRUCTIONS_SIZE = 10 * 1024 * 1024;
    SYMBOL_TABLE_SIZE = 10 * 1024 * 1024;

    help = 0;
    print_instructions = 0;
    print_exit_code = 1;
    print_cycles = 1;
    print_symbols = 0;
    output_filename = 0;

    argc--;
    argv++;
    while (argc > 0 && *argv[0] == '-') {
             if (argc > 0 && !memcmp(argv[0], "-h",  3)) { help = 0;               argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-d",  2)) { debug = 1;              argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-i",  2)) { print_instructions = 1; argc--; argv++; }
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
        printf("Usage: wc4 [-d -i -s -c -ne -nc] INPUT-FILE\n\n");
        printf("Flags\n");
        printf("-d      Debug output\n");
        printf("-i      Output instructions during execution\n");
        printf("-s      Output symbol table\n");
        printf("-o      Output code without executing it. Use - for stdout.\n");
        printf("-ne     Don't print exit code\n");
        printf("-nc     Don't print cycles\n");
        printf("-h      Help\n");
        exit(1);
    }

    input_filename = argv[0];

    input = malloc(10 * 1024 * 1024);
    instructions = malloc(INSTRUCTIONS_SIZE);
    memset(instructions, 0, INSTRUCTIONS_SIZE);
    symbol_table = malloc(SYMBOL_TABLE_SIZE);
    memset(symbol_table, 0, SYMBOL_TABLE_SIZE);
    next_symbol = symbol_table;

    fwd_function_backpatches = malloc(sizeof(struct fwd_function_backpatch) * MAX_FWD_FUNCTION_BACKPATCHES);
    memset(fwd_function_backpatches, 0, sizeof(struct fwd_function_backpatch) * MAX_FWD_FUNCTION_BACKPATCHES);

    data = malloc(DATA_SIZE);
    data_start = data;
    global_variable_count = 0;

    all_structs = malloc(sizeof(struct str_desc *) * MAX_STRUCTS);
    all_structs_count = 0;

    add_builtin("exit",    INSTR_EXIT);
    add_builtin("open",    INSTR_OPEN);
    add_builtin("read",    INSTR_READ);
    add_builtin("write",   INSTR_WRIT);
    add_builtin("close",   INSTR_CLOS);
    add_builtin("printf",  INSTR_PRTF);
    add_builtin("dprintf", INSTR_DPRT);
    add_builtin("malloc",  INSTR_MALC);
    add_builtin("free",    INSTR_FREE);
    add_builtin("memset",  INSTR_MSET);
    add_builtin("memcmp",  INSTR_MCMP);
    add_builtin("strcmp",  INSTR_SCMP);

    iptr = instructions;
    f  = open(input_filename, 0, 0); // O_RDONLY = 0
    if (f < 0) { printf("Unable to open input file\n"); exit(1); }
    input_size = read(f, input, 10 * 1024 * 1024);
    input[input_size] = 0;
    if (input_size < 0) { printf("Unable to read input file\n"); exit(1); }
    close(f);

    cur_line = 1;
    next();
    parse();

    if (print_symbols) do_print_symbols();

    if (output_filename) {
        output_code(output_filename);
        exit(0);
    }

    run(argc, argv, print_instructions);
    exit(0);
}
