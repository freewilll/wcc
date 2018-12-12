import pytest
import re
import subprocess
import tempfile


def check(code, expected_output, expected_exit_code):
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

@pytest.mark.parametrize("code", [0, 1, 2, 255])
def test_exit_codes(code):
    check("int main() {return %s;}" % code, "", code)
