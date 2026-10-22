#include "usb_display.h"
#include "main.h"
#include "lcd1602.h"
#include <string.h>

#define RX_SIZE 512U
#define RX_MASK (RX_SIZE - 1U)
static uint8_t rx_buffer[RX_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
volatile uint32_t usb_rx_bytes;
volatile uint32_t usb_rx_dropped;
volatile uint32_t usb_lcd_frames;
char usb_lcd_text[2][16];
static uint8_t row, column, started, dirty, visible;
static uint32_t last_refresh, last_blink;

static void clear_text(void)
{
    memset(usb_lcd_text, ' ', sizeof(usb_lcd_text));
    row = column = 0;
    dirty = 1;
}

static void next_line(void)
{
    if (row == 0) row = 1;
    else {
        memcpy(usb_lcd_text[0], usb_lcd_text[1], 16);
        memset(usb_lcd_text[1], ' ', 16);
    }
    column = 0;
    dirty = 1;
}

static void put_character(uint8_t ch)
{
    /* CRLF uses LF to advance. A bare CR returns to column zero. */
    if (ch == '\r') { column = 0; return; }
    if (ch == '\f') { clear_text(); started = 1; return; }
    if (!started) {
        clear_text();
        started = 1;
    }
    if (ch == '\n') { next_line(); return; }
    if (ch == '\b' || ch == 127) {
        if (column > 0) {
            --column;
            usb_lcd_text[row][column] = ' ';
            dirty = 1;
        }
        return;
    }
    if (ch == '\t') ch = ' ';
    if (ch < 32) return;
    if (ch > 126) ch = '?';
    if (column == 16) next_line();
    usb_lcd_text[row][column++] = (char)ch;
    dirty = 1;
}

void USB_Display_Init(void)
{
    rx_head = rx_tail = 0;
    usb_rx_bytes = usb_rx_dropped = 0;
    row = column = started = visible = 0;
    last_refresh = last_blink = 0;
    clear_text();
    lcd1602_init();
    lcd1602_write_lines(usb_lcd_text[0], usb_lcd_text[1]);
    usb_lcd_frames = 1;
    dirty = 0;
}

void USB_Display_Receive(const uint8_t *data, uint32_t length)
{
    /* Called only by the USB interrupt. Never call the LCD driver here. */
    for (uint32_t i = 0; i < length; ++i) {
        uint16_t next = (uint16_t)((rx_head + 1U) & RX_MASK);
        ++usb_rx_bytes;
        if (next == rx_tail) { ++usb_rx_dropped; continue; }
        rx_buffer[rx_head] = data[i];
        __DMB();
        rx_head = next;
    }
}

void USB_Display_Poll(void)
{
    uint8_t received = 0;
    while (rx_tail != rx_head) {
        uint8_t ch = rx_buffer[rx_tail];
        __DMB();
        rx_tail = (uint16_t)((rx_tail + 1U) & RX_MASK);
        put_character(ch);
        received = 1;
    }
    if (received) {
        visible = 1;
        dirty = 1;
    }
    uint32_t now = HAL_GetTick();
    if (visible && dirty && (uint32_t)(now - last_refresh) >= 10U) {
        lcd1602_write_lines(usb_lcd_text[0], usb_lcd_text[1]);
        ++usb_lcd_frames;
        dirty = 0;
        last_refresh = now;
    }
    if ((uint32_t)(now - last_blink) >= 500U) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        last_blink = now;
    }
}

void USB_Display_SetVisible(uint8_t show)
{
    if (show && !visible) dirty = 1;
    visible = show != 0;
}

uint8_t USB_Display_IsVisible(void)
{
    return visible;
}
