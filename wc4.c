#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char *input;
int input_size;
int ip;
long *instructions;
char *data;
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
long *cur_function_symbol;
int is_lvalue;
int seen_return;
long *cur_while_start;

long *symbol_table;
long *next_symbol;
long *cur_symbol;
long cur_type;

int SYMBOL_TYPE;
int SYMBOL_IDENTIFIER;
int SYMBOL_SCOPE;
int SYMBOL_VALUE;
int SYMBOL_SIZE;
int SYMBOL_STACK_INDEX;
int SYMBOL_FUNCTION_PARAM_COUNT;
int SYMBOL_BUILTIN;

long DATA_SIZE;
long INSTRUCTIONS_SIZE;
long SYMBOL_TABLE_SIZE;

// In order of precedence
enum {
    TOK_EOF=1,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING_LITERAL,
    TOK_INT,
    TOK_IF,
    TOK_ELSE,
    TOK_CHAR,
    TOK_VOID,
    TOK_WHILE,          // 10
    TOK_CONTINUE,
    TOK_RETURN,
    TOK_ENUM,
    TOK_SIZEOF,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_RBRACKET,
    TOK_LBRACKET,
    TOK_RCURLY,
    TOK_LCURLY,         // 20
    TOK_SEMI,
    TOK_COMMA,
    TOK_EQ,
    TOK_PLUS_EQ,
    TOK_MINUS_EQ,
    TOK_TERNARY,
    TOK_COLON,
    TOK_OR,
    TOK_AND,
    TOK_DBL_EQ,         // 30
    TOK_NOT_EQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_MOD,            // 40
    TOK_LOGICAL_NOT,
    TOK_ADDRESS_OF,
    TOK_INC,
    TOK_DEC,
};

enum { TYPE_VOID=1, TYPE_ENUM=2, TYPE_INT, TYPE_CHAR };
enum { TYPE_PTR=2 };

enum {
    INSTR_LINE=1,
    INSTR_LEA,
    INSTR_IMM,
    INSTR_JMP,
    INSTR_JSR,
    INSTR_BZ,
    INSTR_BNZ,
    INSTR_ENT,
    INSTR_ADJ,
    INSTR_LEV,
    INSTR_LI,
    INSTR_LC,
    INSTR_SI,
    INSTR_SC,
    INSTR_OR,
    INSTR_AND,
    INSTR_EQ,
    INSTR_NE,
    INSTR_LT,
    INSTR_GT,
    INSTR_LE,
    INSTR_GE,
    INSTR_ADD,
    INSTR_SUB,
    INSTR_MUL,
    INSTR_DIV,
    INSTR_MOD,
    INSTR_PSH,
    INSTR_OPEN,
    INSTR_READ,
    INSTR_CLOS,
    INSTR_PRTF,
    INSTR_MALC,
    INSTR_FREE,
    INSTR_MSET,
    INSTR_MCMP,
    INSTR_SCMP,
    INSTR_EXIT,
};

