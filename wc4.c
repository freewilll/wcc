#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char *input;
int input_size;
int ip;
long int *instructions;
char *data;
long int *iptr;

int cur_token;
int cur_scope;
char *cur_identifier;
int cur_integer;
char *cur_string_literal;
long int *cur_function_symbol;
int is_lvalue;

long int *symbol_table;
long int *next_symbol;
long int *cur_symbol;
long int cur_type;

int SYMBOL_TYPE;
int SYMBOL_IDENTIFIER;
int SYMBOL_SCOPE;
int SYMBOL_VALUE;
int SYMBOL_SIZE;
int SYMBOL_STACK_INDEX;
int SYMBOL_FUNCTION_PARAM_COUNT;
int SYMBOL_BUILTIN;

// In order of precedence
enum {
    TOK_EOF=1,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING_LITERAL,
    TOK_INT,
    TOK_IF,
    TOK_CHAR,
    TOK_VOID,
    TOK_WHILE,
    TOK_RETURN,
    TOK_ENUM,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_RBRACKET,
    TOK_LBRACKET,
    TOK_RCURLY,
    TOK_LCURLY,
    TOK_SEMI,
    TOK_COMMA,
    TOK_EQ,
    TOK_OR,
    TOK_AND,
    TOK_DBL_EQ,
    TOK_NOT_EQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_MOD,
    TOK_LOGICAL_NOT,
};

enum { TYPE_VOID=1, TYPE_ENUM=2, TYPE_INT, TYPE_CHAR };
enum { TYPE_PTR=2 };

enum {
    INSTR_LEA=1,
    INSTR_IMM,
    INSTR_JSR,
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
    INSTR_PRTF,
    INSTR_EXIT,
};

void next() {
    while (ip < input_size) {
        char *i;
        i = input;

        if (input_size - ip >= 2 && (i[ip] == '/' && i[ip + 1] == '/')) {
            ip += 2;
            while (i[ip++] != '\n');
            continue;
        }


        else if (input_size - ip >= 2 && !memcmp(i+ip, "if",     2)    ) { ip += 2; cur_token = TOK_IF;                         }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "int",    3)    ) { ip += 3; cur_token = TOK_INT;                        }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "char",   4)    ) { ip += 4; cur_token = TOK_CHAR;                       }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "while",  5)    ) { ip += 5; cur_token = TOK_WHILE;                      }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "return", 6)    ) { ip += 6; cur_token = TOK_RETURN;                     }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "enum",   4)    ) { ip += 4; cur_token = TOK_ENUM;                       }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "&&",     2)    ) { ip += 2; cur_token = TOK_AND;                        }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "||",     2)    ) { ip += 2; cur_token = TOK_OR;                         }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "==",     2)    ) { ip += 2; cur_token = TOK_DBL_EQ;                     }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "!=",     2)    ) { ip += 2; cur_token = TOK_NOT_EQ;                     }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "<=",     2)    ) { ip += 2; cur_token = TOK_LE;                         }
        else if (input_size - ip >= 2 && !memcmp(i+ip, ">=",     2)    ) { ip += 2; cur_token = TOK_GE;                         }
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
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\t'", 4)     ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\t'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\n'", 4)     ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\n'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\''", 4)     ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\''; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\"'", 4)    ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\"'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\\'", 4)    ) { ip += 4; cur_token = TOK_NUMBER; cur_integer = '\\'; }

        else if (input_size - ip >= 3 && i[ip] == '\'' && i[ip+2] == '\'') { ip += 3; cur_token = TOK_NUMBER; cur_integer = i[ip+1]; }

        else if (i[ip] == ' ' || i[ip] == '\t' || i[ip] == '\n') { ip++; continue; }

        else if ((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z')) {
            cur_token = TOK_IDENTIFIER;
            char *id = malloc(128);
            int idp = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) { id[idp] = i[ip]; idp++; ip++; }
            id[idp] = 0;
            cur_identifier = id;
        }

        else if ((i[ip] >= '0' && i[ip] <= '9')) {
            cur_token = TOK_NUMBER;
            int value = 0;
            while ((i[ip] >= '0' && i[ip] <= '9') && ip < input_size) { value = value * 10 + (i[ip] - '0'); ip++; }
            cur_integer = value;
        }

        else if (input_size - ip >= 1 && i[ip] == '#') {
            // Ignore CPP directives
            while (i[ip++] != '\n');
            continue;
        }

        else if (i[ip] == '"') {
            cur_token = TOK_STRING_LITERAL;
            char *sl = malloc(128);
            int slp = 0;
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
                        printf("Unknown \\ escape in string literal: %s\n", input + ip);
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
            printf("Unknown token: %s", input + ip);
            exit(1);
        }

        return;
    }

    cur_token = TOK_EOF;
}

