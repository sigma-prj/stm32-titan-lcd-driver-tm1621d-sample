/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define GPIO_PIN_COUNT  GPIO_PIN_12
#define GPIO_PIN_DATA   GPIO_PIN_15

// Pins configuration for NUCLEO-F030R8 board
#define GPIO_PIN_CS     GPIO_PIN_11
#define GPIO_PIN_WR     GPIO_PIN_8

// Pins configuration for stand-alone STM32F030K6T6
/*
 #define GPIO_PIN_CS   GPIO_PIN_13
 #define GPIO_PIN_WR   GPIO_PIN_14
 */

#define CMD_SYS_ENABLE  0b100000000010
#define CMD_SYS_DISABLE 0b100000000000
#define CMD_LCD_ON      0b100000000110
#define CMD_LCD_OFF     0b100000000100
#define CMD_BIAS_INIT   0b100001010000  // 1/2 BIAS voltage, 1/4 duty cycle

#define CMD_SIZE        12              // LCD Driver command size in bits
#define DATA_SIZE       13              // LCD Driver data chunk size in bits

#define LCD_SEG_FIRST   9
#define LCD_SEG_LAST    14

#define SYMBOL_ID_E     10
#define SYMBOL_ID_R     11
#define SYMBOL_ID_BLANK 12

#define MICRO_SEC_SCALE 2                // Needs to be manually adjusted, depends on MCU and operating frequency
#define PIN_INPUT_DELAY 3                // LCD Driver input pins delay in microseconds (should be at least 2 microseconds)

#define LCD_OFF         0                // LCD State
#define LCD_ON          1                // LCD State

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

uint8_t lcd_matrix[LCD_SEG_LAST - LCD_SEG_FIRST + 1]; // Buffer used for transmission to LCD Driver

uint8_t lcd_symbols[] =                               // Supported LCD symbols
    {
        0b01111101, // '0'
        0b01100000, // '1'
        0b00111110, // '2'
        0b01111010, // '3'
        0b01100011, // '4'
        0b01011011, // '5'
        0b01011111, // '6'
        0b01110000, // '7'
        0b01111111, // '8'
        0b01111011, // '9'
        0b00011111, // 'E'
        0b00000110, // 'r'
        0b00000000  // Blank
    };

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void submit_bits_array(uint16_t bits, uint8_t count);  // Low-level implementation to send n-bits to LCD controller
static void submit_cmd(uint16_t cmd);                         // Sends command to LCD controller
static void submit_data(uint8_t addr, uint8_t data);          // Sends data chunk to LCD controller
static void submit_matrix();                                  // Sends entire matrix-buffer content into LCD controller

static void display_number(uint16_t number);                  // Displays 3-digit number on LCD
static void display_err();                                    // Displays 'Err' message on LCD
static void put_matrix_symbol(uint8_t symbol_pos, uint8_t symbol_val); // Places specific symbol into matrix-buffer under specific position

static void manual_delay_micro_secs(uint32_t micro_secs);     // Manual delay function (delay using basic for-cycle)

static void lcd_switch_off(void);                             // Sends corresponding set of commands to LCD Driver to switch-off LCD

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void submit_bits_array(uint16_t bits, uint8_t count)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_WR, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_CS, GPIO_PIN_RESET);
  manual_delay_micro_secs(PIN_INPUT_DELAY);

  for (uint8_t idx = 0; idx < count; ++idx)
  {
    uint16_t mask = (1 << (count - idx - 1));
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_WR, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_DATA,
        (bits & mask) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    manual_delay_micro_secs(PIN_INPUT_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_WR, GPIO_PIN_SET);
    manual_delay_micro_secs(PIN_INPUT_DELAY);
  }

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_CS, GPIO_PIN_SET);
  manual_delay_micro_secs(PIN_INPUT_DELAY);
}

static void submit_cmd(uint16_t cmd)
{
  submit_bits_array(cmd, CMD_SIZE);
}

static void submit_data(uint8_t addr, uint8_t data)
{
  uint16_t buffer = 0b1010000000000;
  buffer |= (uint16_t) data;
  buffer |= ((uint16_t) addr) << 4;
  submit_bits_array(buffer, DATA_SIZE);
}

static void submit_matrix()
{
  for (uint8_t idx = 0; idx <= (LCD_SEG_LAST - LCD_SEG_FIRST); ++idx)
  {
    submit_data(LCD_SEG_FIRST + idx, lcd_matrix[idx]);
  }
}

static void display_number(uint16_t number)
{
  if (number < 1000)
  {
    uint8_t digit_1 = number / 100;
    uint8_t digit_2 = (number / 10) % 10;
    uint8_t digit_3 = number % 10;
    put_matrix_symbol(0, !digit_1 ? SYMBOL_ID_BLANK : digit_1);
    put_matrix_symbol(1, (!digit_1 && !digit_2) ? SYMBOL_ID_BLANK : digit_2);
    put_matrix_symbol(2, digit_3);
    submit_matrix();
  }
  else
  {
    display_err();
  }
}

static void display_err()
{
  put_matrix_symbol(0, SYMBOL_ID_E);
  put_matrix_symbol(1, SYMBOL_ID_R);
  put_matrix_symbol(2, SYMBOL_ID_R);
  submit_matrix();
}

static void display_blank()
{
  put_matrix_symbol(0, SYMBOL_ID_BLANK);
  put_matrix_symbol(1, SYMBOL_ID_BLANK);
  put_matrix_symbol(2, SYMBOL_ID_BLANK);
  submit_matrix();
}

static void put_matrix_symbol(uint8_t symbol_pos, uint8_t symbol_val)
{
  // Setting LCD symbol in buffer - according to LCD module data-sheet. lcd_matrix array index - SEG #, bit position - COM #
  uint8_t seg_part_a = (2 - symbol_pos) << 1;
  uint8_t seg_part_b = seg_part_a + 1;
  lcd_matrix[seg_part_a] = lcd_symbols[symbol_val] >> 4;
  lcd_matrix[seg_part_b] = lcd_symbols[symbol_val] & 0x0F;
}

static void manual_delay_micro_secs(uint32_t micro_secs)
{
  uint32_t acc = 0;
  for (uint32_t i = 0; i < micro_secs * MICRO_SEC_SCALE; ++i)
  {
    acc += i;
  }
}

static void lcd_switch_off(void)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_CS, GPIO_PIN_SET);
  manual_delay_micro_secs(PIN_INPUT_DELAY);
  submit_cmd(CMD_LCD_OFF);
  submit_cmd(CMD_SYS_DISABLE);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // Reset LCD Driver in case of it is still running after last MCU reboot
  lcd_switch_off();
  display_blank();
  HAL_Delay(4000);

  // Deactivates CS line (sets "CS-high"), to make sure LCD driver will be ready to accept next "CS-low" activate signal
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_CS, GPIO_PIN_SET);
  // Minimum Output Pin delay time (according to Titan LCD Driver datasheet)
  manual_delay_micro_secs(PIN_INPUT_DELAY);

  // Activates LDC Driver system
  submit_cmd(CMD_SYS_ENABLE);
  // Initialization of 1/2 BIAS voltage level, 1/4 Multiplex.
  submit_cmd(CMD_BIAS_INIT);
  // Switches LCD on. 1/2 BIAS signals should appear on COM-1-4 pins at this point.
  submit_cmd(CMD_LCD_ON);
  // Displays sample number
  display_number(235);

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct =
  { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct =
  { 0 };

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
      | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct =
  { 0 };
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin | GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA8 PA11 PA15 */
  GPIO_InitStruct.Pin = LD2_Pin | GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
