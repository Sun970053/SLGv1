/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include <unistd.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

#include "slg_task.h"
#include "csp_task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

#define TEST_MODE 			0
#define SLG_MODE			1

#define TURNON_BLUELED		1
#define TURNOFF_BLUELED		2
#define TURNON_REDLED		3
#define TURNOFF_REDLED 		4
#define BLINKON_BLUELED 	5
#define BLINKOFF_BLUELED 	6
#define IDLE_CMD			10
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
TaskHandle_t rxCmdTaskHandler = NULL;
TaskHandle_t processCmdTaskHandler = NULL;
TaskHandle_t menuDisplayTaskHandler = NULL;
TaskHandle_t blinkLEDTaskHandler = NULL;
QueueHandle_t cmdQueue;

PUTCHAR_PROTOTYPE
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
	return ch;
}

char cmdBuffer[20] = {0};
char rxData = 0;
uint8_t cmdLen = 0;
uint8_t statusFlag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART2)
	{
		rxData = huart->Instance->DR;
		HAL_UART_Receive_IT(huart, (uint8_t*)&rxData, 1);

		cmdBuffer[cmdLen++] = rxData % 0xFF;
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		// check if received byte is user pressing enter
		if(rxData == '\r')
		{
			printf("\r\n");
			cmdLen = 0;

			// notify cmd handling tasks
			xTaskNotifyFromISR(menuDisplayTaskHandler, 0, eNoAction, &xHigherPriorityTaskWoken);
			xTaskNotifyFromISR(rxCmdTaskHandler, 0, eNoAction, &xHigherPriorityTaskWoken);
		}else
			printf("%d", rxData-48);

		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == B1_Pin)
  {
	  if(statusFlag == TEST_MODE)
	  {
		  printf("\r\nChange CMD mode: SLG mode\r\n");
		  statusFlag = SLG_MODE;
	  }
	  else if(statusFlag == SLG_MODE)
	  {
		  printf("\r\nChange CMD mode: Test mode\r\n");
		  statusFlag = TEST_MODE;
	  }
	  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	  xTaskNotifyFromISR(menuDisplayTaskHandler, 0, eNoAction, &xHigherPriorityTaskWoken);
  }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void LEDBlinkTask(void* pvParameter);
void menuDisplayTask(void* pvParameter);
void rxCmdTask(void* pvParameter);
void processCmdTask(void* pvParameter);

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
    static StaticTask_t TimerTaskTCB;
    static StackType_t TimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer = &TimerTaskTCB;
    *ppxTimerTaskStackBuffer = TimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
    static StaticTask_t IdleTaskTCB;
    static StackType_t IdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &IdleTaskTCB;
    *ppxIdleTaskStackBuffer = IdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void LEDBlinkTask(void* pvParameter)
{
	while(1)
	{
		HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);
		vTaskDelay(200);
	}
}

void menuDisplayTask(void* pvParameter)
{
	while(1){
		if(statusFlag == TEST_MODE)
		{
			printf("===========TEST_MODE===========\r\n");
			printf("Turn on blue LED -----------> 1\r\n");
			printf("Turn off blue LED ----------> 2\r\n");
			printf("Turn on red LED ------------> 3\r\n");
			printf("Turn off red LED -----------> 4\r\n");
			printf("Blink green LED ------------> 5\r\n");
			printf("Turn off green LED ---------> 6\r\n");
			printf("Idle command ---------------> 10\r\n\r\n");
		}
		else if(statusFlag == SLG_MODE)
		{
			printf("===========SLG_MODE===========\r\n");
			printf("\"Ping\" between subsystems to detect if subsystem is alive. ----------------> 1\r\n");
			printf("\"Start\" SLG into receiving mode and stream of received packets to OBC. ----> 2\r\n");
			printf("\"Stop\" SLG’s active (receive or transmit) mode. ---------------------------> 3\r\n");
			printf("Start SLG's \"transmit\" mode. ----------------------------------------------> 4 (no use)\r\n");
			printf("Retrieve and \"synchronize SLG parameters\" with OBC. -----------------------> 5\r\n");
			printf("Retrieve and \"synchronize SLG regional operation parameters\" with OBC. ----> 6\r\n");
			printf("Retrieve and \"synchronize Tx parameters\". ---------------------------------> 7 (no use)\r\n");
			printf("Retrieve and \"synchronize Tx packet part A\". ------------------------------> 8 (no use)\r\n");
			printf("Retrieve and \"synchronize Tx packet part B\". ------------------------------> 9 (no use)\r\n");
			printf("Retrieve and \"synchronize LoRa whitelist devices\". ------------------------> 10\r\n");
			printf("\"Prints SLG parameters\" that are on SLG. ----------------------------------> 11\r\n");
			printf("\"Print Regional parameters\". ----------------------------------------------> 12\r\n");
			printf("Prints in debugger console \"Tx parameters\" on the SLG ---------------------> 13 (no use)\r\n");
			printf("Prints in debugger console contents of \"Tx packet part A\". ----------------> 14 (no use)\r\n");
			printf("Prints in debugger console contents of \"Tx packet part B\". ----------------> 15 (no use)\r\n");
			printf("\"Prints device whitelist\" that is stored on SLG. --------------------------> 16\r\n");
			printf("\"Prints section A\" of housekeeping data. ----------------------------------> 17\r\n");
			printf("\"Prints section B\" of housekeeping data. ----------------------------------> 18\r\n");
		}

		xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
	}
}

