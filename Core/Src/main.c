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
#include "fatfs.h"

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

#include "DS1307.h"
#include "slg_task.h"
#include "csp_task.h"
#include "HashTable.h"
#include "fatfs_sd.h"
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

#define DISPLAY_MENU        0
#define SDCARD_COMMAND      100
#define CREATE_FILE         1
#define CREATE_DIR          2
#define COPY_FILE           3
#define MOVE_FILE           4
#define DELETE_FILE         5
#define READ_FILE           6
#define WRITE_FILE          7
#define SCAN_FILE_SYSTEM    8
#define REAL_TIME_CLOCK     9
#define PARAM_ON            10
#define PARAM_OFF           11
#define MOUNT_SDCARD        12
#define UNMOUNT_SDCARD      13
#define CHECK_SPACE         14
#define CSP_COMMAND         20
#define CSP_RX_ON           21
#define CSP_RX_OFF          22
#define SLG_COMMAND         30
#define SLG_SF              53
#define SLG_CR              54
#define TIMER_TEST          60

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
TaskHandle_t rxCmdTaskHandler = NULL;
TaskHandle_t processCmdTaskHandler = NULL;
TaskHandle_t menuDisplayTaskHandler = NULL;
TaskHandle_t rtcTaskHandler = NULL;
TaskHandle_t timerTaskHandler = NULL;
extern TaskHandle_t xTaskHandle_SLG_I2C_RX;
QueueHandle_t rxCmdQueue;

PUTCHAR_PROTOTYPE
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
	return ch;
}

/* -----Command Line----- */
Hash_Table* ht;
typedef struct
{
	uint8_t temp[30];
	uint8_t cmd[10];
	uint8_t param[30];
	uint8_t param2[30];
	uint8_t cmdLen;
	uint8_t paramLen;
	uint8_t paramLen2;
}rxCmd;
rxCmd myRxCmd;
uint8_t pos = 0;
uint8_t rxData = 0;
volatile uint8_t uartFlag = 0;

/* -----SD CARD----- */
FATFS fs; // file system
FIL file; // file
FRESULT fresult; // to store the result
char buffer[1024]; // to store data
UINT br, bw;
FATFS *pfs;
DWORD fre_clust;
uint32_t total, free_space;
DIR dir; // Directory
FILINFO fno; // File info
uint8_t readBuff[10240] = {0};
//char path[512];
uint8_t timer2Flag = 0;
uint8_t timer3Flag = 0;

/* -----RTC----- */
RTC_DateTypeDef gDate;
RTC_TimeTypeDef gTime;

/* -----LORA----- */
/* sf_10_cr45: 2500 ms
 * sf_9_cr45: 2247 ms
 * sf_8_cr45: 2134 ms
 * sf_7_cr45: 2070 ms
 * sf_10_cr48: 2592 ms
 * sf_9_cr48: 2298 ms
 * sf_8_cr48: 2163 ms
 * sf_7_cr48: 2090 ms */
uint32_t cr_sf_array[4][4] = {{2070, 2134, 2247, 2500},
							{2077, 2143, 2264, 2530},
							{2083, 2153, 2281, 2561},
							{2090, 2163, 2298, 2592}};
