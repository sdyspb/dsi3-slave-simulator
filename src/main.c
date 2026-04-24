/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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
#include "gpio.h"  // Added GPIO header
#include "tim.h"
#include "tim2.h"  // Include header for TIM2
#include "spi.h"  // Added SPI header
#include "dma.h"  // Added DMA header
#include "usart.h"
#include "comp.h"  // Added COMP header
#include "string.h"
#include "stdio.h"
#include "stdarg.h"
#include "encoding.h" // Added for response encoding

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */
extern SPI_HandleTypeDef hspi2;
/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CLI_BUFFER_SIZE 128
#define HEARTBEAT_INTERVAL 500  // ms
#define CHIP_DURATION_US 6.35f  // Duration of one chip in microseconds
#define SYMBOL_DURATION_US (CHIP_DURATION_US * 3)  // Duration of one symbol (3 chips) in microseconds
#define MIN_CHIP_DURATION_US 2.7f  // Minimum chip duration in microseconds
#define MAX_CHIP_DURATION_US 10.0f // Maximum chip duration in microseconds
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
SPI_HandleTypeDef hspi2;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

// Variables for response transmission
volatile uint8_t response_active = 0;           // Flag indicating if response transmission is active
volatile uint8_t response_chips[3];             // Array to store the 3 chips of the response
volatile uint8_t current_chip_index = 0;        // Index of current chip being transmitted
volatile uint8_t current_chip_position = 0;     // Position within the current chip (0-2)
/* USER CODE END PV */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
UART_HandleTypeDef huart1;
uint8_t cli_buffer[CLI_BUFFER_SIZE];
uint32_t cli_idx = 0;
uint8_t rx_data;
uint32_t last_heartbeat_toggle = 0;
uint8_t heartbeat_enabled = 1;

