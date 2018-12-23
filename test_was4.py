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
