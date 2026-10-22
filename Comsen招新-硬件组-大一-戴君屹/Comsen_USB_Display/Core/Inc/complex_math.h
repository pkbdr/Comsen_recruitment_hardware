#ifndef COMPLEX_MATH_H
#define COMPLEX_MATH_H
#include "basic_calc.h"

/* Internal arithmetic backend shared by the real and complex parser. */
basic_calc_status_t calc_value_binary(basic_calc_complex_t a,
                                       basic_calc_complex_t b, char operation,
                                       int allow_complex, basic_calc_complex_t *out);
/* Function numbers: sin, cos, tan, log10, ln, sqrt (0..5). */
basic_calc_status_t calc_value_function(unsigned function, basic_calc_angle_t angle,
                                         int allow_complex, basic_calc_complex_t *value);
#endif