uint16_t adc_buffer[ADC_BUFFER_SIZE] __attribute__((section(".dma_buffer"), aligned(32))); // Buffer to store ADC samples in DMA-accessible memory
uint8_t comp_started = 0; // Flag to track if comparator has been started
uint8_t spi_rx_complete_flag = 0; // Flag set in HAL_SPI_RxCpltCallback to indicate completion
uint32_t led_turn_on_time = 0; // Time when LED was turned on
uint8_t red_led_state = 0; // Current state of the red LED (0 = off, 1 = on)
uint8_t comparator_triggered_flag = 0; // Flag set in comparator interrupt to indicate trigger
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
void CLI_ProcessCommand(char *cmd);
void CLI_SendString(char *str);
void CLI_PrintHelp(void);
void CLI_PrintSystemInfo(void);
void CLI_SendPrompt(void);
void Response_SetValue(uint8_t value);
void Response_SetChipValue(uint8_t chip_num, uint8_t value);
void Response_Timer_Init(void);
void CLI_TestResp(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_DMA_Init();   // Insert DMA initialization here. It should be done before ADC initialization to ensure DMA is ready for ADC operations
  MX_SPI2_Init();  // Call SPI2 initialization. It should be done after DMA initialization since SPI2 uses DMA for ADC data reception
  MX_USART1_UART_Init();  // Then USART1
  MX_COMP1_Init(); // Initialize comparator
  MX_TIM2_Init();  // Initialize TIM2 for high precision timing
  Response_Timer_Init(); // Initialize response timer
  
  /* USER CODE BEGIN 2 */
  
  // Initialize adc_buffer with ascending values starting from zero
  for(int i = 0; i < ADC_BUFFER_SIZE; i++) {
      adc_buffer[i] = 0xDEAD; // Fill with a known pattern for testing
  }
  
  // Turn OFF heartbeat LED initially (set to HIGH for active-low LED)
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
  
  // Don't start comparator automatically - wait for explicit CLI command
  // Start comparator after all initializations are complete
  // if(HAL_COMP_Start(&hcomp1) != HAL_OK)
  // {
  //     // Error handling - could add error indication if needed
  //     CLI_SendString("\r\nError: Comparator start failed.\r\n");
  // }
  
  // Small delay to ensure UART is ready
  HAL_Delay(100);
  
  // Send welcome message via USART1
  CLI_SendString("================================\r");
  CLI_SendString("DSI3 Slave Simulator Started\r");
  CLI_SendString("Type 'help' for available commands\r");
  CLI_SendString("================================\r");
  CLI_SendPrompt();

  char dbg[64];
  sprintf(dbg, "ADC buffer addr: %p\r", adc_buffer);
  CLI_SendString(dbg);
  CLI_SendPrompt();

  if ((uint32_t)adc_buffer % 32 != 0) {
    CLI_SendString("Error: adc_buffer is not 32-byte aligned!\r");
  } else {
    CLI_SendString("OK: adc_buffer is 32-byte aligned.\r");
  }
  CLI_SendPrompt();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Handle incoming CLI commands
    // Use a small timeout to prevent blocking
    if (HAL_UART_Receive(&huart1, &rx_data, 1, 10) == HAL_OK) {
      if (rx_data == '\r' || rx_data == '\n') {
        if (cli_idx > 0) {
          cli_buffer[cli_idx] = '\0';
          CLI_ProcessCommand((char*)cli_buffer);
          cli_idx = 0;
          CLI_SendPrompt();
        }
      } else if (rx_data >= 32 && rx_data <= 126) {  // Printable ASCII chars
        if (cli_idx < CLI_BUFFER_SIZE - 1) {
          cli_buffer[cli_idx++] = rx_data;
          // Echo the character back
          HAL_UART_Transmit(&huart1, &rx_data, 1, HAL_MAX_DELAY);
        }
      } else if (rx_data == 127 || rx_data == 8) {  // Backspace or DEL
        if (cli_idx > 0) {
          cli_idx--;
          // Echo backspace
          uint8_t bs_char[] = {0x08, 0x20, 0x08}; // Move back, erase, move back
          HAL_UART_Transmit(&huart1, bs_char, 3, HAL_MAX_DELAY);
        }
      }
    }

    // Heartbeat LED toggle
    if (heartbeat_enabled && ((HAL_GetTick() - last_heartbeat_toggle) >= HEARTBEAT_INTERVAL)) {
      // For active-low LED, toggle by inverting the current state
      if(HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == GPIO_PIN_RESET) {
        // LED currently ON (active low), turn it OFF
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
      } else {
        // LED currently OFF (inactive), turn it ON
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
      }
      last_heartbeat_toggle = HAL_GetTick();
    }

    /* USER CODE BEGIN 3 */
    
    // Handle red LED control based on SPI Rx completion
    if (spi_rx_complete_flag) {
        // Turn on red LED (GPIO_PD3) when SPI reception completes
        HAL_GPIO_WritePin(GPIOD, RED_LED_Pin, GPIO_PIN_RESET); // Active-low, so RESET = ON
        red_led_state = 1;
        led_turn_on_time = HAL_GetTick(); // Record when LED was turned on
        spi_rx_complete_flag = 0; // Reset the flag immediately
    }
    
    // Check if 200ms have passed since turning on the LED, then turn it off
    if (red_led_state && (HAL_GetTick() - led_turn_on_time >= 200)) {
        HAL_GPIO_WritePin(GPIOD, RED_LED_Pin, GPIO_PIN_SET); // Turn off the LED
        red_led_state = 0;
    }
    
    // Check if comparator was triggered and start SPI DMA if needed
    if (comparator_triggered_flag) {
        comparator_triggered_flag = 0; // Reset the flag immediately
        
        // Start DMA receive for ADC_BUFFER_SIZE samples (each sample is 16-bit)
        if (HAL_SPI_Receive_DMA(&hspi2, (uint8_t*)adc_buffer, ADC_BUFFER_SIZE) != HAL_OK) {
            // Error handling could be added here if needed
            // For now, we just continue
        }
    }
    
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 64;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* MPU Configuration */
void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  // Configure region 0 - D2 SRAM - non-cacheable (for ADC buffer accessible by DMA)
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x30000000; // Base address of D2 SRAM
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB; // Size of D2 SRAM available
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE; // Non-cacheable memory for DMA
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  Send test response for BADC0DB4 symbol without pauses between symbols
  * @param  None
  * @retval None
  */
void CLI_TestResp(void)
{
  CLI_SendString("Starting test response transmission...\r\n");
  
  // Symbol to transmit: BADC0DB4
  // Breaking down into hex digits: B A D C 0 D B 4
  uint8_t test_symbols[] = {0xB, 0xA, 0xD, 0xC, 0x0, 0xD, 0xB, 0x4};
  
  CLI_SendString("Transmitting symbols: ");
  
  // Print all hex digits being transmitted at once
  for(int i = 0; i < 8; i++)
  {
    uint8_t value = test_symbols[i];
    char hex_char[2] = {0};
    if(value < 10) {
      hex_char[0] = '0' + value;
    } else {
      hex_char[0] = 'A' + (value - 10);
    }
    CLI_SendString(hex_char);
  }
  CLI_SendString("\r\n");
  
  // Transmit 8 three-chip symbols without pauses between transmissions
  for(int i = 0; i < 8; i++)
  {
    uint8_t value = test_symbols[i];
    
    // Transmit the symbol using timer
    Response_SetValue(value);
    
    // Wait for transmission to complete before sending next symbol
    while(response_active) {
      // Wait for response to complete
    }
  }
  
  CLI_SendString("Test response transmission completed.\r\n");
}

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
    // In error mode, blink faster to indicate error
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_2);
    HAL_Delay(100);
  }
  /* USER CODE END Error_Handler_Debug */
}

