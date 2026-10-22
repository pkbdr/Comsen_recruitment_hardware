#ifndef CALCULATOR_UI_H
#define CALCULATOR_UI_H
#include <stdint.h>

typedef struct {
    char input[16];
    char result[16];
    char result_top[16];
    uint8_t input_len;
    uint8_t has_result;
    uint8_t result_ok;
    uint8_t shifted;
    uint8_t radians;
    uint8_t complex_mode;
} calculator_ui_t;

void calculator_ui_init(calculator_ui_t *ui);
/* Physical T0..T29 key number. Returns 1 for a handled calculator key. */
uint8_t calculator_ui_key(calculator_ui_t *ui, uint8_t key);
void calculator_ui_frame(const calculator_ui_t *ui, char top[16], char bottom[16]);
#endif
