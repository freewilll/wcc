import pytest
import subprocess
import tempfile
import re


def check_exit_code(code, expected_result):
    with tempfile.NamedTemporaryFile() as temp:
        with open(temp.name, 'w') as f:
            f.write(code)
        output = subprocess.check_output(["./wc4", "-nc", f.name]).decode('utf-8')
        result = re.sub("exit ", "", str(output).split("\n")[-2])
        assert int(result) == expected_result


def check_output(code, expected_output, with_exit_code=False):
    with tempfile.NamedTemporaryFile() as temp:
        with open(temp.name, 'w') as f:
            f.write(code)
        args = ["./wc4", "-nc", f.name] if with_exit_code else ["./wc4", "-nc", "-ne", f.name]
        output = subprocess.check_output(args).decode('utf-8')
        assert output == expected_output


@pytest.mark.parametrize("expr,expected_result", [
    ("1",               1),
    ("1+2",             3),
    ("2-1",             1),
    ("3-2",             1),
    ("3-2-1",           0),
    ("3+2-1",           4),
    ("3-2+1",           2),
    ("1+2*3",           7),
    ("2*3+4",           10),
    ("2*3/2",           3),
    ("6/2",             3),
    ("6%2",             0),
    ("7%2",             1),
    ("1*2+3*4+5*10",    64),
    ("1*2+3*4+5*10+10", 74),
    ("(1+2)*3",         9),
    ("(2*3)+1",         7),
    ("3-(2+1)",         0),
    ("3-(2+1)*2",       -3),
    ("3-(2+1)*2+10",    7),
    ("-1",              -1),
    ("-1+2",            1),
    ("-1-1",            -2),
    # ("2--1",            3),  # FIXME Minus minus number is treated as pre decrement
    ("-(2*3)",          -6),
    ("-(2*3)+1",        -5),
    ("-(2*3)*2+1",      -11),

    ("1 == 1",          1),
    ("2 == 2",          1),
    ("1 == 0",          0),
    ("0 == 1",          0),
    ("1 != 1",          0),
    ("2 != 2",          0),
    ("1 != 0",          1),
    ("0 != 1",          1),

    ("!0",              1),
    ("!1",              0),
    ("!2",              0),

    ("~0",             -1),
    ("~1",             -2),
    ("~2",             -3),

    ("0 <  1",          1),
    ("0 <= 1",          1),
    ("0 >  1",          0),
    ("0 >= 1",          0),
    ("1 <  1",          0),
    ("1 <= 1",          1),
    ("1 >  1",          0),
    ("1 >= 1",          1),

    ("0 || 0",          0),
    ("0 || 1",          1),
    ("1 || 0",          1),
    ("1 || 1",          1),
    ("0 && 0",          0),
    ("0 && 1",          0),
    ("1 && 0",          0),
    ("1 && 1",          1),

    # Operator precedence tests
    ("1+2==3",          1),
    ("1+2>=3==1",       1),

    ("0 && 0 || 0",     0), # && binds more strongly than ||
    ("0 && 0 || 1",     1),
    ("0 && 1 || 0",     0),
    ("0 && 1 || 1",     1),
    ("1 && 0 || 0",     0),
    ("1 && 0 || 1",     1),
    ("1 && 1 || 0",     1),
    ("1 && 1 || 1",     1),

    ("0 + 0 && 0",      0), # + binds more strongly than &&
    ("0 + 0 && 1",      0),
    ("0 + 1 && 0",      0),
    ("0 + 1 && 1",      1),
    ("1 + 0 && 0",      0),
    ("1 + 0 && 1",      1),
    ("1 + 1 && 0",      0),
    ("1 + 1 && 1",      1),
])
def test_expr(expr, expected_result):
    check_exit_code("int main() {return %s;}" % expr, expected_result)


def test_argc_count():
    check_exit_code("int main(int argc, int argv) {return argc;}", 1)


def test_assignment():
    check_exit_code("""
        int g;
        int main(int argc, char **argv) {
            int a;
            int b;
            a = 1;
            b = 2;
            g = 3;
            return a+b+argc+g;
        }
    """, 7);


def test_function_calls():
    check_exit_code("""
        int g;

        int get_g() {
            return g;
        }

        int foo(int a) {
            return get_g() + a;
        }

        int main(int argc, char **argv) {
            g = 2;
            return foo(1);
        }

    """, 3);


def test_hello_world():
    check_output("""
        int main(int argc, char **argv) {
            return printf("Hello world!\n");
        }
    """, "Hello world!\nexit 13\n", with_exit_code=True)

    check_output("""
        int main(int argc, char **argv) {
            int a;
            a = 1;
            printf("1 + 1 = %d\n", a + 1);
        }
    """, "1 + 1 = 2\n")


