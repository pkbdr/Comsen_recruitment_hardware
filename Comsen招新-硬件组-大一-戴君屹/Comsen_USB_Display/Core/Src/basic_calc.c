/* Written with AI assistance for the expression/scientific calculator.
 * Independent of the supplied libcalculator_engine.a and of the input driver.
 */
#include "basic_calc.h"
#include "complex_math.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    const char *input;
    size_t length;
    size_t position;
    unsigned depth;
    basic_calc_angle_t angle;
    int allow_complex;
} calc_reader_t;

#define CALC_PI 3.14159265358979323846
#define CALC_E 2.71828182845904523536
#define CALC_MAX_DEPTH 16U

static basic_calc_status_t read_expression(calc_reader_t *reader, basic_calc_complex_t *value);
static basic_calc_status_t read_unary(calc_reader_t *reader, basic_calc_complex_t *value);

static void skip_spaces(calc_reader_t *reader)
{
    while (reader->position < reader->length &&
           (reader->input[reader->position] == ' ' ||
            reader->input[reader->position] == '\t')) {
        ++reader->position;
    }
}

static int is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static basic_calc_status_t read_number(calc_reader_t *reader, basic_calc_complex_t *number)
{
    int digits = 0;
    double value = 0.0;
    skip_spaces(reader);
    while (reader->position < reader->length &&
           is_digit(reader->input[reader->position])) {
        value = value * 10.0 + (reader->input[reader->position++] - '0');
        ++digits;
    }
    if (reader->position < reader->length &&
        reader->input[reader->position] == '.') {
        double place = 0.1;
        ++reader->position;
        while (reader->position < reader->length &&
               is_digit(reader->input[reader->position])) {
            value += (reader->input[reader->position++] - '0') * place;
            place *= 0.1;
            ++digits;
        }
    }
    if (digits == 0) return BASIC_CALC_SYNTAX;
    if (!isfinite(value)) return BASIC_CALC_RANGE;
    number->real = value;
    number->imag = 0;
    /* The natural coefficient notation 2i is allowed, but 2(3) is not. */
    if (reader->position < reader->length && reader->input[reader->position] == 'i') {
        if (!reader->allow_complex) return BASIC_CALC_DOMAIN;
        ++reader->position;
        number->real = 0;
        number->imag = value;
    }
    return BASIC_CALC_OK;
}

static int match_name(calc_reader_t *reader, const char *name)
{
    size_t count = strlen(name);
    if (reader->length - reader->position < count ||
        memcmp(reader->input + reader->position, name, count)) return 0;
    reader->position += count;
    return 1;
}

static basic_calc_status_t read_parentheses(calc_reader_t *reader, basic_calc_complex_t *value)
{
    basic_calc_status_t status;
    skip_spaces(reader);
    if (reader->position == reader->length ||
        reader->input[reader->position] != '(') return BASIC_CALC_SYNTAX;
    if (reader->depth == CALC_MAX_DEPTH) return BASIC_CALC_RANGE;
    ++reader->position;
    ++reader->depth;
    status = read_expression(reader, value);
    --reader->depth;
    if (status != BASIC_CALC_OK) return status;
    skip_spaces(reader);
    if (reader->position == reader->length ||
        reader->input[reader->position++] != ')') return BASIC_CALC_SYNTAX;
    return BASIC_CALC_OK;
}

static basic_calc_status_t read_primary(calc_reader_t *reader, basic_calc_complex_t *value)
{
    static const char *const names[] = {"sin", "cos", "tan", "log", "ln", "sqrt"};
    skip_spaces(reader);
    if (reader->position == reader->length) return BASIC_CALC_SYNTAX;
    if (reader->input[reader->position] == '(') return read_parentheses(reader, value);
    if (match_name(reader, "pi")) { value->real = CALC_PI; value->imag = 0; return BASIC_CALC_OK; }
    if (match_name(reader, "e")) { value->real = CALC_E; value->imag = 0; return BASIC_CALC_OK; }
    if (match_name(reader, "i")) {
        if (!reader->allow_complex) return BASIC_CALC_DOMAIN;
        value->real = 0;
        value->imag = 1;
        return BASIC_CALC_OK;
    }
    for (unsigned fn = 0; fn < sizeof(names) / sizeof(names[0]); ++fn) {
        if (!match_name(reader, names[fn])) continue;
        basic_calc_status_t status = read_parentheses(reader, value);
        if (status != BASIC_CALC_OK) return status;
        return calc_value_function(fn, reader->angle, reader->allow_complex, value);
    }
    return read_number(reader, value);
}

static basic_calc_status_t read_power(calc_reader_t *reader, basic_calc_complex_t *value)
{
    basic_calc_complex_t exponent;
    basic_calc_status_t status = read_primary(reader, value);
    if (status != BASIC_CALC_OK) return status;
    skip_spaces(reader);
    if (reader->position == reader->length ||
        reader->input[reader->position] != '^') return BASIC_CALC_OK;
    if (reader->depth == CALC_MAX_DEPTH) return BASIC_CALC_RANGE;
    ++reader->position;
    ++reader->depth;
    /* Right associative, with unary signs allowed on the exponent. */
    status = read_unary(reader, &exponent);
    --reader->depth;
    if (status != BASIC_CALC_OK) return status;
    return calc_value_binary(*value, exponent, '^', reader->allow_complex, value);
}

