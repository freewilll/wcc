import pytest
import subprocess
import tempfile
import re


def check_exit_code(code, expected_result):
    with tempfile.NamedTemporaryFile() as temp:
        with open(temp.name, 'w') as f:
            f.write(code)
        output = subprocess.check_output(["./wc4", f.name]).decode('utf-8')
        result = re.sub("exit ", "", str(output).split("\n")[-2])
        assert int(result) == expected_result


def check_output(code, expected_output):
    with tempfile.NamedTemporaryFile() as temp:
        with open(temp.name, 'w') as f:
            f.write(code)
        output = subprocess.check_output(["./wc4", f.name]).decode('utf-8')
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
    check_exit_code("int main(int argc, int argv) {return argc;}", 2)


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
    """, 8);


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
    """, "Hello world!\nexit 13\n")

    check_output("""
        int main(int argc, char **argv) {
            int a;
            a = 1;
            printf("1 + 1 = %d\n", a + 1);
        }
    """, "1 + 1 = 2\nexit 0\n")

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
        """, "1\n2\nexit 0\n");

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
    """, "1\n2\n1\n0\nexit 0\n");

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
    """, "0\n1\n2\n1\nexit 0\n");

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
        "8 0 8 0",
        "exit 0"
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
        "08 24 00",
        "exit 0"
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
    """, "1 2 3 4\nexit 0\n")
