#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char *input;
int input_size;
int ip;
int *instructions;
int *iptr;

int cur_token;
char *cur_identifier;
int cur_integer;
char *cur_string_literal;

long int *symbols;

int SYMBOL_TYPE;
int SYMBOL_IDENTIFIER;
int SYMBOL_SCOPE;
int SYMBOL_VALUE;
int SYMBOL_SIZE;

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

enum { TYPE_INT=1, TYPE_CHAR, TYPE_PTR_TO_INT, TYPE_PTR_TO_CHAR };

enum {
    INSTR_IMM=1,
    INSTR_ADJ,
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


        else if (input_size - ip >= 2 && !memcmp(i+ip, "if",    2)     ) { ip += 2; cur_token = TOK_IF;                         }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "int",   3)     ) { ip += 3; cur_token = TOK_INT;                        }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "char",  4)     ) { ip += 4; cur_token = TOK_CHAR;                       }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "while", 5)     ) { ip += 5; cur_token = TOK_WHILE;                      }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "enum", 4)      ) { ip += 4; cur_token = TOK_ENUM;                       }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "&&",    2)     ) { ip += 2; cur_token = TOK_AND;                        }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "||",    2)     ) { ip += 2; cur_token = TOK_OR;                         }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "==",    2)     ) { ip += 2; cur_token = TOK_DBL_EQ;                     }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "!=",    2)     ) { ip += 2; cur_token = TOK_NOT_EQ;                     }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "<=",    2)     ) { ip += 2; cur_token = TOK_LE;                         }
        else if (input_size - ip >= 2 && !memcmp(i+ip, ">=",    2)     ) { ip += 2; cur_token = TOK_GE;                         }
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

void expression(int level) {
    if (cur_token == TOK_LOGICAL_NOT) {
        next();

        *iptr++ = INSTR_IMM;
        *iptr++ = 0;
        *iptr++ = INSTR_PSH;
        expression(1024); // Fake highest precedence, bind nothing
        *iptr++ = INSTR_EQ;
    }
    else if (cur_token == TOK_MINUS) {
        next();

        if (cur_token == TOK_NUMBER) {
            *iptr++ = INSTR_IMM;
            *iptr++ = -cur_integer;
            next();
        }
        else {
            *iptr++ = INSTR_IMM;
            *iptr++ = -1;
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            *iptr++ = INSTR_MUL;
        }
    }
    else if (cur_token == TOK_LPAREN) {
        next();
        expression(TOK_COMMA);
        next();
    }
    else {
        *iptr++ = INSTR_IMM;
        *iptr++ = cur_integer;
        next();
    }

    while (cur_token >= level) {
        // In order or precedence
        if (cur_token == TOK_MULTIPLY) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_LOGICAL_NOT);
            *iptr++ = INSTR_MUL;
        }
        else if (cur_token == TOK_DIVIDE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_LOGICAL_NOT);
            *iptr++ = INSTR_DIV;
        }
        else if (cur_token == TOK_MOD) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_LOGICAL_NOT);
            *iptr++ = INSTR_MOD;
        }
        else if (cur_token == TOK_PLUS) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            *iptr++ = INSTR_ADD;
        }
        else if (cur_token == TOK_MINUS) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            *iptr++ = INSTR_SUB;
        }
        else if (cur_token == TOK_LT) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_LT;
        }
        else if (cur_token == TOK_GT) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_GT;
        }
        else if (cur_token == TOK_LE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_LE;
        }
        else if (cur_token == TOK_GE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_PLUS);
            *iptr++ = INSTR_GE;
        }
        else if (cur_token == TOK_DBL_EQ) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_LT);
            *iptr++ = INSTR_EQ;
        }
        else if (cur_token == TOK_NOT_EQ) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_LT);
            *iptr++ = INSTR_NE;
        }
        else if (cur_token == TOK_AND) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_DBL_EQ);
            *iptr++ = INSTR_AND;
        }
        else if (cur_token == TOK_OR) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_AND);
            *iptr++ = INSTR_OR;
        }
    }
}