def test_pointer_to_int():
    check_output("""
        int g;

        int main(int argc, char **argv) {
            int *pi;
            int **ppi;
            pi = &g;
            ppi = &pi;
            *pi = 1;
            printf("%d\n", g);
            **ppi = 2;
            printf("%d\n", g);
        }
        """, "1\n2\n");


def test_prefix_inc_dec():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            i = 0;
            printf("%d\n", ++i);
            printf("%d\n", ++i);
            printf("%d\n", --i);
            printf("%d\n", --i);
        }
    """, "1\n2\n1\n0\n");


def test_postfix_inc_dec():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            i = 0;
            printf("%d\n", i++);
            printf("%d\n", i++);
            printf("%d\n", i--);
            printf("%d\n", i--);
        }
    """, "0\n1\n2\n1\n");


def test_inc_dec_sizes():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            int *pi;
            int **ppi;
            char c;
            char *pc;
            char **ppc;

            i = 0;
            pi = 0;
            ppi = 0;
            c = 0;
            pc = 0;
            ppc = 0;

            i++;   printf("%d ", i);    i--;   printf("%d ", i);   ++i;   printf("%d ", i);    i--;   printf("%d\n", i);
            pi++;  printf("%d ", pi);   pi--;  printf("%d ", pi);  ++pi;  printf("%d ", pi);   pi--;  printf("%d\n", pi);
            ppi++; printf("%d ", ppi);  ppi--; printf("%d ", ppi); ++ppi; printf("%d ", ppi);  ppi--; printf("%d\n", ppi);
            c++;   printf("%d ", c);    c--;   printf("%d ", c);   ++c;   printf("%d ", c);    c--;   printf("%d\n", c);
            pc++;  printf("%d ", pc);   pc--;  printf("%d ", pc);  ++pc;  printf("%d ", pc);   pc--;  printf("%d\n", pc);
            ppc++; printf("%d ", ppc);  ppc--; printf("%d ", ppc); ++ppc; printf("%d ", ppc);  ppc--; printf("%d\n", ppc);
        }
    """, "\n".join([
        "1 0 1 0",
        "8 0 8 0",
        "8 0 8 0",
        "1 0 1 0",
        "1 0 1 0",
        "8 0 8 0"
    ]) + "\n");


def test_pointer_arithmetic():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            int *pi;
            int **ppi;
            char c;
            char *pc;
            char **ppc;

            i = 0;
            pi = 0;
            ppi = 0;
            c = 0;
            pc = 0;
            ppc = 0;

            i   = i   + 1; printf("%02d ", i);    i   = i   + 2; printf("%02d ", i);   i   = i   - 3; printf("%02d\n", i);    i   = i   - 1;
            pi  = pi  + 1; printf("%02d ", pi);   pi  = pi  + 2; printf("%02d ", pi);  pi  = pi  - 3; printf("%02d\n", pi);   pi  = pi  - 1;
            ppi = ppi + 1; printf("%02d ", ppi);  ppi = ppi + 2; printf("%02d ", ppi); ppi = ppi - 3; printf("%02d\n", ppi);  ppi = ppi - 1;
            c   = c   + 1; printf("%02d ", c);    c   = c   + 2; printf("%02d ", c);   c   = c   - 3; printf("%02d\n", c);    c   = c   - 1;
            pc  = pc  + 1; printf("%02d ", pc);   pc  = pc  + 2; printf("%02d ", pc);  pc  = pc  - 3; printf("%02d\n", pc);   pc  = pc  - 1;
            ppc = ppc + 1; printf("%02d ", ppc);  ppc = ppc + 2; printf("%02d ", ppc); ppc = ppc - 3; printf("%02d\n", ppc);  ppc = ppc - 1;
        }
    """, "\n".join([
        "01 03 00",
        "08 24 00",
        "08 24 00",
        "01 03 00",
        "01 03 00",
        "08 24 00"
    ]) + "\n");


def test_malloc():
    check_output("""
        int main(int argc, char **argv) {
            int *pi;
            pi = malloc(32);
            *pi = 1;
            *++pi = 2;
            *++pi = -1; // Gets overwritten
            *pi++ = 3;
            *pi = 4;
            printf("%d %d %d %d\n", *(pi - 3), *(pi - 2), *(pi - 1), *pi);
        }
    """, "1 2 3 4\n")


