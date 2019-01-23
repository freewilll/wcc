import pytest
import subprocess
import tempfile
import re


def check_exit_code(code, expected_result):
    with tempfile.NamedTemporaryFile(suffix=".c") as temp_c_file:
        with open(temp_c_file.name, 'w') as f:
            f.write(code)
        subprocess.check_output(["./wc4", f.name]).decode('utf-8')
        temp_s_filename = re.sub(r"\.c", ".s", temp_c_file.name)
        temp_executable_filename = re.sub(r"\.s", "", temp_s_filename)
        output = subprocess.check_output(["gcc", temp_s_filename, "-o", temp_executable_filename])
        if expected_result < 0:
            expected_result = 256 + expected_result;
        expected_result = expected_result & 0xff
        assert subprocess.run(temp_executable_filename, check=False).returncode == expected_result


def check_output(code, expected_output, exit_code=None):
    with tempfile.NamedTemporaryFile(suffix=".c", delete=False) as temp_c_file:
        with open(temp_c_file.name, 'w') as f:
            f.write(code)
        subprocess.check_output(["./wc4", f.name]).decode('utf-8')
        temp_s_filename = re.sub(r"\.c", ".s", temp_c_file.name)
        temp_executable_filename = re.sub(r"\.s", "", temp_s_filename)
        output = subprocess.check_output(["gcc", temp_s_filename, "-o", temp_executable_filename])
        result = subprocess.run([temp_executable_filename], check=False, stdout=subprocess.PIPE)
        if exit_code:
            assert result.returncode == exit_code
        assert result.stdout.decode('utf-8') == expected_output


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
    ("2- -1",            3),
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

    ("3 & 5",           1),
    ("3 | 5",           7),
    ("3 ^ 5",           6),

    ("1 << 2",                         4),
    ("1 << 3",                         8),
    ("1 << 31",               2147483648),
    ("1 << 63",     -9223372036854775808),
    ("256 >> 2",                      64),
    ("256 >> 3",                      32),
    ("8192 >> 8",                     32),

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

    ("0 ==  1  & 0",    0),
    ("1 &   1  ^ 3",    2),
    ("1 ^   1  | 1",    1),

    ("1 + 1 << 4",     32), # + binds more strongly than <<
    ("1 + 16 >> 3",     2), # + binds more strongly than >>

    # Hex
    ("0x0",             0),
    ("0x1",             1),
    ("0x9",             9),
    ("0xa",            10),
    ("0xf",            15),
    ("0x10",           16),
    ("0x40",           64),
    ("0xff",          255),
    ("0x100",         256),
])
def test_expr(expr, expected_result):
    check_output("""
        int main() {
            printf("%%ld", %s);
        }
    """ % expr, str(expected_result))


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
            return g + a + b + argc;
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


def test_split_function_declaration_and_definition():
    check_output("""
        int foo();
        int bar(int i);

        int foo() {
            return 1;
        }

        int bar(int i) {
            return i;
        }

        int main(int argc, char **argv) {
            printf("%d %d %d\n", foo(), bar(1), bar(2));
        }
    """, "1 1 2\n", 0)


def test_split_function_declaration_and_definition_backpatching():
    check_output("""
        void foo();
        void bar();

        int main(int argc, char **argv) {
            foo();
            bar();
            foo();
            bar();
        }

        void foo() { printf("foo\n"); }
        void bar() { printf("bar\n"); }
    """, "foo\nbar\nfoo\nbar\n", 0)


