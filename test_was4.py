import pytest
import re
import subprocess
import tempfile


def check(code, expected_output, expected_exit_code):
    output = None

    try:
        with tempfile.NamedTemporaryFile(suffix=".c") as temp:
            with tempfile.NamedTemporaryFile(suffix=".ws") as temp_output:
                with open(temp.name, 'w') as f:
                    f.write(code)
                output = subprocess.check_output(["./wc4", "-o", temp_output.name, temp.name]).decode('utf-8')
                output = subprocess.check_output(["./was4", temp_output.name]).decode('utf-8')

                object_filename = re.sub(".ws", ".o", temp_output.name)
                executable_filename = re.sub(".ws", "", temp_output.name)

                output = subprocess.check_output([
                    "ld", object_filename,
                    "-o",  executable_filename,
                    "-lc",
                    "--dynamic-linker",  "/lib64/ld-linux-x86-64.so.2"]).decode('utf-8')

                try:
                    output = None
                    output = subprocess.check_output([executable_filename]).decode('utf-8')
                    assert output == expected_output
                except subprocess.CalledProcessError as e:
                    assert e.returncode == expected_exit_code

    except subprocess.CalledProcessError as e:
        print(output)
        raise


def test_hello_world():
    check("""
        int main() {
            printf("%s\n", "Hello world!");
        }
    """, "Hello world!\n", 0)


@pytest.mark.parametrize("count", [1, 2, 3, 4, 5, 6, 7])
def test_hello_world_args(count):
    s = ','.join([str(c + 1) for c in range(count)])
    f = ','.join(["%d" for c in range(count)])
    check("""
        int main() {
            printf("%s\n", %s);
        }
    """ % (f, s), "%s\n" % s, 0)


@pytest.mark.parametrize("code", [0, 1, 2, 255])
def test_exit_codes(code):
    check("int main() {return %s;}" % code, "", code)


def test_string_literal_escapes():
    check("""
        int main() {
            printf("\\\\-");
            printf("\\t-");
            printf("\\'-");
            printf("\\"-");
            printf("\\n");
        }
    """, "\\-\t-'-\"-\n", 0)


def test_function_calls():
    check("""
        int foo0() {
            printf("0 ");
        }

        int foo1(long p1) {
            printf("%d ", p1);
        }

        int foo2(long p1, long p2) {
            printf("%d %d ", p1, p2);
        }

        int foo3(long p1, long p2, long p3) {
            printf("%d %d %d ", p1, p2, p3);
        }

        int foo4(long p1, long p2, long p3, long p4) {
            printf("%d %d %d %d ", p1, p2, p3, p4);
        }

        int foo5(long p1, long p2, long p3, long p4, long p5) {
            printf("%d %d %d %d %d ", p1, p2, p3, p4, p5);
        }

        int foo6(long p1, long p2, long p3, long p4, long p5, long p6) {
            printf("%d %d %d %d %d %d ", p1, p2, p3, p4, p5, p6);
        }

        int foo7(long p1, long p2, long p3, long p4, long p5, long p6, long p7) {
            printf("%d %d %d %d %d %d %d ", p1, p2, p3, p4, p5, p6, p7);
        }

        int foo8(long p1, long p2, long p3, long p4, long p5, long p6, long p7, long p8) {
            printf("%d %d %d %d %d %d ", p1, p2, p3, p4, p5, p6);
            printf("%d %d ", p7, p8);
        }

        int main(int argc, char **argv) {
            foo0();
            foo1(1);
            foo1(2);
            foo2(3, 4);
            foo2(5, 6);
            foo2(7, 8);

            foo3(9, 10, 11);
            foo3(12, 13, 14);
            foo4(15, 16, 17, 18);
            foo5(19, 20, 21, 22, 23);

            foo6(24, 25, 26, 27, 28, 29);
            foo7(30, 31, 32, 33, 34, 35, 36);
            foo8(37, 38, 39, 40, 41, 42, 43, 44);

            printf("\n");

            return 0;
        }
    """, " ".join([str(s) for s in range(45)]) + " \n", 0)


def test_local_vars():
    check("""
        void foo(int i, int j) {
            int k;
            int l;
            k = 3;
            l = 4;
            printf("%d %d %d %d\n", i, j, k, l);

        }

        int main(int argc, char **argv) {
            foo(1, 2);
            return 0;
        }

    """, "1 2 3 4\n", 0)


def test_global_ints():
    check("""
        int m;
        int n;

        void foo(int i, int j) {
            int k;
            int l;
            k = 3;
            l = 4;
            printf("%d %d %d %d %d %d\n", i, j, k, l, m, n);
        }

        int main(int argc, char **argv) {
            m = 5;
            n = 6;
            foo(1, 2);
            return 0;
        }
    """, "1 2 3 4 5 6\n", 0)


def test_chars():
    check("""
        int i1;
        char c1;
        int i2;
        char c2;
        char c3;
        char c4;

        void foo() {
            printf("%d %d %d %d %d %d\n", i1, c1, i2, c2, c3, c4);
        }

        void bar(char c1, char c2, int i) {
            printf("%d %d %d\n", c1, c2, i);
        }

        void baz() {
            int i1;
            char c1;
            int i2;
            char c2;
            char c3;
            char c4;

            i1 = 10;
            c1 = 20;
            i2 = 30;
            c2 = 40;
            c3 = 50;
            c4 = 60;
            printf("%d %d %d %d %d %d\n", i1, c1, i2, c2, c3, c4);
        }

        int main() {
            i1 = 1;
            c1 = 2;
            i2 = 3;
            c2 = 4;
            c3 = 5;
            c4 = 6;

            foo();
            bar(1, 2, 3);
            baz();
            foo();
        }
    """, "1 2 3 4 5 6\n1 2 3\n10 20 30 40 50 60\n1 2 3 4 5 6\n", 0)


