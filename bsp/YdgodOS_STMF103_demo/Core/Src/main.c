/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "oled_shell.h"
#include "task.h"
#include "yd_shell.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define YDGODOS_VERSION       "2.0.0"
#define SHELL_TASK_STACK      512U
#define OLED_TASK_STACK       256U
#define HEARTBEAT_TASK_STACK  128U
#define HEARTBEAT_PERIOD_MS   500U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
typedef enum {
  LED_MODE_AUTO = 0,
  LED_MODE_ON,
  LED_MODE_OFF
} LedMode;

static volatile LedMode led_mode = LED_MODE_AUTO;
static yd_shell_t os_shell;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void ShellTask(void *argument);
static void HeartbeatTask(void *argument);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int ConsoleRead(uint8_t *ch)
{
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) == RESET) {
    return 0;
  }
  return (HAL_UART_Receive(&huart1, ch, 1U, 0U) == HAL_OK) ? 1 : 0;
}

static void ConsoleWrite(const uint8_t *data, uint16_t length)
{
  if ((data != NULL) && (length != 0U)) {
    oled_shell_write_tx(data, length);
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, length, 1000U);
  }
}

static void LedWrite(uint8_t on)
{
  /* Blue Pill onboard LED on PC13 is active-low. */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static uint16_t CompleteLed(uint8_t argument_index, const char *prefix,
                            const char **candidates, uint16_t max_candidates)
{
  static const char *const modes[] = {"auto", "on", "off", "toggle"};
  uint16_t index;
  uint16_t count = 0U;

  (void)prefix;
  if (argument_index != 1U) {
    return 0U;
  }
  for (index = 0U;
       (index < (uint16_t)(sizeof(modes) / sizeof(modes[0]))) &&
       (count < max_candidates);
       index++) {
    candidates[count++] = modes[index];
  }
  return count;
}

static int CommandInfo(yd_shell_t *shell, int argc, char **argv)
{
  (void)argc;
  (void)argv;
  yd_shell_printf(shell,
                  "YdgodOS     : v%s\r\n"
                  "Kernel      : FreeRTOS %s\r\n"
                  "MCU         : STM32F103C8T6 (Cortex-M3)\r\n"
                  "System clock: %lu Hz\r\n"
                  "Console     : USART1, PA9/PA10, 115200 8N1\r\n"
                  "Heartbeat   : PC13, active-low, %lu ms\r\n",
                  YDGODOS_VERSION, tskKERNEL_VERSION_NUMBER,
                  HAL_RCC_GetHCLKFreq(), (uint32_t)HEARTBEAT_PERIOD_MS);
  return 0;
}

static int CommandUptime(yd_shell_t *shell, int argc, char **argv)
{
  uint32_t seconds = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
  uint32_t days = seconds / 86400U;
  uint32_t hours;
  uint32_t minutes;

  (void)argc;
  (void)argv;
  seconds %= 86400U;
  hours = seconds / 3600U;
  seconds %= 3600U;
  minutes = seconds / 60U;
  seconds %= 60U;
  yd_shell_printf(shell, "up %lu day(s), %02lu:%02lu:%02lu\r\n",
                  days, hours, minutes, seconds);
  return 0;
}

static char TaskStateLetter(eTaskState state)
{
  static const char letters[] = {'R', 'r', 'B', 'S', 'D', '?'};
  return letters[(state <= eInvalid) ? (unsigned int)state : 5U];
}

static int CommandTasks(yd_shell_t *shell, int argc, char **argv)
{
  TaskStatus_t status[8];
  UBaseType_t count;
  UBaseType_t index;
  uint32_t total_runtime;

  (void)argc;
  (void)argv;
  count = uxTaskGetSystemState(status, 8U, &total_runtime);
  yd_shell_puts(shell, "NAME             S PRI STACK_FREE\r\n");
  for (index = 0U; index < count; index++) {
    yd_shell_printf(shell, "%-16s %c %3lu %10u\r\n",
                    status[index].pcTaskName,
                    TaskStateLetter(status[index].eCurrentState),
                    (uint32_t)status[index].uxCurrentPriority,
                    (unsigned int)status[index].usStackHighWaterMark);
  }
  yd_shell_printf(shell, "%lu task(s); S: R=running r=ready B=blocked S=suspended\r\n",
                  (uint32_t)count);
  return 0;
}

static int CommandHeap(yd_shell_t *shell, int argc, char **argv)
{
  (void)argc;
  (void)argv;
  yd_shell_printf(shell, "heap free: %lu bytes, minimum ever: %lu bytes\r\n",
                  (uint32_t)xPortGetFreeHeapSize(),
                  (uint32_t)xPortGetMinimumEverFreeHeapSize());
  return 0;
}

static const char *LedModeName(void)
{
  if (led_mode == LED_MODE_ON) {
    return "on";
  }
  if (led_mode == LED_MODE_OFF) {
    return "off";
  }
  return "auto";
}

static int CommandLed(yd_shell_t *shell, int argc, char **argv)
{
  if (argc == 1) {
    yd_shell_printf(shell, "PC13 mode: %s, output: %s\r\n", LedModeName(),
                    (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ? "on" : "off");
    return 0;
  }
  if (strcmp(argv[1], "auto") == 0) {
    led_mode = LED_MODE_AUTO;
  } else if (strcmp(argv[1], "on") == 0) {
    led_mode = LED_MODE_ON;
    LedWrite(1U);
  } else if (strcmp(argv[1], "off") == 0) {
    led_mode = LED_MODE_OFF;
    LedWrite(0U);
  } else if (strcmp(argv[1], "toggle") == 0) {
    led_mode = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ?
               LED_MODE_OFF : LED_MODE_ON;
    LedWrite((led_mode == LED_MODE_ON) ? 1U : 0U);
  } else {
    yd_shell_puts(shell, "usage: led [auto|on|off|toggle]\r\n");
    return -1;
  }
  yd_shell_printf(shell, "PC13 mode: %s\r\n", LedModeName());
  return 0;
}

static int CommandReboot(yd_shell_t *shell, int argc, char **argv)
{
  (void)argc;
  (void)argv;
  yd_shell_puts(shell, "Rebooting...\r\n");
  vTaskDelay(pdMS_TO_TICKS(20U));
  NVIC_SystemReset();
  return 0;
}

static const yd_shell_command_t shell_commands[] = {
  {"info",   "info",                         "Show board and OS information", CommandInfo, NULL},
  {"uptime", "uptime",                       "Show system uptime", CommandUptime, NULL},
  {"tasks",  "tasks",                        "List FreeRTOS tasks", CommandTasks, NULL},
  {"heap",   "heap",                         "Show FreeRTOS heap usage", CommandHeap, NULL},
  {"led",    "led [auto|on|off|toggle]",      "Control the PC13 indicator", CommandLed, CompleteLed},
  {"reboot", "reboot",                       "Reset the MCU", CommandReboot, NULL}
};

static void PrintBootInfo(void)
{
  yd_shell_puts(&os_shell,"YdgodOS v" YDGODOS_VERSION " - STM32F103 edition\r\n"
    "[ OK ] System clock: 72 MHz\r\n"
    "[ OK ] Kernel: FreeRTOS " tskKERNEL_VERSION_NUMBER "\r\n"
    "[ OK ] Console: USART1 PA9(TX)/PA10(RX), 115200 8N1\r\n"
    "[ OK ] Run indicator: PC13 heartbeat (active-low)\r\n"
    "Type 'help' to list commands. Tab completes; Ctrl-L clears.\r\n\r\n"
    "\r\n"
    "\r\n"
    "      Ydgod OS\r\n"
    "\r\n"
    "        >'v'<\r\n"
    "\r\n"

    );
}

static void ShellTask(void *argument)
{
  (void)argument;
  yd_shell_init(&os_shell, ConsoleRead, ConsoleWrite,
                shell_commands,
                (uint16_t)(sizeof(shell_commands) / sizeof(shell_commands[0])),
                "~$");
  PrintBootInfo();
  yd_shell_prompt(&os_shell);
  for (;;) {
    yd_shell_poll(&os_shell);
    vTaskDelay(pdMS_TO_TICKS(1U));
  }
}

static void HeartbeatTask(void *argument)
{
  TickType_t last_wake = xTaskGetTickCount();
  (void)argument;
  for (;;) {
    if (led_mode == LED_MODE_AUTO) {
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    } else {
      LedWrite((led_mode == LED_MODE_ON) ? 1U : 0U);
    }
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
  }
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  if ((xTaskCreate(ShellTask, "shell", SHELL_TASK_STACK, NULL, 2U, NULL) != pdPASS) ||
      (xTaskCreate(oled_shell_task, "oled", OLED_TASK_STACK, NULL, 1U, NULL) != pdPASS) ||
      (xTaskCreate(HeartbeatTask, "heartbeat", HEARTBEAT_TASK_STACK, NULL, 1U, NULL) != pdPASS)) {
    Error_Handler();
  }
  vTaskStartScheduler();
  Error_Handler();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void vApplicationMallocFailedHook(void)
{
  Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
  (void)task;
  (void)task_name;
  Error_Handler();
}

void vAssertCalled(const char *file, int line)
{
  (void)file;
  (void)line;
  Error_Handler();
}

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
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