def test_hello_world():
    check_output("""
        int main(int argc, char **argv) {
            return printf("Hello world!\\n");
        }
    """, "Hello world!\n", exit_code=13)

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
            int i;
            int *pi;
            int **ppi;
            pi = &g;
            ppi = &pi;
            *pi = 1;
            printf("%d\n", g);
            **ppi = 2;
            printf("%d\n", g);
            printf("%d\n", **ppi);

            pi = &i;
            *pi = 3;
            printf("%d\n", i);
            **ppi = 4;
            printf("%d\n", i);
        }
        """, "1\n2\n2\n3\n4\n");


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
            char c;
            char *pc;
            char **ppc;
            short s;
            short *ps;
            short **pps;
            int i;
            int *pi;
            int **ppi;
            long l;
            long *pl;
            long **ppl;

            c = 0;
            pc = 0;
            ppc = 0;
            s = 0;
            ps = 0;
            pps = 0;
            i = 0;
            pi = 0;
            ppi = 0;
            l = 0;
            pl = 0;
            ppl = 0;

            c++;   printf("%d ", c);    c--;   printf("%d ", c);   ++c;   printf("%d ", c);    c--;   printf("%d\n", c);
            pc++;  printf("%d ", pc);   pc--;  printf("%d ", pc);  ++pc;  printf("%d ", pc);   pc--;  printf("%d\n", pc);
            ppc++; printf("%d ", ppc);  ppc--; printf("%d ", ppc); ++ppc; printf("%d ", ppc);  ppc--; printf("%d\n", ppc);

            s++;   printf("%d ", s);    s--;   printf("%d ", s);   ++s;   printf("%d ", s);    s--;   printf("%d\n", s);
            ps++;  printf("%d ", ps);   ps--;  printf("%d ", ps);  ++ps;  printf("%d ", ps);   ps--;  printf("%d\n", ps);
            pps++; printf("%d ", pps);  pps--; printf("%d ", pps); ++pps; printf("%d ", pps);  pps--; printf("%d\n", pps);

            i++;   printf("%d ", i);    i--;   printf("%d ", i);   ++i;   printf("%d ", i);    i--;   printf("%d\n", i);
            pi++;  printf("%d ", pi);   pi--;  printf("%d ", pi);  ++pi;  printf("%d ", pi);   pi--;  printf("%d\n", pi);
            ppi++; printf("%d ", ppi);  ppi--; printf("%d ", ppi); ++ppi; printf("%d ", ppi);  ppi--; printf("%d\n", ppi);

            l++;   printf("%d ", l);    l--;   printf("%d ", l);   ++l;   printf("%d ", l);    l--;   printf("%d\n", l);
            pl++;  printf("%d ", pl);   pl--;  printf("%d ", pl);  ++pl;  printf("%d ", pl);   pl--;  printf("%d\n", pl);
            ppl++; printf("%d ", ppl);  ppl--; printf("%d ", ppl); ++ppl; printf("%d ", ppl);  ppl--; printf("%d\n", ppl);
        }
    """, "\n".join([
        "1 0 1 0",
        "1 0 1 0",
        "8 0 8 0",
        "1 0 1 0",
        "2 0 2 0",
        "8 0 8 0",
        "1 0 1 0",
        "4 0 4 0",
        "8 0 8 0",
        "1 0 1 0",
        "8 0 8 0",
        "8 0 8 0",
    ]) + "\n");


def test_pointer_arithmetic():
    check_output("""
        int main(int argc, char **argv) {
            char c;
            char *pc;
            char **ppc;
            int s;
            int *ps;
            int **pps;
            int i;
            int *pi;
            int **ppi;
            int l;
            int *pl;
            int **ppl;

            c = 0;
            pc = 0;
            ppc = 0;
            s = 0;
            ps = 0;
            pps = 0;
            i = 0;
            pi = 0;
            ppi = 0;
            l = 0;
            pl = 0;
            ppl = 0;

            c   = c   + 1; printf("%02d ", c);    c   = c   + 2; printf("%02d ", c);   c   = c   - 3; printf("%02d\n", c);    c   = c   - 1;
            pc  = pc  + 1; printf("%02d ", pc);   pc  = pc  + 2; printf("%02d ", pc);  pc  = pc  - 3; printf("%02d\n", pc);   pc  = pc  - 1;
            ppc = ppc + 1; printf("%02d ", ppc);  ppc = ppc + 2; printf("%02d ", ppc); ppc = ppc - 3; printf("%02d\n", ppc);  ppc = ppc - 1;
            s   = s   + 1; printf("%02d ", s);    s   = s   + 2; printf("%02d ", s);   s   = s   - 3; printf("%02d\n", s);    s   = s   - 1;
            ps  = ps  + 1; printf("%02d ", ps);   ps  = ps  + 2; printf("%02d ", ps);  ps  = ps  - 3; printf("%02d\n", ps);   ps  = ps  - 1;
            pps = pps + 1; printf("%02d ", pps);  pps = pps + 2; printf("%02d ", pps); pps = pps - 3; printf("%02d\n", pps);  pps = pps - 1;
            i   = i   + 1; printf("%02d ", i);    i   = i   + 2; printf("%02d ", i);   i   = i   - 3; printf("%02d\n", i);    i   = i   - 1;
            pi  = pi  + 1; printf("%02d ", pi);   pi  = pi  + 2; printf("%02d ", pi);  pi  = pi  - 3; printf("%02d\n", pi);   pi  = pi  - 1;
            ppi = ppi + 1; printf("%02d ", ppi);  ppi = ppi + 2; printf("%02d ", ppi); ppi = ppi - 3; printf("%02d\n", ppi);  ppi = ppi - 1;
            l   = l   + 1; printf("%02d ", l);    l   = l   + 2; printf("%02d ", l);   l   = l   - 3; printf("%02d\n", l);    l   = l   - 1;
            pl  = pl  + 1; printf("%02d ", pl);   pl  = pl  + 2; printf("%02d ", pl);  pl  = pl  - 3; printf("%02d\n", pl);   pl  = pl  - 1;
            ppl = ppl + 1; printf("%02d ", ppl);  ppl = ppl + 2; printf("%02d ", ppl); ppl = ppl - 3; printf("%02d\n", ppl);  ppl = ppl - 1;
        }
    """, "\n".join([
        "01 03 00",
        "01 03 00",
        "08 24 00",
        "01 03 00",
        "04 12 00",
        "08 24 00",
        "01 03 00",
        "04 12 00",
        "08 24 00",
        "01 03 00",
        "04 12 00",
        "08 24 00",
    ]) + "\n");