void rxCmdTask(void* pvParameter)
{
	uint8_t newCmd;
	while(1)
	{
		// block until notification
		xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);

		// critical section
		taskENTER_CRITICAL();

		uint8_t pos = 0;
		uint8_t preNum = 0;
		while(cmdBuffer[pos] != '\r' && cmdBuffer[pos] != '\n')
		{
			// get command code
			newCmd = preNum*10 + (cmdBuffer[pos]-48);
			preNum = newCmd;
			pos++;
		}

		// get command code
		// re-enable interrupts
		taskEXIT_CRITICAL();
		// add command to queue
		xQueueSend(cmdQueue, &newCmd, (TickType_t)0);
		// force context switch
		taskYIELD();
	}
}

void processCmdTask(void* pvParameter)
{
	uint8_t newCmd;
	char rxBuffer;

	while(1)
	{
		if(cmdQueue != 0)
		{
			if(xQueueReceive(cmdQueue, (void *)&rxBuffer, portMAX_DELAY))
			{
				newCmd = rxBuffer;
				if(statusFlag == TEST_MODE){
					switch(newCmd)
					{
					case TURNON_BLUELED:
						HAL_GPIO_WritePin(LD6_GPIO_Port, LD6_Pin, SET);
						printf("\r\nTurn on Blue LED\r\n");
						break;
					case TURNOFF_BLUELED:
						HAL_GPIO_WritePin(LD6_GPIO_Port, LD6_Pin, RESET);
						printf("\r\nTurn off Blue LED\r\n");
						break;
					case TURNON_REDLED:
						HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, SET);
						printf("\r\nTurn on Red LED\r\n");
						break;
					case TURNOFF_REDLED:
						HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, RESET);
						printf("\r\nTurn off Red LED\r\n");
						break;
					case BLINKON_BLUELED:
						xTaskCreate(LEDBlinkTask, "Blink", configMINIMAL_STACK_SIZE, (void*)NULL, tskIDLE_PRIORITY, &blinkLEDTaskHandler);
						printf("\r\nLED blink show started\r\n");
						break;
					case BLINKOFF_BLUELED:
						if(blinkLEDTaskHandler != NULL)
						{
							// Suspend task
							vTaskSuspend(blinkLEDTaskHandler);
							// turn led off
							HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, RESET);
							printf("\nLED blink stopped.\r\n");
						}
					case IDLE_CMD:
						printf("\r\nThis command is for testing.\r\n");
					default:
						printf("\r\nPlease input the correct command number.\r\n");
						break;
					}
				}
				else if(statusFlag == SLG_MODE)
				{
					cmd_slg_handle(newCmd);
				}
			}
		}
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  //create cmd queue
  cmdQueue = xQueueCreate(10, sizeof(char));
  HAL_UART_Receive_IT(&huart2,(uint8_t*)&rxData,1); // Enabling interrupt receive again
  csp_start();
  xTaskCreate(processCmdTask, "PROCESS", 4096, (void*)NULL, 2, &processCmdTaskHandler);
  xTaskCreate(rxCmdTask, "RX", 1024, (void*)NULL, 2, &rxCmdTaskHandler);
  xTaskCreate(menuDisplayTask, "MENU", 1024, (void*)NULL, 1, &menuDisplayTaskHandler);

  vTaskStartScheduler();
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
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
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
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