void next() {
    char *i;
    char *id;
    int idp;
    int value;
    char *sl;
    int slp;

    while (ip < input_size) {
        i = input;

        if (input_size - ip >= 2 && (i[ip] == '/' && i[ip + 1] == '/')) {
            ip += 2;
            while (i[ip++] != '\n');
            cur_line++;
            continue;
        }

        else if (input_size - ip >= 2 && !memcmp(i+ip, "if",       2)  ) { ip += 2; cur_token = TOK_IF;                         }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "else",     4)  ) { ip += 4; cur_token = TOK_ELSE;                       }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "int",      3)  ) { ip += 3; cur_token = TOK_INT;                        }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "long",     4)  ) { ip += 4; cur_token = TOK_INT;                        }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "void",     4)  ) { ip += 4; cur_token = TOK_VOID;                       }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "char",     4)  ) { ip += 4; cur_token = TOK_CHAR;                       }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "while",    5)  ) { ip += 5; cur_token = TOK_WHILE;                      }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "continue", 8)  ) { ip += 8; cur_token = TOK_CONTINUE;                   }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "return",   6)  ) { ip += 6; cur_token = TOK_RETURN;                     }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "enum",     4)  ) { ip += 4; cur_token = TOK_ENUM;                       }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "sizeof",   6)  ) { ip += 6; cur_token = TOK_SIZEOF;                     }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "&&",       2)  ) { ip += 2; cur_token = TOK_AND;                        }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "||",       2)  ) { ip += 2; cur_token = TOK_OR;                         }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "==",       2)  ) { ip += 2; cur_token = TOK_DBL_EQ;                     }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "!=",       2)  ) { ip += 2; cur_token = TOK_NOT_EQ;                     }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "<=",       2)  ) { ip += 2; cur_token = TOK_LE;                         }
        else if (input_size - ip >= 2 && !memcmp(i+ip, ">=",       2)  ) { ip += 2; cur_token = TOK_GE;                         }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "++",       2)  ) { ip += 2; cur_token = TOK_INC;                        }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "--",       2)  ) { ip += 2; cur_token = TOK_DEC;                        }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "+=",       2)  ) { ip += 2; cur_token = TOK_PLUS_EQ;                    }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "-=",       2)  ) { ip += 2; cur_token = TOK_MINUS_EQ;                   }
        else if (input_size - ip >= 1 && i[ip] == '('                  ) { ip += 1; cur_token = TOK_LPAREN;                     }
        else if (input_size - ip >= 1 && i[ip] == ')'                  ) { ip += 1; cur_token = TOK_RPAREN;                     }
        else if (input_size - ip >= 1 && i[ip] == '['                  ) { ip += 1; cur_token = TOK_LBRACKET;                   }
        else if (input_size - ip >= 1 && i[ip] == ']'                  ) { ip += 1; cur_token = TOK_RBRACKET;                   }
        else if (input_size - ip >= 1 && i[ip] == '{'                  ) { ip += 1; cur_token = TOK_LCURLY;                     }
        else if (input_size - ip >= 1 && i[ip] == '}'                  ) { ip += 1; cur_token = TOK_RCURLY;                     }
        else if (input_size - ip >= 1 && i[ip] == '+'                  ) { ip += 1; cur_token = TOK_PLUS;                       }
        else if (input_size - ip >= 1 && i[ip] == '-'                  ) { ip += 1; cur_token = TOK_MINUS;                      }
        else if (input_size - ip >= 1 && i[ip] == '*'                  ) { ip += 1; cur_token = TOK_MULTIPLY;                   }
        else if (input_size - ip >= 1 && i[ip] == '/'                  ) { ip += 1; cur_token = TOK_DIVIDE;                     }
        else if (input_size - ip >= 1 && i[ip] == '%'                  ) { ip += 1; cur_token = TOK_MOD;                        }
        else if (input_size - ip >= 1 && i[ip] == ','                  ) { ip += 1; cur_token = TOK_COMMA;                      }
        else if (input_size - ip >= 1 && i[ip] == ';'                  ) { ip += 1; cur_token = TOK_SEMI;                       }
        else if (input_size - ip >= 1 && i[ip] == '='                  ) { ip += 1; cur_token = TOK_EQ;                         }
        else if (input_size - ip >= 1 && i[ip] == '<'                  ) { ip += 1; cur_token = TOK_LT;                         }
        else if (input_size - ip >= 1 && i[ip] == '>'                  ) { ip += 1; cur_token = TOK_GT;                         }
        else if (input_size - ip >= 1 && i[ip] == '!'                  ) { ip += 1; cur_token = TOK_LOGICAL_NOT;                }
        else if (input_size - ip >= 1 && i[ip] == '&'                  ) { ip += 1; cur_token = TOK_ADDRESS_OF;                 }
        else if (input_size - ip >= 1 && i[ip] == '?'                  ) { ip += 1; cur_token = TOK_TERNARY;                    }
        else if (input_size - ip >= 1 && i[ip] == ':'                  ) { ip += 1; cur_token = TOK_COLON;                      }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\t'", 4)     ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\t'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\n'", 4)     ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\n'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\''", 4)     ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\''; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\"'", 4)    ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\"'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\\'", 4)    ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\\'; }

        else if (input_size - ip >= 3 && i[ip] == '\'' && i[ip+2] == '\'') { cur_integer = i[ip+1]; ip += 3; cur_token = TOK_NUMBER; }

        else if (i[ip] == ' ' || i[ip] == '\t') { ip++; continue; }
        else if (i[ip] == '\n') { ip++; cur_line++; continue; }

        else if ((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z')) {
            cur_token = TOK_IDENTIFIER;
            id = malloc(1024);
            idp = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) { id[idp] = i[ip]; idp++; ip++; }
            id[idp] = 0;
            cur_identifier = id;
        }

        else if ((i[ip] >= '0' && i[ip] <= '9')) {
            cur_token = TOK_NUMBER;
            value = 0;
            while ((i[ip] >= '0' && i[ip] <= '9') && ip < input_size) { value = value * 10 + (i[ip] - '0'); ip++; }
            cur_integer = value;
        }

        else if (input_size - ip >= 1 && i[ip] == '#') {
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
            printf("%d: Unknown token on line\n", cur_line);
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

long *lookup_symbol(char *name, int scope) {
    long *s;
    s = symbol_table;

    while (s[0]) {
        if (s[SYMBOL_SCOPE] == scope && !strcmp((char *) s[SYMBOL_IDENTIFIER], name)) return s;
        s += SYMBOL_SIZE;
    }

    if (scope != 0) return lookup_symbol(name, 0);

    printf("%d: Unknown symbol \"%s\"\n", cur_line, name);
    exit(1);
}

long lookup_function(char *name) {
    long *symbol;
    symbol = lookup_symbol(name, 0);
    return symbol[SYMBOL_VALUE];
}

void want_rvalue() {
    if (is_lvalue) {
        *iptr++ = cur_type == TYPE_CHAR ? INSTR_LC : INSTR_LI;
        is_lvalue = 0;
    }
}

int parse_type() {
    int type;

    if (cur_token == TOK_VOID) type = TYPE_VOID;
    else if (cur_token == TOK_INT) type = TYPE_INT;
    else if (cur_token == TOK_CHAR) type = TYPE_CHAR;

    next();
    while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }
    return type;
}

