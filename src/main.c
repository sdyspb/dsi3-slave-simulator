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
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
UART_HandleTypeDef huart1;
uint8_t cli_buffer[CLI_BUFFER_SIZE];
uint32_t cli_idx = 0;
uint8_t rx_data;
uint32_t last_heartbeat_toggle = 0;
uint8_t heartbeat_enabled = 1;

uint16_t adc_buffer[ADC_BUFFER_SIZE]; // Buffer to store ADC samples
uint8_t comp_started = 0; // Flag to track if comparator has been started
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
  MX_SPI2_Init();  // Call SPI2 initialization
  MX_DMA_Init();   // Insert DMA initialization here
  MX_USART1_UART_Init();  // Then USART1
  MX_COMP1_Init(); // Initialize comparator
  MX_TIM2_Init();  // Initialize TIM2 for high precision timing
  
  /* USER CODE BEGIN 2 */
  
  // Turn OFF heartbeat LED initially (set to HIGH for active-low LED)
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
  
  // Start comparator after all initializations are complete
  if(HAL_COMP_Start(&hcomp1) != HAL_OK)
  {
      // Error handling - could add error indication if needed
      CLI_SendString("\r\nError: Comparator start failed.\r\n");
  }
  
  // Small delay to ensure UART is ready
  HAL_Delay(100);
  
  // Send welcome message via USART1
  CLI_SendString("================================\r");
  CLI_SendString("DSI3 Slave Simulator Started\r");
  CLI_SendString("Type 'help' for available commands\r");
  CLI_SendString("================================\r");
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

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

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

/* USER CODE BEGIN 4 */

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
  CLI_SendString("  Available commands:\r");
  CLI_SendString("  help          - Show this help message\r");
  CLI_SendString("  sysinfo       - Display system information\r");
  CLI_SendString("  heartbeat     - Toggle heartbeat LED (GPIO_PD2)\r");
  CLI_SendString("  testadc       - Test ADC capture of 128 samples via SPI2 DMA\r");
  CLI_SendString("  getbuf        - Display ADC buffer contents (sample number - hex value)\r");
//  CLI_SendString("  <cr>/<lf>     - Show this help message\r");
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
  snprintf(buffer, sizeof(buffer), "  UART Baudrate: 115200\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  CLI Version: 1.0\r");
  CLI_SendString(buffer);
  snprintf(buffer, sizeof(buffer), "  Status: Running\r");
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
    CLI_SendString("  Testing ADC capture of 128 samples...\r");   // Updated message to reflect 128 samples
    
    // Turn on red LED (GPIO_PD3) to indicate start of capture
    HAL_GPIO_WritePin(GPIOD, RED_LED_Pin, GPIO_PIN_RESET); // Active-low, so RESET = ON
    
    // Initialize SPI2 if not already done
    if(hspi2.Instance == NULL){
        MX_SPI2_Init();
    }
    
    // Start comparator if not already started
    if(!comp_started){
        if(HAL_COMP_Start(&hcomp1) != HAL_OK)
        {
            // Error handling - could add error indication if needed
            CLI_SendString("  Error: Comparator start failed.\r");
        }
        comp_started = 1;
    }
    
    // Start DMA receive for 128 samples (each sample is 16-bit)
    if (HAL_SPI_Receive_DMA(&hspi2, (uint8_t*)adc_buffer, ADC_BUFFER_SIZE * 2) != HAL_OK) {
        CLI_SendString("  Error: Failed to start ADC capture via DMA.\r");
    }
    
    // Wait briefly to allow for some data capture
    HAL_Delay(100);
    
    // Turn off red LED (GPIO_PD3) to indicate end of capture attempt
    HAL_GPIO_WritePin(GPIOD, RED_LED_Pin, GPIO_PIN_SET); // Active-high, so SET = OFF
    
    CLI_SendString("  ADC capture test initiated.\r");
  }else if(strcmp(cmd, "getbuf") == 0){
    // Output ADC buffer contents in console with a single timestamp
    CLI_SendString("  ADC Buffer Contents:\r");
    
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
  }else{
    CLI_SendString("  Unknown command. Type 'help' for available commands.\r");
  }
}
/* USER CODE END 4 */

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