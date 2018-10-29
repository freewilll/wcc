#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

char input[10240];
int input_size;
int ip; // input position

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
int TOK_COMMA          = 18;
int TOK_SEMI           = 19;
int TOK_EQ             = 20;
int TOK_LT             = 21;
int TOK_GT             = 22;
int TOK_NOT            = 23;
int TOK_AND            = 24;
int TOK_OR             = 25;
int TOK_DBL_EQ         = 26;
int TOK_NOT_EQ         = 27;

void * get_next_token() {
    while (ip < input_size) {
        void *token = malloc(TOKEN_SIZE);
        void *v = token;

        char *i = input;
             if (input_size - ip >= 2 && i[ip] == 'i' && i[ip+1] == 'f'                                                      ) { ip += 2; *((int *) v) = TOK_IF;       }
        else if (input_size - ip >= 3 && i[ip] == 'i' && i[ip+1] == 'n' && i[ip+2] == 't'                                    ) { ip += 3; *((int *) v) = TOK_INT;      }
        else if (input_size - ip >= 4 && i[ip] == 'c' && i[ip+1] == 'h' && i[ip+2] == 'a' && i[ip+3] == 'r'                  ) { ip += 4; *((int *) v) = TOK_CHAR;     }
        else if (input_size - ip >= 4 && i[ip] == 'v' && i[ip+1] == 'o' && i[ip+2] == 'i' && i[ip+3] == 'd'                  ) { ip += 4; *((int *) v) = TOK_VOID;     }
        else if (input_size - ip >= 5 && i[ip] == 'w' && i[ip+1] == 'h' && i[ip+2] == 'i' && i[ip+3] == 'l' && i[ip+3] == 'e') { ip += 5; *((int *) v) = TOK_WHILE;    }

        else if (input_size - ip >= 2 && i[ip] == '&' && i[ip+1] == '&') { ip += 2; *((int *) v) = TOK_AND;      }
        else if (input_size - ip >= 2 && i[ip] == '|' && i[ip+1] == '|') { ip += 2; *((int *) v) = TOK_OR;       }
        else if (input_size - ip >= 2 && i[ip] == '=' && i[ip+1] == '=') { ip += 2; *((int *) v) = TOK_DBL_EQ;   }
        else if (input_size - ip >= 2 && i[ip] == '!' && i[ip+1] == '=') { ip += 2; *((int *) v) = TOK_NOT_EQ;   }
        else if (input_size - ip >= 1 && i[ip] == '('                  ) { ip += 1; *((int *) v) = TOK_RPAREN;   }
        else if (input_size - ip >= 1 && i[ip] == ')'                  ) { ip += 1; *((int *) v) = TOK_LPAREN;   }
        else if (input_size - ip >= 1 && i[ip] == '['                  ) { ip += 1; *((int *) v) = TOK_RBRACKET; }
        else if (input_size - ip >= 1 && i[ip] == ']'                  ) { ip += 1; *((int *) v) = TOK_LBRACKET; }
        else if (input_size - ip >= 1 && i[ip] == '{'                  ) { ip += 1; *((int *) v) = TOK_RCURLY;   }
        else if (input_size - ip >= 1 && i[ip] == '}'                  ) { ip += 1; *((int *) v) = TOK_LCURLY;   }
        else if (input_size - ip >= 1 && i[ip] == '+'                  ) { ip += 1; *((int *) v) = TOK_PLUS;     }
        else if (input_size - ip >= 1 && i[ip] == '-'                  ) { ip += 1; *((int *) v) = TOK_MINUS;    }
        else if (input_size - ip >= 1 && i[ip] == '*'                  ) { ip += 1; *((int *) v) = TOK_ASTERISK; }
        else if (input_size - ip >= 1 && i[ip] == ','                  ) { ip += 1; *((int *) v) = TOK_COMMA;    }
        else if (input_size - ip >= 1 && i[ip] == ';'                  ) { ip += 1; *((int *) v) = TOK_SEMI;     }
        else if (input_size - ip >= 1 && i[ip] == '='                  ) { ip += 1; *((int *) v) = TOK_EQ;       }
        else if (input_size - ip >= 1 && i[ip] == '<'                  ) { ip += 1; *((int *) v) = TOK_LT;       }
        else if (input_size - ip >= 1 && i[ip] == '>'                  ) { ip += 1; *((int *) v) = TOK_GT;       }
        else if (input_size - ip >= 1 && i[ip] == '!'                  ) { ip += 1; *((int *) v) = TOK_NOT;      }

        else if (input_size - ip >= 4 && i[ip] == '\'' && i[ip+1] == '\\' && i[ip+2] == 't'  && i[ip+3] == '\'') { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\t'; }
        else if (input_size - ip >= 4 && i[ip] == '\'' && i[ip+1] == '\\' && i[ip+2] == 'n'  && i[ip+3] == '\'') { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\n'; }
        else if (input_size - ip >= 4 && i[ip] == '\'' && i[ip+1] == '\\' && i[ip+2] == '\'' && i[ip+3] == '\'') { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\''; }
        else if (input_size - ip >= 4 && i[ip] == '\'' && i[ip+1] == '\\' && i[ip+2] == '\"' && i[ip+3] == '\'') { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\"'; }
        else if (input_size - ip >= 4 && i[ip] == '\'' && i[ip+1] == '\\' && i[ip+2] == '\\' && i[ip+3] == '\'') { ip += 4; *((int *) v) = TOK_INTEGER; *((int *) (token + TOKEN_POS_INTEGER)) = '\\'; }
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

    ip = 0;

    void *token = 0;
    while (!token || (token && *((int *) token) != TOK_EOF)) token = get_next_token();

    exit(0);
}
