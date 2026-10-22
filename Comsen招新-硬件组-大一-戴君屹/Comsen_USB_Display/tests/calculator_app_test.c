/* Host tests run the actual application modules, with HAL/LCD replaced by spies. */
#include "basic_calc.h"
#include "calculator_ui.h"
#include "input_text.h"
#include "touch_keys.h"
#include "usb_display.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t fake_now;
static unsigned lcd_writes;
static char lcd_top[16], lcd_bottom[16];
extern volatile uint32_t usb_rx_bytes, usb_rx_dropped;
extern char usb_lcd_text[2][16];

uint32_t HAL_GetTick(void) { return fake_now; }
void HAL_GPIO_TogglePin(void *port, uint16_t pin) { (void)port; (void)pin; }
void lcd1602_init(void) {}
void lcd1602_write_lines(const char top[16], const char bottom[16])
{
    memcpy(lcd_top, top, 16);
    memcpy(lcd_bottom, bottom, 16);
    ++lcd_writes;
}

static void expect_line(const char line[16], const char *text)
{
    char expected[16];
    assert(strlen(text) <= 16);
    memset(expected, ' ', 16);
    memcpy(expected, text, strlen(text));
    assert(memcmp(line, expected, 16) == 0);
}

/* Physical key -> chip/channel, independently transcribed from KEY_MAPPING.md. */
static const uint8_t channels[30] = {
    4,3,7,12,11,5,2,0,13,10,6,1,15,14,9,
    1,4,7,11,14,2,5,8,10,13,3,6,0,9,12
};

static uint32_t scan(touch_keys_t *keys, uint8_t physical, int down, uint32_t now)
{
    uint16_t a = 0, b = 0;
    if (down) {
        if (physical < 15) a = (uint16_t)(1U << channels[physical]);
        else b = (uint16_t)(1U << channels[physical]);
    }
    return touch_keys_update(keys, a, b, now);
}

static void press(calculator_ui_t *ui, touch_keys_t *keys, uint8_t key)
{
    fake_now += 40;
    assert(scan(keys, key, 1, fake_now) == 0);
    uint32_t event = scan(keys, key, 1, fake_now + 30);
    assert(event == (1UL << key));
    assert(calculator_ui_key(ui, key) == 1);
    /* A held key must not repeat. */
    assert(scan(keys, key, 1, fake_now + 200) == 0);
    assert(scan(keys, key, 0, fake_now + 210) == 0);
    assert(scan(keys, key, 0, fake_now + 240) == 0);
    fake_now += 240;
}