int get_type_inc_dec_size(int type) {
    // How much will the ++ operator increment a type?
    return type <= TYPE_CHAR || type == TYPE_CHAR + TYPE_PTR ? 1 : sizeof(long);
}

void expression(int level) {
    int org_token;
    int org_type;
    int first_arg_is_pointer;
    int factor;
    long *temp_iptr;
    long *false_jmp;
    long *true_done_jmp;
    long *symbol;
    int type;
    int scope;
    long *address;
    long param_count;
    long stack_index;
    int builtin;

    if (cur_token == TOK_LOGICAL_NOT) {
        next();
        *iptr++ = INSTR_IMM;
        *iptr++ = 0;
        *iptr++ = INSTR_PSH;
        expression(1024); // Fake highest precedence, bind nothing
        want_rvalue();
        *iptr++ = INSTR_EQ;
        is_lvalue = 0;
        cur_type = TYPE_INT;
    }
    else if (cur_token == TOK_ADDRESS_OF) {
        next();
        expression(1024); // Fake highest precedence, bind nothing
        cur_type = cur_type + TYPE_PTR;
        is_lvalue = 0;
    }
    else if (cur_token == TOK_INC || cur_token == TOK_DEC) {
        // Prefix increment & decrement
        org_token = cur_token;
        next();
        expression(1024); // Fake highest precedence, bind nothing
        org_type = cur_type;
        if (!is_lvalue) {
            printf("%d: Cannot prefix increment/decrement an rvalue on line\n", cur_line);
            exit(1);
        }
        *iptr++ = INSTR_PSH; // Push address
        want_rvalue();
        *iptr++ = INSTR_PSH;
        *iptr++ = INSTR_IMM;
        *iptr++ = get_type_inc_dec_size(cur_type);
        *iptr++ = org_token == TOK_INC ? INSTR_ADD : INSTR_SUB;
        *iptr++ = INSTR_SI;
        is_lvalue = 0;
        cur_type = org_type;
    }
    else if (cur_token == TOK_MULTIPLY) {
        next();
        expression(TOK_INC);
        if (cur_type <= TYPE_CHAR) {
            printf("%d: Cannot derefence a non-pointer\n", cur_line);
            exit(1);
        }
        want_rvalue(); // This does the dereferencing
        is_lvalue = 1;
        cur_type = cur_type - TYPE_PTR;
    }
    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            *iptr++ = INSTR_IMM;
            *iptr++ = -cur_integer;
            next();
            is_lvalue = 0;
            cur_type = TYPE_INT;
        }
        else {
            *iptr++ = INSTR_IMM;
            *iptr++ = -1;
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            want_rvalue();
            *iptr++ = INSTR_MUL;
            is_lvalue = 0;
        }
    }
    else if (cur_token == TOK_LPAREN) {
        next();
        if (cur_token == TOK_VOID || cur_token == TOK_INT || cur_token == TOK_CHAR) {
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
        cur_type = TYPE_INT;
        next();
        is_lvalue = 0;
    }
    else if (cur_token == TOK_STRING_LITERAL) {
        *iptr++ = INSTR_IMM;
        *iptr++ = (long) data;
        while (*data++ = *cur_string_literal++);
        cur_type = TYPE_CHAR + TYPE_PTR;
        is_lvalue = 0;
        next();
    }
    else if (cur_token == TOK_IDENTIFIER) {
        symbol = lookup_symbol(cur_identifier, cur_scope);
        next();
        type = symbol[SYMBOL_TYPE];
        scope = symbol[SYMBOL_SCOPE];
        if (type == TYPE_ENUM) {
            *iptr++ = INSTR_IMM;
            *iptr++ = symbol[SYMBOL_VALUE];
            is_lvalue = 0;
            cur_type = symbol[SYMBOL_TYPE];
        }
        else if (cur_token == TOK_LPAREN) {
            // Function call
            next();
            param_count = 0;
            while (cur_token != TOK_RPAREN) {
                expression(TOK_PLUS);
                want_rvalue();
                *iptr++ = INSTR_PSH;
                if (cur_token == TOK_COMMA) next();
                param_count++;
            }
            consume(TOK_RPAREN);
            builtin = symbol[SYMBOL_BUILTIN];
            if (builtin) {
                *iptr++ = builtin;
                if (!strcmp("printf", (char *) symbol[SYMBOL_IDENTIFIER]))  *iptr++ = param_count;
                is_lvalue = 0;
            }
            else {
                *iptr++ = INSTR_JSR;
                *iptr++ = symbol[SYMBOL_VALUE];
                *iptr++ = INSTR_ADJ;
                *iptr++ = param_count;
                is_lvalue = 0;
            }
            cur_type = symbol[SYMBOL_TYPE];
        }
        else if (scope == 0) {
            // Global symbol
            address = (long *) symbol[SYMBOL_VALUE];
            *iptr++ = INSTR_IMM;
            *iptr++ = (long) address;
            cur_type = symbol[SYMBOL_TYPE];
            is_lvalue = 1;
        }
        else {
            // Local symbol
            param_count = cur_function_symbol[SYMBOL_FUNCTION_PARAM_COUNT];
            *iptr++ = INSTR_LEA;
            if (symbol[SYMBOL_STACK_INDEX] >= 0) {
                stack_index = param_count - symbol[SYMBOL_STACK_INDEX] - 1;
                *iptr++ = stack_index + 2; // Step over pushed PC and BP
            }
            else {
                *iptr++ = symbol[SYMBOL_STACK_INDEX];
            }
            cur_type = symbol[SYMBOL_TYPE];
            is_lvalue = 1;
        }
    }
    else if (cur_token == TOK_SIZEOF) {
        next();
        consume(TOK_LPAREN);
        *iptr++ = INSTR_IMM;
        *iptr++ = parse_type() == TYPE_CHAR ? 1 : sizeof(long);
        consume(TOK_RPAREN);
        is_lvalue = 0;
        cur_type = TYPE_INT;
    }
    else {
        printf("%d: Unexpected token %d in expression\n", cur_line, cur_token);
        exit(1);
    }

    if (cur_token == TOK_LBRACKET) {
        next();
        want_rvalue();

        if (cur_type <= TYPE_CHAR) {
            printf("%d: Cannot do [] on a non-pointer for type %ld\n", cur_line, cur_type);
            exit(1);
        }

        org_type = cur_type;
        *iptr++ = INSTR_PSH;
        expression(TOK_COMMA);
        want_rvalue();
        *iptr++ = INSTR_PSH;
        *iptr++ = INSTR_IMM;
        *iptr++ = get_type_inc_dec_size(org_type);
        *iptr++ = INSTR_MUL;
        *iptr++ = INSTR_ADD;
        consume(TOK_RBRACKET);
        is_lvalue = 1;
        cur_type = org_type - TYPE_PTR;
    }

    while (cur_token >= level) {
        // In order or precedence

        if (cur_token == TOK_INC || cur_token == TOK_DEC) {
            // Postfix increment and decrement
            *iptr++ = INSTR_PSH;
            want_rvalue();
            *iptr++ = INSTR_PSH;
            *iptr++ = INSTR_IMM;
            *iptr++ = get_type_inc_dec_size(cur_type);
            *iptr++ = cur_token == TOK_INC ? INSTR_ADD : INSTR_SUB;
            *iptr++ = INSTR_SI;

            // Dirty!
            *iptr++ = INSTR_PSH;
            *iptr++ = INSTR_IMM;
            *iptr++ = get_type_inc_dec_size(cur_type);
            *iptr++ = cur_token == TOK_INC ? INSTR_SUB : INSTR_ADD;

            is_lvalue = 0;
            next();
        }
        else if (cur_token == TOK_MULTIPLY) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_INC);
            want_rvalue();
            *iptr++ = INSTR_MUL;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_DIVIDE) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_INC);
            want_rvalue();
            *iptr++ = INSTR_DIV;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_MOD) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_INC);
            want_rvalue();
            *iptr++ = INSTR_MOD;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_PLUS || cur_token == TOK_MINUS) {
            org_token = cur_token;
            org_type = cur_type;
            first_arg_is_pointer = cur_type > TYPE_CHAR;
            factor = first_arg_is_pointer
                ? get_type_inc_dec_size(cur_type)
                : 1;
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            want_rvalue();
            *iptr++ = INSTR_PSH;
            *iptr++ = INSTR_IMM;
            *iptr++ = factor;
            *iptr++ = INSTR_MUL;
            *iptr++ = org_token == TOK_PLUS ? INSTR_ADD : INSTR_SUB;
            cur_type = org_type;
        }
        else if (cur_token == TOK_LT) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            want_rvalue();
            *iptr++ = INSTR_LT;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_GT) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            want_rvalue();
            *iptr++ = INSTR_GT;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_LE) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            want_rvalue();
            *iptr++ = INSTR_LE;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_GE) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            want_rvalue();
            *iptr++ = INSTR_GE;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_DBL_EQ) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_LT);
            want_rvalue();
            *iptr++ = INSTR_EQ;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_NOT_EQ) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_LT);
            want_rvalue();
            *iptr++ = INSTR_NE;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_AND) {
            temp_iptr = iptr;
            *iptr++ = INSTR_BZ;
            *iptr++ = 0;
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_DBL_EQ);
            want_rvalue();
            *iptr++ = INSTR_AND;
            *(temp_iptr + 1) = (long) iptr;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_OR) {
            temp_iptr = iptr;
            *iptr++ = INSTR_BNZ;
            *iptr++ = 0;
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_AND);
            want_rvalue();
            *iptr++ = INSTR_OR;
            *(temp_iptr + 1) = (long) iptr;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_TERNARY) {
            next();
            want_rvalue();
            false_jmp = iptr;
            *iptr++ = INSTR_BZ;
            *iptr++ = 0;
            expression(TOK_OR);
            want_rvalue();
            true_done_jmp = iptr;
            *iptr++ = INSTR_JMP;
            *iptr++ = 0;
            consume(TOK_COLON);
            *(false_jmp + 1) = (long) iptr;
            expression(TOK_OR);
            want_rvalue();
            *(true_done_jmp + 1) = (long) iptr;
        }
        else if (cur_token == TOK_EQ) {
            next();
            if (!is_lvalue) {
                printf("%d: Cannot assign to an rvalue\n", cur_line);
                exit(1);
            }
            org_type = type;
            *iptr++ = INSTR_PSH;
            expression(TOK_EQ);
            want_rvalue();
            *iptr++ = cur_type == TYPE_CHAR ? INSTR_SC : INSTR_SI;
            type = org_type;
            is_lvalue = 1;
        }
        else if (cur_token == TOK_PLUS_EQ || cur_token == TOK_MINUS_EQ) {
            org_token = cur_token;
            next();
            if (!is_lvalue) {
                printf("%d: Cannot assign to an rvalue\n", cur_line);
                exit(1);
            }
            org_type = cur_type;
            *iptr++ = INSTR_PSH;
            want_rvalue();
            *iptr++ = INSTR_PSH;
            *iptr++ = INSTR_IMM;
            *iptr++ = get_type_inc_dec_size(org_type);
            *iptr++ = INSTR_PSH;
            expression(TOK_EQ);
            want_rvalue();
            *iptr++ = INSTR_MUL;
            *iptr++ = org_token == TOK_PLUS_EQ ? INSTR_ADD : INSTR_SUB;
            cur_type = org_type;
            *iptr++ = cur_type == TYPE_CHAR ? INSTR_SC : INSTR_SI;
            is_lvalue = 0;
        }
        else
            return; // Bail once we hit something unknown

        is_lvalue = 0;
    }
}