void expect(int token) {
    if (cur_token != token) {
        printf("Expected token %d, got %d\n", token, cur_token);
        exit(1);
    }
}

void consume(int token) {
    expect(token);
    next();
}

long int *lookup_symbol(char *name, int scope) {
    long int *s = symbol_table;

    while (s[0]) {
        if (s[SYMBOL_SCOPE] == scope && !strcmp((char *) s[SYMBOL_IDENTIFIER], name)) return s;
        s += SYMBOL_SIZE;
    }

    if (scope != 0) return lookup_symbol(name, 0);

    printf("Unknown symbol: %s\n", name);
    exit(1);
}

long int lookup_function(char *name) {
    long int *symbol = lookup_symbol(name, 0);
    return symbol[SYMBOL_VALUE];
}

void want_rvalue() {
    if (is_lvalue) {
        *iptr++ = cur_type == TYPE_INT ? INSTR_LI : INSTR_LC;
        is_lvalue = 0;
    }
}

void expression(int level) {
    int param_count;

    if (cur_token == TOK_LOGICAL_NOT) {
        next();

        *iptr++ = INSTR_IMM;
        *iptr++ = 0;
        *iptr++ = INSTR_PSH;
        expression(1024); // Fake highest precedence, bind nothing
        want_rvalue();
        *iptr++ = INSTR_EQ;
        is_lvalue = 0;
    }
    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            *iptr++ = INSTR_IMM;
            *iptr++ = -cur_integer;
            next();
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
        expression(TOK_COMMA);
        next();
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
        *iptr++ = (long int) data;
        while (*data++ = *cur_string_literal++);
        cur_type = TYPE_CHAR + TYPE_PTR;
        is_lvalue = 0;
        next();
    }
    else if (cur_token == TOK_IDENTIFIER) {
        long int *symbol = lookup_symbol(cur_identifier, cur_scope);
        next();
        int type = symbol[SYMBOL_TYPE];
        int scope = symbol[SYMBOL_SCOPE];
        if (type == TYPE_ENUM) {
            *iptr++ = INSTR_IMM;
            *iptr++ = symbol[SYMBOL_VALUE];
            is_lvalue = 0;
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
            int builtin = symbol[SYMBOL_BUILTIN];
            if (builtin) {
                *iptr++ = builtin;
                *iptr++ = param_count;
            }
            else {
                *iptr++ = INSTR_JSR;
                *iptr++ = symbol[SYMBOL_VALUE];
                *iptr++ = INSTR_ADJ;
                *iptr++ = param_count;
            }
            cur_type = symbol[SYMBOL_TYPE];
        }
        else if (scope == 0) {
            // Global symbol
            long int *address = (long int *) symbol[SYMBOL_VALUE];
            *iptr++ = INSTR_IMM;
            *iptr++ = (long int) address;
            cur_type = symbol[SYMBOL_TYPE];
            is_lvalue = 1;
        }
        else {
            // Local symbol
            long int param_count = cur_function_symbol[SYMBOL_FUNCTION_PARAM_COUNT];
            *iptr++ = INSTR_LEA;
            if (symbol[SYMBOL_STACK_INDEX] >= 0) {
                long int stack_index = param_count - symbol[SYMBOL_STACK_INDEX] - 1;
                *iptr++ = stack_index + 2; // Step over pushed PC and BP
            }
            else {
                *iptr++ = symbol[SYMBOL_STACK_INDEX];
            }
            cur_type = symbol[SYMBOL_TYPE];
            is_lvalue = 1;
        }
    }
    else {
        printf("Unknown token in expression: %d\n", cur_token);
        exit(1);
    }

    while (cur_token >= level) {
        // In order or precedence
        if (cur_token == TOK_MULTIPLY) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_LOGICAL_NOT);
            want_rvalue();
            *iptr++ = INSTR_MUL;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_DIVIDE) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_LOGICAL_NOT);
            want_rvalue();
            *iptr++ = INSTR_DIV;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_MOD) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_LOGICAL_NOT);
            want_rvalue();
            *iptr++ = INSTR_MOD;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_PLUS) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            want_rvalue();
            *iptr++ = INSTR_ADD;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_MINUS) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            want_rvalue();
            *iptr++ = INSTR_SUB;
            cur_type = TYPE_INT;
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
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_DBL_EQ);
            want_rvalue();
            *iptr++ = INSTR_AND;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_OR) {
            next();
            want_rvalue();
            *iptr++ = INSTR_PSH;
            expression(TOK_AND);
            want_rvalue();
            *iptr++ = INSTR_OR;
            cur_type = TYPE_INT;
        }
        else if (cur_token == TOK_EQ) {
            next();
            if (!is_lvalue) {
                printf("Cannot assign to an rvalue\n");
                exit(1);
            }
            *iptr++ = INSTR_PSH;
            expression(TOK_OR);
            want_rvalue();
            *iptr++ = cur_type == TYPE_INT ? INSTR_SI : INSTR_SC;
        }

        is_lvalue = 0;
    }
}

