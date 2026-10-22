/* AI-written integration of the student's input helpers and basic_calc. */
#include "calculator_ui.h"
#include "basic_calc.h"
#include "input_text.h"
#include <string.h>

static void reset_result(calculator_ui_t *ui)
{
    memset(ui->result, ' ', sizeof(ui->result));
    memset(ui->result_top, ' ', sizeof(ui->result_top));
    ui->has_result = 0;
    ui->result_ok = 0;
}

void calculator_ui_init(calculator_ui_t *ui)
{
    clear_input(ui->input, &ui->input_len);
    reset_result(ui);
    ui->shifted = 0;
    ui->radians = 0;
    ui->complex_mode = 0;
}

static void evaluate(calculator_ui_t *ui)
{
    basic_calc_angle_t angle = ui->radians ? BASIC_CALC_RAD : BASIC_CALC_DEG;
    if (ui->complex_mode) {
        ui->result_ok = basic_calc_render_complex(ui->input, ui->input_len, angle,
                                                 ui->result_top, ui->result)
                        == BASIC_CALC_OK;
    } else {
        ui->result_ok = basic_calc_render_angle(ui->input, ui->input_len, angle, ui->result)
                        == BASIC_CALC_OK;
    }
    ui->has_result = 1;
}

static void append_token(calculator_ui_t *ui, const char *token, uint8_t starts_number)
{
    size_t count = strlen(token);
    if (ui->has_result && ui->result_ok && starts_number) {
        clear_input(ui->input, &ui->input_len);
    }
    reset_result(ui);
    if (count > sizeof(ui->input) - ui->input_len) {
        memcpy(ui->result, "Input full      ", sizeof(ui->result));
        memcpy(ui->result_top, ui->input, sizeof(ui->result_top));
        ui->has_result = 1;
        return;
    }
    /* All-or-nothing capacity check above prevents partial function names. */
    for (size_t i = 0; i < count; ++i) append_input(ui->input, &ui->input_len, token[i]);
}

uint8_t calculator_ui_key(calculator_ui_t *ui, uint8_t key)
{
    static const char characters[30] = {
        0, 0, 0, 0, 0, '(', ')', 0, 0, 0,
        '7', '8', '9', 0, 0, '4', '5', '6', '*', '/',
        '1', '2', '3', '+', '-', '0', '.', 0, 0, 0
    };
    char ch;
    if (key >= 30) return 0;
    if (key == 0) { /* SHIFT is a one-shot prefix; press again to cancel. */
        ui->shifted = !ui->shifted;
        return 1;
    }
    if (key == 14) { /* AC */
        clear_input(ui->input, &ui->input_len);
        reset_result(ui);
        ui->shifted = 0; /* Preserve the selected angle/real-complex modes on AC. */
        return 1;
    }
    if (ui->shifted) {
        ui->shifted = 0;
        if (key == 2) { /* SHIFT+MODE toggles real/complex without entering diagnostics. */
            ui->complex_mode = !ui->complex_mode;
            if (ui->has_result) evaluate(ui);
            return 1;
        }
        if (key == 11) { /* SHIFT+8: angle unit, matching the angle key legend. */
            ui->radians = !ui->radians;
            if (ui->has_result) evaluate(ui);
            return 1;
        }
        const char *token = NULL;
        switch (key) {
        case 10: token = "pi"; break; /* SHIFT+7 */
        case 12: token = "i"; ui->complex_mode = 1; break; /* SHIFT+9 */
        case 15: token = "e"; break;  /* SHIFT+4 */
        case 16: token = "log("; break; /* SHIFT+5 */
        case 17: token = "ln("; break; /* SHIFT+6 */
        case 18: token = "^"; break; /* SHIFT+multiply */
        case 19: token = "sqrt("; break; /* SHIFT+divide */
        case 20: token = "sin("; break; /* SHIFT+1 */
        case 21: token = "cos("; break; /* SHIFT+2 */
        case 22: token = "tan("; break; /* SHIFT+3 */
        default: break;
        }
        if (token != NULL) {
            append_token(ui, token, token[0] != '^');
            return 1;
        }
        /* Other keys cancel SHIFT and retain their ordinary action. */
    }
    if (key == 13 || key == 1) { /* DEL / BACK */
        delete_input(ui->input, &ui->input_len);
        reset_result(ui);
        return 1;
    }
    if (key == 4 || key == 29) { /* Both top OK and original evaluate key. */
        evaluate(ui);
        return 1;
    }
    ch = characters[key];
    if (ch == 0) return 0;
    char token[2] = {ch, '\0'};
    append_token(ui, token, (ch >= '0' && ch <= '9') || ch == '.' || ch == '(');
    return 1;
}

void calculator_ui_frame(const calculator_ui_t *ui, char top[16], char bottom[16])
{
    memcpy(top, ui->has_result && ui->complex_mode && !ui->shifted
                ? ui->result_top : ui->input, 16);
    if (ui->shifted) {
        if (ui->complex_mode) memcpy(bottom, ui->radians ? "SHIFT CPLX RAD  " : "SHIFT CPLX DEG  ", 16);
        else memcpy(bottom, ui->radians ? "SHIFT RAD       " : "SHIFT DEG       ", 16);
    }
    else if (ui->has_result) memcpy(bottom, ui->result, 16);
    else memset(bottom, ' ', 16);
}