void statement() {
    long *body_start;
    long *false_jmp;
    long *true_done_jmp;
    long *old_cur_while_start;

    if (cur_token == TOK_INT || cur_token == TOK_CHAR) {
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
        next();
        consume(TOK_LPAREN);
        // Preserve so that we can have nested whiles
        old_cur_while_start = cur_while_start;
        cur_while_start = iptr;
        expression(TOK_COMMA);
        want_rvalue();
        consume(TOK_RPAREN);
        *iptr++ = INSTR_BZ;
        *iptr++ = 0;
        body_start = iptr;
        statement();
        *iptr++ = INSTR_JMP;
        *iptr++ = (long) cur_while_start;
        *(body_start - 1) = (long) iptr;
        cur_while_start = old_cur_while_start;
    }
    else if (cur_token == TOK_CONTINUE) {
        next();
        *iptr++ = INSTR_JMP;
        *iptr++ = (long) cur_while_start;
        consume(TOK_SEMI);
    }
    else if (cur_token == TOK_IF) {
        next();
        consume(TOK_LPAREN);
        expression(TOK_COMMA);
        want_rvalue();
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
        want_rvalue();
        *iptr++ = INSTR_LEV;
        consume(TOK_SEMI);
        seen_return = 1;
    }
    else {
        expression(TOK_COMMA);
        consume(TOK_SEMI);
    }
}