def test_char_pointer_arithmetic():
    check_output("""
        int main(int argc, char **argv) {
            char *pc;
            int *start;
            pc = malloc(4);
            start = pc;
            *pc++ = 'f';
            *pc++ = 'o';
            *pc++ = 'o';
            *pc++ = 0;
            printf("%s\n", start);
        }
    """, "foo\n")


def test_while():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            i = 0; while (i < 3) { printf("%d ", i); i++; }
            i = 0; while (i++ < 3) printf("%d ", i);
            printf("\n");
        }
    """, "0 1 2 1 2 3 \n")


def test_string_copy():
    check_output("""
        int main(int argc, char **argv) {
            char *src;
            char *dst;
            char *osrc;
            char *odst;
            int i;
            src = "foo";
            osrc = src;
            dst = malloc(4);
            odst = dst;
            while (*dst++ = *src++); // The coolest c code
            printf("%s=%s\n", osrc, odst);
        }
    """, "foo=foo\n")


def test_while_continue():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            i = 0;
            while (i++ < 5) {
                printf("%d", i);
                continue;
                printf("X", i);
            }
            printf("\n");
        }
    """, "12345\n")


def test_if_else():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            i = 0;
            if (i == 0) printf("1 zero\n");
            if (i != 0) printf("1 one\n");
            i = 1;
            if (i == 0) printf("2 zero\n");
            if (i != 0) printf("2 one\n");

            if (0) printf("4 if\n"); else printf("4 else\n");
            if (1) printf("5 if\n"); else printf("5 else\n");
        }
    """, "\n".join([
        "1 zero",
        "2 one",
        "4 else",
        "5 if"
    ]) + "\n");


def test_and_or_shortcutting():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            i = 0;
            i == 0 || printf("|| with 0\n");
            i == 1 || printf("|| with 0\n");
            i == 0 && printf("&& with 0\n");
            i == 1 && printf("&& with 0\n");
        }
    """, "\n".join([
        "|| with 0",
        "&& with 0"
    ]) + "\n");


def test_sizeof():
    check_output("""
        int main(int argc, char **argv) {
            printf("%lu ", sizeof(int));
            printf("%lu ", sizeof(long));
            printf("%lu ", sizeof(char));
            printf("%lu ", sizeof(void *));
            printf("%lu ", sizeof(int *));
            printf("%lu ", sizeof(long *));
            printf("%lu ", sizeof(char *));
            printf("%lu ", sizeof(int **));
            printf("%lu ", sizeof(long **));
            printf("%lu ", sizeof(char **));
            printf("\n");
        }
    """, "8 8 1 8 8 8 8 8 8 8 \n")


def test_ternary():
    check_output("""
        int main(int argc, char **argv) {
            printf("%d %d %s\n",
                (0 ? 1 : 2),
                (1 ? 1 : 2),
                (1 ? "foo" : "bar")
            );
        }
    """, "2 1 foo\n")


def test_bracket_lookup():
    check_output("""
        int main(int argc, char **argv) {
            long *pi;
            long *opi;
            pi = malloc(3 * sizeof(long));
            opi  =pi;
            pi[0] = 1;
            pi[1] = 2;
            pi = pi + 3;
            pi[-1] = 3;
            printf("%ld %ld %ld\n", *opi, *(opi + 1), *(opi + 2));
        }
    """, "1 2 3\n")


def test_casting():
    check_output("""
        int main(int argc, char **argv) {
            int *pi;
            int *pj;
            pi = 0;
            pj = pi + 1;
            printf("%ld\n", (long) pj - (long) pi);
            pj = (int *) (((char *) pi) + 1);
            printf("%ld\n", (long) pj - (long) pi);
        }
    """, "8\n1\n")


def test_local_comma_var_declarations():
    check_output("""
        int main(int argc, char **argv) {
            int i, *pi;
            i = 1;
            pi = &i;
            printf("%d ", *pi);
            *pi = 2;
            printf("%d\n", *pi);
        }
    """, "1 2\n")


def test_global_comma_var_declarations():
    check_output("""
        int i, j;

        int main() {
            i = 1;
            j = 2;
            printf("%d %d\n", i, j);
        }
    """, "1 2\n")


def test_free():
    check_output("""
        int main(int argc, char **argv) {
            int *pi;
            pi = malloc(17);
            free(pi);
        }
    """, "")


def test_mem_functions():
    check_output("""
        int main(int argc, char **argv) {
            long *pi1, *pi2;

            pi1 = malloc(32);
            pi2 = malloc(32);
            memset(pi1, 0, 32);
            printf("%ld %ld ", pi1[0], pi1[3]);
            memset(pi1, -1, 32);
            printf("%ld %ld\n", pi1[0], pi1[3]);
            printf("%ld\n", memcmp(pi1, pi2, 32));

            printf("%d ",  strcmp("foo", "foo"));
            printf("%d ",  strcmp("foo", "aaa"));
            printf("%d\n", strcmp("foo", "ggg"));
        }
    """, "\n".join([
        "0 0 -1 -1",
        "255",
        "0 5 -1"
    ]) + "\n");


