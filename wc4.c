#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char input[10240];
int input_size;
int ip;      // input position
int instructions[10240];
int *iptr = instructions;

int DEBUG = 1;

int cur_token;
char *cur_identifier;
int cur_integer;
char *cur_string_literal;

int TOK_EOF            = 0;
int TOK_IDENTIFIER     = 1;
int TOK_NUMBER         = 2;
int TOK_STRING_LITERAL = 3;
int TOK_INT            = 4;
int TOK_IF             = 5;
int TOK_CHAR           = 6;
int TOK_VOID           = 7;
int TOK_WHILE          = 8;
int TOK_RPAREN         = 9;
int TOK_LPAREN         = 10;
int TOK_RBRACKET       = 11;
int TOK_LBRACKET       = 12;
int TOK_RCURLY         = 13;
int TOK_LCURLY         = 14;
int TOK_SEMI           = 15;
int TOK_COMMA          = 16; // In order of precedence
int TOK_EQ             = 17;
int TOK_OR             = 18;
int TOK_AND            = 19;
int TOK_DBL_EQ         = 20;
int TOK_NOT_EQ         = 21;
int TOK_LT             = 22;
int TOK_GT             = 23;
int TOK_LE             = 24;
int TOK_GE             = 25;
int TOK_PLUS           = 26;
int TOK_MINUS          = 27;
int TOK_MULTIPLY       = 28;
int TOK_DIVIDE         = 29;
int TOK_MOD            = 30;
int TOK_LOGICAL_NOT    = 31;

int TYPE_INT  = 0;
int TYPE_CHAR = 1;

int INSTR_IMM = 1;
int INSTR_ADJ = 2;
int INSTR_EQ  = 3;
int INSTR_NE  = 4;
int INSTR_LT  = 5;
int INSTR_GT  = 6;
int INSTR_LE  = 7;
int INSTR_GE  = 8;
int INSTR_ADD = 9;
int INSTR_SUB = 10;
int INSTR_MUL = 11;
int INSTR_DIV = 12;
int INSTR_MOD = 13;
int INSTR_PSH = 14;

void next() {
    while (ip < input_size) {
        char *i = input;
             if (input_size - ip >= 2 && !memcmp(i+ip, "if",    2)     ) { ip += 2; cur_token = TOK_IF;                         }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "int",   3)     ) { ip += 2; cur_token = TOK_INT;                        }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "int",   4)     ) { ip += 2; cur_token = TOK_CHAR;                       }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "while", 5)     ) { ip += 2; cur_token = TOK_WHILE;                      }
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

        else if (input_size - ip >= 2 && (i[ip] == '/' && i[ip + 1] <= '/')) {
            ip += 2;
            while (i[ip++] != '\n');
            continue;
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
            expression(TOK_MULTIPLY);
            *iptr++ = INSTR_LT;
        }
        else if (cur_token == TOK_GT) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            *iptr++ = INSTR_GT;
        }
        else if (cur_token == TOK_LE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
            *iptr++ = INSTR_LE;
        }
        else if (cur_token == TOK_GE) {
            next();
            *iptr++ = INSTR_PSH;
            expression(TOK_MULTIPLY);
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
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: wc4 INPUT-FILE\n");
        exit(1);
    }

    char *filename = argv[1];
    int f = open(filename, 0); // O_RDONLY = 0
    if (f < 0) { printf("Unable to open input file\n"); exit(1); }
    input_size = read(f, input, 10240);
    input[input_size] = 0;
    if (input_size < 0) { printf("Unable to read input file\n"); exit(1); }
    close(f);

    next();
    expression(TOK_COMMA);

    int a;
    iptr = instructions;
    int *stack = malloc(10240);
    int *stack_ptr = stack + 10240 - 1;

    while (*iptr) {
        int instr = *iptr++;

        if (DEBUG) {
            printf("a = %-10d ", a);
            printf("%.4s", &"IMM ADJ ADD SUB MUL DIV PSH"[instr * 4 - 4]);
            if (instr <= INSTR_ADJ) printf(" %d", *iptr);
            printf("\n");
        }

             if (instr == INSTR_IMM) a = *iptr++;
        else if (instr == INSTR_PSH) *--stack_ptr = a;
        else if (instr == INSTR_EQ ) a = *stack_ptr++ == a;
        else if (instr == INSTR_NE ) a = *stack_ptr++ != a;
        else if (instr == INSTR_LT ) a = *stack_ptr++ < a;
        else if (instr == INSTR_GT ) a = *stack_ptr++ > a;
        else if (instr == INSTR_LE ) a = *stack_ptr++ <= a;
        else if (instr == INSTR_GE ) a = *stack_ptr++ >= a;
        else if (instr == INSTR_ADD) a = *stack_ptr++ + a;
        else if (instr == INSTR_SUB) a = *stack_ptr++ - a;
        else if (instr == INSTR_MUL) a = *stack_ptr++ * a;
        else if (instr == INSTR_DIV) a = *stack_ptr++ / a;
        else if (instr == INSTR_MOD) a = *stack_ptr++ % a;
        else {
            printf("WTF instruction %d\n", instr);
            exit(1);
        }
    }

    printf("Result: %d\n", a);
    exit(0);
}
