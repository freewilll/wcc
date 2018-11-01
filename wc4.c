#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char input[10240];
int input_size;
int ip;      // input position
void *token; // current token
int instructions[10240];
int *iptr = instructions;

int DEBUG = 1;

int TOKEN_SIZE               = 8;
int TOKEN_POS_IDENTIFIER     = 8;
int TOKEN_POS_INTEGER        = 8;
int TOKEN_POS_STRING_LITERAL = 8;

int TOK_EOF            = 0;
int TOK_IDENTIFIER     = 1;
int TOK_INTEGER        = 2;
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
int TOK_PLUS           = 15;
int TOK_MINUS          = 16;
int TOK_ASTERISK       = 17;
int TOK_SLASH          = 18;
int TOK_COMMA          = 19;
int TOK_SEMI           = 20;
int TOK_EQ             = 21;
int TOK_LT             = 22;
int TOK_GT             = 23;
int TOK_NOT            = 24;
int TOK_AND            = 25;
int TOK_OR             = 26;
int TOK_DBL_EQ         = 27;
int TOK_NOT_EQ         = 28;

int TYPE_INT  = 0;
int TYPE_CHAR = 1;

int INSTR_IMM = 1;
int INSTR_ADJ = 2;
int INSTR_ADD = 3;
int INSTR_SUB = 4;
int INSTR_MUL = 5;
int INSTR_DIV = 6;
int INSTR_PSH = 7;

void * get_next_token() {
    while (ip < input_size) {
        void *token = malloc(TOKEN_SIZE);
        void *v = token;

        char *i = input;
             if (input_size - ip >= 2 && !memcmp(i+ip, "if",    2)     ) { ip += 2; *((int *) v) = TOK_IF;       }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "int",   3)     ) { ip += 2; *((int *) v) = TOK_INT;      }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "int",   4)     ) { ip += 2; *((int *) v) = TOK_CHAR;     }
        else if (input_size - ip >= 5 && !memcmp(i+ip, "while", 5)     ) { ip += 2; *((int *) v) = TOK_WHILE;    }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "&&",    2)     ) { ip += 2; *((int *) v) = TOK_AND;      }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "||",    2)     ) { ip += 2; *((int *) v) = TOK_OR;       }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "==",    2)     ) { ip += 2; *((int *) v) = TOK_DBL_EQ;   }
        else if (input_size - ip >= 2 && !memcmp(i+ip, "!=",    2)     ) { ip += 2; *((int *) v) = TOK_NOT_EQ;   }
        else if (input_size - ip >= 1 && i[ip] == '('                  ) { ip += 1; *((int *) v) = TOK_LPAREN;   }
        else if (input_size - ip >= 1 && i[ip] == ')'                  ) { ip += 1; *((int *) v) = TOK_RPAREN;   }
        else if (input_size - ip >= 1 && i[ip] == '['                  ) { ip += 1; *((int *) v) = TOK_LBRACKET; }
        else if (input_size - ip >= 1 && i[ip] == ']'                  ) { ip += 1; *((int *) v) = TOK_RBRACKET; }
        else if (input_size - ip >= 1 && i[ip] == '{'                  ) { ip += 1; *((int *) v) = TOK_LCURLY;   }
        else if (input_size - ip >= 1 && i[ip] == '}'                  ) { ip += 1; *((int *) v) = TOK_RCURLY;   }
        else if (input_size - ip >= 1 && i[ip] == '+'                  ) { ip += 1; *((int *) v) = TOK_PLUS;     }
        else if (input_size - ip >= 1 && i[ip] == '-'                  ) { ip += 1; *((int *) v) = TOK_MINUS;    }
        else if (input_size - ip >= 1 && i[ip] == '*'                  ) { ip += 1; *((int *) v) = TOK_ASTERISK; }
        else if (input_size - ip >= 1 && i[ip] == '/'                  ) { ip += 1; *((int *) v) = TOK_SLASH;    }
        else if (input_size - ip >= 1 && i[ip] == ','                  ) { ip += 1; *((int *) v) = TOK_COMMA;    }
        else if (input_size - ip >= 1 && i[ip] == ';'                  ) { ip += 1; *((int *) v) = TOK_SEMI;     }
        else if (input_size - ip >= 1 && i[ip] == '='                  ) { ip += 1; *((int *) v) = TOK_EQ;       }
        else if (input_size - ip >= 1 && i[ip] == '<'                  ) { ip += 1; *((int *) v) = TOK_LT;       }
        else if (input_size - ip >= 1 && i[ip] == '>'                  ) { ip += 1; *((int *) v) = TOK_GT;       }
        else if (input_size - ip >= 1 && i[ip] == '!'                  ) { ip += 1; *((int *) v) = TOK_NOT;      }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\t'", 4)     ) { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\t'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\n'", 4)     ) { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\n'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\''", 4)     ) { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\''; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\"'", 4)    ) { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\"'; }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\\'", 4)    ) { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\\'; }

        else if (input_size - ip >= 3 && i[ip] == '\'' && i[ip+2] == '\'') { ip += 3; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = i[ip+1]; }

        else if (i[ip] == ' ' || i[ip] == '\t' || i[ip] == '\n') { ip++; continue; }

        else if ((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z')) {
            *((int *) v) = TOK_IDENTIFIER;
            char *id = malloc(128);
            int idp = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) { id[idp] = i[ip]; idp++; ip++; }
            id[idp] = 0;
            *((char **) (token + TOKEN_POS_IDENTIFIER)) = id;
        }

        else if ((i[ip] >= '0' && i[ip] <= '9')) {
            *((int *) v) = TOK_INTEGER;
            int value = 0;
            while ((i[ip] >= '0' && i[ip] <= '9') && ip < input_size) { value = value * 10 + (i[ip] - '0'); ip++; }
            *((int *) (token + TOKEN_POS_INTEGER)) = value;
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
            *((int *) v) = TOK_STRING_LITERAL;
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
            *((char **) (token + TOKEN_POS_STRING_LITERAL)) = sl;
        }

        else {
            printf("Unknown token: %s", input + ip);
            exit(1);
        }

        return v;
    }

    void *token = malloc(TOKEN_SIZE);
    *((int *) token) = TOK_EOF;
    return token;
}

