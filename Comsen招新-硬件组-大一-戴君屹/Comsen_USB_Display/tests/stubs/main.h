#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
#define GPIOC ((void *)0x40011000UL)
#define GPIO_PIN_13 ((uint16_t)0x2000)
#define __DMB() ((void)0)
uint32_t HAL_GetTick(void);
void HAL_GPIO_TogglePin(void *port, uint16_t pin);
#endif