uint8_t sf_setting = 10;
uint8_t cr_setting = 1;
//extern uint8_t isr_rxData[SLAVE_RX_BUFFER_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART2)
	{
		/* Enabling interrupt receive again */
		HAL_UART_Receive_IT(&huart2,(uint8_t*)&rxData,1);
		/* As always, xHigherPriorityTaskWoken is initialized to pdFALSE */
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xQueueSendToBackFromISR( rxCmdQueue, &rxData, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == B1_Pin)
  {

  }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void init_Date_Time(void);
void get_Date_Time(void);
void clearRxCmd(rxCmd* myRxCmd);
void SD_init(void);
FRESULT scanSD(char* prevPath);
void initHashTable(void);
void slgMenuDisplay(void);
void sdcardMenuDisplay(void);
void LEDBlinkTask(void* pvParameter);
void rxCmdTask(void* pvParameter);
void processCmdTask(void* pvParameter);
void dataStorageTask(void* pvParameter);

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

void rtcTask(void* pvParameter)
{
	while(1)
	{
		get_Date_Time();
		printf("%02d-%02d-%04d\r\n", gDate.Date, gDate.Month, 2000 + gDate.Year);
		printf("%02d:%02d:%02d\r\n", gTime.Hours, gTime.Minutes, gTime.Seconds);
		vTaskDelay(5000);
	}
}

void timerTask(void* pvParameter)
{
	/* initialize RTC to 00:00 */
	init_Date_Time();
	/* Auto Reload */
	htim2.Instance->ARR = *(int*)pvParameter * 10 - 1;
	printf("Timer testing task start.. \r\n");
	HAL_TIM_Base_Start_IT(&htim2);
	while(1)
	{
		if(timer2Flag == 1)
		{
			get_Date_Time();
			int milisec = (1.0f - (float)gTime.SubSeconds / (float)gTime.SecondFraction) * 1000;
			if(milisec >= 1000) milisec = 999;
			printf("Current time: %02d:%02d.%03d \r\n", gTime.Minutes, gTime.Seconds, milisec);
			htim2.Instance->CNT &= 0x0;
			timer2Flag = 0;
		}
	}
}

void rxCmdTask(void* pvParameter)
{
	char rxQueueData;
	while(1)
	{
		// receive characters one by one
		if(xQueueReceive( rxCmdQueue, (void *)&rxQueueData, portMAX_DELAY ))
		{
			switch(rxQueueData)
			{
			case ' ':
				// first space
				if(uartFlag == 0)
				{
					// clear array
					if(myRxCmd.cmdLen)
						memset(myRxCmd.cmd, '\0', myRxCmd.cmdLen);
					memcpy(myRxCmd.cmd, myRxCmd.temp, pos + 1);
					memset(myRxCmd.temp, '\0', pos + 1);
					myRxCmd.cmdLen = pos + 1;
				}
				// second space
				else if(uartFlag == 1)
				{
					// clear array
					if(myRxCmd.paramLen)
						memset(myRxCmd.param, '\0', myRxCmd.paramLen);
					memcpy(myRxCmd.param, myRxCmd.temp, pos + 1);
					memset(myRxCmd.temp, '\0', pos + 1);
					myRxCmd.paramLen = pos + 1;
				}
				pos = 0;
				uartFlag++;
				printf(" ");
				break;
			// check if received byte is user pressing enter
			case '\r':
				// two parameter
				if(uartFlag == 2)
				{
					// clear array
					if(myRxCmd.paramLen2)
						memset(myRxCmd.param2, '\0', myRxCmd.paramLen2);
					memcpy(myRxCmd.param2, myRxCmd.temp, pos + 1);
					myRxCmd.paramLen2 = pos + 1;
				}
				// one parameter
				else if(uartFlag == 1)
				{
					// clear array
					if(myRxCmd.paramLen)
						memset(myRxCmd.param, '\0', myRxCmd.paramLen);
					memcpy(myRxCmd.param, myRxCmd.temp, pos + 1);
					myRxCmd.paramLen = pos + 1;
				}
				// only command
				else
				{
					// clear array
					if(myRxCmd.cmdLen)
						memset(myRxCmd.cmd, '\0', myRxCmd.cmdLen);
					memcpy(myRxCmd.cmd, myRxCmd.temp, pos + 1);
					myRxCmd.cmdLen = pos + 1;
				}
				memset(myRxCmd.temp, '\0', pos + 1);
				pos = 0;
				uartFlag = 0;
				printf("\r\n");
				// notify cmd handling tasks
				xTaskNotify(processCmdTaskHandler, 0, eNoAction);
				// Send myRxCmd structure to queue
				//xQueueSend(processCmdQueue, &myRxCmd, (TickType_t)0);
				// force context switch
				taskYIELD();
				break;
			default:
				myRxCmd.temp[pos] = rxQueueData;
				pos++;
				printf("%c", rxQueueData);
			}
		}
	}
}

/*
 * Command Codes:
 * DISPLAY_MENU        0
 * CREATE_FILE         1
 * CREATE_DIR          2
 * COPY_FILE           3
 * MOVE_FILE           4
 * DELETE_FILE         5
 * READ_FILE           6
 * WRITE_FILE          7
 * SCAN_FILE_SYSTEM    8
 * REAL_TIME_CLOCK     9
 * REAL_TIME_CLOCK_ON  10
 * REAL_TIME_CLOCK_OFF 11
 * MOUNT_SDCARD        12
 * UNMOUNT_SDCARD      13
 * CHECK_SPACE         14
 * */
void processCmdTask(void* pvParameter)
{
	int cmdCode = -1;
	int paramCode = -1;
	while(1)
	{
		// block until notification
		xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
		cmdCode = HT_searchKey(ht, (char*)&myRxCmd.cmd[0]);
		paramCode = HT_searchKey(ht, (char*)&myRxCmd.param[0]);
		switch(cmdCode)
		{
			case DISPLAY_MENU:
				if(paramCode == SDCARD_COMMAND)
				{
					sdcardMenuDisplay();
				}
				else if(paramCode == SLG_COMMAND)
				{
					slgMenuDisplay();
				}
				else
				{
					printf("Display the command menu -----------> help [sd/slg] \r\n");
				}
				break;
			case CREATE_FILE:
			{
				if(myRxCmd.paramLen == 0)
				{
					printf("File name is invalid !\r\n");
				}
				else
				{
					uint8_t filename[20];
					memcpy(filename, myRxCmd.param, myRxCmd.paramLen);
					/* Creating/Reading a file */
					fresult = f_open(&file, (char*)filename, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
					if(fresult == FR_OK) printf("Create file %s successfully !\r\n", filename);
					/* Close file */
					fresult = f_close(&file);
					if(fresult == FR_OK) printf("Close file !\r\n");
				}
				break;
			}
			case CREATE_DIR:
			{
				if(myRxCmd.paramLen == 0)
				{
					printf("Directory name is invalid !\r\n");
				}
				else
				{
					uint8_t dirname[30] = {0};
					memcpy(dirname, myRxCmd.param, myRxCmd.paramLen);
					/* Creating a directory */
					fresult = f_mkdir((char*)dirname);
					if(fresult == FR_OK) printf("Create directory %s successfully !\r\n", dirname);
				}
				break;
			}
			case COPY_FILE:
			{
				if(myRxCmd.paramLen == 0 || myRxCmd.paramLen2 == 0)
				{
					printf("File name is invalid !\r\n");
				}
				else
				{
					/* source file */
					fresult = f_open(&file, (char*)myRxCmd.param, FA_READ | FA_OPEN_EXISTING);
					if(fresult == FR_OK)
						printf("%s open successfully !\r\n", myRxCmd.param);

					fresult = f_read(&file, readBuff, f_size(&file), &br);
					if(fresult == FR_OK)
						printf("%s read successfully !\r\n", myRxCmd.param);
					f_close(&file);
					/* destination file */
					fresult = f_open(&file, (char*)myRxCmd.param2, FA_WRITE | FA_CREATE_ALWAYS);
					if(fresult == FR_OK)
						printf("%s open successfully !\r\n", myRxCmd.param2);

					fresult = f_write(&file, (char*)readBuff, br, &bw);
					if(fresult == FR_OK)
						printf("%s write successfully !\r\n", myRxCmd.param2);
					f_close(&file);
					memset(readBuff, '\0', strlen((char*)readBuff));
				}
				break;
			}
			case MOVE_FILE:
			{
				if(myRxCmd.paramLen == 0)
				{
					printf("Directory name is invalid !\r\n");
				}
				else
				{
					/* source file */
					fresult = f_open(&file, (char*)myRxCmd.param, FA_READ | FA_OPEN_EXISTING);
					if(fresult == FR_OK)
						printf("%s open successfully !\r\n", myRxCmd.param);

					fresult = f_read(&file, readBuff, f_size(&file), &br);
					if(fresult == FR_OK)
						printf("%s read successfully !\r\n", myRxCmd.param);
					f_close(&file);
					/* destination file */
					fresult = f_open(&file, (char*)myRxCmd.param2, FA_WRITE | FA_CREATE_ALWAYS);
					if(fresult == FR_OK)
					{
						printf("%s open successfully !\r\n", myRxCmd.param2);
						fresult = f_write(&file, (char*)readBuff, br, &bw);
						if(fresult == FR_OK)
							printf("%s write successfully !\r\n", myRxCmd.param2);
						f_close(&file);

						/* Remove file */
						fresult = f_unlink((char*)myRxCmd.param);
						if(fresult == FR_OK)
							printf("%s removed successfully !\r\n", myRxCmd.param);
						else if(fresult == FR_NO_FILE)
							printf("%s is not found !\r\n", myRxCmd.param);
					}
					memset(readBuff, '\0', strlen((char*)readBuff));
				}
				break;
			}
			case DELETE_FILE:
			{
				if(myRxCmd.paramLen == 0)
				{
				  printf("File name is invalid !\r\n");
				}
				else
				{
				  /* Remove files */
				  fresult = f_unlink((char*)myRxCmd.param);
				  if(fresult == FR_OK)
					  printf("%s removed successfully !\r\n", myRxCmd.param);
				  else if(fresult == FR_NO_FILE)
					  printf("%s is not found !\r\n", myRxCmd.param);
				}
				break;
			}
			case READ_FILE:
			{
				if(myRxCmd.paramLen == 0)
				{
					printf("File name is invalid !\r\n");
				}
				else
				{
					/* Open file to read */
					fresult = f_open(&file, (char*)myRxCmd.param, FA_READ);
					if(fresult == FR_OK)
						printf("%s read successfully !\r\n", myRxCmd.param);
					else if(fresult == FR_NO_FILE)
						printf("%s is not found !\r\n", myRxCmd.param);
					/* Read string from the file */
					f_read(&file, readBuff, f_size(&file), &br);
					printf("Read %s :\r\n", myRxCmd.param);
					printf("%s\r\n", readBuff);
					/* Close file */
					fresult = f_close(&file);
					if(fresult == FR_OK) printf("Close file !\r\n");
					memset(readBuff, '\0', strlen((char*)readBuff));
				}
				break;
			}
			case SCAN_FILE_SYSTEM:
			{
				fresult = scanSD("/");
				if(fresult != FR_OK)
					printf("Scan SD card.. fail !\r\n");
				break;
			}
			case REAL_TIME_CLOCK:
			{

				if(paramCode == PARAM_ON)
				{
					vTaskResume(rtcTaskHandler);
					printf("Resume RTC task ! \r\n");
				}
				else if(paramCode == PARAM_OFF)
				{
					vTaskSuspend(rtcTaskHandler);
					printf("Suspend RTC task ! \r\n");
				}
				else
				{
					printf("Invalid parameter !\r\n");
				}
				break;
			}
			case MOUNT_SDCARD:
			{
				fresult = f_mount(&fs, "", 1);
				if(fresult != FR_OK)
					printf("error in mounting SD CARD... \r\n");
				else
					printf("SD CARD mounted successfully... \r\n");
				break;
			}
			case UNMOUNT_SDCARD:
			{
				fresult = f_mount(NULL, "", 1);
				if(fresult != FR_OK)
					printf("error in unmounting SD CARD... \r\n");
				else
					printf("SD CARD unmounted successfully... \r\n");
				break;
			}
			case CHECK_SPACE:
			{
				/* Check free space */
				f_getfree("", &fre_clust, &pfs);

				total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
				printf("SD CARD total size: \t%lu\r\n", total);
				free_space = (uint32_t)(fre_clust * pfs->csize * 0.5);
				printf("SD CARD free space: \t%lu\r\n", free_space);
				break;
			}
			case CSP_COMMAND:
				if(paramCode == PARAM_ON)
				{
					vTaskResume(xTaskHandle_SLG_I2C_RX);
					printf("Resume I2C RX task ! \r\n");
				}
				else if(paramCode == PARAM_OFF)
				{
					vTaskSuspend(xTaskHandle_SLG_I2C_RX);
					printf("Suspend I2C RX task ! \r\n");
				}
				else
				{
					printf("Invalid csp paramter !\r\n");
				}
				break;
			case SLG_COMMAND:
				if(paramCode >= 31 && paramCode <= 52)
				{
					// three-part string
					cmd_slg_handle(paramCode, (char*)&myRxCmd.param2[0]);
				}
				else if(paramCode == SLG_SF)
				{
					uint8_t sf = (uint8_t)atoi((char*)&myRxCmd.param2[0]);
					if(sf>=7 && sf <=12)
					{
						sf_setting = sf;
						printf("Set sf: %u \r\n", sf_setting);
					}
					else
						printf("Parameter is out of range ! \r\n");
				}
				else if(paramCode == SLG_CR)
				{
					uint8_t cr = (uint8_t)atoi((char*)&myRxCmd.param2[0]);
					if(cr>=1 && cr <=4)
					{
						cr_setting = cr;
						printf("Set cr: 4/%u \r\n", cr_setting + 4);
					}
					else
						printf("Parameter is out of range ! \r\n");
				}
				else
					printf("Invalid parameter ! \r\n");
				break;
			case TIMER_TEST:
			{
				if(paramCode == PARAM_ON)
				{
					if(strlen((char*)myRxCmd.param2) != 0)
					{
						int milli = atoi((char*)myRxCmd.param2);
						int result = xTaskCreate(timerTask, "TIMER", 512, (void*)&milli, 1, &timerTaskHandler);
						if(result == pdTRUE)
							printf("Success to create timer testing task ! \r\n");
						else
							printf("Fail to create timer testing task! Error Code: %d\r\n", result);
					}
					else
					{
						printf("Invalid parameter ! \r\n");
					}
				}
				else if(paramCode == PARAM_OFF)
				{
					if(timerTaskHandler != NULL)
					{
						vTaskDelete(timerTaskHandler);
						timerTaskHandler = NULL;
						printf("Timer testing task was deleted !\r\n");
					}
					else
					{
						printf("Timer testing task isn't exist !\r\n");
					}
					htim2.Instance->CNT &= 0x0;
					HAL_TIM_Base_Stop_IT(&htim3);
				}
				else
				{
					printf("Invalid parameter ! \r\n");
				}
				break;
			}
			default:
				printf("\r\nPlease input the correct command.\r\n");
				break;
		}
		clearRxCmd(&myRxCmd);
		/* force context switch */
		taskYIELD();
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
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  //create cmd queue
  rxCmdQueue = xQueueCreate(10, sizeof(char));
  HAL_UART_Receive_IT(&huart2,(uint8_t*)&rxData,1); // Enabling interrupt receive again
  //HAL_I2C_Slave_Receive_IT(&hi2c1, isr_rxData, SLAVE_RX_BUFFER_SIZE);
  initHashTable();
  SD_init();
  csp_start();
  xTaskCreate(processCmdTask, "PROCESS", 4096*3, (void*)NULL, 3, &processCmdTaskHandler);
  xTaskCreate(rxCmdTask, "RX", 512, (void*)NULL, 3, &rxCmdTaskHandler);
  xTaskCreate(rtcTask, "RTC", 512, (void*)NULL, 1, &rtcTaskHandler);
  vTaskSuspend(rtcTaskHandler);
  //vTaskSuspend(xTaskHandle_SLG_I2C_RX);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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
  hi2c1.Init.OwnAddress1 = 14;
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
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x15;
  sTime.Minutes = 0x20;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_SATURDAY;
  sDate.Month = RTC_MONTH_MARCH;
  sDate.Date = 0x9;
  sDate.Year = 0x24;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 84-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 3000000-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 8400-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 25000-1;
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_CS_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

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
void init_Date_Time()
{
	gTime.Hours = 0x14;
	gTime.Minutes = 0x00;
	gTime.Seconds = 0x00;
	gDate.Date = 0x26;
	/* Get the RTC current Time */
	HAL_RTC_SetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
	/* Get the RTC current Date */
	HAL_RTC_SetDate(&hrtc, &gDate, RTC_FORMAT_BIN);
}

void get_Date_Time(void)
{
	/* Get the RTC current Time */
	HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
	/* Get the RTC current Date */
	HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);
}

void clearRxCmd(rxCmd* myRxCmd)
{
	memset(myRxCmd->cmd, '\0', strlen((char*)myRxCmd->cmd));
	myRxCmd->cmdLen = 0;
	memset(myRxCmd->param, '\0', strlen((char*)myRxCmd->param));
	myRxCmd->paramLen = 0;
	memset(myRxCmd->param2, '\0', strlen((char*)myRxCmd->param2));
	myRxCmd->paramLen2 = 0;
}

void SD_init(void)
{
	/* Mount SD Card */
	fresult = f_mount(&fs, "", 0);
	if(fresult != FR_OK)
		printf("error in mounting SD CARD... \r\n");
	else
		printf("SD CARD mounted successfully... \r\n");
	/* Check free space */
	f_getfree("", &fre_clust, &pfs);

	total = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
	printf("SD CARD total size: \t%lu\r\n", total);
	free_space = (uint32_t)(fre_clust * pfs->csize * 0.5);
	printf("SD CARD free space: \t%lu\r\n", free_space);
	/* Creating/Reading a file */
	fresult = f_open(&file, "test.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	/* Writing text */
	fresult = f_puts("First line\n", &file);
	/* Close file */
	fresult = f_close(&file);
	/* Open file to read */
	fresult = f_open(&file, "test.txt", FA_READ);
	/* Read string from the file */
	f_read(&file, buffer, f_size(&file), &br);
	printf("%s\r\n", buffer);
	/* Close file */
	fresult = f_close(&file);

	/* Updating an existing file */
	fresult = f_open(&file, "test.txt", FA_OPEN_ALWAYS | FA_WRITE);
	/* Move to offset to the end to the file */
	fresult = f_lseek(&file, f_size(&file));
	/* Writing text */
	fresult = f_puts("This is updated data and it should be in the end\n", &file);
	f_close(&file);
	/* Open file to read */
	fresult = f_open(&file, "test.txt", FA_READ);
	/* Read string from the file */
	f_read(&file, buffer, f_size(&file), &br);
	printf("%s\r\n", buffer);
	f_close(&file);
	/* Remove files */
	fresult = f_unlink("test.txt");
	if(fresult == FR_OK) printf("test.txt removed successfully\r\n");
}

FRESULT scanSD(char* prevPath)
{
	UINT i;
	DIR thisdir;
	/* path record */
	char* path = (char*)pvPortMalloc(50*sizeof(char));
	sprintf(path, "%s", prevPath);
	f_opendir(&thisdir, path);
	while(1)
	{
		fresult = f_readdir(&thisdir, &fno);
		if(fresult != FR_OK || fno.fname[0] == 0) break;
		/* if it is a directory */
		if(fno.fattrib & AM_DIR)
		{
			if (strcmp ("System Volume Information", fno.fname) == 0) continue;
			printf("Dir: %s\r\n", fno.fname);
			i = strlen(path);
			sprintf(&path[i], "/%s", fno.fname);
			fresult = scanSD(path);
			if(fresult != FR_OK) break;
			path[i] = 0;
		}
		else
		{
			printf("File: %s/%s\r\n", path, fno.fname);
		}
	}
	f_closedir(&thisdir);
	vPortFree(path);
	return fresult;
}

/*
 * Command Codes:
 * DISPLAY_MENU        0
 * SDCARD_MENU         100
 * SLG_MENU            101
 * CREATE_FILE         1
 * CREATE_DIR          2
 * COPY_FILE           3
 * MOVE_FILE           4
 * DELETE_FILE         5
 * READ_FILE           6
 * WRITE_FILE          7
 * SCAN_FILE_SYSTEM    8
 * REAL_TIME_CLOCK     9
 * PRARM_ON            10
 * PRARM_OFF           11
 * MOUNT_SDCARD        12
 * UNMOUNT_SDCARD      13
 * CHECK_SPACE         14
 * */

void sdcardMenuDisplay()
{
	printf("Display the command menu ----> help    [sd/slg] \r\n");
	printf("Create file -----------------> mkfile  [filename] \r\n");
	printf("Create directory ------------> mkdir   [dirname] \r\n");
	printf("Copy file -------------------> cpfile  [source]   [destination] \r\n");
	printf("Move file -------------------> mvfile  [source]   [directory] \r\n");
	printf("Delete file -----------------> del     [filename] \r\n");
	printf("Read  file ------------------> read    [filename] \r\n");
	printf("Write file ------------------> write   [filename] [text] \r\n");
	printf("Scan file system ------------> ls \r\n");
	printf("Get current time ------------> rtc     [on/off]\r\n");
	printf("Mount SD Card ---------------> mount \r\n");
	printf("Unmount SD Card -------------> unmount \r\n");
	printf("Check SD Card space ---------> check \r\n");
	printf("CSP communication -----------> csp     [option]\r\n");
	printf("Timer test ------------------> tim     [millisecond]");
}

/*
 * Command Codes:
 * CSP_COMMAND                      20
 *
 * SLG_COMMAND                      30
 * SLG_PORT_PING                    31
 * SLG_PORT_START                   32
 * SLG_PORT_STOP                    33
 * SLG_SYNC                         51
 * SLG_PRINT                        52
 * PARAM_SLG_PARAMS                 35
 * PARAM_SLG_REGIONAL_PARAMS        36
 * PARAM_SLG_WHITELIST              40
 * SLG_PORT_PRINT_HOUSEKEEPING_A    47
 * SLG_PORT_PRINT_HOUSEKEEPING_B    48
 * SLG_FORWARD_ON                   49
 * SLG_FORWARD_OFF                  50
 *  */

void slgMenuDisplay()
{
	printf("Display the command menu -----------> help [sd/slg] \r\n");
	printf("I2C interrupt and RX_I2C task ------> csp  [on/off] \r\n");
	printf("Ping between subsystems ------------> slg  ping \r\n");
	printf("Start SLG into receiving mode ------> slg  start \r\n");
	printf("Stop SLG's active mode -------------> slg  stop \r\n");
	printf("Synchronize SLG params -------------> slg  sync     [param/region/white] \r\n");
	printf("Print SLG params -------------------> slg  print    [param/region/white] \r\n");
	printf("Print SLG housekeeping A -----------> slg  hka \r\n");
	printf("Print SLG housekeeping B -----------> slg  hkb \r\n");
	printf("Init & start to receive data -------> slg  fon      [filename]\r\n");
	printf("Stop to receive data ---------------> slg  foff \r\n");
	printf("Set Spreading Factor ---------------> slg  sf       [7-12] \r\n");
	printf("Set Spreading Factor ---------------> slg  cr       [1-4] \r\n");
}

void initHashTable(void)
{
	// define the function pointer
	HT_malloc = &pvPortMalloc;
	HT_free = &vPortFree;
	// select commands
	ht = HT_createTable(50);
	HT_insertItem(ht, "help",    0);
	HT_insertItem(ht, "sd",      100);
	HT_insertItem(ht, "mkfile",  1);
	HT_insertItem(ht, "mkdir",   2);
	HT_insertItem(ht, "cpfile",  3);
	HT_insertItem(ht, "mvfile",  4);
	HT_insertItem(ht, "del",     5);
	HT_insertItem(ht, "read",    6);
	HT_insertItem(ht, "write",   7);
	HT_insertItem(ht, "ls",      8);
	HT_insertItem(ht, "rtc",     9);
	HT_insertItem(ht, "on",      10);
	HT_insertItem(ht, "off",     11);
	HT_insertItem(ht, "mount",   12);
	HT_insertItem(ht, "unmount", 13);
	HT_insertItem(ht, "check",   14);
	HT_insertItem(ht, "csp",     20);
	HT_insertItem(ht, "slg",     30);
	HT_insertItem(ht, "ping",    31);
	HT_insertItem(ht, "start",   32);
	HT_insertItem(ht, "stop",    33);
	HT_insertItem(ht, "sync",    51);
	HT_insertItem(ht, "print",   52);
	HT_insertItem(ht, "param",   35);
	HT_insertItem(ht, "region",  36);
	HT_insertItem(ht, "white",   40);
	HT_insertItem(ht, "hka",     47);
	HT_insertItem(ht, "hkb",     48);
	HT_insertItem(ht, "fon",     49);
	HT_insertItem(ht, "foff",    50);
	HT_insertItem(ht, "sf",      53);
	HT_insertItem(ht, "cr",      54);
	HT_insertItem(ht, "tim",     60);
	HT_print(ht);

	printf("Search Key %s: Value %d \r\n", "check", HT_searchKey(ht, "check"));
}
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
	if(htim == &htim2)
	{
		timer2Flag = 1;
	}
	if(htim == &htim3)
	{
		timer3Flag = 1;
	}
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
