#include "basic_calc.h"
#include "complex_math.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;

static void expect(const char *text, basic_calc_angle_t angle, double real, double imag)
{
    basic_calc_complex_t result = {876, 543};
    basic_calc_status_t status = basic_calc_evaluate_complex(text, strlen(text), angle, &result);
    if (status != BASIC_CALC_OK || fabs(result.real - real) > 1e-12 * fmax(1, fabs(real)) ||
        fabs(result.imag - imag) > 1e-12 * fmax(1, fabs(imag))) {
        fprintf(stderr, "%s: status %d, got %.17g%+.17gi; expected %.17g%+.17gi\n",
                text, status, result.real, result.imag, real, imag);
        assert(0);
    }
    ++checks;
}

static void error(const char *text, basic_calc_status_t wanted)
{
    basic_calc_complex_t result = {876, 543};
    assert(basic_calc_evaluate_complex(text, strlen(text), BASIC_CALC_RAD, &result) == wanted);
    assert(result.real == 876 && result.imag == 543);
    ++checks;
}

static void line_is(const char line[16], const char *text)
{
    char padded[16];
    assert(strlen(text) <= 16);
    memset(padded, ' ', 16);
    memcpy(padded, text, strlen(text));
    assert(memcmp(line, padded, 16) == 0);
}