def test_integer_sizes():
    check_output("""
        int main() {
            int i;
            char *data;

            data = malloc(8);

            printf("%ld ", sizeof(void));
            printf("%ld ", sizeof(char));
            printf("%ld ", sizeof(short));
            printf("%ld ", sizeof(int));
            printf("%ld ", sizeof(long));
            printf("%ld ", sizeof(void *));
            printf("%ld ", sizeof(char *));
            printf("%ld ", sizeof(short *));
            printf("%ld ", sizeof(int *));
            printf("%ld ", sizeof(long *));
            printf("%ld ", sizeof(int **));
            printf("%ld ", sizeof(char **));
            printf("%ld ", sizeof(short **));
            printf("%ld ", sizeof(int **));
            printf("%ld ", sizeof(long **));
            printf("\n\n");

            memset(data, -1, 8); *((char  *) data) = 1; printf("%016lx\n", *((long *) data));
            memset(data, -1, 8); *((short *) data) = 1; printf("%016lx\n", *((long *) data));
            memset(data, -1, 8); *((int   *) data) = 1; printf("%016lx\n", *((long *) data));
            memset(data, -1, 8); *((long  *) data) = 1; printf("%016lx\n", *((long *) data));
            printf("\n");

            memset(data, 1, 8);
            printf("%016lx\n", *((char  *) data));
            printf("%016lx\n", *((short *) data));
            printf("%016lx\n", *((int   *) data));
            printf("%016lx\n", *((long  *) data));
        }
    """, "\n".join([
        "1 1 2 4 8 8 8 8 8 8 8 8 8 8 8 ",
        "",
        "ffffffffffffff01",
        "ffffffffffff0001",
        "ffffffff00000001",
        "0000000000000001",
        "",
        "0000000000000001",
        "0000000000000101",
        "0000000001010101",
        "0101010101010101",
    ]) + "\n", 0)


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

@pytest.mark.xfail() # TODO
def test_while():
    check_output("""
        int main(int argc, char **argv) {
            int i;
            i = 0; while (i < 3) { printf("%d ", i); i++; }
            i = 0; while (i++ < 3) printf("%d ", i);
            printf("\n");
        }
    """, "0 1 2 1 2 3 \n")


@pytest.mark.xfail() # TODO
def test_for():
    check_output("""
        int main() {
            int i;

            for (i = 0; i < 10; i++) printf("%d ", i);
            printf("\n");

            for (i = 0; i < 10; i++) {
                if (i == 5) continue;
                printf("%d ", i);
            }
            printf("\n");
        }
    """, "0 1 2 3 4 5 6 7 8 9 \n0 1 2 3 4 6 7 8 9 \n", 0)

@pytest.mark.xfail() # TODO
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