void parse_function_body(char *func_name) {
    int is_main, seen_return;
    is_main = !strcmp(func_name, "main");
    printf("is_main %d\n", is_main);
    seen_return = 0;
    int local_symbol_count = 0;

    consume(TOK_LCURLY);

    // Parse symbols first
    while (cur_token == TOK_INT || cur_token == TOK_CHAR) {
        int type = cur_token == TOK_INT ? TYPE_INT : TYPE_CHAR;
        next();
        while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

        expect(TOK_IDENTIFIER);
        cur_symbol = next_symbol;
        next_symbol[SYMBOL_TYPE] = type;
        next_symbol[SYMBOL_IDENTIFIER] = (long int) cur_identifier;
        next_symbol[SYMBOL_SCOPE] = cur_scope;
        next_symbol[SYMBOL_STACK_INDEX] = -1 - local_symbol_count++;
        next_symbol += SYMBOL_SIZE;
        next();
        expect(TOK_SEMI);
        while (cur_token == TOK_SEMI) next();
    }

    *iptr++ = INSTR_ENT;
    *iptr++ = local_symbol_count * 8; // allocate stack space for locals
    while (cur_token != TOK_RCURLY) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token == TOK_RETURN) {
            next();
            if (cur_token != TOK_SEMI) expression(TOK_COMMA);
            want_rvalue();
            *iptr++ = INSTR_LEV;
            consume(TOK_SEMI);
            seen_return = 1;
        }

        else {
            expression(TOK_COMMA);
        }
    }

    if (is_main && !seen_return) {
        printf("not seen return\n");
        *iptr++ = INSTR_IMM;
        *iptr++ = 0;
    }

    *iptr++ = INSTR_LEV;
    consume(TOK_RCURLY);
}