/**
 * @brief  Initialize TIM1 for response signal generation
 * @retval None
 */
void Response_Timer_Init(void)
{
  // Calculate the timer clock frequency (typically APB2 frequency)
  // According to project information memory: TIM1 connects to APB2 bus
  // We need to get the actual clock frequency during runtime
  uint32_t timer_clock_freq = HAL_RCC_GetPCLK2Freq();
  
  // Calculate the timer period for 6.35us duration
  // Period = (timer_clock_freq / 1000000) * 6.35
  uint32_t timer_period = (uint32_t)(((float)timer_clock_freq / 1000000.0f) * 6.35f);
  
  // Configure timer period for 6.35us
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = timer_period - 1; // Subtract 1 because counter counts from 0
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)  // Change to base timer init instead of PWM
  {
    Error_Handler();
  }
  
  // Configure timer interrupt
  HAL_NVIC_SetPriority(TIM1_UP_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
  
}

/**
 * @brief  Handle TIM1 Update event (timer overflow)
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1) {
    if(response_active) {
      // Move to next chip position
      current_chip_position++;
      
      if(current_chip_position >= 3) {  // Each chip lasts for 3 timer periods (actual chip + 2 additional periods)
        current_chip_position = 0;
        current_chip_index++;
        
        if(current_chip_index >= 3) {  // All 3 chips transmitted
          // Stop timer and deactivate response
          HAL_TIM_Base_Stop_IT(&htim1);
          response_active = 0; // Сбросим флаг после завершения передачи
          return;
        }
      }
      
      // Update GPIO outputs based on next chip value (if moving to next chip)
      if(current_chip_position == 0) { // Just finished a chip, move to next
        uint8_t chip_value = response_chips[current_chip_index];
        Response_SetChipValue(1, chip_value);  // We only care about setting the value, not the chip number
      }
    }
  }
}

/**
 * @brief  Sets the response value by encoding it and setting the chip values
 * @param  value: 4-bit value to encode and transmit (0-15)
 * @retval None
 */
