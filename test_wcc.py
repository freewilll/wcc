import pytest
import subprocess
import tempfile
import re


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
    ("2--1",            3),
    ("-(2*3)",          -6),
    ("-(2*3)+1",        -5),
    ("-(2*3)*2+1",      -11),
    ("1==1",            1),
    ("2==2",            1),
    ("1==0",            0),
    ("0==1",            0),
    ("1!=1",            0),
    ("2!=2",            0),
    ("1!=0",            1),
    ("0!=1",            1),
    ("!0",              1),
    ("!1",              0),
    ("!2",              0),
])
def test_expr(expr, expected_result):
    with tempfile.NamedTemporaryFile() as temp:
        with open(temp.name, 'w') as f:
            f.write(expr)
        output = subprocess.check_output(["./wc4", f.name]).decode('utf-8')
        result = re.sub("Result: ", "", str(output).split("\n")[-2])
        assert int(result) == expected_result