static void check_keys_and_calculator(void)
{
    touch_keys_t keys;
    calculator_ui_t ui;
    touch_keys_init(&keys);
    calculator_ui_init(&ui);
    for (uint8_t i = 0; i < 30; ++i) {
        uint16_t raw = (uint16_t)(1U << channels[i]);
        assert(touch_keys_map(i < 15 ? raw : 0, i < 15 ? 0 : raw) == (1UL << i));
    }
    assert(touch_keys_map(0xffff, 0xffff) == 0x3fffffffUL);
    assert(touch_keys_map(1U << 8, 1U << 15) == 0); /* unused pins */
    /* End-to-end through raw scans, debounce, UI, parser and fixed LCD frame. */
    press(&ui, &keys, 20); /* 1 */
    press(&ui, &keys, 23); /* + */
    press(&ui, &keys, 21); /* 2 */
    press(&ui, &keys, 18); /* * */
    press(&ui, &keys, 22); /* 3 */
    press(&ui, &keys, 4);  /* top OK */
    char top[16], bottom[16];
    calculator_ui_frame(&ui, top, bottom);
    expect_line(top, "1+2*3");
    expect_line(bottom, "7");
    press(&ui, &keys, 4); /* repeated OK leaves result and expression intact */
    assert(ui.input_len == 5);
    press(&ui, &keys, 25); /* digit after success starts fresh */
    expect_line(ui.input, "0");
    press(&ui, &keys, 14); /* AC */
    expect_line(ui.input, "");
    assert(ui.input_len == 0 && !ui.has_result);
    press(&ui, &keys, 13); /* DEL on empty */
    assert(ui.input_len == 0);
    press(&ui, &keys, 20);
    press(&ui, &keys, 21);
    press(&ui, &keys, 13);
    expect_line(ui.input, "1");
    press(&ui, &keys, 19); /* / */
    press(&ui, &keys, 25);
    press(&ui, &keys, 29); /* original calculate key */
    expect_line(ui.result, "Divide by zero");
    press(&ui, &keys, 13);
    press(&ui, &keys, 21);
    press(&ui, &keys, 29);
    expect_line(ui.result, "0.5");
    press(&ui, &keys, 14);
    press(&ui, &keys, 20);
    press(&ui, &keys, 23);
    press(&ui, &keys, 29);
    expect_line(ui.result, "Syntax ERROR");
    press(&ui, &keys, 21); /* complete an incomplete expression after error */
    press(&ui, &keys, 29);
    expect_line(ui.result, "3");
    press(&ui, &keys, 14);
    press(&ui, &keys, 29);
    expect_line(ui.result, "Empty input");
    press(&ui, &keys, 14);
    for (unsigned i = 0; i < 16; ++i) press(&ui, &keys, 20);
    assert(ui.input_len == 16);
    press(&ui, &keys, 21);
    assert(ui.input_len == 16);
    expect_line(ui.input, "1111111111111111");
    expect_line(ui.result, "Input full");

    press(&ui, &keys, 13);
    press(&ui, &keys, 21);
    expect_line(ui.input, "1111111111111112");
    assert(calculator_ui_key(&ui, 255) == 0);
    assert(calculator_ui_key(&ui, 0) == 1 && ui.shifted);
    assert(calculator_ui_key(&ui, 0) == 1 && !ui.shifted);

    /* Stage 5: actual SHIFT/button entry, parentheses and angle selection. */
    press(&ui, &keys, 14);
    const uint8_t grouped[] = {5,20,23,21,6,18,22,4}; /* (1+2)*3 */
    for (unsigned i = 0; i < sizeof(grouped); ++i) press(&ui, &keys, grouped[i]);
    expect_line(ui.result, "9");
    press(&ui, &keys, 14);
    const uint8_t sine[] = {0,20,22,25,6,4}; /* SHIFT+1, 30, ), OK */
    for (unsigned i = 0; i < sizeof(sine); ++i) press(&ui, &keys, sine[i]);
    expect_line(ui.input, "sin(30)");
    expect_line(ui.result, "0.5");
    press(&ui, &keys, 0);
    calculator_ui_frame(&ui, top, bottom);
    expect_line(bottom, "SHIFT DEG");
    press(&ui, &keys, 11); /* SHIFT+8 switches unit, reevaluates displayed input */
    assert(ui.radians);
    expect_line(ui.result, "-0.988031624");
    press(&ui, &keys, 14);
    assert(ui.radians); /* AC preserves angle mode */
    const uint8_t rad_sine[] = {0,20,0,10,19,21,6,29}; /* sin(pi/2) */
    for (unsigned i = 0; i < sizeof(rad_sine); ++i) press(&ui, &keys, rad_sine[i]);
    expect_line(ui.input, "sin(pi/2)");
    expect_line(ui.result, "1");
    press(&ui, &keys, 14);
    const uint8_t root[] = {0,19,12,6,4}; /* sqrt(9) */
    for (unsigned i = 0; i < sizeof(root); ++i) press(&ui, &keys, root[i]);
    expect_line(ui.result, "3");
    press(&ui, &keys, 14);
    const uint8_t logarithm[] = {0,16,20,25,25,6,4}; /* log(100) */
    for (unsigned i = 0; i < sizeof(logarithm); ++i) press(&ui, &keys, logarithm[i]);
    expect_line(ui.result, "2");
    press(&ui, &keys, 14);
    const uint8_t natural_log[] = {0,17,0,15,6,4}; /* ln(e) */
    for (unsigned i = 0; i < sizeof(natural_log); ++i) press(&ui, &keys, natural_log[i]);
    expect_line(ui.result, "1");
    press(&ui, &keys, 14);
    const uint8_t powers[] = {21,0,18,22,0,18,21,4}; /* 2^3^2 */
    for (unsigned i = 0; i < sizeof(powers); ++i) press(&ui, &keys, powers[i]);
    expect_line(ui.result, "512");
    press(&ui, &keys, 14);
    for (unsigned i = 0; i < 14; ++i) press(&ui, &keys, 20);
    press(&ui, &keys, 0);
    press(&ui, &keys, 20); /* sin( must not partially fit into two remaining cells */
    assert(ui.input_len == 14);
    expect_line(ui.input, "11111111111111");
    expect_line(ui.result, "Input full");

    /* Stage 6: mode toggle and i entry flow through the real raw key scanner. */
    press(&ui, &keys, 14);
    press(&ui, &keys, 0);
    press(&ui, &keys, 2); /* SHIFT+MODE */
    assert(ui.complex_mode);
    const uint8_t complex_product[] = {5,20,23,0,12,6,18,5,20,24,0,12,6,4};
    for (unsigned i = 0; i < sizeof(complex_product); ++i) press(&ui, &keys, complex_product[i]);
    expect_line(ui.input, "(1+i)*(1-i)");
    calculator_ui_frame(&ui, top, bottom);
    expect_line(top, "Re=2");
    expect_line(bottom, "Im=0");
    press(&ui, &keys, 14);
    assert(ui.complex_mode); /* AC preserves real/complex as well as angle mode */
    const uint8_t complex_root[] = {0,19,24,20,6,4}; /* sqrt(-1) */
    for (unsigned i = 0; i < sizeof(complex_root); ++i) press(&ui, &keys, complex_root[i]);
    calculator_ui_frame(&ui, top, bottom);
    expect_line(top, "Re=0");
    expect_line(bottom, "Im=1");
    press(&ui, &keys, 0);
    press(&ui, &keys, 2); /* switch to real and reevaluate */
    assert(!ui.complex_mode && !ui.result_ok);
    expect_line(ui.result, "Math ERROR");
    press(&ui, &keys, 0);
    press(&ui, &keys, 2);
    assert(ui.complex_mode && ui.result_ok);
    calculator_ui_frame(&ui, top, bottom);
    expect_line(bottom, "Im=1");
    calculator_ui_init(&ui); /* real default; typing i automatically enables complex */
    press(&ui, &keys, 21);
    press(&ui, &keys, 0);
    press(&ui, &keys, 12);
    assert(ui.complex_mode);
    press(&ui, &keys, 4);
    calculator_ui_frame(&ui, top, bottom);
    expect_line(top, "Re=0");
    expect_line(bottom, "Im=2");
    press(&ui, &keys, 14);
    const uint8_t complex_div_zero[] = {0,12,19,25,4}; /* i/0 */
    for (unsigned i = 0; i < sizeof(complex_div_zero); ++i) press(&ui, &keys, complex_div_zero[i]);
    calculator_ui_frame(&ui, top, bottom);
    expect_line(top, "i/0");
    expect_line(bottom, "Divide by zero");
    press(&ui, &keys, 13);
    press(&ui, &keys, 21);
    press(&ui, &keys, 4);
    calculator_ui_frame(&ui, top, bottom);
    expect_line(top, "Re=0");
    expect_line(bottom, "Im=0.5");
    press(&ui, &keys, 14);
    for (unsigned i = 0; i < 14; ++i) press(&ui, &keys, 20);
    press(&ui, &keys, 0);
    press(&ui, &keys, 20);
    calculator_ui_frame(&ui, top, bottom);
    expect_line(top, "11111111111111");
    expect_line(bottom, "Input full");

    touch_keys_init(&keys);
    assert(scan(&keys, 20, 1, 100) == 0);
    assert(scan(&keys, 20, 0, 110) == 0);
    assert(scan(&keys, 20, 1, 120) == 0);
    assert(scan(&keys, 20, 1, 149) == 0);
    assert(scan(&keys, 20, 1, 150) == (1UL << 20));
    assert(scan(&keys, 20, 1, 10000) == 0);
    /* Unsigned timestamp subtraction remains valid when HAL tick wraps. */
    touch_keys_init(&keys);
    assert(scan(&keys, 20, 1, UINT32_MAX - 10U) == 0);
    assert(scan(&keys, 20, 1, 18) == 0);
    assert(scan(&keys, 20, 1, 19) == (1UL << 20));
    touch_keys_init(&keys);
    uint16_t simultaneous = (1U << 2) | (1U << 5);
    assert(touch_keys_update(&keys, 0, simultaneous, 0) == 0);
    assert(touch_keys_update(&keys, 0, simultaneous, 31) == 0);
}