void Response_SetValue(uint8_t value)
{
    // Encode the 4-bit value into three chip symbols
    Response_EncodeData(value, &response_chips[0], &response_chips[1], &response_chips[2]);
    
    // Initialize transmission variables
    current_chip_index = 0;
    current_chip_position = 0;
    response_active = 1; // Убедимся, что флаг установлен
    
    // Set initial chip value
    uint8_t chip_value = response_chips[0];
    Response_SetChipValue(1, chip_value);  // Set first chip value immediately
    
    // Start timer to begin transmission
    if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  Sets the value of a specific chip (0, 1, or 2)
 * @param  chip_num: Chip number (1, 2, or 3)
 * @param  value: Chip value (0, 1, or 2)
 * @retval None
 */
void Response_SetChipValue(uint8_t chip_num, uint8_t value)
{
  switch(value) {
    case 0: // Both pins LOW
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);  // RESP_12MA LOW
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET); // RESP_24MA LOW
      break;
    case 1: // RESP_12MA HIGH, RESP_24MA LOW
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);    // RESP_12MA HIGH
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET); // RESP_24MA LOW
      break;
    case 2: // RESP_12MA LOW, RESP_24MA HIGH
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);  // RESP_12MA LOW
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);   // RESP_24MA HIGH
      break;
    default:
      // Invalid value, set both to LOW
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
      break;
  }
}

/**
  * @brief  This function provides delay in milliseconds
  * @param  delay: Delay in milliseconds
  * @retval None
  */
void CLI_Delay(uint32_t delay)
{
  HAL_Delay(delay);
}

/**
  * @brief  Send a string over UART
  * @param  str: String to send
  * @retval None
  */
void CLI_SendString(char *str)
{
  static uint8_t first_message_sent = 0;  // Flag to track if first message has been sent
  
  // Get current time in 100-microsecond units
  uint32_t time_ms = get_us_tick() * 100;  // Convert to microseconds
  uint32_t time_s = time_ms / 1000000;    // Convert to seconds
  uint32_t remainder_us = time_ms % 1000000; // Remaining microseconds
  
  // Format time string as "sssssssss.ffffff: " (seconds.microseconds)
  char time_str[30];  // Enough space for formatted time
  
  // For the first message, don't add \r\n at the beginning to avoid extra line
  if (!first_message_sent) {
    sprintf(time_str, "%09lu.%06lu: ", time_s, remainder_us);
    first_message_sent = 1;
  } else {
    sprintf(time_str, "\r\n%09lu.%06lu: ", time_s, remainder_us);
  }
  
  // Send the timestamp
  HAL_UART_Transmit(&huart1, (uint8_t*)time_str, strlen(time_str), HAL_MAX_DELAY);
  
  // Send the actual message
  HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
}

/**
  * @brief  Send raw string without timestamp
  * @param  str: String to send
  * @retval None
  */
void CLI_SendRawString(char *str)
{
  HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
}

/**
  * @brief  Sends the CLI prompt
  * @param  None
  * @retval None
  */
void CLI_SendPrompt(void)
{
  CLI_SendString("\r\n> ");
}

/**
  * @brief  Prints the help menu
  * @param  None
  * @retval None
  */
void CLI_PrintHelp(void)
{
  char buffer[100];
  
  snprintf(buffer, sizeof(buffer), "\r\nAvailable Commands:\r\n");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  help          - Display this help message\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  sysinfo       - Display system information\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  heartbeat     - Toggle heartbeat LED\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  startcomp     - Start comparator for triggering ADC captures\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  testadc       - Test ADC capture of 128 samples via SPI2 DMA\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  getbuf        - Get ADC buffer contents\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  testresp      - Send test response for BADC0DB4 symbol\r");
  CLI_SendString(buffer);
}

/**
  * @brief  Prints system information
  * @param  None
  * @retval None
  */
void CLI_PrintSystemInfo(void)
{
  char buffer[200];
  snprintf(buffer, sizeof(buffer), "  System Information:\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  Board: DSI3 Simulator Board\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  MCU: STM32H743VIT6\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  Clock: 400 MHz\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  CLI Version: 1.0\r");
  CLI_SendString(buffer);
}

/**
  * @brief  Processes CLI commands
  * @param  cmd: Pointer to the command string
  * @retval None
  */
