"""Independent complex AST/reference and LCD-boundary checks of the native DLL."""
import cmath
import ctypes
import math
import random
import struct
import sys
from pathlib import Path


class Complex(ctypes.Structure):
    _fields_ = [('real', ctypes.c_double), ('imag', ctypes.c_double)]


class Guarded(ctypes.Structure):
    _fields_ = [('before', ctypes.c_uint32), ('line', ctypes.c_char * 16),
                ('after', ctypes.c_uint32)]


lib = ctypes.CDLL(str(Path(sys.argv[1]).resolve()))
lib.basic_calc_evaluate_complex.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                           ctypes.c_int, ctypes.POINTER(Complex)]
lib.basic_calc_evaluate_complex.restype = ctypes.c_int
lib.basic_calc_format_complex.argtypes = [Complex, ctypes.c_void_p, ctypes.c_void_p]
lib.basic_calc_format_complex.restype = ctypes.c_int
lib.basic_calc_render_complex.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                         ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
lib.basic_calc_render_complex.restype = ctypes.c_int
rng = random.Random(2026102206)


def canonical(value):
    # Match the documented upper-bank convention when Im is exactly zero.
    return complex(value.real, 0.0 if value.imag == 0 else value.imag)


def case(angle, depth=0):
    choice = rng.randrange(10) if depth < 2 else 0
    if choice == 0:
        n = rng.randrange(-5, 6)
        if rng.randrange(2):
            return (str(n) + 'i'), complex(0, n)
        return str(n), complex(n)
    if choice <= 4:
        left, a = case(angle, depth + 1)
        right, b = case(angle, depth + 1)
        op = rng.choice('+-*/')
        if op == '+': result = a + b
        elif op == '-': result = a - b
        elif op == '*': result = a * b
        else:
            if abs(b) < 1e-10: raise ValueError
            result = a / b
        return f'({left}{op}{right})', canonical(result)
    if choice == 5:
        text, base = case(angle, depth + 1)
        exponent = rng.choice([-3, -1, 0, 2, 3, 0.5, 1j])
        if base == 0: raise ValueError
        exponent_text = 'i' if exponent == 1j else str(exponent)
        return f'({text})^{exponent_text}', canonical(base ** exponent)
    text, value = case(angle, depth + 1)
    function = rng.choice(['sin', 'cos', 'tan', 'sqrt', 'ln', 'log'])
    if function in ('sin', 'cos', 'tan'):
        if angle == 0: value *= math.pi / 180
        if function == 'tan' and abs(cmath.cos(value)) < 1e-10: raise ValueError
        result = getattr(cmath, function)(value)
    elif function == 'sqrt': result = cmath.sqrt(value)
    elif function == 'ln':
        if abs(value) < 1e-10: raise ValueError
        result = cmath.log(value)
    else:
        if abs(value) < 1e-10: raise ValueError
        result = cmath.log10(value)
    return f'{function}({text})', canonical(result)


count = 0
for _ in range(12000):
    angle = rng.randrange(2)
    try:
        text, expected = case(angle)
    except (ValueError, OverflowError, ZeroDivisionError):
        continue
    if len(text) > 16 or not math.isfinite(expected.real) or not math.isfinite(expected.imag):
        continue
    encoded = text.encode('ascii')
    source = (ctypes.c_char * len(encoded)).from_buffer_copy(encoded)
    actual = Complex(876, 543)
    status = lib.basic_calc_evaluate_complex(source, len(encoded), angle, ctypes.byref(actual))
    got = complex(actual.real, actual.imag)
    assert status == 0 and abs(got - expected) <= 1e-10 * max(1, abs(expected)), (
        text, angle, status, got, expected)
    count += 1


def guarded():
    buffer = Guarded()
    buffer.before = 0x12345678
    buffer.after = 0xABCDEF12
    return buffer, ctypes.byref(buffer, Guarded.line.offset)


def check_guards(buffer):
    assert buffer.before == 0x12345678 and buffer.after == 0xABCDEF12


display_count = 0
for _ in range(4000):
    a = struct.unpack('d', rng.getrandbits(64).to_bytes(8, 'little'))[0]
    b = struct.unpack('d', rng.getrandbits(64).to_bytes(8, 'little'))[0]
    top, top_pointer = guarded()
    bottom, bottom_pointer = guarded()
    status = lib.basic_calc_format_complex(Complex(a, b), top_pointer, bottom_pointer)
    check_guards(top)
    check_guards(bottom)
    if not math.isfinite(a) or not math.isfinite(b):
        assert status == 4
        continue
    assert status == 0
    assert top.line.startswith(b'Re=') and bottom.line.startswith(b'Im=')
    for row, wanted in [(top.line, a), (bottom.line, b)]:
        displayed = float(row[3:].decode('ascii').strip())
        assert math.isclose(displayed, wanted, rel_tol=5.1e-6, abs_tol=5e-324), (wanted, row)
    display_count += 1

for _ in range(3000):
    data = rng.randbytes(rng.randrange(17))
    source = (ctypes.c_char * max(1, len(data)))()
    ctypes.memmove(source, data, len(data))
    top, top_pointer = guarded()
    bottom, bottom_pointer = guarded()
    status = lib.basic_calc_render_complex(source, len(data), rng.randrange(2), top_pointer, bottom_pointer)
    assert status in range(6)
    check_guards(top)
    check_guards(bottom)

print(f'PASS: {count} complex expressions, {display_count} finite two-row displays, '
      '3000 arbitrary byte inputs; real/complex mode and LCD bounds')