void globals() {
    long int *globals = symbols;
    while (cur_token != TOK_EOF) {
        if (cur_token == TOK_SEMI)  {
            next();
            continue;
        }


        if (cur_token == TOK_INT || cur_token == TOK_CHAR) {
            int type;
            type = cur_token == TOK_INT ? TYPE_INT : TYPE_CHAR;
            next();
            if (cur_token == TOK_MULTIPLY) {
                type += 2;
                next();
            }

            expect(TOK_IDENTIFIER);
            globals[SYMBOL_TYPE] = type;
            globals[SYMBOL_IDENTIFIER] = (long int) cur_identifier;
            globals[SYMBOL_SCOPE] = 0;
            globals += SYMBOL_SIZE;
            next();
        }

        else if (cur_token == TOK_ENUM) {
            consume(TOK_ENUM);
            consume(TOK_LCURLY);
            int number = 0;
            while (cur_token != TOK_RCURLY) {
                expect(TOK_IDENTIFIER);
                next();
                if (cur_token == TOK_EQ) {
                    next();
                    expect(TOK_NUMBER);
                    number = cur_integer;
                    next();
                }

                globals[SYMBOL_TYPE] = TYPE_INT;
                globals[SYMBOL_IDENTIFIER] = (long int) cur_identifier;
                globals[SYMBOL_SCOPE] = 0;
                globals[SYMBOL_VALUE] = number++;

                globals += SYMBOL_SIZE;

                if (cur_token == TOK_COMMA) next();
            }
            consume(TOK_RCURLY);
        }

        else {
            printf("Expected int or char\n");
            exit(1);
        }
    }

    printf("Globals:\n");
    long int *s = symbols;
    while (s[0]) {
        int type = s[SYMBOL_TYPE];
        char *identifier = (char *) s[SYMBOL_IDENTIFIER];
        int scope = s[SYMBOL_SCOPE];
        printf("%d %s\n", type, identifier);
        s += SYMBOL_SIZE;
    }

}

void run() {
    int *stack = malloc(10240);

    int a;
    int *pc;
    pc = instructions;         // program counter
    int *sp;
    sp = stack + 10240 - 1;    // stack pointer

    int DEBUG;
    DEBUG = 1;

    while (*pc) {
        int instr = *pc++;

        if (DEBUG) {
            printf("a = %-10d ", a);
            printf("%.4s", &"IMM ADJ ADD SUB MUL DIV PSH"[instr * 4 - 4]);
            if (instr <= INSTR_ADJ) printf(" %d", *pc);
            printf("\n");
        }

             if (instr == INSTR_IMM) a = *pc++;
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
        else {
            printf("WTF instruction %d\n", instr);
            exit(1);
        }
    }

    printf("Result: %d\n", a);
}

int main(int argc, char **argv) {
    char *filename;

    int i;
    int eval_expression;
    i = 1;
    while (i < argc) {
        if (!memcmp(argv[i], "-e", 2)) eval_expression = 1;
        else filename = argv[i];
        i++;
    }

    if (!filename) {
        printf("Usage: wc4 INPUT-FILE\n");
        exit(1);
    }

    input = malloc(10240);
    instructions = malloc(10240);
    symbols = malloc(1024);
    memset(symbols, 0, 10240);

    SYMBOL_TYPE = 0;
    SYMBOL_IDENTIFIER = 8;
    SYMBOL_SCOPE = 16;
    SYMBOL_VALUE = 24;
    SYMBOL_SIZE = 32;

    iptr = instructions;
    int f;
    f  = open(filename, 0); // O_RDONLY = 0
    if (f < 0) { printf("Unable to open input file\n"); exit(1); }
    input_size = read(f, input, 10240);
    input[input_size] = 0;
    if (input_size < 0) { printf("Unable to read input file\n"); exit(1); }
    close(f);

    next();
    if (eval_expression) {
        expression(TOK_COMMA);
        run();
    }

    else
        globals();

    exit(0);
}
