/* AI-written real/complex math. No dependency on the supplied calculator .a.
 * Complex functions use principal branches, with Im=0 on the upper bank of
 * the negative real axis. Both parts of a complex trig argument use DEG/RAD.
 */
#include "complex_math.h"
#include <math.h>
#include <stdint.h>

#define PI 3.14159265358979323846
#define LN10 2.30258509299404568402

static basic_calc_status_t finish(basic_calc_complex_t value, basic_calc_complex_t *out)
{
    if (!isfinite(value.real) || !isfinite(value.imag)) return BASIC_CALC_RANGE;
    if (value.real == 0) value.real = 0;
    if (value.imag == 0) value.imag = 0;
    *out = value;
    return BASIC_CALC_OK;
}

static basic_calc_status_t multiply(basic_calc_complex_t a, basic_calc_complex_t b,
                                     basic_calc_complex_t *out)
{
    basic_calc_complex_t value = {a.real * b.real - a.imag * b.imag,
                                  a.real * b.imag + a.imag * b.real};
    if (!isfinite(value.real) || !isfinite(value.imag)) {
        double sa = fmax(fabs(a.real), fabs(a.imag));
        double sb = fmax(fabs(b.real), fabs(b.imag));
        int ea, eb;
        double ma = frexp(sa, &ea), mb = frexp(sb, &eb);
        double ar = a.real / sa, ai = a.imag / sa;
        double br = b.real / sb, bi = b.imag / sb;
        value.real = scalbn((ar * br - ai * bi) * ma * mb, ea + eb);
        value.imag = scalbn((ar * bi + ai * br) * ma * mb, ea + eb);
    }
    if (value.real == 0 && value.imag == 0 &&
        (a.real != 0 || a.imag != 0) && (b.real != 0 || b.imag != 0)) return BASIC_CALC_RANGE;
    return finish(value, out);
}

static basic_calc_status_t divide(basic_calc_complex_t a, basic_calc_complex_t b,
                                   basic_calc_complex_t *out)
{
    double scale = fmax(fabs(b.real), fabs(b.imag));
    if (scale == 0) return BASIC_CALC_DIV_ZERO;
    /* Normalize denominator to avoid squaring huge/tiny components. */
    double br = b.real / scale, bi = b.imag / scale;
    double ar = a.real / scale, ai = a.imag / scale;
    double divisor = br * br + bi * bi;
    basic_calc_complex_t value = {(ar * br + ai * bi) / divisor,
                                  (ai * br - ar * bi) / divisor};
    if (!isfinite(value.real) || !isfinite(value.imag)) {
        double sa = fmax(fabs(a.real), fabs(a.imag));
        int ea, eb;
        double ma = frexp(sa, &ea), mb = frexp(scale, &eb);
        ar = a.real / sa;
        ai = a.imag / sa;
        value.real = scalbn(((ar * br + ai * bi) / divisor) * (ma / mb), ea - eb);
        value.imag = scalbn(((ai * br - ar * bi) / divisor) * (ma / mb), ea - eb);
    }
    if (value.real == 0 && value.imag == 0 && (a.real != 0 || a.imag != 0)) return BASIC_CALC_RANGE;
    return finish(value, out);
}

static double log_magnitude(basic_calc_complex_t value)
{
    double scale = fmax(fabs(value.real), fabs(value.imag));
    double x = value.real / scale, y = value.imag / scale;
    return log(scale) + 0.5 * log(x * x + y * y);
}

static basic_calc_status_t power(basic_calc_complex_t base, basic_calc_complex_t exponent,
                                  int allow_complex, basic_calc_complex_t *out)
{
    if (base.real == 0 && base.imag == 0) {
        if (exponent.imag != 0 || exponent.real == 0) return BASIC_CALC_DOMAIN;
        if (exponent.real < 0) return BASIC_CALC_DIV_ZERO;
        basic_calc_complex_t zero = {0, 0};
        return finish(zero, out);
    }
    if (base.imag == 0 && exponent.imag == 0 &&
        (base.real > 0 || floor(exponent.real) == exponent.real)) {
        basic_calc_complex_t value = {pow(base.real, exponent.real), 0};
        if (value.real == 0) return BASIC_CALC_RANGE;
        return finish(value, out);
    }
    if (!allow_complex) return BASIC_CALC_DOMAIN;
    /* Integer powers preserve exact values such as i^2 without trig roundoff. */
    if (exponent.imag == 0 && floor(exponent.real) == exponent.real &&
        fabs(exponent.real) <= 2147483647.0) {
        basic_calc_complex_t value = {1, 0};
        uint32_t count = (uint32_t)fabs(exponent.real);
        if (exponent.real < 0) {
            basic_calc_status_t status = divide(value, base, &base);
            if (status != BASIC_CALC_OK) return status;
        }
        while (count) {
            basic_calc_status_t status;
            if (count & 1U) {
                status = multiply(value, base, &value);
                if (status != BASIC_CALC_OK) return status;
            }
            count >>= 1;
            if (count) {
                status = multiply(base, base, &base);
                if (status != BASIC_CALC_OK) return status;
            }
        }
        return finish(value, out);
    }
    double magnitude_log = log_magnitude(base);
    double phase = atan2(base.imag, base.real);
    double radius = exp(exponent.real * magnitude_log - exponent.imag * phase);
    double angle = exponent.real * phase + exponent.imag * magnitude_log;
    if (radius == 0) return BASIC_CALC_RANGE;
    basic_calc_complex_t value = {radius * cos(angle), radius * sin(angle)};
    return finish(value, out);
}