void function_body(char *func_name) {
    int is_main;
    int local_symbol_count;
    int base_type, type;

    seen_return = 0;
    is_main = !strcmp(func_name, "main");
    seen_return = 0;
    local_symbol_count = 0;

    consume(TOK_LCURLY);

    // Parse symbols first
    while (cur_token == TOK_INT || cur_token == TOK_CHAR) {
        base_type = cur_token == TOK_INT ? TYPE_INT : TYPE_CHAR;
        next();

        while (cur_token != TOK_SEMI) {
            type = base_type;
            while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

            if (cur_token == TOK_EQ) {
                printf("%d: Declarations with assignments aren't implemented\n", cur_line);
                exit(1);
            }


            expect(TOK_IDENTIFIER);
            cur_symbol = next_symbol;
            next_symbol[SYMBOL_TYPE] = type;
            next_symbol[SYMBOL_IDENTIFIER] = (long) cur_identifier;
            next_symbol[SYMBOL_SCOPE] = cur_scope;
            next_symbol[SYMBOL_STACK_INDEX] = -1 - local_symbol_count++;
            next_symbol += SYMBOL_SIZE;
            next();
            if (cur_token == TOK_COMMA) next();
        }
        expect(TOK_SEMI);
        while (cur_token == TOK_SEMI) next();
    }

    *iptr++ = INSTR_ENT;
    *iptr++ = local_symbol_count * sizeof(long); // allocate stack space for locals

    while (cur_token != TOK_RCURLY) statement();

    if (is_main && !seen_return) {
        *iptr++ = INSTR_IMM;
        *iptr++ = 0;
        cur_type = TYPE_INT;
        is_lvalue = 0;
    }

    *iptr++ = INSTR_LEV;
    consume(TOK_RCURLY);
}