void expect(int type) {
    if ((*((int *) token) != type))  {
        printf("Expected token %d got %d\n", type, *((int *) token));
        exit(1);
    }
}

void visit_expr();

void visit_factor() {
    if (*((int *) token) == TOK_LPAREN) {
        token = get_next_token();
        visit_expr();
        expect(TOK_RPAREN);
        token = get_next_token();
        return;
    }
    else {
        expect(TOK_INTEGER);
        *iptr++ = INSTR_IMM;
        *iptr++ = *((int *) (token + TOKEN_POS_INTEGER));
        token = get_next_token();
    }
}

void visit_term_tail() {
    if (*((int *) token) == TOK_ASTERISK) {
        token = get_next_token();
        *iptr++ = INSTR_PSH;
        visit_factor();
        *iptr++ = INSTR_MUL;
        visit_term_tail();
    }
    else if (*((int *) token) == TOK_SLASH) {
        token = get_next_token();
        *iptr++ = INSTR_PSH;
        visit_factor();
        *iptr++ = INSTR_DIV;
        visit_term_tail();
    }
    else
        return;
}

void visit_term() {
    visit_factor();
    visit_term_tail();
}

void visit_expr_tail() {
    if (*((int *) token) == TOK_PLUS) {
        token = get_next_token();
        *iptr++ = INSTR_PSH;
        visit_term();
        *iptr++ = INSTR_ADD;
        visit_expr_tail();
    }
    else if (*((int *) token) == TOK_MINUS) {
        token = get_next_token();
        *iptr++ = INSTR_PSH;
        visit_term();
        *iptr++ = INSTR_SUB;
        visit_expr_tail();
    }
    else
        return;
}

void visit_expr() {
    visit_term();
    visit_expr_tail();
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

    token = get_next_token();
    visit_expr();

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
        else if (instr == INSTR_ADD) a = *stack_ptr++ + a;
        else if (instr == INSTR_SUB) a = *stack_ptr++ - a;
        else if (instr == INSTR_MUL) a = *stack_ptr++ * a;
        else if (instr == INSTR_DIV) a = *stack_ptr++ / a;
        else {
            printf("WTF instruction %d\n", instr);
            exit(1);
        }
    }

    printf("Result: %d\n", a);
    exit(0);
}