static void receive(const char *bytes, size_t length)
{
    USB_Display_Receive((const uint8_t *)bytes, (uint32_t)length);
    fake_now += 20;
    USB_Display_Poll();
}

static void check_usb_and_screen_ownership(void)
{
    fake_now = 0;
    USB_Display_Init();
    assert(!USB_Display_IsVisible());
    receive("HEL", 3);
    receive("LO\r\nWORLD", 9);
    expect_line(lcd_top, "HELLO");
    expect_line(lcd_bottom, "WORLD");
    assert(USB_Display_IsVisible());
    USB_Display_SetVisible(0);
    lcd1602_write_lines("1+2             ", "3               ");
    unsigned before = lcd_writes;
    fake_now += 20;
    USB_Display_Poll();
    assert(lcd_writes == before);
    expect_line(lcd_top, "1+2"); /* old serial data must not overwrite calculator */
    receive("\fUSB", 4);
    assert(USB_Display_IsVisible());
    expect_line(lcd_top, "USB");
    receive("\bX", 2);
    expect_line(lcd_top, "USX");
    receive("\f1234567890123456", 17);
    receive("\nA", 2); /* exactly-full row then newline advances only once */
    expect_line(lcd_top, "1234567890123456");
    expect_line(lcd_bottom, "A");
    receive("\f1234567890123456ABCDEFGHIJKLMNOPZ", 34);
    expect_line(lcd_top, "ABCDEFGHIJKLMNOP");
    expect_line(lcd_bottom, "Z");

    USB_Display_Init();
    char burst[600];
    memset(burst, 'X', sizeof(burst));
    USB_Display_Receive((const uint8_t *)burst, sizeof(burst));
    assert(usb_rx_bytes == 600 && usb_rx_dropped == 89);
    fake_now += 20;
    USB_Display_Poll();
    expect_line(lcd_top, "XXXXXXXXXXXXXXXX");
    expect_line(lcd_bottom, "XXXXXXXXXXXXXXX");
    receive("\fWRAP", 5); /* exercise head/tail wrap after the full queue */
    expect_line(lcd_top, "WRAP");
    assert(usb_rx_dropped == 89);
}

int main(void)
{
    check_keys_and_calculator();
    check_usb_and_screen_ownership();
    puts("PASS: all 30 mappings, raw-key calculator flows, input boundaries, "
         "debounce/hold/tick-wrap, USB controls/FIFO and LCD ownership");
    return 0;
}