basic_calc_status_t calc_value_binary(basic_calc_complex_t a, basic_calc_complex_t b,
                                       char operation, int allow_complex,
                                       basic_calc_complex_t *out)
{
    if (!allow_complex && (a.imag != 0 || b.imag != 0)) return BASIC_CALC_DOMAIN;
    if (operation == '^') return power(a, b, allow_complex, out);
    if (operation == '*' && (a.imag != 0 || b.imag != 0)) return multiply(a, b, out);
    if (operation == '/' && (a.imag != 0 || b.imag != 0)) return divide(a, b, out);
    basic_calc_complex_t value = {0, 0};
    switch (operation) {
    case '+': value.real = a.real + b.real; value.imag = a.imag + b.imag; break;
    case '-': value.real = a.real - b.real; value.imag = a.imag - b.imag; break;
    case '*': value.real = a.real * b.real; break;
    case '/':
        if (b.real == 0) return BASIC_CALC_DIV_ZERO;
        value.real = a.real / b.real;
        break;
    default: return BASIC_CALC_SYNTAX;
    }
    return finish(value, out);
}

static void trig_parts(double argument, basic_calc_angle_t angle, double *s, double *c)
{
    if (angle == BASIC_CALC_DEG) {
        argument = fmod(argument, 360.0);
        if (argument < 0) argument += 360.0;
        if (argument == 0) { *s = 0; *c = 1; return; }
        if (argument == 90) { *s = 1; *c = 0; return; }
        if (argument == 180) { *s = 0; *c = -1; return; }
        if (argument == 270) { *s = -1; *c = 0; return; }
        argument *= PI / 180.0;
    }
    *s = sin(argument);
    *c = cos(argument);
}

basic_calc_status_t calc_value_function(unsigned function, basic_calc_angle_t angle,
                                         int allow_complex, basic_calc_complex_t *value)
{
    basic_calc_complex_t result = {0, 0};
    if (function > 5) return BASIC_CALC_SYNTAX;
    if (value->imag == 0 &&
        !(allow_complex && value->real < 0 && function >= 3)) {
        double x = value->real;
        if (function <= 2) {
            double s, c;
            trig_parts(x, angle, &s, &c);
            if (function == 0) result.real = s;
            else if (function == 1) result.real = c;
            else {
                if (fabs(c) < 1e-14) return BASIC_CALC_DOMAIN;
                result.real = s / c;
            }
        } else if (function <= 4) {
            if (x <= 0) return BASIC_CALC_DOMAIN;
            result.real = function == 3 ? log10(x) : log(x);
        } else {
            if (x < 0) return BASIC_CALC_DOMAIN;
            result.real = sqrt(x);
        }
        return finish(result, value);
    }
    if (!allow_complex) return BASIC_CALC_DOMAIN;
    if (function <= 2) {
        double s, c;
        double y = value->imag * (angle == BASIC_CALC_DEG ? PI / 180.0 : 1.0);
        trig_parts(value->real, angle, &s, &c);
        if (function == 0) {
            result.real = s * cosh(y);
            result.imag = c * sinh(y);
        } else if (function == 1) {
            result.real = c * cosh(y);
            result.imag = -s * sinh(y);
        } else if (fabs(y) > 20) {
            double q = exp(-2 * fabs(y));
            double denominator = 1 + 2 * q * (c * c - s * s) + q * q;
            result.real = 4 * q * s * c / denominator;
            result.imag = (y < 0 ? -1 : 1) * (1 - q * q) / denominator;
        } else {
            basic_calc_complex_t numerator = {s * cosh(y), c * sinh(y)};
            basic_calc_complex_t denominator = {c * cosh(y), -s * sinh(y)};
            return divide(numerator, denominator, value);
        }
    } else if (function <= 4) {
        if (value->real == 0 && value->imag == 0) return BASIC_CALC_DOMAIN;
        result.real = log_magnitude(*value);
        result.imag = atan2(value->imag, value->real);
        if (function == 3) { result.real /= LN10; result.imag /= LN10; }
    } else {
        double scale = fmax(fabs(value->real), fabs(value->imag));
        if (scale == 0) return finish(result, value);
        double ar = fabs(value->real) / scale;
        double ai = value->imag / scale;
        double t = sqrt(scale) * sqrt((hypot(ar, ai) + ar) / 2);
        if (value->real >= 0) {
            result.real = t;
            result.imag = value->imag / (2 * t);
        } else {
            result.real = fabs(value->imag) / (2 * t);
            result.imag = value->imag < 0 ? -t : t;
        }
    }
    return finish(result, value);
}