void parse() {
    cur_scope = 0;

    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }

        if (cur_token == TOK_INT || cur_token == TOK_CHAR) {
            int type = cur_token == TOK_INT ? TYPE_INT : TYPE_CHAR;
            next();
            while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }

            expect(TOK_IDENTIFIER);
            cur_symbol = next_symbol;
            next_symbol[SYMBOL_TYPE] = type;
            next_symbol[SYMBOL_IDENTIFIER] = (long int) cur_identifier;
            next_symbol[SYMBOL_SCOPE] = 0;
            next_symbol += SYMBOL_SIZE;
            next();

            if (cur_token == TOK_LPAREN) {
                cur_scope++;
                next();
                // Function definition
                cur_symbol[SYMBOL_VALUE] = (long int) iptr;
                int param_count = 0;
                while (cur_token != TOK_RPAREN) {
                    int type;
                    if (cur_token == TOK_INT || cur_token == TOK_CHAR) {
                        type = cur_token == TOK_INT ? TYPE_INT : TYPE_CHAR;
                        next();
                        while (cur_token == TOK_MULTIPLY) { type += TYPE_PTR; next(); }
                    }
                    else { printf("Unknown type in function def %d\n", cur_token); exit(1); }

                    consume(TOK_IDENTIFIER);
                    next_symbol[SYMBOL_TYPE] = type;
                    next_symbol[SYMBOL_IDENTIFIER] = (long int) cur_identifier;
                    next_symbol[SYMBOL_SCOPE] = cur_scope;
                    next_symbol[SYMBOL_STACK_INDEX] = param_count++;
                    next_symbol += SYMBOL_SIZE;
                    if (cur_token == TOK_COMMA) next();
                }
                cur_symbol[SYMBOL_FUNCTION_PARAM_COUNT] = param_count;
                consume(TOK_RPAREN);
                cur_function_symbol = cur_symbol;
                parse_function_body(cur_identifier);
            }
            else {
                // Global symbol
                cur_symbol[SYMBOL_VALUE] = (long int) data;
                data += sizeof(long int);
            }
        }

        else if (cur_token == TOK_ENUM) {
            consume(TOK_ENUM);
            consume(TOK_LCURLY);
            long int number = 0;
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
                next_symbol[SYMBOL_IDENTIFIER] = (long int) cur_identifier;
                next_symbol[SYMBOL_SCOPE] = 0;
                next_symbol[SYMBOL_VALUE] = number++;
                next_symbol += SYMBOL_SIZE;

                if (cur_token == TOK_COMMA) next();
            }
            consume(TOK_RCURLY);
            consume(TOK_SEMI);
        }

        else {
            printf("Expected global declaration or function\n");
            exit(1);
        }
    }
}

long int run(long int argc, char **argv, int print_instructions) {
    long int *stack = malloc(10240);

    long int a;
    long int *pc;
    long int *sp, *bp;
    long int *t;
    sp = stack + 10240 - 1;    // stack pointer
    bp = sp;

    *--sp = INSTR_EXIT; // call exit if main returns
    *--sp = INSTR_PSH;
    t = sp;
    *--sp = argc;
    *--sp = (long int) argv;
    *--sp = (long int) t;

    a = 0;

    pc = (long int *) lookup_function("main");

    while (*pc) {
        int instr = *pc++;

        if (print_instructions) {
            printf("a = %-20ld ", a);
            printf("sp = %-20ld ", (long int) sp);
            printf("%.5s", &"LEA  IMM  JSR  ENT  ADJ  LEV  LI   LC   SI   SC   OR   AND  EQ   NE   LT   GT   LE   GE   ADD  SUB  MUL  DIV  MOD  PSH  PRTF EXIT"[instr * 5 - 5]);
            if (instr <= INSTR_ADJ) printf(" %ld", *pc);
            printf("\n");
        }

             if (instr == INSTR_LEA) a = (long int) (bp + *pc++);                                   // load local address
        else if (instr == INSTR_IMM) a = *pc++;                                                     // load global address or immediate
        else if (instr == INSTR_JSR) { *--sp = (long int) (pc + 1); pc = (long int *)*pc; }         // jump to subroutine
        else if (instr == INSTR_ENT) { *--sp = (long int) bp; bp = sp; sp = sp - *pc++; }           // enter subroutine
        else if (instr == INSTR_ADJ) sp = sp + *pc++;                                               // stack adjust
        else if (instr == INSTR_LEV) { sp = bp; bp = (long int *) *sp++; pc = (long int *) *sp++; } // leave subroutine
        else if (instr == INSTR_LI)  a = *(long int *)a;                                            // load int
        else if (instr == INSTR_LC)  a = *(char *)a;                                                // load char
        else if (instr == INSTR_SI) *(int *) *sp++ = a;                                             // store int
        else if (instr == INSTR_SC) a = *(char *) *sp++ = a;                                        // store char
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

        else if (instr == INSTR_PRTF) { t = sp + *pc++; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); }
        else if (instr == INSTR_EXIT) { printf("exit %ld\n", *sp); return *sp; }

        else {
            printf("WTF instruction %d\n", instr);
            exit(1);
        }
    }

    // Should never get here
    return 0;
}

