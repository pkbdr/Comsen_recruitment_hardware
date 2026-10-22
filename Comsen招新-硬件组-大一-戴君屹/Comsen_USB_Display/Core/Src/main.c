/* Basic touch calculator + USB CDC display for the Comsen STM32F103C8 board.
 * Clock/GPIO/TIM3 setup is reused from the upstream CubeMX example.
 * LCD driver is supplied by the upstream repository. Application is bare-metal.
 * Student GPIO readers retained below; AI integration is in the main loop,
 * calculator_ui.c, touch_keys.c, and basic_calc.c. Input helpers: input_text.c.
 */
#include "main.h"
#include "usb_device.h"
#include "usb_display.h"
#include "lcd1602.h"
#include "calculator_ui.h"
#include "touch_keys.h"
#include <string.h>
TIM_HandleTypeDef htim3;
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
static void delay_us(uint16_t us)
{
    uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);

    while ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim3) - start) < us)
    {
    }
}
static uint16_t read_touch_chip1(void)
{
  uint16_t keys1 = 0;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
  HAL_Delay(3);
  for(uint8_t i=0; i < 16; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    delay_us(10);
    GPIO_PinState level = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);
    if(level == GPIO_PIN_RESET)
    {
      keys1|=(1U<<i);
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    delay_us(10);
  }
  return keys1;
}
static uint16_t read_touch_chip2(void)
{
  uint16_t keys2 = 0;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
  HAL_Delay(3);
  for(uint8_t i=0; i < 16; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    delay_us(10);
    GPIO_PinState level = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9);
    if(level == GPIO_PIN_RESET)
    {
      keys2|=(1U<<i);
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    delay_us(10);
  }
  return keys2;
}
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM3_Init();
    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) Error_Handler();
    USB_Display_Init();
    /* Force USB disconnect before initialization, allowing enumeration after
       an SWD reset while the USB cable remains attached. */
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_12;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &pin);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_Delay(20);
    pin.Mode = GPIO_MODE_INPUT;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);
    MX_USB_DEVICE_Init();
    calculator_ui_t calculator;
    touch_keys_t keys;
    char previous_top[16], previous_bottom[16];
    uint8_t frame_valid = 0;
    uint8_t diagnostic = 0;
    uint32_t last_lcd = 0;
    calculator_ui_init(&calculator);
    touch_keys_init(&keys);
    while (1) {
        /* USB processing owns the LCD only after new PC data arrives. */
        USB_Display_Poll();
        uint16_t state1 = read_touch_chip1();
        uint16_t state2 = read_touch_chip2();
        uint32_t now = HAL_GetTick();
        uint32_t pressed = touch_keys_update(&keys, state1, state2, now);
        for (uint8_t key = 0; key < TOUCH_KEYS_COUNT; ++key) {
            if (!(pressed & (1UL << key))) continue;
            if (key == 2 && calculator.shifted && !diagnostic) {
                calculator_ui_key(&calculator, key); /* SHIFT+MODE: real/complex. */
                USB_Display_SetVisible(0);
            } else if (key == 2) { /* MODE: raw two-chip diagnostic / calculator. */
                diagnostic = !diagnostic;
                calculator.shifted = 0;
                USB_Display_SetVisible(0);
                frame_valid = 0;
            } else if (diagnostic || calculator_ui_key(&calculator, key)) {
                USB_Display_SetVisible(0);
            }
        }
        if (USB_Display_IsVisible()) {
            /* Invalidate cache so returning to unchanged calculator text redraws. */
            frame_valid = 0;
        } else {
            char top[16], bottom[16];
            if (diagnostic) {
                for (unsigned i = 0; i < 16; ++i) {
                    top[i] = state1 & (1U << i) ? '1' : '0';
                    bottom[i] = state2 & (1U << i) ? '1' : '0';
                }
            } else {
                calculator_ui_frame(&calculator, top, bottom);
            }
            if ((uint32_t)(now - last_lcd) >= 10U &&
                (!frame_valid || memcmp(top, previous_top, 16) ||
                 memcmp(bottom, previous_bottom, 16))) {
                lcd1602_write_lines(top, bottom);
                memcpy(previous_top, top, 16);
                memcpy(previous_bottom, bottom, 16);
                frame_valid = 1;
                last_lcd = now;
            }
        }
        HAL_Delay(1);
    }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6|GPIO_PIN_8, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB11 PB12 PB13
                           PB14 PB15 PB6 PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_6|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB7 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
