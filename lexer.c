#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

void init_lexer(char *filename) {
    ip = 0;
    input = malloc(10 * 1024 * 1024);
    void *f  = fopen(filename, "r");
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

// Lex U, L, LL integer constant suffixes and determine type
// See http://port70.net/~nsz/c/c99/n1256.html#6.4.4.1p5
static void finish_integer_constant(int is_decimal) {
    char *i = input;
    int is_long = 0;
    int is_unsigned = 0;

    while (1) {
        if (ip < input_size && (i[ip] == 'U' || i[ip] == 'u')) { is_unsigned = 1; ip++; }
        else if (ip < input_size && (i[ip] == 'L' || i[ip] == 'l')) { is_long = 1; ip++; }
        else break;
    }

    int may_not_be_unsigned = is_decimal && !is_unsigned;

    if (!is_unsigned && !is_long && cur_long >= 0 && cur_long < 0x80000000); // int
    else if (!may_not_be_unsigned && !is_long && cur_long >= 0 && cur_long <= 0xffffffff) is_unsigned = 1; // unsigned int
    else if (!is_unsigned && cur_long >= 0) is_long = 1; // long int
    else {
        // unsigned long
        if (warn_integer_constant_too_large && may_not_be_unsigned && cur_long < 0)
            printf("Warning: Integer literal doesn't fit in a signed long %lu\n", cur_long);
        is_unsigned = 1;
        is_long = 1;
    }

    cur_lexer_type = new_type(is_long ? TYPE_LONG : TYPE_INT);
    cur_lexer_type->is_unsigned = is_unsigned;
}

// Lexer. Lex a next token or TOK_EOF if the file is ended
void next() {
    char *i = input;

    while (ip < input_size) {
        char c1 = i[ip];
        char c2 = i[ip + 1];
        char c3 = ip >= 3 ? i[ip + 2] : 0;

        if (c1 == ' ' || c1 == '\t') { ip++; continue; }
        else if (c1 == '\n') { ip++; cur_line++; continue; }

        else if (c1 == '/' && input_size - ip >= 2 && c2 == '/') {
            ip += 2;
            while (i[ip++] != '\n');
            cur_line++;
            continue;
        }

        else if (                        c1 == '('                          )  { ip += 1;  cur_token = TOK_LPAREN;                     }
        else if (                        c1 == ')'                          )  { ip += 1;  cur_token = TOK_RPAREN;                     }
        else if (                        c1 == '['                          )  { ip += 1;  cur_token = TOK_LBRACKET;                   }
        else if (                        c1 == ']'                          )  { ip += 1;  cur_token = TOK_RBRACKET;                   }
        else if (                        c1 == '{'                          )  { ip += 1;  cur_token = TOK_LCURLY;                     }
        else if (                        c1 == '}'                          )  { ip += 1;  cur_token = TOK_RCURLY;                     }
        else if (                        c1 == ','                          )  { ip += 1;  cur_token = TOK_COMMA;                      }
        else if (                        c1 == ';'                          )  { ip += 1;  cur_token = TOK_SEMI;                       }
        else if (                        c1 == '?'                          )  { ip += 1;  cur_token = TOK_TERNARY;                    }
        else if (                        c1 == ':'                          )  { ip += 1;  cur_token = TOK_COLON;                      }
        else if (input_size - ip >= 2 && c1 == '&' && c2 == '&'             )  { ip += 2;  cur_token = TOK_AND;                        }
        else if (input_size - ip >= 2 && c1 == '|' && c2 == '|'             )  { ip += 2;  cur_token = TOK_OR;                         }
        else if (input_size - ip >= 2 && c1 == '=' && c2 == '='             )  { ip += 2;  cur_token = TOK_DBL_EQ;                     }
        else if (input_size - ip >= 2 && c1 == '!' && c2 == '='             )  { ip += 2;  cur_token = TOK_NOT_EQ;                     }
        else if (input_size - ip >= 2 && c1 == '<' && c2 == '='             )  { ip += 2;  cur_token = TOK_LE;                         }
        else if (input_size - ip >= 2 && c1 == '>' && c2 == '='             )  { ip += 2;  cur_token = TOK_GE;                         }
        else if (input_size - ip >= 2 && c1 == '+' && c2 == '+'             )  { ip += 2;  cur_token = TOK_INC;                        }
        else if (input_size - ip >= 2 && c1 == '-' && c2 == '-'             )  { ip += 2;  cur_token = TOK_DEC;                        }
        else if (input_size - ip >= 2 &&              c1 == '+' && c2 == '=')  { ip += 2;  cur_token = TOK_PLUS_EQ;                    }
        else if (input_size - ip >= 2 &&              c1 == '-' && c2 == '=')  { ip += 2;  cur_token = TOK_MINUS_EQ;                   }
        else if (input_size - ip >= 2 &&              c1 == '*' && c2 == '=')  { ip += 2;  cur_token = TOK_MULTIPLY_EQ;                }
        else if (input_size - ip >= 2 &&              c1 == '/' && c2 == '=')  { ip += 2;  cur_token = TOK_DIVIDE_EQ;                  }
        else if (input_size - ip >= 2 &&              c1 == '%' && c2 == '=')  { ip += 2;  cur_token = TOK_MOD_EQ;                     }
        else if (input_size - ip >= 2 &&              c1 == '&' && c2 == '=')  { ip += 2;  cur_token = TOK_BITWISE_AND_EQ;             }
        else if (input_size - ip >= 2 &&              c1 == '|' && c2 == '=')  { ip += 2;  cur_token = TOK_BITWISE_OR_EQ;              }
        else if (input_size - ip >= 2 &&              c1 == '^' && c2 == '=')  { ip += 2;  cur_token = TOK_BITWISE_XOR_EQ;             }
        else if (input_size - ip >= 3 && c1 == '>' && c2 == '>' && c3 == '=')  { ip += 3;  cur_token = TOK_BITWISE_RIGHT_EQ;           }
        else if (input_size - ip >= 3 && c1 == '<' && c2 == '<' && c3 == '=')  { ip += 3;  cur_token = TOK_BITWISE_LEFT_EQ;            }
        else if (input_size - ip >= 2 && c1 == '-' && c2 == '>'             )  { ip += 2;  cur_token = TOK_ARROW;                      }
        else if (input_size - ip >= 2 && c1 == '>' && c2 == '>'             )  { ip += 2;  cur_token = TOK_BITWISE_RIGHT;              }
        else if (input_size - ip >= 2 && c1 == '<' && c2 == '<'             )  { ip += 2;  cur_token = TOK_BITWISE_LEFT;               }
        else if (                        c1 == '+'                          )  { ip += 1;  cur_token = TOK_PLUS;                       }
        else if (                        c1 == '-'                          )  { ip += 1;  cur_token = TOK_MINUS;                      }
        else if (                        c1 == '*'                          )  { ip += 1;  cur_token = TOK_MULTIPLY;                   }
        else if (                        c1 == '/'                          )  { ip += 1;  cur_token = TOK_DIVIDE;                     }
        else if (                        c1 == '%'                          )  { ip += 1;  cur_token = TOK_MOD;                        }
        else if (                        c1 == '='                          )  { ip += 1;  cur_token = TOK_EQ;                         }
        else if (                        c1 == '<'                          )  { ip += 1;  cur_token = TOK_LT;                         }
        else if (                        c1 == '>'                          )  { ip += 1;  cur_token = TOK_GT;                         }
        else if (                        c1 == '!'                          )  { ip += 1;  cur_token = TOK_LOGICAL_NOT;                }
        else if (                        c1 == '~'                          )  { ip += 1;  cur_token = TOK_BITWISE_NOT;                }
        else if (                        c1 == '&'                          )  { ip += 1;  cur_token = TOK_ADDRESS_OF;                 }
        else if (                        c1 == '|'                          )  { ip += 1;  cur_token = TOK_BITWISE_OR;                 }
        else if (                        c1 == '^'                          )  { ip += 1;  cur_token = TOK_XOR;                        }
        else if (                        c1 == '#'                          )  { ip += 1;  cur_token = TOK_HASH;                       }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\t'",  4          )) { ip += 4;  cur_token = TOK_INTEGER; cur_long = '\t';   }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\n'",  4          )) { ip += 4;  cur_token = TOK_INTEGER; cur_long = '\n';   }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\''",  4          )) { ip += 4;  cur_token = TOK_INTEGER; cur_long = '\'';   }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\"'", 4          )) { ip += 4;  cur_token = TOK_INTEGER; cur_long = '\"';   }
        else if (input_size - ip >= 4 && !memcmp(i+ip, "'\\\\'", 4          )) { ip += 4;  cur_token = TOK_INTEGER; cur_long = '\\';   }
        else if (input_size - ip >= 3 && !memcmp(i+ip, "...",  3            )) { ip += 3;  cur_token = TOK_ELLIPSES;                   }

        else if (input_size - ip >= 3 && c1 == '\'' && i[ip+2] == '\'') { cur_long = i[ip+1]; ip += 3; cur_token = TOK_INTEGER; }

        // Identifier or keyword
        else if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_') {
            cur_token = TOK_IDENTIFIER;
            cur_identifier = malloc(1024);
            int j = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) {
                cur_identifier[j] = i[ip];
                j++; ip++;
            }
            cur_identifier[j] = 0;

                 if (!strcmp(cur_identifier, "if"           )) { cur_token = TOK_IF;        }
            else if (!strcmp(cur_identifier, "else"         )) { cur_token = TOK_ELSE;      }
            else if (!strcmp(cur_identifier, "signed"       )) { cur_token = TOK_SIGNED;    }
            else if (!strcmp(cur_identifier, "unsigned"     )) { cur_token = TOK_UNSIGNED;  }
            else if (!strcmp(cur_identifier, "char"         )) { cur_token = TOK_CHAR;      }
            else if (!strcmp(cur_identifier, "short"        )) { cur_token = TOK_SHORT;     }
            else if (!strcmp(cur_identifier, "int"          )) { cur_token = TOK_INT;       }
            else if (!strcmp(cur_identifier, "long"         )) { cur_token = TOK_LONG;      }
            else if (!strcmp(cur_identifier, "float"        )) { cur_token = TOK_FLOAT;     }
            else if (!strcmp(cur_identifier, "double"       )) { cur_token = TOK_DOUBLE;    }
            else if (!strcmp(cur_identifier, "void"         )) { cur_token = TOK_VOID;      }
            else if (!strcmp(cur_identifier, "struct"       )) { cur_token = TOK_STRUCT;    }
            else if (!strcmp(cur_identifier, "union"        )) { cur_token = TOK_UNION;     }
            else if (!strcmp(cur_identifier, "typedef"      )) { cur_token = TOK_TYPEDEF;   }
            else if (!strcmp(cur_identifier, "do"           )) { cur_token = TOK_DO;        }
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
            else if (!strcmp(cur_identifier, "extern"       )) { cur_token = TOK_EXTERN;    }
            else if (!strcmp(cur_identifier, "static"       )) { cur_token = TOK_STATIC;    }
            else if (!strcmp(cur_identifier, "const"        )) { cur_token = TOK_CONST;     }
            else if (!strcmp(cur_identifier, "volatile"     )) { cur_token = TOK_VOLATILE;  }
            else if (!strcmp(cur_identifier, "include"      )) { cur_token = TOK_INCLUDE;   }
            else if (!strcmp(cur_identifier, "define"       )) { cur_token = TOK_DEFINE;    }
            else if (!strcmp(cur_identifier, "undef"        )) { cur_token = TOK_UNDEF;     }
            else if (!strcmp(cur_identifier, "ifdef"        )) { cur_token = TOK_IFDEF;     }
            else if (!strcmp(cur_identifier, "endif"        )) { cur_token = TOK_ENDIF;     }


            for (j = 0; j < all_typedefs_count; j++) {
                if (!strcmp(all_typedefs[j]->identifier, cur_identifier)) {
                    cur_token = TOK_TYPEDEF_TYPE;
                    cur_lexer_type = dup_type(all_typedefs[j]->struct_type);
                }
            }
        }

        // Hex numeric literal
        else if (c1 == '0' && input_size - ip >= 2 && (i[ip+1] == 'x' || i[ip+1] == 'X')) {
            ip += 2;
            cur_token = TOK_INTEGER;
            cur_long = 0;
            while (((i[ip] >= '0' && i[ip] <= '9')  || (i[ip] >= 'A' && i[ip] <= 'F') || (i[ip] >= 'a' && i[ip] <= 'f')) && ip < input_size) {
                cur_long = cur_long * 16 + (
                    i[ip] >= 'a'
                    ? i[ip] - 'a' + 10
                    : i[ip] >= 'A'
                        ? i[ip] - 'A' + 10
                        : i[ip] - '0');
                ip++;
            }
            finish_integer_constant(0);
        }

        // Integer, octal and floating point literal
        else if ((c1 >= '0' && c1 <= '9') || (input_size - ip >= 2 && c1 == '.' && c2 >= '0' && c2 <= '9')) {
            // Note the current i and ip in case a floating point is lexed later on
            char *start = i;
            int start_ip = ip;

            int has_leading_zero = c1 == '0';
            long octal_integer = 0;
            long decimal_integer = 0;
            while ((i[ip] >= '0' && i[ip] <= '9') && ip < input_size) {
                octal_integer = octal_integer * 8  + (i[ip] - '0');
                decimal_integer = decimal_integer * 10 + (i[ip] - '0');
                ip++;
            }

            int is_floating_point = i[ip] == '.' || i[ip] == 'e' || i[ip] == 'E';

            if (!is_floating_point && has_leading_zero) {
                // It's an octal number
                cur_token = TOK_INTEGER;
                cur_long = octal_integer;
                finish_integer_constant(0);
            }
            else if (!is_floating_point) {
                // It's a decimal number
                cur_token = TOK_INTEGER;
                cur_long = decimal_integer;
                finish_integer_constant(1);
            }
            else {
                char *new_i;
                cur_long_double = strtold(start + start_ip, &new_i);
                ip = start_ip + new_i - start - start_ip;

                cur_token = TOK_FLOATING_POINT_NUMBER;

                int type = TYPE_DOUBLE;
                if (i[ip] == 'f' || i[ip] == 'F') { type = TYPE_FLOAT; ip++; }
                if (i[ip] == 'l' || i[ip] == 'L') { type = TYPE_LONG_DOUBLE; ip++; }

                cur_lexer_type = new_type(type);
            }
        }

        else if (c1 == '.') {
            ip += 1;
            cur_token = TOK_DOT;
        }

        // Ignore other CPP directives
        else if (c1 == '#') {
            while (i[ip++] != '\n');
            cur_line++;
            continue;
        }

        // String literal
        else if (i[ip] == '"') {
            cur_token = TOK_STRING_LITERAL;
            cur_string_literal = malloc(MAX_STRING_LITERAL_SIZE);
            int j = 0;
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
            if (j >= MAX_STRING_LITERAL_SIZE) panic1d("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
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
