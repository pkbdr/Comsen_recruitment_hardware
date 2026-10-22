#include "basic_calc.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;

static void expect_value(const char *input, double expected)
{
    double result = 876.0;
    assert(basic_calc_evaluate(input, strlen(input), &result) == BASIC_CALC_OK);
    assert(fabs(result - expected) <= 1e-12 * fmax(1.0, fabs(expected)));
    ++checks;
}

static void expect_error(const char *input, basic_calc_status_t expected)
{
    double result = 876.0;
    assert(basic_calc_evaluate(input, strlen(input), &result) == expected);
    assert(result == 876.0);
    ++checks;
}

static void expect_display(double value, const char *expected)
{
    struct { char before; char line[16]; char after; } guarded;
    char padded[16];
    guarded.before = 'A';
    guarded.after = 'Z';
    memset(padded, ' ', sizeof(padded));
    assert(strlen(expected) <= sizeof(padded));
    memcpy(padded, expected, strlen(expected));
    assert(basic_calc_format(value, guarded.line) == BASIC_CALC_OK);
    if (memcmp(padded, guarded.line, sizeof(padded)) != 0) {
        fprintf(stderr, "format %.17g: expected [%.16s], got [%.16s]\n",
                value, padded, guarded.line);
        assert(0);
    }
    assert(guarded.before == 'A' && guarded.after == 'Z');
    ++checks;
}