void parse() {
    int type;
    long number;
    int param_count;

    cur_scope = 0;

    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token == TOK_VOID || cur_token == TOK_INT || cur_token == TOK_CHAR) {
            type = parse_type();
            expect(TOK_IDENTIFIER);
            cur_symbol = next_symbol;
            next_symbol[SYMBOL_TYPE] = type;
            next_symbol[SYMBOL_IDENTIFIER] = (long) cur_identifier;
            next_symbol[SYMBOL_SCOPE] = 0;
            next_symbol += SYMBOL_SIZE;
            next();

            if (cur_token == TOK_LPAREN) {
                cur_scope++;
                next();
                // Function definition
                cur_symbol[SYMBOL_VALUE] = (long) iptr;
                param_count = 0;
                while (cur_token != TOK_RPAREN) {
                    if (cur_token == TOK_INT || cur_token == TOK_CHAR) {
                        type = cur_token == TOK_INT ? TYPE_INT : TYPE_CHAR;
                        next();
                        while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }
                    }
                    else {
                        printf("%d: Unknown type token in function def %d\n", cur_line, cur_token);
                        exit(1);
                    }

                    consume(TOK_IDENTIFIER);
                    next_symbol[SYMBOL_TYPE] = type;
                    next_symbol[SYMBOL_IDENTIFIER] = (long) cur_identifier;
                    next_symbol[SYMBOL_SCOPE] = cur_scope;
                    next_symbol[SYMBOL_STACK_INDEX] = param_count++;
                    next_symbol += SYMBOL_SIZE;
                    if (cur_token == TOK_COMMA) next();
                }
                cur_symbol[SYMBOL_FUNCTION_PARAM_COUNT] = param_count;
                consume(TOK_RPAREN);
                cur_function_symbol = cur_symbol;
                function_body((char *) cur_symbol[SYMBOL_IDENTIFIER]);
            }
            else {
                // Global symbol
                cur_symbol[SYMBOL_VALUE] = (long) data;
                data += sizeof(long);
            }
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

                next_symbol[SYMBOL_TYPE] = TYPE_ENUM;
                next_symbol[SYMBOL_IDENTIFIER] = (long) cur_identifier;
                next_symbol[SYMBOL_SCOPE] = 0;
                next_symbol[SYMBOL_VALUE] = number++;
                next_symbol += SYMBOL_SIZE;

                if (cur_token == TOK_COMMA) next();
            }
            consume(TOK_RCURLY);
            consume(TOK_SEMI);
        }

        else {
            printf("%d: Expected global declaration or function\n", cur_line);
            exit(1);
        }
    }
}