def test_arithmetic():
    check("""
        int test_cmp(long i, long j) {
            printf("%d ", i == j);
            printf("%d ", i != j);
            printf("%d ", i < j);
            printf("%d ", i > j);
            printf("%d ", i <= j);
            printf("%d ", i >= j);
            printf("- ");
        }

        int main() {
            long i;
            long j;

            i = 5;
            j = 2;
            printf("%d ", i + j);
            printf("%d ", i - j);
            printf("%d ", i * j);
            printf("%d ", i / j);
            printf("%d ", i % j);
            printf("- ");

            test_cmp(2, 2);
            test_cmp(2, 3);
            test_cmp(3, 2);

            printf("\n");
        }
    """, "7 3 10 2 1 - 1 0 0 0 1 1 - 0 1 1 0 1 0 - 0 1 0 1 0 1 - \n", 0)


def test_jumps():
    check("""
        int main() {
            long i;
            long j;
            i = 1;
            j = 0;

            printf("%d\n", (i == 0 ? 2 : 1));
            printf("%d\n", (i == 1 ? 2 : 1));

            i = 0; if (i) printf("true\n"); else printf("false\n");
            i = 1; if (i) printf("true\n"); else printf("false\n");

            i = 0;
            while (i++ < 10) printf("%d ", i);
            printf("\n");

            if (0) printf("1"); else if (0) printf("2"); else printf("3"); printf("\n");
            if (0) printf("1"); else if (1) printf("2"); else printf("3"); printf("\n");
            if (1) printf("1"); else if (0) printf("2"); else printf("3"); printf("\n");
            if (1) printf("1"); else if (1) printf("2"); else printf("3"); printf("\n");
        }
    """, "1\n2\nfalse\ntrue\n1 2 3 4 5 6 7 8 9 10 \n3\n2\n1\n1\n", 0)


def test_logical_or_and():
    check("""
        int main() {
            long i;
            long j;
            i = 1;
            j = 0;

            i = 0; printf("%d %d ", (i && j++), j);
            i = 1; printf("%d %d ", (i && j++), j);
            printf("\n");

            i = 0; printf("%d %d ", (i || j++), j);
            i = 1; printf("%d %d ", (i || j++), j);
            printf("\n");

            printf("%d ", 0 || 0);
            printf("%d ", 0 || 1);
            printf("%d ", 1 || 0);
            printf("%d ", 1 || 1);
            printf("\n");

            printf("%d ", 0 && 0);
            printf("%d ", 0 && 1);
            printf("%d ", 1 && 0);
            printf("%d ", 1 && 1);
            printf("\n");
        }
    """, "0 0 0 1 \n1 2 1 2 \n0 1 1 1 \n0 0 0 1 \n", 0)


def test_bitwise_not():
    check("""
        int main() {
            printf("%d\n", ~0);
            printf("%d\n", ~1);
            printf("%d\n", ~2);
            printf("%d\n", ~3);
        }
    """, "-1\n-2\n-3\n-4\n", 0)


def test_builtins():
    with tempfile.NamedTemporaryFile() as input_file:
        with open(input_file.name, 'w') as f:
            f.write("foo\n");
            f.flush()

            with tempfile.NamedTemporaryFile(delete=False) as output_file1:
                with tempfile.NamedTemporaryFile(delete=False) as output_file2:
                    check("""
                        int main(int argc, char **argv) {
                            int f;
                            char *data;

                            data = malloc(16);
                            f = open("%s", 0, 0);
                            if (f < 0) { printf("bad FH\\n"); exit(1); }
                            printf("%%ld\\n", read(f, data, 16));
                            printf("%%s", data);
                            close(f);

                            memset(data, 0, 16);
                            data[0] = 'b';
                            data[1] = 'a';
                            data[2] = 'r';
                            printf("%%s\\n", data);

                            printf("%%d\\n", memcmp(data, "foo", 3) == 0);
                            printf("%%d\\n", memcmp(data, "bar", 3) == 0);

                            printf("%%d\\n", strcmp(data, "foo") == 0);
                            printf("%%d\\n", strcmp(data, "bar") == 0);

                            free(data);

                            f = open("%s", 577, 420);
                            if (f < 0) { printf("bad FH\\n"); exit(1); }
                            dprintf(f, "foo\\n");
                            close(f);

                            f = open("%s", 577, 420);
                            if (f < 0) { printf("bad FH\\n"); exit(1); }
                            write(f, "bar\\n", 4);
                            close(f);

                            exit(4);
                        }
                    """ % (input_file.name, output_file1.name, output_file2.name), "4\nfoo\nbar\n0\n1\n0\n1\n", 4);

                    with open(output_file1.name) as f:
                        assert f.read() == "foo\n"
                    with open(output_file2.name) as f:
                        assert f.read() == "bar\n"


def check_negative_imm():
    check("""
        int main(int argc, char **argv) {
            int i;
            i = -1;
            printf("%d\n", i);
        }
    """, "-1\n", 0)