void add_builtin(char *identifier, int instruction) {
    long int *symbol = next_symbol;
    symbol[SYMBOL_TYPE] = TYPE_VOID;
    symbol[SYMBOL_IDENTIFIER] = (long int) identifier;
    symbol[SYMBOL_BUILTIN] = instruction;
    next_symbol += SYMBOL_SIZE;
}

int main(int argc, char **argv) {
    char *filename;

    int i;
    int debug, print_instructions, show_symbols;
    print_instructions = 0;
    show_symbols = 0;
    i = 1;
    while (i < argc) {
        if (!memcmp(argv[i], "-d", 2)) debug = 1;
        else if (!memcmp(argv[i], "-i", 2)) print_instructions = 1;
        else if (!memcmp(argv[i], "-s", 2)) show_symbols = 1;
        else filename = argv[i];
        i++;
    }

    if (!filename) {
        printf("Usage: wc4 INPUT-FILE\n");
        exit(1);
    }

    input = malloc(10240);
    instructions = malloc(10240);
    symbol_table = malloc(10240);
    memset(symbol_table, 0, 10240);
    next_symbol = symbol_table;
    data = malloc(10240);

    SYMBOL_TYPE                 = 0;
    SYMBOL_IDENTIFIER           = 1;
    SYMBOL_SCOPE                = 2;
    SYMBOL_VALUE                = 3;
    SYMBOL_STACK_INDEX          = 4;
    SYMBOL_FUNCTION_PARAM_COUNT = 5;
    SYMBOL_BUILTIN              = 6;
    SYMBOL_SIZE                 = 7; // Number of long ints

    add_builtin("printf", INSTR_PRTF);

    iptr = instructions;
    int f;
    f  = open(filename, 0); // O_RDONLY = 0
    if (f < 0) { printf("Unable to open input file\n"); exit(1); }
    input_size = read(f, input, 10240);
    input[input_size] = 0;
    if (input_size < 0) { printf("Unable to read input file\n"); exit(1); }
    close(f);

    next();
    parse();

    if (show_symbols) {
        printf("Symbols:\n");
        long int *s = symbol_table;
        while (s[0]) {
            long int type = s[SYMBOL_TYPE];
            char *identifier = (char *) s[SYMBOL_IDENTIFIER];
            long int scope = s[SYMBOL_SCOPE];
            long int value = s[SYMBOL_VALUE];
            long int stack_index = s[SYMBOL_STACK_INDEX];
            printf("%-20ld %ld %ld %-2ld %-20ld %s\n", (long int) s, type, scope, stack_index, value, identifier);
            s += SYMBOL_SIZE;
        }
        printf("\n");
    }

    run(argc, argv, print_instructions);

    exit(0);
}