def test_open_read_write_close():
    with tempfile.NamedTemporaryFile() as input_file:
        with open(input_file.name, 'w') as f:
            f.write("foo\n")
            f.flush()

            with tempfile.NamedTemporaryFile(delete=False) as output_file:
                check_output("""
                    int main(int argc, char **argv) {
                        int f;
                        char *data;
                        data = malloc(16);
                        f = open("%s", 0, 0);
                        printf("%%zd\n", read(f, data, 16));
                        close(f);

                        f = open("%s", 577, 420);
                        if (f < 0) { printf("bad FH\\n"); exit(1); }
                        write(f, "foo\n", 4);
                        close(f);
                    }
                """ % (input_file.name, output_file.name), "4\n")

                with open(output_file.name) as f:
                    assert f.read() == "foo\n"

def test_plus_equals():
    check_output("""
        int main(int argc, char **argv) {
            int i, *pi;
            i = 0;
            pi = 0;
            i += 2;
            pi += 2;
            printf("%d %ld ", i, (long) pi);
            pi -= 3;
            printf("%d %ld\n", i, (long) pi);
        }
    """, "2 16 2 -8\n")


def test_exit():
    check_output("""int main(int argc, char **argv) { exit(3); }""", "exit 3\n", with_exit_code=True)


def test_cast_in_function_call():
    # Test precedence error combined with expression parser not stopping at unknown tokens

    check_output("""
        int main(int argc, char **argv) {
            char *pi;
            pi = "foo";
            return strcmp((char *) pi, "foo");
        }
    """, "")


def test_array_lookup_of_string_literal():
    check_output("""
        int main() {
            printf("%.3s ", &"foobar"[0]);
            printf("%.3s\n", &"foobar"[3]);
        }
    """, "foo bar\n")


def test_nested_while_continue():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            char *s;
            i = 0;
            s = "foo";

            while (s[i++] != 0) {
                printf("%d ", i);
                while (0);
                continue;
            }
            printf("\n");
        }
    """, "1 2 3 \n")


def test_func_returns_are_lvalues():
    check_exit_code("""
        int foo() {
            int t;
            t = 1;
            return t;
        }

        int main(int argc, char **argv) {
            int i;
            i = foo();
            return i;
        }
    """, 1)


def test_bad_or_and_stack_consumption():
    check_exit_code("""
        int main(int argc, char **argv) {
            long ip;
            ip = 0;
            while ((1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1) && ip < 2) {
                ip += 1;
            }
        }
    """, 0)


def test_double_deref_assign_with_cast():
    check_output("""
        int main(int argc, char **argv) {
            long i, a, *sp, *stack;
            stack = malloc(32);
            sp = stack;
            i = 10;
            a = 20;
            *sp = (long) &i;
            *(long *) *sp++ = a;
                printf("%ld\n", i);

        }
    """, "20\n")


def test_double_assign():
    check_output("""
        int main(int argc, char **argv) {
            long a, b;
            a = b = 1;
            printf("%ld %ld\n", a, b);
        }
    """, "1 1\n")


def test_print_assignment_with_one_arg():
    check_output("""
        int main() {
            long a, b;
            a = printf("%d ", 1);
            printf("%ld\n", a);
            a = b = printf("%d x ", 2);
            printf("%ld %ld\n", a, b);
        }
    """, "1 2\n2 x 4 4\n")


def test_int_char_interbreeding():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            char *c;
            c = (char *) &i;
            i = 1 + 256 * 2 + 256 * 256 + 3 + 256 * 256 * 256 * 4;;
            printf("%d ", *c);
            printf("%d ", i);
            *c = 5;
            printf("%d\n", i);
        }
    """, "4 67174916 67174917\n")


def test_first_arg_to_or_and_and_must_be_rvalue():
    check_output("""
        int main(int argc, char **argv) {
            long *sp;
            long a;
            sp = malloc(32);
            *sp = 0;
            a = 1;
            a = *(sp++) || a;
            printf("%ld\n", a);
        }
    """, "1\n")


def test_enum():
    check_output("""
        enum {A, B};
        enum {C=2, D};

        int main(int argc, char **argv) {
            printf("%d %d %d %d\n", A, B, C, D);
        }
    """, "0 1 2 3\n")