@pytest.mark.xfail() # TODO
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
    """, "4\n1\n")


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


def test_assign_operation():
    check_output("""
        int main(int argc, char **argv) {
            char c, *pc;
            short s, *ps;
            int i, *pi;
            long l, *pl;
            c = 0; pc = 0; s = 0; ps = 0; i = 0; pi = 0; l = 0; pl = 0;
            c += 2; pc += 2;
            s += 2; ps += 2;
            i += 2; pi += 2;
            l += 2; pl += 2;
            printf("%d %ld %d %ld %d %ld %ld %ld\n", c, (long) pc, s, (long) ps, i, (long) pi, l, (long) pl);
            pc -= 3; ps -= 3; pi -= 3; pl -= 3;
            printf("%d %ld %d %ld %d %ld %ld %ld\n", c, (long) pc, s, (long) ps, i, (long) pi, l, (long) pl);
        }
    """, "2 2 2 4 2 8 2 16\n2 -1 2 -2 2 -4 2 -8\n")


def test_exit():
    check_output("""int main(int argc, char **argv) { exit(3); }""", "", exit_code=3)


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

@pytest.mark.xfail() # TODO
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

@pytest.mark.xfail() # TODO
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


@pytest.mark.parametrize("type, size", [
    ("char",  3 * 1),
    ("short", 3 * 2),
    ("int",   3 * 4),
    ("long",  3 * 8),
])
def test_simple_struct(type, size):
    check_output("""
        struct s {
            %s i, j;
            %s k;
        };

        struct s* gs;

        int main(int argc, char **argv) {
            struct s *s1, *s2;
            s1 = malloc(sizeof(struct s));
            s2 = malloc(sizeof(struct s));
            gs = malloc(sizeof(struct s));

            (*s1).i = 1;
            (*s1).j = 2;
            (*s2).i = 3;
            (*s2).j = 4;
            printf("%%d %%d %%d %%d\n", (*s1).i, (*s1).j, (*s2).i, (*s2).j);

            s1->i = -1;
            s1->j= -2;
            printf("%%d %%d\n", s1->i, s1->j);

            gs->i = 10;
            gs->j = 20;
            printf("%%d %%d\n", gs->i, gs->j);

            printf("%%ld\n", sizeof(struct s));
        }
    """ % (type, type), "1 2 3 4\n-1 -2\n10 20\n%d\n" % size, 0)


def test_struct_member_alignment():
    check_output("""
        struct sc1 { char  c1;           };
        struct sc2 { char  c1; char c2;  };
        struct ss1 { short c1;           };
        struct ss2 { short c1; short s1; };
        struct si1 { int   c1;           };
        struct si2 { int   c1; int i1;   };
        struct sl1 { long  c1;           };
        struct sl2 { long  c1; long l1;  };

        struct cc { char c1; char  c2; };
        struct cs { char c1; short s1; };
        struct ci { char c1; int   i1; };
        struct cl { char c1; long  l1; };

        struct ccc { char c1; char  c2; char c3; };
        struct csc { char c1; short c2; char c3; };
        struct cic { char c1; int   c2; char c3; };
        struct clc { char c1; long  c2; char c3; };

        int main(int argc, char **argv) {
            struct cc *vc;
            struct cs *vs;
            struct ci *vi;
            struct cl *vl;

            printf("%ld %ld ",  sizeof(struct sc1), sizeof(struct sc2));
            printf("%ld %ld ",  sizeof(struct ss1), sizeof(struct ss2));
            printf("%ld %ld ",  sizeof(struct si1), sizeof(struct si2));
            printf("%ld %ld\n", sizeof(struct sl1), sizeof(struct sl2));

            printf("%ld %ld %ld %ld\n", sizeof(struct cc),  sizeof(struct cs),  sizeof(struct ci),  sizeof(struct cl));
            printf("%ld %ld %ld %ld\n", sizeof(struct ccc), sizeof(struct csc), sizeof(struct cic), sizeof(struct clc));

            vc = 0;
            vs = 0;
            vi = 0;
            vl = 0;
            printf("%ld %ld %ld %ld\n",
                (long) &(vc->c2) - (long) &(vc->c1),
                (long) &(vs->s1) - (long) &(vs->c1),
                (long) &(vi->i1) - (long) &(vi->c1),
                (long) &(vl->l1) - (long) &(vl->c1)
            );
        }
    """, "1 2 2 4 4 8 8 16\n2 4 8 16\n3 6 12 24\n1 2 4 8\n", 0)


def test_struct_alignment_bug():
    check_output("""
        struct s {
            int    i1;
            long   l1;
            int    i2;
            short  s1;
        };

        int main() {
            struct s *eh;

            printf("%ld ", (long) &(eh->l1) - (long) eh);
            printf("%ld ", (long) &(eh->i2) - (long) eh);
            printf("%ld ", (long) &(eh->s1) - (long) eh);
            printf("%ld\n", sizeof(struct s));
        }
    """, "8 16 20 24\n", 0)


def test_nested_struct():
    check_output("""
        struct s1 {
            int i, j;
        };

        struct s2 {
            struct s1 *s;
        };

        int main(int argc, char **argv) {
            struct s1 *s1;
            struct s2 *s2;

            s1 = malloc(sizeof(struct s1));
            s2 = malloc(sizeof(struct s2));
            s2->s = s1;
            s1->i = 1;
            s1->j = 2;
            printf("%d %d\n", s2->s->i, s2->s->j);
            printf("%ld %ld\n", sizeof(struct s1), sizeof(struct s2));

            s2->s->i = 3;
            s2->s->j = 4;
            printf("%d %d ", s1->i, s1->j);
            printf("%d %d\n", s2->s->i, s2->s->j);
        }
    """, "1 2\n8 8\n3 4 3 4\n", 0)


def test_function_returning_a_pointer_to_a_struct():
    check_output("""
        struct s {
            int i, j;
        };

        struct s *foo() {
            struct s *s;

            s = malloc(sizeof(struct s));
            s->i = 1;
            s->j = 2;
            return s;
        }

        int main(int argc, char **argv) {
            struct s *s;
            s = foo();
            printf("%d %d\n", s->i, s->j);
        }
    """, "1 2\n", 0)


def test_function_with_a_pointer_to_a_struct_argument():
    check_output("""
        struct s {
            int i, j;
        };

        int foo(struct s *s) {
            return s->i + s->j;
        }

        int main(int argc, char **argv) {
            struct s *s;

            s = malloc(sizeof(struct s));
            s->i = 1;
            s->j = 2;

            printf("%d\n", foo(s));
        }
    """, "3\n", 0)


def test_struct_additions():
    check_output("""
        struct s {
            long i, j;
        };

        int main(int argc, char **argv) {
            struct s *s;

            s = 0;
            s++;

            printf("%ld ", s);
            printf("%ld ", s + 1);
            printf("%ld ", s++);
            printf("%ld\n", ++s);
        }
    """, "16 32 16 48\n", 0)


def test_struct_casting():
    check_output("""
        struct s {int i;};

        int main() {
            int i;
            struct s *s;

            i = 1;
            s = (struct s*) &i;
            printf("%d\n", s->i);
        }
    """, "1\n", 0)



def test_packed_struct():
    check_output("""
        struct                              s1 {int i; char c; int j;};
        struct __attribute__ ((__packed__)) s2 {int i; char c; int j;};
        struct __attribute__ ((packed))     s3 {int i; char c; int j;};

        int main() {
            struct s1 *s1;
            struct s2 *s2;
            struct s3 *s3;

            printf("%ld %ld %ld\n", sizeof(struct s1), (long) &(s1->c) - (long) s1, (long) &(s1->j) - (long) s1);
            printf("%ld %ld %ld\n", sizeof(struct s2), (long) &(s2->c) - (long) s2, (long) &(s2->j) - (long) s2);
            printf("%ld %ld %ld\n", sizeof(struct s3), (long) &(s3->c) - (long) s3, (long) &(s3->j) - (long) s3);
        }
    """, "12 4 8\n9 4 5\n9 4 5\n", 0)


def test_unary_precedence():
    check_output("""
        struct s {
            int i, j;
        };

        struct fwd_function_backpatch {
            long *iptr;
            long *symbol;
        };

        struct fwd_function_backpatch *fwd_function_backpatches;

        int main(int argc, char **argv) {
            struct s *s;
            s = malloc(sizeof(struct s));
            s->i = 0;
            printf("%d\n", !s[0].i);
            printf("%d\n", ~s[0].i);
            printf("%d\n", -s[0].i);
            printf("%ld\n", (long) &s[0].j - (long) &s[0]);
        }
    """, "1\n-1\n0\n4\n", 0)