void CLI_ProcessCommand(char *cmd)
{
  // Parse the command and perform actions accordingly
  if(strcmp(cmd, "help") == 0){
    CLI_PrintHelp();
  }else if(strcmp(cmd, "sysinfo") == 0){
    CLI_PrintSystemInfo();
  }else if(strcmp(cmd, "heartbeat") == 0){
    // Toggle heartbeat state
    heartbeat_enabled = !heartbeat_enabled;
    if(heartbeat_enabled){
      CLI_SendString("\r\nHeartbeat enabled.\r\n");
    }else{
      CLI_SendString("\r\nHeartbeat disabled.\r\n");
    }
  }else if(strcmp(cmd, "testadc") == 0){       // Changed from "test_adc_capture" to "testadc"
    // Test ADC capture functionality without COMP trigger
    CLI_SendString("  Testing ADC capture.\r");   // Updated message to reflect 128 samples
  
    if (hspi2.State == HAL_SPI_STATE_BUSY_RX) {
        HAL_SPI_DMAStop(&hspi2);
        CLI_SendString("  Clearing ADC busy RX\r");
    }    
   
    // SCB_InvalidateDCache_by_Addr((uint32_t*)adc_buffer, ADC_BUFFER_SIZE * sizeof(uint16_t));

    // Start DMA receive for ADC_BUFFER_SIZE bytes (each sample is 16-bit)
    if (HAL_SPI_Receive_DMA(&hspi2, (uint8_t*)adc_buffer, ADC_BUFFER_SIZE) != HAL_OK) {
        CLI_SendString("  Error: Failed to start ADC capture via DMA.\r");
    } else {
        CLI_SendString("  ADC capture test initiated.\r");
    }
  }else if(strcmp(cmd, "startcomp") == 0){
    // Start comparator
    // Check if comparator is already enabled by checking the EN bit in CFGR register
    if (HAL_IS_BIT_SET(COMP1->CFGR, COMP_CFGRx_EN)) {
        // Check if it's already running in interrupt mode
        if(hcomp1.State == HAL_COMP_STATE_BUSY) {
            CLI_SendString("  Comparator is already running in interrupt mode.\r");
        } else {
            // Not busy but still enabled, try to start interrupt mode
            if(HAL_COMP_Start_IT(&hcomp1) != HAL_OK) {
                CLI_SendString("  Error: Comparator interrupt start failed.\r");
            } else {
                CLI_SendString("  Comparator interrupt started successfully.\r");
                comp_started = 1; // Set the flag to indicate comparator has been started
            }
        }
    } else {
        // Start comparator normally
        if(HAL_COMP_Start_IT(&hcomp1) != HAL_OK) {
            CLI_SendString("  Error: Comparator start failed.\r");
        } else {
            CLI_SendString("  Comparator started successfully.\r");
            comp_started = 1; // Set the flag to indicate comparator has been started
        }
    }
  }else if(strcmp(cmd, "getbuf") == 0){
    // Output ADC buffer contents in console with a single timestamp
    CLI_SendString("\r\n  ADC Buffer Contents:\r\n");
    
    if (hspi2.State == HAL_SPI_STATE_BUSY_RX) {
        HAL_SPI_DMAStop(&hspi2);
        CLI_SendString("  Clearing ADC busy RX\r");
    }    
    // Send buffer contents with only one timestamp
    char buffer[256]; // Buffer to hold multiple entries
    int pos = 0;
    
    for(int i = 0; i < ADC_BUFFER_SIZE; i++) {
        // Calculate remaining space in buffer
        int remaining_space = sizeof(buffer) - pos;
        
        // Add new entry to buffer
        int written = snprintf(buffer + pos, remaining_space, "%3d - 0x%04X\r\n", i, adc_buffer[i]);
        
        // Check if we have enough space for more items
        if(remaining_space - written < 60 || i == ADC_BUFFER_SIZE - 1 || pos > sizeof(buffer) - 60) {
            // Send the chunk as raw string (without adding new timestamps)
            CLI_SendRawString(buffer);
            
            // Reset buffer
            pos = 0;
            memset(buffer, 0, sizeof(buffer));
        } else {
            pos += written;
        }
    }
    
    CLI_SendString("  Buffer dump complete.\r");
  }else if(strcmp(cmd, "testresp") == 0){
    CLI_TestResp();
  }else{
    CLI_SendString("  Unknown command. Type 'help' for available commands.\r");
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function handles TIM1 update interrupt.
  */
void TIM1_UP_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim1);
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