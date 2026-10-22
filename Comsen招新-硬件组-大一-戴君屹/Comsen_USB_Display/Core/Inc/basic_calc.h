#ifndef BASIC_CALC_H
#define BASIC_CALC_H

#include <stddef.h>

#define BASIC_CALC_INPUT_CAPACITY 16U
#define BASIC_CALC_LCD_WIDTH 16U

typedef enum {
    BASIC_CALC_OK = 0,
    BASIC_CALC_EMPTY,
    BASIC_CALC_SYNTAX,
    BASIC_CALC_DIV_ZERO,
    BASIC_CALC_RANGE,
    BASIC_CALC_DOMAIN
} basic_calc_status_t;

typedef enum { BASIC_CALC_DEG = 0, BASIC_CALC_RAD } basic_calc_angle_t;

typedef struct { double real, imag; } basic_calc_complex_t;

/* AI-assisted arithmetic module, separate from the student's input helpers.
 * Input is a bounded byte array, not necessarily a NUL-terminated string.
 * Grammar: decimals, one optional unary sign, + - * / ^, parentheses,
 * sin/cos/tan/log/ln/sqrt, pi/e constants, spaces/tabs. No implicit products.
 * Default trigonometric angle unit is degrees; _angle APIs also accept radians.
 * On failure, *result is left unchanged. No dynamic allocation or HAL needed.
 */
basic_calc_status_t basic_calc_evaluate(const char *input, size_t length,
                                        double *result);
basic_calc_status_t basic_calc_evaluate_angle(const char *input, size_t length,
                                              basic_calc_angle_t angle,
                                              double *result);
/* Complex mode uses principal branches. Numeric imaginary literals such as
 * 2i are accepted; general implicit multiplication remains unsupported.
 * Result is left unchanged on failure, just as for the real API.
 */
basic_calc_status_t basic_calc_evaluate_complex(const char *input, size_t length,
                                                basic_calc_angle_t angle,
                                                basic_calc_complex_t *result);

/* Writes exactly 16 LCD characters, padded with spaces; no NUL terminator.
 * Rounds to 9 significant decimal digits, using scientific notation as needed.
 * The output buffer must have room for 16 characters.
 */
basic_calc_status_t basic_calc_format(double value, char line[BASIC_CALC_LCD_WIDTH]);

/* Convenience interface for the OK key: result or readable ASCII error text. */
basic_calc_status_t basic_calc_render(const char *input, size_t length,
                                      char line[BASIC_CALC_LCD_WIDTH]);
basic_calc_status_t basic_calc_render_angle(const char *input, size_t length,
                                            basic_calc_angle_t angle,
                                            char line[BASIC_CALC_LCD_WIDTH]);
/* Complex result is split into Re=/Im= lines, 6 significant digits per part. */
basic_calc_status_t basic_calc_format_complex(basic_calc_complex_t value,
                                              char top[16], char bottom[16]);
basic_calc_status_t basic_calc_render_complex(const char *input, size_t length,
                                              basic_calc_angle_t angle,
                                              char top[16], char bottom[16]);

#endif