static basic_calc_status_t read_unary(calc_reader_t *reader, basic_calc_complex_t *value)
{
    int negative = 0;
    skip_spaces(reader);
    if (reader->position < reader->length &&
        (reader->input[reader->position] == '+' || reader->input[reader->position] == '-')) {
        negative = reader->input[reader->position++] == '-';
    }
    basic_calc_status_t status = read_power(reader, value);
    if (status == BASIC_CALC_OK && negative) {
        value->real = value->real == 0 ? 0 : -value->real;
        value->imag = value->imag == 0 ? 0 : -value->imag;
    }
    return status;
}

/* Consume a multiplication/division chain before the caller adds/subtracts. */
static basic_calc_status_t read_term(calc_reader_t *reader, basic_calc_complex_t *term)
{
    basic_calc_status_t status = read_unary(reader, term);
    if (status != BASIC_CALC_OK) return status;
    for (;;) {
        char operation;
        basic_calc_complex_t right;
        skip_spaces(reader);
        if (reader->position == reader->length) return BASIC_CALC_OK;
        operation = reader->input[reader->position];
        if (operation != '*' && operation != '/') return BASIC_CALC_OK;
        ++reader->position;
        status = read_unary(reader, &right);
        if (status != BASIC_CALC_OK) return status;
        status = calc_value_binary(*term, right, operation, reader->allow_complex, term);
        if (status != BASIC_CALC_OK) return status;
    }
}

static basic_calc_status_t read_expression(calc_reader_t *reader, basic_calc_complex_t *value)
{
    basic_calc_status_t status = read_term(reader, value);
    if (status != BASIC_CALC_OK) return status;
    for (;;) {
        char operation;
        basic_calc_complex_t right;
        skip_spaces(reader);
        if (reader->position == reader->length) break;
        operation = reader->input[reader->position];
        if (operation != '+' && operation != '-') break;
        ++reader->position;
        status = read_term(reader, &right);
        if (status != BASIC_CALC_OK) return status;
        status = calc_value_binary(*value, right, operation, reader->allow_complex, value);
        if (status != BASIC_CALC_OK) return status;
    }
    return BASIC_CALC_OK;
}

static basic_calc_status_t evaluate_value(const char *input, size_t length,
                                           basic_calc_angle_t angle, int allow_complex,
                                           basic_calc_complex_t *result)
{
    calc_reader_t reader = {input, length, 0, 0, angle, allow_complex};
    basic_calc_complex_t value;
    if (input == NULL || result == NULL) return BASIC_CALC_SYNTAX;
    if (angle != BASIC_CALC_DEG && angle != BASIC_CALC_RAD) return BASIC_CALC_DOMAIN;
    if (length > BASIC_CALC_INPUT_CAPACITY) return BASIC_CALC_RANGE;
    skip_spaces(&reader);
    if (reader.position == length) return BASIC_CALC_EMPTY;
    basic_calc_status_t status = read_expression(&reader, &value);
    if (status != BASIC_CALC_OK) return status;
    skip_spaces(&reader);
    if (reader.position != length) return BASIC_CALC_SYNTAX;
    *result = value;
    return BASIC_CALC_OK;
}

basic_calc_status_t basic_calc_evaluate_complex(const char *input, size_t length,
                                                basic_calc_angle_t angle,
                                                basic_calc_complex_t *result)
{
    return evaluate_value(input, length, angle, 1, result);
}

basic_calc_status_t basic_calc_evaluate_angle(const char *input, size_t length,
                                              basic_calc_angle_t angle, double *result)
{
    basic_calc_complex_t value;
    if (result == NULL) return BASIC_CALC_SYNTAX;
    basic_calc_status_t status = evaluate_value(input, length, angle, 0, &value);
    if (status == BASIC_CALC_OK) *result = value.real;
    return status;
}

basic_calc_status_t basic_calc_evaluate(const char *input, size_t length,
                                        double *result)
{
    return basic_calc_evaluate_angle(input, length, BASIC_CALC_DEG, result);
}

static void write_message(char *line, const char *message)
{
    size_t i = 0;
    memset(line, ' ', BASIC_CALC_LCD_WIDTH);
    while (message[i] != '\0' && i < BASIC_CALC_LCD_WIDTH) {
        line[i] = message[i];
        ++i;
    }
}

