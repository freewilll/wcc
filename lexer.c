#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

static char *input;             // Input file data
static char input_is_freeable;  // Does the inut need to be freed when done?
static int input_size;          // Size of the input file
static int ip;                  // Offset into *input

// Copies
static int           old_ip;
static int           old_cur_line;
static int           old_cur_token;
static Type*         old_cur_lexer_type;
static char*         old_cur_identifier;
static long          old_cur_long;
static long double   old_cur_long_double;
static StringLiteral old_cur_string_literal;

static void init_lexer(void) {
    ip = 0;
    cur_line = 1;
    cur_identifier = wmalloc(MAX_IDENTIFIER_SIZE);
    cur_string_literal.data = wmalloc(MAX_STRING_LITERAL_SIZE * 4);

    next();
}

void free_lexer(void) {
    free_and_null(cur_filename);
    free_and_null(cur_identifier);
    if (input_is_freeable) wfree(input);
    free_and_null(cur_string_literal.data);
}

void init_lexer_from_filename(char *filename) {
    FILE *f  = fopen(filename, "r");

    if (f == 0) {
        perror(filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    input_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    input = wmalloc(input_size + 1);
    input_is_freeable = 1;
    int read = fread(input, 1, input_size, f);
    if (read != input_size) {
        printf("Unable to read input file\n");
        exit(1);
    }

    input[input_size] = 0;
    fclose(f);

    cur_filename = wstrdup(filename);

    init_lexer();
}

void init_lexer_from_string(char *string) {
    input = string;
    input_size = strlen(string);
    input_is_freeable = 0;

    cur_filename = 0;

    init_lexer();
}

static void skip_whitespace(void) {
    char *i = input;

    while (ip < input_size) {
        if (i[ip] == ' ' || i[ip] == '\t' || i[ip] == '\f' || i[ip] == '\v' || i[ip] == '\r')
            ip++;
        else if (i[ip] == '\n') {
            ip++;
            cur_line++;
        }
        else return;
    }
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
            warning("Warning: Integer literal doesn't fit in a signed long %lu", cur_long);
        is_unsigned = 1;
        is_long = 1;
    }

    cur_lexer_type = new_type(is_long ? TYPE_LONG : TYPE_INT);
    cur_lexer_type->is_unsigned = is_unsigned;
}

void lex_non_hex_literal(void) {
    char *i = input;

    // Note the current i and ip in case a floating point is lexed later on
    char *start = i;
    int start_ip = ip;

    int has_leading_zero = i[ip] == '0';
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

static void lex_octal_literal(void) {
    char *i = input;

    cur_long = 0;
    int c = 0;
    while (i[ip] >= '0' && i[ip] <= '7') {
        cur_long = cur_long * 8 + i[ip] - '0';
        ip++;
        c++;
        if (c == 3) break;
    }
}

static void lex_hex_literal(void) {
    char *i = input;

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
}

void finish_string_literal(int size, int is_wide_char) {
    int *data = (int *) cur_string_literal.data;
    cur_token = TOK_STRING_LITERAL;

    cur_string_literal.is_wide_char = is_wide_char;
    cur_string_literal.size = size + 1;

    if (!is_wide_char)
        for (int i = 0; i <= size; i++) cur_string_literal.data[i] = data[i];
}

void lex_single_string_literal(int *size) {
    int *data = (int *) cur_string_literal.data;
    char *i = input;

    while (input_size - ip >= 1 && i[ip] != '"') {
        if (i[ip] != '\\') data[(*size)++] = i[ip++];
        else if (input_size - ip >= 2 && i[ip] == '\\') {
                 if (i[ip + 1] == '\'') { ip += 2; data[(*size)++] = '\''; }
            else if (i[ip + 1] == '"' ) { ip += 2; data[(*size)++] = '\"'; }
            else if (i[ip + 1] == '?' ) { ip += 2; data[(*size)++] = '?'; }
            else if (i[ip + 1] == '\\') { ip += 2; data[(*size)++] = '\\'; }
            else if (i[ip + 1] == 'a' ) { ip += 2; data[(*size)++] = 7; }
            else if (i[ip + 1] == 'b' ) { ip += 2; data[(*size)++] = 8; }
            else if (i[ip + 1] == 'f' ) { ip += 2; data[(*size)++] = 12; }
            else if (i[ip + 1] == 'n' ) { ip += 2; data[(*size)++] = 10; }
            else if (i[ip + 1] == 'r' ) { ip += 2; data[(*size)++] = 13; }
            else if (i[ip + 1] == 't' ) { ip += 2; data[(*size)++] = 9; }
            else if (i[ip + 1] == 'v' ) { ip += 2; data[(*size)++] = 11; }
            else if (i[ip + 1] == 'e' ) { ip += 2; data[(*size)++] = 27; }
            else if (i[ip + 1] >= '0' && i[ip + 1] <= '7' ) {
                ip++;
                lex_octal_literal();
                data[(*size)++] = cur_long & 0xff;
            }
            else if (i[ip + 1] == 'x' ) {
                ip += 2;
                lex_hex_literal();
                data[(*size)++] = cur_long & 0xff;
            }
            else error("Unknown \\ escape in string literal");
        }

        data[*size] = 0;
    }

    if (i[ip] != '"') error("Expecting terminating \" in string literal");
    ip++;

    if (*size >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);

    data[*size] = 0;
}

void lex_string_literal(void) {
    char *i = input;

    int is_wide_char = 0;
    int size = 0;

    while (ip < input_size && (i[ip] == '"') || (input_size - ip >= 2 && i[ip] == 'L' && i[ip + 1] == '"')) {
        if (i[ip] == 'L') {
            is_wide_char = 1;
            ip++;
        }

        ip += 1;
        lex_single_string_literal(&size);
        skip_whitespace();
    }

    finish_string_literal(size, is_wide_char);
}

// Lexer. Lex a next token or TOK_EOF if the file is ended
void next(void) {
    char *i = input;

    old_ip                 = ip;
    old_cur_line           = cur_line;
    old_cur_token          = cur_token;
    old_cur_lexer_type     = cur_lexer_type;
    old_cur_identifier     = cur_identifier;
    old_cur_long           = cur_long;
    old_cur_long_double    = cur_long_double;
    old_cur_string_literal = cur_string_literal;

    while (ip < input_size) {
        skip_whitespace();

        if (ip >= input_size) break;

        int left = input_size - ip;
        char c1 = i[ip];
        char c2 = i[ip + 1];
        char c3 = left>= 3 ? i[ip + 2] : 0;

        if (                  c1 == '('                          )  { ip += 1;  cur_token = TOK_LPAREN;                     }
        else if (             c1 == ')'                          )  { ip += 1;  cur_token = TOK_RPAREN;                     }
        else if (             c1 == '['                          )  { ip += 1;  cur_token = TOK_LBRACKET;                   }
        else if (             c1 == ']'                          )  { ip += 1;  cur_token = TOK_RBRACKET;                   }
        else if (             c1 == '{'                          )  { ip += 1;  cur_token = TOK_LCURLY;                     }
        else if (             c1 == '}'                          )  { ip += 1;  cur_token = TOK_RCURLY;                     }
        else if (             c1 == ','                          )  { ip += 1;  cur_token = TOK_COMMA;                      }
        else if (             c1 == ';'                          )  { ip += 1;  cur_token = TOK_SEMI;                       }
        else if (             c1 == '?'                          )  { ip += 1;  cur_token = TOK_TERNARY;                    }
        else if (             c1 == ':'                          )  { ip += 1;  cur_token = TOK_COLON;                      }
        else if (left >= 2 && c1 == '&' && c2 == '&'             )  { ip += 2;  cur_token = TOK_AND;                        }
        else if (left >= 2 && c1 == '|' && c2 == '|'             )  { ip += 2;  cur_token = TOK_OR;                         }
        else if (left >= 2 && c1 == '=' && c2 == '='             )  { ip += 2;  cur_token = TOK_DBL_EQ;                     }
        else if (left >= 2 && c1 == '!' && c2 == '='             )  { ip += 2;  cur_token = TOK_NOT_EQ;                     }
        else if (left >= 2 && c1 == '<' && c2 == '='             )  { ip += 2;  cur_token = TOK_LE;                         }
        else if (left >= 2 && c1 == '>' && c2 == '='             )  { ip += 2;  cur_token = TOK_GE;                         }
        else if (left >= 2 && c1 == '+' && c2 == '+'             )  { ip += 2;  cur_token = TOK_INC;                        }
        else if (left >= 2 && c1 == '-' && c2 == '-'             )  { ip += 2;  cur_token = TOK_DEC;                        }
        else if (left >= 2 && c1 == '+' && c2 == '='             )  { ip += 2;  cur_token = TOK_PLUS_EQ;                    }
        else if (left >= 2 && c1 == '-' && c2 == '='             )  { ip += 2;  cur_token = TOK_MINUS_EQ;                   }
        else if (left >= 2 && c1 == '*' && c2 == '='             )  { ip += 2;  cur_token = TOK_MULTIPLY_EQ;                }
        else if (left >= 2 && c1 == '/' && c2 == '='             )  { ip += 2;  cur_token = TOK_DIVIDE_EQ;                  }
        else if (left >= 2 && c1 == '%' && c2 == '='             )  { ip += 2;  cur_token = TOK_MOD_EQ;                     }
        else if (left >= 2 && c1 == '&' && c2 == '='             )  { ip += 2;  cur_token = TOK_BITWISE_AND_EQ;             }
        else if (left >= 2 && c1 == '|' && c2 == '='             )  { ip += 2;  cur_token = TOK_BITWISE_OR_EQ;              }
        else if (left >= 2 && c1 == '^' && c2 == '='             )  { ip += 2;  cur_token = TOK_BITWISE_XOR_EQ;             }
        else if (left >= 2 && c1 == '-' && c2 == '>'             )  { ip += 2;  cur_token = TOK_ARROW;                      }
        else if (left >= 3 && c1 == '>' && c2 == '>' && c3 == '=')  { ip += 3;  cur_token = TOK_BITWISE_RIGHT_EQ;           }
        else if (left >= 3 && c1 == '<' && c2 == '<' && c3 == '=')  { ip += 3;  cur_token = TOK_BITWISE_LEFT_EQ;            }
        else if (left >= 3 && c1 == '.' && c2 == '.' && c3 == '.')  { ip += 3;  cur_token = TOK_ELLIPSES;                   }
        else if (left >= 2 && c1 == '>' && c2 == '>'             )  { ip += 2;  cur_token = TOK_BITWISE_RIGHT;              }
        else if (left >= 2 && c1 == '<' && c2 == '<'             )  { ip += 2;  cur_token = TOK_BITWISE_LEFT;               }
        else if (             c1 == '+'                          )  { ip += 1;  cur_token = TOK_PLUS;                       }
        else if (             c1 == '-'                          )  { ip += 1;  cur_token = TOK_MINUS;                      }
        else if (             c1 == '*'                          )  { ip += 1;  cur_token = TOK_MULTIPLY;                   }
        else if (             c1 == '/'                          )  { ip += 1;  cur_token = TOK_DIVIDE;                     }
        else if (             c1 == '%'                          )  { ip += 1;  cur_token = TOK_MOD;                        }
        else if (             c1 == '='                          )  { ip += 1;  cur_token = TOK_EQ;                         }
        else if (             c1 == '<'                          )  { ip += 1;  cur_token = TOK_LT;                         }
        else if (             c1 == '>'                          )  { ip += 1;  cur_token = TOK_GT;                         }
        else if (             c1 == '!'                          )  { ip += 1;  cur_token = TOK_LOGICAL_NOT;                }
        else if (             c1 == '~'                          )  { ip += 1;  cur_token = TOK_BITWISE_NOT;                }
        else if (             c1 == '&'                          )  { ip += 1;  cur_token = TOK_AMPERSAND;                  }
        else if (             c1 == '|'                          )  { ip += 1;  cur_token = TOK_BITWISE_OR;                 }
        else if (             c1 == '^'                          )  { ip += 1;  cur_token = TOK_XOR;                        }

        // Hex numeric literal
        else if (c1 == '0' && input_size - ip >= 2 && (i[ip+1] == 'x' || i[ip+1] == 'X')) {
            ip += 2;
            cur_token = TOK_INTEGER;
            lex_hex_literal();
            finish_integer_constant(0);
        }

        // Decimal, octal and floating point literal
        else if ((c1 >= '0' && c1 <= '9') || (input_size - ip >= 2 && c1 == '.' && c2 >= '0' && c2 <= '9'))
            lex_non_hex_literal();

        else if (c1 == '.') {
            ip += 1;
            cur_token = TOK_DOT;
        }

        else if (i[ip] == '#') {
            // Lex # <num> "filename" ...

            ip++;
            skip_whitespace();

            lex_non_hex_literal();
            skip_whitespace();

            int size = 0;
            ip++; // Skip the "
            lex_single_string_literal(&size);
            finish_string_literal(size, 0);

            if (cur_filename) free_and_null(cur_filename);
            cur_filename = wstrdup(cur_string_literal.data);
            cur_line = cur_long - 1;

            while (ip < input_size && i[ip] != '\n') ip++; // Skip non-whitespace
            skip_whitespace(); // Skip EOL if present

            continue;
        }

        // Character literal
        else if ((c1 == '\'') || (input_size - ip >= 2 && c1 == 'L' && c2 == '\'')) {
            int is_wide_char = 0;
            if (c1 == 'L') {
                is_wide_char = 1;
                ip++;
            }

            cur_token = TOK_INTEGER;
            cur_lexer_type = new_type(TYPE_INT);
            cur_long = 0;
            ip += 1;

            int byte_count = 0;
            while (input_size - ip >= 1 && i[ip] != '\'') {
                if (i[ip] != '\\') { cur_long = (cur_long  << 8) + i[ip++]; }
                else if (i[ip] == '\\') {
                    ip++;
                         if (i[ip] == '\'') { ip++; cur_long = (cur_long << 8) +  '\''; }
                    else if (i[ip] == '"' ) { ip++; cur_long = (cur_long << 8) +  '\"'; }
                    else if (i[ip] == '?' ) { ip++; cur_long = (cur_long << 8) +  '?'; }
                    else if (i[ip] == '\\') { ip++; cur_long = (cur_long << 8) +  '\\'; }
                    else if (i[ip] == 'a' ) { ip++; cur_long = (cur_long << 8) +  7; }
                    else if (i[ip] == 'b' ) { ip++; cur_long = (cur_long << 8) +  8; }
                    else if (i[ip] == 'f' ) { ip++; cur_long = (cur_long << 8) +  12; }
                    else if (i[ip] == 'n' ) { ip++; cur_long = (cur_long << 8) +  10; }
                    else if (i[ip] == 'r' ) { ip++; cur_long = (cur_long << 8) +  13; }
                    else if (i[ip] == 't' ) { ip++; cur_long = (cur_long << 8) +  9; }
                    else if (i[ip] == 'v' ) { ip++; cur_long = (cur_long << 8) +  11; }
                    else if (i[ip] == 'e' ) { ip++; cur_long = (cur_long << 8) +  27; }
                    else if (i[ip] >= '0' && i[ip] <= '7' ) {
                        int old_cur_long = cur_long;
                        lex_octal_literal();
                        cur_long &= 0xff;
                        cur_long = (old_cur_long << 8) + cur_long;
                    }
                    else if (i[ip] == 'x' ) {
                        ip++;
                        unsigned int old_cur_long = cur_long;
                        lex_hex_literal();
                        int mask = (1 << (byte_count << 3)) - 1;

                        if (!is_wide_char) {
                            cur_long = ((old_cur_long & mask) << 8) + (cur_long & 0xff);

                            // Sign extend the first byte
                            if (byte_count == 0) cur_long = (int) ((char) cur_long);
                        }
                    }
                    else error("Unknown \\ escape in character literal");
                }
                byte_count++;
            }
            ip++;
        }

        // String literal
        else if ((c1 == '"') || (input_size - ip >= 2 && c1 == 'L' && c2 == '"')) {
            lex_string_literal();
        }

        // Identifier or keyword
        else if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_') {
            cur_token = TOK_IDENTIFIER;

            int j = 0;
            while (((i[ip] >= 'a' && i[ip] <= 'z') || (i[ip] >= 'A' && i[ip] <= 'Z') || (i[ip] >= '0' && i[ip] <= '9') || (i[ip] == '_')) && ip < input_size) {
                if (j == MAX_IDENTIFIER_SIZE) panic("Exceeded maximum identifier size %d", MAX_IDENTIFIER_SIZE);
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
            else if (!strcmp(cur_identifier, "switch"       )) { cur_token = TOK_SWITCH;    }
            else if (!strcmp(cur_identifier, "case"         )) { cur_token = TOK_CASE;      }
            else if (!strcmp(cur_identifier, "default"      )) { cur_token = TOK_DEFAULT;   }
            else if (!strcmp(cur_identifier, "return"       )) { cur_token = TOK_RETURN;    }
            else if (!strcmp(cur_identifier, "enum"         )) { cur_token = TOK_ENUM;      }
            else if (!strcmp(cur_identifier, "sizeof"       )) { cur_token = TOK_SIZEOF;    }
            else if (!strcmp(cur_identifier, "__attribute__")) { cur_token = TOK_ATTRIBUTE; }
            else if (!strcmp(cur_identifier, "packed"       )) { cur_token = TOK_PACKED;    }
            else if (!strcmp(cur_identifier, "__packed__"   )) { cur_token = TOK_PACKED;    }
            else if (!strcmp(cur_identifier, "aligned"      )) { cur_token = TOK_ALIGNED;   }
            else if (!strcmp(cur_identifier, "__aligned__"  )) { cur_token = TOK_ALIGNED;   }
            else if (!strcmp(cur_identifier, "__restrict"   )) { cur_token = TOK_RESTRICT;  }
            else if (!strcmp(cur_identifier, "restrict"     )) { cur_token = TOK_RESTRICT;  }
            else if (!strcmp(cur_identifier, "__asm__"      )) { cur_token = TOK_ASM;       }
            else if (!strcmp(cur_identifier, "inline"       )) { cur_token = TOK_INLINE;    }
            else if (!strcmp(cur_identifier, "__inline"     )) { cur_token = TOK_INLINE;    }
            else if (!strcmp(cur_identifier, "auto"         )) { cur_token = TOK_AUTO;      }
            else if (!strcmp(cur_identifier, "register"     )) { cur_token = TOK_REGISTER;  }
            else if (!strcmp(cur_identifier, "static"       )) { cur_token = TOK_STATIC;    }
            else if (!strcmp(cur_identifier, "extern"       )) { cur_token = TOK_EXTERN;    }
            else if (!strcmp(cur_identifier, "const"        )) { cur_token = TOK_CONST;     }
            else if (!strcmp(cur_identifier, "volatile"     )) { cur_token = TOK_VOLATILE;  }
            else if (!strcmp(cur_identifier, "goto"         )) { cur_token = TOK_GOTO;      }

            else {
                // Lookup typedef by first going through the short list of known typedefs
                // and if found, searching through the symbol table for it
                for (j = 0; j < all_typedefs_count; j++) {
                    if (!strcmp(all_typedefs[j]->identifier, cur_identifier)) {
                        Symbol *symbol = lookup_symbol(cur_identifier, cur_scope, 1);
                        if (symbol && symbol->type->type == TYPE_TYPEDEF) {
                            cur_token = TOK_TYPEDEF_TYPE;
                            cur_lexer_type = dup_type(symbol->type->target);
                            break;
                        }
                    }
                }
            }
        }

        else
            error("Unknown token %d", cur_token);

        return;
    }

    cur_token = TOK_EOF;
}

void rewind_lexer(void) {
    ip                 = old_ip;
    cur_line           = old_cur_line;
    cur_token          = old_cur_token;
    cur_lexer_type     = old_cur_lexer_type;
    cur_identifier     = old_cur_identifier;
    cur_long           = old_cur_long;
    cur_long_double    = old_cur_long_double;
    cur_string_literal = old_cur_string_literal;
}

void expect(int token, char *what) {
    if (cur_token != token) error("Expected %s", what);
}

void consume(int token, char *what) {
    expect(token, what);
    next();
}
