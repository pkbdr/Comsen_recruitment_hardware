#ifndef USB_DISPLAY_H
#define USB_DISPLAY_H
#include <stdint.h>
void USB_Display_Init(void);
void USB_Display_Receive(const uint8_t *data, uint32_t length);
void USB_Display_Poll(void);
/* New serial data selects USB view; a calculator key hides it until more data. */
void USB_Display_SetVisible(uint8_t visible);
uint8_t USB_Display_IsVisible(void);
#endif