static basic_calc_status_t format_precision(double value, char line[16], unsigned precision)
{
    char digits[9];
    size_t position = 0;
    size_t significant = precision;
    uint32_t factor = precision == 6 ? 100000U : 100000000U;
    int exponent = 0;
    uint32_t rounded;
    int negative;
    if (line == NULL) return BASIC_CALC_SYNTAX;
    if (!isfinite(value)) {
        write_message(line, "Range ERROR");
        return BASIC_CALC_RANGE;
    }
    memset(line, ' ', BASIC_CALC_LCD_WIDTH);
    if (value == 0.0) {
        line[0] = '0';
        return BASIC_CALC_OK;
    }
    negative = value < 0.0;
    if (negative) value = -value;
    /* Normalize without pow/log or printf's optional float support. */
    while (value >= 10.0) { value /= 10.0; ++exponent; }
    while (value < 1.0) { value *= 10.0; --exponent; }
    rounded = (uint32_t)(value * factor + 0.5);
    if (rounded >= factor * 10U) {
        rounded = factor;
        ++exponent;
    }
    for (int i = (int)precision - 1; i >= 0; --i) {
        digits[i] = (char)('0' + rounded % 10U);
        rounded /= 10U;
    }
    while (significant > 1 && digits[significant - 1] == '0') --significant;
    if (negative) line[position++] = '-';
    if (exponent >= -4 && exponent < 9) {
        if (exponent < 0) {
            line[position++] = '0';
            line[position++] = '.';
            for (int i = -1; i > exponent; --i) line[position++] = '0';
            for (size_t i = 0; i < significant; ++i) line[position++] = digits[i];
        } else {
            for (int i = 0; i <= exponent; ++i) {
                line[position++] = (size_t)i < significant ? digits[i] : '0';
            }
            if ((size_t)(exponent + 1) < significant) {
                line[position++] = '.';
                for (size_t i = (size_t)exponent + 1; i < significant; ++i) {
                    line[position++] = digits[i];
                }
            }
        }
    } else {
        char exponent_digits[3];
        size_t count = 0;
        unsigned magnitude = (unsigned)(exponent < 0 ? -exponent : exponent);
        line[position++] = digits[0];
        if (significant > 1) {
            line[position++] = '.';
            for (size_t i = 1; i < significant; ++i) line[position++] = digits[i];
        }
        line[position++] = 'e';
        line[position++] = exponent < 0 ? '-' : '+';
        do {
            exponent_digits[count++] = (char)('0' + magnitude % 10U);
            magnitude /= 10U;
        } while (magnitude != 0U);
        while (count > 0) line[position++] = exponent_digits[--count];
    }
    return BASIC_CALC_OK;
}

basic_calc_status_t basic_calc_format(double value, char line[16])
{
    return format_precision(value, line, 9);
}

basic_calc_status_t basic_calc_format_complex(basic_calc_complex_t value,
                                              char top[16], char bottom[16])
{
    char component[16];
    if (top == NULL || bottom == NULL) return BASIC_CALC_SYNTAX;
    if (!isfinite(value.real) || !isfinite(value.imag)) {
        write_message(top, "Complex result");
        write_message(bottom, "Range ERROR");
        return BASIC_CALC_RANGE;
    }
    format_precision(value.real, component, 6);
    memcpy(top, "Re=", 3);
    memcpy(top + 3, component, 13);
    format_precision(value.imag, component, 6);
    memcpy(bottom, "Im=", 3);
    memcpy(bottom + 3, component, 13);
    return BASIC_CALC_OK;
}

static void error_line(basic_calc_status_t status, char line[16])
{
    switch (status) {
    case BASIC_CALC_EMPTY: write_message(line, "Empty input"); break;
    case BASIC_CALC_DIV_ZERO: write_message(line, "Divide by zero"); break;
    case BASIC_CALC_RANGE: write_message(line, "Range ERROR"); break;
    case BASIC_CALC_DOMAIN: write_message(line, "Math ERROR"); break;
    default: write_message(line, "Syntax ERROR"); break;
    }
}

basic_calc_status_t basic_calc_render_angle(const char *input, size_t length,
                                            basic_calc_angle_t angle,
                                            char line[BASIC_CALC_LCD_WIDTH])
{
    double value;
    basic_calc_status_t status;
    if (line == NULL) return BASIC_CALC_SYNTAX;
    status = basic_calc_evaluate_angle(input, length, angle, &value);
    if (status == BASIC_CALC_OK) return basic_calc_format(value, line);
    error_line(status, line);
    return status;
}

basic_calc_status_t basic_calc_render(const char *input, size_t length,
                                      char line[BASIC_CALC_LCD_WIDTH])
{
    return basic_calc_render_angle(input, length, BASIC_CALC_DEG, line);
}

basic_calc_status_t basic_calc_render_complex(const char *input, size_t length,
                                              basic_calc_angle_t angle,
                                              char top[16], char bottom[16])
{
    basic_calc_complex_t value;
    if (top == NULL || bottom == NULL) return BASIC_CALC_SYNTAX;
    basic_calc_status_t status = basic_calc_evaluate_complex(input, length, angle, &value);
    if (status == BASIC_CALC_OK) return basic_calc_format_complex(value, top, bottom);
    if (input != NULL && length <= 16) {
        memmove(top, input, length);
        memset(top + length, ' ', 16 - length);
    } else {
        write_message(top, "Complex input");
    }
    error_line(status, bottom);
    return status;
}
