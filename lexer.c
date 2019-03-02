#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wc4.h"

void init_lexer(char *filename) {
    void *f;

    ip = 0;
    input = malloc(10 * 1024 * 1024);
    f  = fopen(filename, "r");
    if (f == 0) {
        perror(filename);
        exit(1);
    }
    input_size = fread(input, 1, 10 * 1024 * 1024, f);
    if (input_size < 0) {
        printf("Unable to read input file\n");
        exit(1);
    }
    input[input_size] = 0;
    fclose(f);

    cur_filename = filename;
    cur_line = 1;
    next();
}

// Lexer. Lex a next token or TOK_EOF if the file is ended
void next() {
    char *i;        // Assigned to input for brevity
    int j;          // Counter
    char c1, c2;    // Next two characters in input

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
        else if (                        c1 == '#'                        )  { ip += 1;  cur_token = TOK_HASH;                       }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\t'",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\t';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\n'",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\n';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\''",  4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\'';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\"'", 4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\"';    }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\\'", 4        )) { ip += 4;  cur_token = TOK_NUMBER; cur_long = '\\';    }

        else if (input_size - ip >= 3 && c1 == '\'' && i[ip+2] == '\'') { cur_long = i[ip+1]; ip += 3; cur_token = TOK_NUMBER; }

        // Identifier or keyword
        else if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_') {
            cur_token = TOK_IDENTIFIER;
            cur_identifier = malloc(1024);
            j = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) {
                cur_identifier[j] = i[ip];
                j++; ip++;
            }
            cur_identifier[j] = 0;

                 if (!strcmp(cur_identifier, "if"           )) { cur_token = TOK_IF;        }
            else if (!strcmp(cur_identifier, "else"         )) { cur_token = TOK_ELSE;      }
            else if (!strcmp(cur_identifier, "char"         )) { cur_token = TOK_CHAR;      }
            else if (!strcmp(cur_identifier, "short"        )) { cur_token = TOK_SHORT;     }
            else if (!strcmp(cur_identifier, "int"          )) { cur_token = TOK_INT;       }
            else if (!strcmp(cur_identifier, "long"         )) { cur_token = TOK_LONG;      }
            else if (!strcmp(cur_identifier, "void"         )) { cur_token = TOK_VOID;      }
            else if (!strcmp(cur_identifier, "struct"       )) { cur_token = TOK_STRUCT;    }
            else if (!strcmp(cur_identifier, "while"        )) { cur_token = TOK_WHILE;     }
            else if (!strcmp(cur_identifier, "for"          )) { cur_token = TOK_FOR;       }
            else if (!strcmp(cur_identifier, "continue"     )) { cur_token = TOK_CONTINUE;  }
            else if (!strcmp(cur_identifier, "break"        )) { cur_token = TOK_BREAK;     }
            else if (!strcmp(cur_identifier, "return"       )) { cur_token = TOK_RETURN;    }
            else if (!strcmp(cur_identifier, "enum"         )) { cur_token = TOK_ENUM;      }
            else if (!strcmp(cur_identifier, "sizeof"       )) { cur_token = TOK_SIZEOF;    }
            else if (!strcmp(cur_identifier, "__attribute__")) { cur_token = TOK_ATTRIBUTE; }
            else if (!strcmp(cur_identifier, "__packed__"   )) { cur_token = TOK_PACKED;    }
            else if (!strcmp(cur_identifier, "packed"       )) { cur_token = TOK_PACKED;    }
            else if (!strcmp(cur_identifier, "include"      )) { cur_token = TOK_INCLUDE;   }
        }

        // Hex numeric literal
        else if (c1 == '0' && input_size - ip >= 2 && i[ip+1] == 'x') {
            ip += 2;
            cur_token = TOK_NUMBER;
            cur_long = 0;
            while (((i[ip] >= '0' && i[ip] <= '9')  || (i[ip] >= 'a' && i[ip] <= 'f')) && ip < input_size) {
                cur_long = cur_long * 16 + (i[ip] >= 'a' ? i[ip] - 'a' + 10 : i[ip] - '0');
                ip++;
            }
        }

        // Decimal numeric literal
        else if ((c1 >= '0' && c1 <= '9')) {
            cur_token = TOK_NUMBER;
            cur_long = 0;
            while ((i[ip] >= '0' && i[ip] <= '9') && ip < input_size) { cur_long = cur_long * 10 + (i[ip] - '0'); ip++; }
        }

        // Ignore CPP directives
        else if (c1 == '#') {
            while (i[ip++] != '\n');
            cur_line++;
            continue;
        }

        // String literal
        else if (i[ip] == '"') {
            cur_token = TOK_STRING_LITERAL;
            cur_string_literal = malloc(1024);
            j = 0;
            ip += 1;
            while (input_size - ip >= 1 && i[ip] != '"') {
                     if (i[ip] != '\\') { cur_string_literal[j] = i[ip]; j++; ip++; }
                else if (input_size - ip >= 2 && i[ip] == '\\') {
                         if (i[ip+1] == 't' ) cur_string_literal[j++] = 9;
                    else if (i[ip+1] == 'n' ) cur_string_literal[j++] = 10;
                    else if (i[ip+1] == '\\') cur_string_literal[j++] = '\\';
                    else if (i[ip+1] == '\'') cur_string_literal[j++] = '\'';
                    else if (i[ip+1] == '\"') cur_string_literal[j++] = '\"';
                    else panic("Unknown \\ escape in string literal");
                    ip += 2;
                }
            }
            ip++;
            cur_string_literal[j] = 0;
        }

        else
            panic1d("Unknown token %d", cur_token);

        return;
    }

    if (parsing_header)
        finish_parsing_header();
    else
        cur_token = TOK_EOF;
}

void expect(int token, char *what) {
    if (cur_token != token) panic1s("Expected %s", what);
}

void consume(int token, char *what) {
    expect(token, what);
    next();
}