int main(void)
{
    const double pi = 3.14159265358979323846;
    expect("i", BASIC_CALC_RAD, 0, 1);
    expect("-i", BASIC_CALC_RAD, 0, -1);
    expect("2i+3", BASIC_CALC_RAD, 3, 2);
    expect(".5i", BASIC_CALC_RAD, 0, 0.5);
    expect("i+i", BASIC_CALC_RAD, 0, 2);
    expect("(1+i)*(1-i)", BASIC_CALC_RAD, 2, 0);
    expect("(1+i)/(1-i)", BASIC_CALC_RAD, 0, 1);
    expect("1/(1+i)", BASIC_CALC_RAD, 0.5, -0.5);
    expect("1/i", BASIC_CALC_RAD, 0, -1);
    expect("i^2", BASIC_CALC_RAD, -1, 0);
    expect("i^3", BASIC_CALC_RAD, 0, -1);
    expect("i^4", BASIC_CALC_RAD, 1, 0);
    expect("i^-1", BASIC_CALC_RAD, 0, -1);
    expect("(1+i)^2", BASIC_CALC_RAD, 0, 2);
    expect("i^(-1000000000)", BASIC_CALC_RAD, 1, 0);
    expect("sqrt(-1)", BASIC_CALC_RAD, 0, 1);
    expect("sqrt(3+4i)", BASIC_CALC_RAD, 2, 1);
    expect("sqrt(3-4i)", BASIC_CALC_RAD, 2, -1);
    expect("sqrt(-3+4i)", BASIC_CALC_RAD, 1, 2);
    expect("sqrt(-3-4i)", BASIC_CALC_RAD, 1, -2);
    expect("ln(-1)", BASIC_CALC_RAD, 0, pi);
    expect("log(-100)", BASIC_CALC_RAD, 2, pi / log(10));
    expect("ln(e*i)", BASIC_CALC_RAD, 1, pi / 2);
    expect("sin(i)", BASIC_CALC_RAD, 0, sinh(1));
    expect("cos(i)", BASIC_CALC_RAD, cosh(1), 0);
    expect("tan(i)", BASIC_CALC_RAD, 0, tanh(1));
    expect("tan(1000i)", BASIC_CALC_RAD, 0, 1);
    expect("sin(i)", BASIC_CALC_DEG, 0, sinh(pi / 180));
    expect("sin(90+2i)", BASIC_CALC_DEG, cosh(2 * pi / 180), 0);
    expect("(-1)^0.5", BASIC_CALC_RAD, 0, 1);
    expect("i^i", BASIC_CALC_RAD, exp(-pi / 2), 0);
    error("0^i", BASIC_CALC_DOMAIN);
    error("0^0", BASIC_CALC_DOMAIN);
    error("0^-i", BASIC_CALC_DOMAIN);
    error("0^-1", BASIC_CALC_DIV_ZERO);
    error("i/0", BASIC_CALC_DIV_ZERO);
    error("1/(i-i)", BASIC_CALC_DIV_ZERO);
    error("ln(0i)", BASIC_CALC_DOMAIN);
    error("log(0)", BASIC_CALC_DOMAIN);
    error("10^999", BASIC_CALC_RANGE);
    error("(i/10^300)^2", BASIC_CALC_RANGE);
    error("sin(1000i)", BASIC_CALC_RANGE);
    const char *bad[] = {"ii", "i2", "1+2ii", "2 i", "2(i)", "sqrt(", "(i", "i)"};
    for (unsigned j = 0; j < sizeof(bad) / sizeof(bad[0]); ++j) error(bad[j], BASIC_CALC_SYNTAX);

    basic_calc_complex_t result = {876, 543};
    char fixed[16];
    memset(fixed, ' ', sizeof(fixed));
    fixed[0] = 'i';
    assert(basic_calc_evaluate_complex(fixed, 16, BASIC_CALC_RAD, &result) == BASIC_CALC_OK);
    assert(result.real == 0 && result.imag == 1);
    assert(basic_calc_evaluate_complex(NULL, 0, BASIC_CALC_RAD, &result) == BASIC_CALC_SYNTAX);
    assert(basic_calc_evaluate_complex("i", 1, BASIC_CALC_RAD, NULL) == BASIC_CALC_SYNTAX);
    assert(basic_calc_evaluate_complex("i", 1, (basic_calc_angle_t)99, &result) == BASIC_CALC_DOMAIN);
    assert(basic_calc_evaluate_complex("i", 17, BASIC_CALC_RAD, &result) == BASIC_CALC_RANGE);
    double real = 876;
    assert(basic_calc_evaluate("i", 1, &real) == BASIC_CALC_DOMAIN && real == 876);
    assert(basic_calc_evaluate_complex("i/10^300", 8, BASIC_CALC_RAD, &result) == BASIC_CALC_OK);
    assert(result.imag > 0 && fabs(result.imag / 1e-300 - 1) < 1e-14);

    basic_calc_complex_t a = {1.6e308, 0.5e308}, b = {1.2, 0.3};
    assert(calc_value_binary(a, b, '*', 1, &result) == BASIC_CALC_OK);
    assert(fabs(result.real / 1.77e308 - 1) < 1e-14);
    assert(fabs(result.imag / 1.08e308 - 1) < 1e-14);
    a.real = 1.7e308; a.imag = 0.5e308;
    b.real = b.imag = 0.9;
    assert(calc_value_binary(a, b, '/', 1, &result) == BASIC_CALC_OK);
    assert(fabs(result.real / (1.2222222222222222e308) - 1) < 1e-14);
    assert(fabs(result.imag / (-6.666666666666667e307) - 1) < 1e-14);
    a.real = a.imag = 1e308;
    result = a;
    assert(calc_value_function(5, BASIC_CALC_RAD, 1, &result) == BASIC_CALC_OK);
    assert(isfinite(result.real) && isfinite(result.imag));
    assert(calc_value_binary(result, result, '*', 1, &result) == BASIC_CALC_OK);
    assert(fabs(result.real / 1e308 - 1) < 1e-14);
    assert(fabs(result.imag / 1e308 - 1) < 1e-14);

    struct { char before; char top[16]; char bottom[16]; char after; } guarded;
    guarded.before = 'A'; guarded.after = 'Z';
    result.real = 1.0/3; result.imag = -2;
    assert(basic_calc_format_complex(result, guarded.top, guarded.bottom) == BASIC_CALC_OK);
    line_is(guarded.top, "Re=0.333333");
    line_is(guarded.bottom, "Im=-2");
    result.real = DBL_MAX; result.imag = -DBL_MIN * DBL_EPSILON;
    assert(basic_calc_format_complex(result, guarded.top, guarded.bottom) == BASIC_CALC_OK);
    line_is(guarded.top, "Re=1.79769e+308");
    line_is(guarded.bottom, "Im=-4.94066e-324");
    result.real = -0.0; result.imag = -0.0;
    assert(basic_calc_format_complex(result, guarded.top, guarded.bottom) == BASIC_CALC_OK);
    line_is(guarded.top, "Re=0");
    line_is(guarded.bottom, "Im=0");
    assert(basic_calc_render_complex("(1+i)*(1-i)", 11, BASIC_CALC_RAD,
                                     guarded.top, guarded.bottom) == BASIC_CALC_OK);
    line_is(guarded.top, "Re=2");
    line_is(guarded.bottom, "Im=0");
    assert(basic_calc_render_complex("sqrt(", 5, BASIC_CALC_RAD,
                                     guarded.top, guarded.bottom) == BASIC_CALC_SYNTAX);
    line_is(guarded.top, "sqrt(");
    line_is(guarded.bottom, "Syntax ERROR");
    assert(basic_calc_render_complex("i/0", 3, BASIC_CALC_RAD,
                                     guarded.top, guarded.bottom) == BASIC_CALC_DIV_ZERO);
    line_is(guarded.bottom, "Divide by zero");
    result.real = INFINITY;
    assert(basic_calc_format_complex(result, guarded.top, guarded.bottom) == BASIC_CALC_RANGE);
    assert(basic_calc_format_complex(result, NULL, guarded.bottom) == BASIC_CALC_SYNTAX);
    assert(guarded.before == 'A' && guarded.after == 'Z');
    printf("PASS: %u complex value/error cases plus numeric scaling, API and LCD bounds\n", checks);
    return 0;
}