int main(void)
{
    expect_value("1+2", 3.0);
    expect_value("2+3*4", 14.0);
    expect_value("8-6/3", 6.0);
    expect_value("8/4/2", 1.0);
    expect_value("8-3-2", 3.0);
    expect_value("-3*2+10", 4.0);
    expect_value("1+-2", -1.0);
    expect_value("1--2", 3.0);
    expect_value("2*-3", -6.0);
    expect_value("2/-4", -0.5);
    expect_value(" + .5 + 2. ", 2.5);
    expect_value("0.1+0.2", 0.3);
    expect_value("1/3*3", 1.0);
    expect_value("\t12 + 3 * 2  ", 18.0);
    expect_value("9999999999999999", 1e16);
    expect_value("1/0.000000000001", 1e12);
    expect_value("0.00000000000001", 1e-14);
    expect_value("(1+2)*3", 9);
    expect_value("1+(2*3)", 7);
    expect_value("((2+3)*4)/2", 10);
    expect_value("(((((((1)))))))", 1);
    expect_value("2^3^2", 512);
    expect_value("2^-2", 0.25);
    expect_value("-2^2", -4);
    expect_value("(-2)^2", 4);
    expect_value("2^(-2)", 0.25);
    expect_value("1^1^1^1^1^1^1^1", 1);
    expect_value("sqrt(9)", 3);
    expect_value("sqrt(1+8)", 3);
    expect_value("sqrt(2)^2", 2);
    expect_value("sin(30)", 0.5);
    expect_value("cos(60)", 0.5);
    expect_value("tan(45)", 1);
    expect_value("sin(180)", 0);
    expect_value("cos(90)", 0);
    expect_value("tan(180)", 0);
    expect_value("sin(-30)", -0.5);
    expect_value("log(100)", 2);
    expect_value("ln(e)", 1);
    expect_value("ln(e^2)", 2);
    expect_value("log(sqrt(100))", 1);
    expect_value("pi*2", 6.283185307179586);
    expect_value("e", 2.718281828459045);
    expect_value(" sin ( 30 ) ", 0.5);
    expect_error("1/(2-2)", BASIC_CALC_DIV_ZERO);
    expect_error("1/cos(90)", BASIC_CALC_DIV_ZERO);
    expect_error("sqrt(-1)", BASIC_CALC_DOMAIN);
    expect_error("log(0)", BASIC_CALC_DOMAIN);
    expect_error("ln(-1)", BASIC_CALC_DOMAIN);
    expect_error("tan(90)", BASIC_CALC_DOMAIN);
    expect_error("tan(270)", BASIC_CALC_DOMAIN);
    expect_error("(-2)^0.5", BASIC_CALC_DOMAIN);
    expect_error("0^0", BASIC_CALC_DOMAIN);
    expect_error("0^-1", BASIC_CALC_DIV_ZERO);
    expect_error("10^999", BASIC_CALC_RANGE);
    expect_error("10^-999", BASIC_CALC_RANGE);
    const char *bad_functions[] = {"()", "1+2)", "(1+2", "sin()", "sin30",
        "sin(30", "sin(30))", "2pi", "pi(2)", "sqrt", "log()", "cos(0))"};
    for (size_t i = 0; i < sizeof(bad_functions) / sizeof(bad_functions[0]); ++i) {
        expect_error(bad_functions[i], BASIC_CALC_SYNTAX);
    }
    double angle_result = 876;
    assert(basic_calc_evaluate_angle("sin(pi/2)", 9, BASIC_CALC_RAD, &angle_result) == BASIC_CALC_OK);
    assert(fabs(angle_result - 1) < 1e-12);
    assert(basic_calc_evaluate_angle("cos(pi)", 7, BASIC_CALC_RAD, &angle_result) == BASIC_CALC_OK);
    assert(fabs(angle_result + 1) < 1e-12);
    assert(basic_calc_evaluate_angle("tan(pi/2)", 9, BASIC_CALC_RAD, &angle_result) == BASIC_CALC_DOMAIN);
    assert(basic_calc_evaluate_angle("1", 1, (basic_calc_angle_t)99, &angle_result) == BASIC_CALC_DOMAIN);
    checks += 4;
    expect_error("", BASIC_CALC_EMPTY);
    expect_error(" \t ", BASIC_CALC_EMPTY);
    expect_error("1/0", BASIC_CALC_DIV_ZERO);
    expect_error("1/-0.0", BASIC_CALC_DIV_ZERO);
    expect_error("1/0+2", BASIC_CALC_DIV_ZERO);
    const char *invalid[] = {"1+", "+", ".", "1..2", "1 2", "--1",
        "1**2", "1//2", "1+*2", "*1", "(1+2", "2(3)", "1e2", "1=",
        "1\n2", "1\r", "1,2", "a", "1\t2"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        expect_error(invalid[i], BASIC_CALC_SYNTAX);
    }
    expect_error("12345678901234567", BASIC_CALC_RANGE);

    /* These are not C strings: exactly 16 bytes or a prefix of a larger array. */
    const char fixed[16] = {'1','+','2',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
    const char prefix[] = {'1','+','2','/','0'};
    double result = 876.0;
    assert(basic_calc_evaluate(fixed, sizeof(fixed), &result) == BASIC_CALC_OK && result == 3.0);
    assert(basic_calc_evaluate(prefix, 3, &result) == BASIC_CALC_OK && result == 3.0);
    assert(basic_calc_evaluate(NULL, 1, &result) == BASIC_CALC_SYNTAX);
    assert(basic_calc_evaluate("1", 1, NULL) == BASIC_CALC_SYNTAX);
    const char embedded_nul[] = {'1', '\0', '+', '2'};
    assert(basic_calc_evaluate(embedded_nul, sizeof(embedded_nul), &result) == BASIC_CALC_SYNTAX);
    checks += 5;

    expect_display(0.0, "0");
    expect_display(-0.0, "0");
    expect_display(3.0, "3");
    expect_display(0.1 + 0.2, "0.3");
    expect_display(-12.5, "-12.5");
    expect_display(1.0 / 3.0, "0.333333333");
    expect_display(123456789, "123456789");
    expect_display(999999999.6, "1e+9");
    expect_display(99999999.96, "100000000");
    expect_display(0.0001, "0.0001");
    expect_display(-0.000123456789, "-0.000123456789");
    expect_display(0.00001, "1e-5");
    expect_display(1e12, "1e+12");
    expect_display(-1.23456789e100, "-1.23456789e+100");
    expect_display(-DBL_MAX, "-1.79769313e+308");
    expect_display(DBL_MIN * DBL_EPSILON, "4.94065646e-324");

    char line[16];
    assert(basic_calc_render("2+3*4", 5, line) == BASIC_CALC_OK);
    assert(memcmp(line, "14              ", 16) == 0);
    assert(basic_calc_render("1/0", 3, line) == BASIC_CALC_DIV_ZERO);
    assert(memcmp(line, "Divide by zero  ", 16) == 0);
    assert(basic_calc_render("1+", 2, line) == BASIC_CALC_SYNTAX);
    assert(memcmp(line, "Syntax ERROR    ", 16) == 0);
    assert(basic_calc_render("", 0, line) == BASIC_CALC_EMPTY);
    assert(memcmp(line, "Empty input     ", 16) == 0);
    assert(basic_calc_format(INFINITY, line) == BASIC_CALC_RANGE);
    assert(memcmp(line, "Range ERROR     ", 16) == 0);
    assert(basic_calc_format(NAN, line) == BASIC_CALC_RANGE);
    assert(basic_calc_render("sqrt(-1)", 8, line) == BASIC_CALC_DOMAIN);
    assert(memcmp(line, "Math ERROR      ", 16) == 0);
    assert(basic_calc_format(1.0, NULL) == BASIC_CALC_SYNTAX);
    assert(basic_calc_render("1", 1, NULL) == BASIC_CALC_SYNTAX);
    /* Input/output may share a buffer: evaluation finishes before formatting. */
    memcpy(line, fixed, sizeof(line));
    assert(basic_calc_render(line, sizeof(line), line) == BASIC_CALC_OK);
    assert(memcmp(line, "3               ", 16) == 0);
    checks += 10;
    printf("PASS: %u arithmetic, error, bounded-input and LCD checks\n", checks);
    return 0;
}