void print_instruction(long *pc, int relative, int print_pc) {
    int instr;
    long operand;
    instr = *pc;

    if (print_pc) printf("%-15ld ", (long) pc - (long) instructions);
    printf("%.5s", &"LINE LEA  IMM  JMP  JSR  BZ   BNZ  ENT  ADJ  LEV  LI   LC   SI   SC   OR   AND  EQ   NE   LT   GT   LE   GE   ADD  SUB  MUL  DIV  MOD  PSH  OPEN READ CLOS PRTF MALC FREE MSET MCMP SCMP EXIT "[instr * 5 - 5]);
    if (instr <= INSTR_ADJ) {
        operand = *(pc + 1);
        if (relative) {
            if ((instr == INSTR_IMM) && (operand < 0 || operand > 1024 * 1024))
                printf(" string literal or symbol");
            else if ((instr == INSTR_JSR) || (instr == INSTR_JMP) || (instr == INSTR_BZ) || (instr == INSTR_BNZ))
                printf(" %ld", operand - (long) instructions);
            else
                printf(" %ld", operand);
        }
    }

    printf("\n");
}

void do_print_code() {
    long *pc;
    int instr;

    pc = instructions;
    while (*pc) {
        instr = *pc;
        print_instruction(pc, 1, 1);
        if (instr <= INSTR_ADJ) pc += 2;
        else if (instr == INSTR_PRTF) pc += 2;
        else pc++;
    }
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
            printf("%-5ld> ", cycle);
            printf("pc = %-15ld ", (long) pc - 8);
            printf("a = %-15ld ", a);
            printf("sp = %-15ld ", (long) sp);
            print_instruction(pc, 0, 0);
        }

        instr = *pc++;

             if (instr == INSTR_LINE) pc++;                                                 // No-op, print line number
        else if (instr == INSTR_LEA) a = (long) (bp + *pc++);                               // load local address
        else if (instr == INSTR_IMM) a = *pc++;                                             // load global address or immediate
        else if (instr == INSTR_JMP) pc = (long *) *pc;                                     // jump
        else if (instr == INSTR_JSR) { *--sp = (long) (pc + 1); pc = (long *)*pc; }         // jump to subroutine
        else if (instr == INSTR_BZ)  pc = a ? pc + 1 : (long *) *pc;                        // branch if zero
        else if (instr == INSTR_BNZ) pc = a ? (long *) *pc : pc + 1;                        // branch if not zero
        else if (instr == INSTR_ENT) { *--sp = (long) bp; bp = sp; sp = sp - *pc++; }       // enter subroutine
        else if (instr == INSTR_ADJ) sp = sp + *pc++;                                       // stack adjust
        else if (instr == INSTR_LEV) { sp = bp; bp = (long *) *sp++; pc = (long *) *sp++; } // leave subroutine
        else if (instr == INSTR_LI)  a = *(long *)a;                                        // load int
        else if (instr == INSTR_LC)  a = *(char *)a;                                        // load char
        else if (instr == INSTR_SI) *(long *) *sp++ = a;                                    // store int
        else if (instr == INSTR_SC) a = *(char *) *sp++ = a;                                // store char
        else if (instr == INSTR_PSH) *--sp = a;
        else if (instr == INSTR_OR ) a = *sp++ || a;
        else if (instr == INSTR_AND) a = *sp++ && a;
        else if (instr == INSTR_EQ ) a = *sp++ == a;
        else if (instr == INSTR_NE ) a = *sp++ != a;
        else if (instr == INSTR_LT ) a = *sp++ < a;
        else if (instr == INSTR_GT ) a = *sp++ > a;
        else if (instr == INSTR_LE ) a = *sp++ <= a;
        else if (instr == INSTR_GE ) a = *sp++ >= a;
        else if (instr == INSTR_ADD) a = *sp++ + a;
        else if (instr == INSTR_SUB) a = *sp++ - a;
        else if (instr == INSTR_MUL) a = *sp++ * a;
        else if (instr == INSTR_DIV) a = *sp++ / a;
        else if (instr == INSTR_MOD) a = *sp++ % a;
        else if (instr == INSTR_OPEN) { a = open((char *) sp[1], *sp); sp += 2; }
        else if (instr == INSTR_READ) { a = read(sp[2], (char *) sp[1], *sp); sp += 3; }
        else if (instr == INSTR_CLOS) a = close(*sp++);
        else if (instr == INSTR_PRTF) { t = sp + *pc++; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6], t[-7], t[-8], t[-9], t[-10]); sp += *(pc - 1); }
        else if (instr == INSTR_MALC) a = (long) malloc(*sp++);
        else if (instr == INSTR_FREE) free((void *) *sp++);
        else if (instr == INSTR_MSET) { a = (long) memset((char *) sp[2], sp[1], *sp); sp += 3; }
        else if (instr == INSTR_MCMP) { a = (long) memcmp((char *) sp[2], (char *) sp[1], *sp); sp += 3; }
        else if (instr == INSTR_SCMP) { a = (long) strcmp((char *) sp[1], (char *) *sp); sp += 2; }
        else if (instr == INSTR_EXIT) {
            if (print_cycles && print_exit_code) printf("exit %ld in %ld cycles,\n", *sp, cycle);
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
    long *symbol;
    symbol = next_symbol;
    symbol[SYMBOL_TYPE] = TYPE_VOID;
    symbol[SYMBOL_IDENTIFIER] = (long) identifier;
    symbol[SYMBOL_BUILTIN] = instruction;
    next_symbol += SYMBOL_SIZE;
}

