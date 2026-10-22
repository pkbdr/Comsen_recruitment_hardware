"""Deterministic independent arithmetic and LCD-boundary checks of the C DLL.

Run with Python 3 and the path to a native shared build of basic_calc.c.
"""
import ctypes
import math
import random
import struct
import sys
from fractions import Fraction
from pathlib import Path

lib = ctypes.CDLL(str(Path(sys.argv[1]).resolve()))
lib.basic_calc_evaluate.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                    ctypes.POINTER(ctypes.c_double)]
lib.basic_calc_evaluate.restype = ctypes.c_int
lib.basic_calc_format.argtypes = [ctypes.c_double, ctypes.c_void_p]
lib.basic_calc_format.restype = ctypes.c_int
lib.basic_calc_render.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
lib.basic_calc_render.restype = ctypes.c_int
rng = random.Random(20261021)


class Guarded(ctypes.Structure):
    _fields_ = [("before", ctypes.c_uint32), ("line", ctypes.c_char * 16),
                ("after", ctypes.c_uint32)]


def output():
    buf = Guarded()
    buf.before, buf.after = 0x12345678, 0xA5A5A5A5
    return buf, ctypes.byref(buf, Guarded.line.offset)


def guards_ok(buf):
    assert buf.before == 0x12345678 and buf.after == 0xA5A5A5A5


# Exact rational reference, grouped independently using a list of additive terms.
expression_count = 0
for _ in range(4000):
    count = rng.randrange(1, 5)
    atoms, values = [], []
    for i in range(count):
        denominator = rng.choice([1, 10, 100])
        numerator = rng.randrange(-99, 100)
        atom = str(numerator) if denominator == 1 else f"{numerator / denominator:.2f}"
        atoms.append(atom)
        values.append(Fraction(atom))
    ops = [rng.choice("+-*/") for _ in range(count - 1)]
    text = atoms[0] + "".join(op + atom for op, atom in zip(ops, atoms[1:]))
    if len(text) > 16:
        continue
    expression_count += 1
    terms = [values[0]]
    zero_division = False
    for op, value in zip(ops, values[1:]):
        if op == "+":
            terms.append(value)
        elif op == "-":
            terms.append(-value)
        elif op == "*":
            terms[-1] *= value
        elif value == 0:
            zero_division = True
            break
        else:
            terms[-1] /= value
    result = ctypes.c_double(876.0)
    encoded = text.encode("ascii")
    # No trailing NUL is allocated, so bounded reading is required.
    source = (ctypes.c_char * len(encoded)).from_buffer_copy(encoded)
    status = lib.basic_calc_evaluate(source, len(encoded), ctypes.byref(result))
    if zero_division:
        assert status == 3 and result.value == 876.0, text
    else:
        expected = float(sum(terms))
        assert status == 0 and math.isclose(result.value, expected,
                                           rel_tol=1e-11, abs_tol=1e-11), (text, result.value, expected)

# Independently generate ASTs with parentheses/functions/powers; no text eval.
def scientific_case(depth=0):
    choice = rng.randrange(9) if depth < 2 else 0
    if choice == 0:
        token = rng.choice([str(rng.randrange(1, 31)), "0.5", "pi", "e"])
        return token, {"pi": math.pi, "e": math.e}.get(token, float(token) if token not in ("pi", "e") else 0)
    if choice <= 3:
        left, a = scientific_case(depth + 1)
        right, b = scientific_case(depth + 1)
        op = rng.choice("+-*/")
        if op == "+": value = a + b
        elif op == "-": value = a - b
        elif op == "*": value = a * b
        else: value = a / b
        return f"({left}{op}{right})", value
    if choice == 4:
        base, a = scientific_case(depth + 1)
        exponent = rng.randrange(-3, 5)
        if a == 0 and exponent == 0: raise ValueError
        value = math.pow(a, exponent)
        if value == 0 and a != 0: raise OverflowError
        return f"({base}^{exponent})", value
    argument, value = scientific_case(depth + 1)
    function = rng.choice(["sin", "cos", "tan", "sqrt", "log", "ln"])
    if function == "sin": result = math.sin(math.radians(value))
    elif function == "cos": result = math.cos(math.radians(value))
    elif function == "tan":
        radians = math.radians(value)
        if abs(math.cos(radians)) < 1e-14: raise ValueError
        result = math.tan(radians)
    elif function == "sqrt": result = math.sqrt(value)
    elif function == "log": result = math.log10(value)
    else: result = math.log(value)
    return f"{function}({argument})", result


scientific_count = 0
for _ in range(6000):
    try:
        text, expected = scientific_case()
    except (ValueError, OverflowError, ZeroDivisionError):
        continue
    if len(text) > 16 or not math.isfinite(expected):
        continue
    encoded = text.encode("ascii")
    source = (ctypes.c_char * len(encoded)).from_buffer_copy(encoded)
    result = ctypes.c_double(876.0)
    status = lib.basic_calc_evaluate(source, len(encoded), ctypes.byref(result))
    assert status == 0 and math.isclose(result.value, expected, rel_tol=1e-10,
                                       abs_tol=1e-10), (text, status, result.value, expected)
    scientific_count += 1

# Exercise formatting across the complete finite IEEE double range.
for _ in range(10000):
    value = struct.unpack("d", rng.getrandbits(64).to_bytes(8, "little"))[0]
    buf, destination = output()
    status = lib.basic_calc_format(value, destination)
    guards_ok(buf)
    if math.isfinite(value):
        assert status == 0
        displayed = buf.line.decode("ascii").strip()
        parsed = float(displayed)
        assert math.isclose(parsed, value, rel_tol=5.1e-9,
                            abs_tol=5e-324), (value, displayed)
    else:
        assert status == 4

# Arbitrary bounded bytes must return a known status without writing out of bounds.
for _ in range(4000):
    data = rng.randbytes(rng.randrange(17))
    source = (ctypes.c_char * max(1, len(data)))()
    ctypes.memmove(source, data, len(data))
    buf, destination = output()
    status = lib.basic_calc_render(source, len(data), destination)
    assert status in range(6)
    guards_ok(buf)

print(f"PASS: {expression_count} bounded arithmetic expressions, {scientific_count} "
      "parenthesis/function/power expressions, "
      "10000 floating-point displays, 4000 arbitrary byte inputs")