void do_print_symbols() {
    long *s, type, scope, value, stack_index;
    char *identifier;

    printf("Symbols:\n");
    s = symbol_table;
    while (s[0]) {
        type = s[SYMBOL_TYPE];
        identifier = (char *) s[SYMBOL_IDENTIFIER];
        scope = s[SYMBOL_SCOPE];
        value = s[SYMBOL_VALUE];
        stack_index = s[SYMBOL_STACK_INDEX];
        printf("%-20ld %-3ld %-3ld %-3ld %-20ld %s\n", (long) s, type, scope, stack_index, value, identifier);
        s += SYMBOL_SIZE;
    }
    printf("\n");
}

int main(int argc, char **argv) {
    char *filename;
    int f;
    int debug, print_symbols, print_code;

    DATA_SIZE = 10 * 1024 * 1024;
    INSTRUCTIONS_SIZE = 10 * 1024 * 1024;
    SYMBOL_TABLE_SIZE = 10 * 1024 * 1024;

    print_instructions = 0;
    print_code = 0;
    print_exit_code = 1;
    print_cycles = 1;
    print_symbols = 0;

    argc--;
    argv++;
    while (argc > 0 && *argv[0] == '-') {
             if (argc > 0 && !memcmp(argv[0], "-d",  2)) { debug = 1;              argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-i",  2)) { print_instructions = 1; argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-s",  2)) { print_symbols = 1;      argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-c",  2)) { print_code = 1;         argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-ne", 3)) { print_exit_code = 0;    argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-nc", 3)) { print_cycles = 0;       argc--; argv++; }
        else { printf("Unknown parameter %s\n", argv[0]); exit(1); }
    }

    if (argc < 1) {
        printf("Usage: wc4 INPUT-FILE\n");
        exit(1);
    }

    filename = argv[0];

    input = malloc(10 * 1024 * 1024);
    instructions = malloc(INSTRUCTIONS_SIZE);
    memset(instructions, 0, INSTRUCTIONS_SIZE);
    symbol_table = malloc(SYMBOL_TABLE_SIZE);
    memset(symbol_table, 0, SYMBOL_TABLE_SIZE);
    next_symbol = symbol_table;
    data = malloc(DATA_SIZE);

    SYMBOL_TYPE                 = 0;
    SYMBOL_IDENTIFIER           = 1;
    SYMBOL_SCOPE                = 2;
    SYMBOL_VALUE                = 3;
    SYMBOL_STACK_INDEX          = 4;
    SYMBOL_FUNCTION_PARAM_COUNT = 5;
    SYMBOL_BUILTIN              = 6;
    SYMBOL_SIZE                 = 7; // Number of longs

    add_builtin("exit",   INSTR_EXIT);
    add_builtin("open",   INSTR_OPEN);
    add_builtin("read",   INSTR_READ);
    add_builtin("close",  INSTR_CLOS);
    add_builtin("printf", INSTR_PRTF);
    add_builtin("malloc", INSTR_MALC);
    add_builtin("free",   INSTR_FREE);
    add_builtin("memset", INSTR_MSET);
    add_builtin("memcmp", INSTR_MCMP);
    add_builtin("strcmp", INSTR_SCMP);

    iptr = instructions;
    f  = open(filename, 0); // O_RDONLY = 0
    if (f < 0) { printf("Unable to open input file\n"); exit(1); }
    input_size = read(f, input, 10 * 1024 * 1024);
    input[input_size] = 0;
    if (input_size < 0) { printf("Unable to read input file\n"); exit(1); }
    close(f);

    cur_line = 1;
    next();
    parse();

    if (print_symbols) do_print_symbols();

    if (print_code) {
        do_print_code();
        exit(0);
    }

    run(argc, argv, print_instructions);
    exit(0);
}
