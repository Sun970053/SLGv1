/*
 * csp_task.c
 *
 *  Created on: Nov 8, 2023
 *      Author: e1406
 */

#include "csp_task.h"

#define TASK_PRIORITY_CSP_ROUTER 1
#define TASK_PRIORITY_SLG_I2C_RX 1
#define TASK_PRIORITY_SLG_RECEIVING 1


TaskHandle_t xTaskHandle_CSP_ROUTER = NULL;
TaskHandle_t xTaskHandle_SLG_I2C_RX = NULL;
TaskHandle_t xTaskHandle_SLG_RECEIVING = NULL;

QueueHandle_t i2cRxQueue;
QueueHandle_t slg_sfch;
csp_i2c_interface_data_t ifdata1;

extern void get_Date_Time(void);
extern RTC_TimeTypeDef gTime;

//uint8_t new_data[SLAVE_RX_BUFFER_SIZE];
/** Interface definition */
csp_iface_t csp_if_i2c =
{
	.name = "I2C",
	.nexthop = csp_i2c_tx,
    .addr = CSP_DB_ADD,
	.interface_data = &ifdata1
};

int csp_i2c_init()
{

	/* Regsiter interface */
	csp_iflist_add(&csp_if_i2c);

	return CSP_ERR_NONE;
}

void CSP_Router_Task(void* pvParameter)
{
	/* Here there be routing */
	while (1)
    {
		csp_route_work();
	}
}

void router_stop()
{
	if(xTaskHandle_CSP_ROUTER != NULL)
	{
		vTaskDelete(xTaskHandle_CSP_ROUTER);
		xTaskHandle_CSP_ROUTER = NULL;
	}
}
//---------------------------------------------------------
/* I2C interrupt */
#define SLAVE_RX_BUFFER_SIZE 254
uint8_t isr_rxData[SLAVE_RX_BUFFER_SIZE];
uint8_t rxcount = 0;

int countAddr = 0;
int countererror = 0;

void process_i2c_data()
{
//	// dynamic array
//	uint8_t* process_data = (uint8_t*)malloc(rxcount*sizeof(uint8_t));
//	memcpy(process_data, isr_rxData, rxcount);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(xTaskHandle_SLG_I2C_RX, 0, eNoAction, &xHigherPriorityTaskWoken);
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
	HAL_I2C_EnableListen_IT(hi2c);
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
	if(TransferDirection == I2C_DIRECTION_TRANSMIT)
	{
		rxcount = 0;
		countAddr++;
		// Refresh I2C Rx array
		memset(isr_rxData, '\0', SLAVE_RX_BUFFER_SIZE);
		// receive using sequential function
		HAL_I2C_Slave_Seq_Receive_IT(hi2c, isr_rxData + rxcount, 1, I2C_FIRST_FRAME);
	}
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if(hi2c->Instance == I2C1)
	{
		rxcount++;
		if(rxcount < SLAVE_RX_BUFFER_SIZE)
		{
			if(rxcount < SLAVE_RX_BUFFER_SIZE-1)
			{
				HAL_I2C_Slave_Seq_Receive_IT(hi2c, isr_rxData + rxcount, 1, I2C_NEXT_FRAME);
			}
			else if(rxcount == SLAVE_RX_BUFFER_SIZE-1)
			{
				HAL_I2C_Slave_Seq_Receive_IT(hi2c, isr_rxData + rxcount, 1, I2C_LAST_FRAME);
			}
		}

		if(rxcount == SLAVE_RX_BUFFER_SIZE) process_i2c_data();
	}
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
	if(hi2c->Instance == I2C1)
	{
		countererror++;
		uint32_t errorcode = HAL_I2C_GetError(hi2c);
		if (errorcode == 4) process_i2c_data();
		HAL_I2C_EnableListen_IT(hi2c);
	}
}

//--------------------------------------------------------
void SLG_I2C_RX(void* pvParameter)
{
	//uint8_t new_data[SLAVE_RX_BUFFER_SIZE] = {0};
    csp_packet_t * packet = (csp_packet_t *)csp_buffer_get(SLAVE_RX_BUFFER_SIZE);
    while(1)
    {
    	// block until notification
    	if(xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdPASS)
        {
    		HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
    		HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
    		get_Date_Time();
    		int milisec = (1.0f - (float)gTime.SubSeconds / (float)gTime.SecondFraction) * 1000;

			printf("Current time: %02d:%02d.%03d \r\n", gTime.Minutes, gTime.Seconds, milisec);
            csp_id_setup_rx(packet);
            packet->frame_length = rxcount;
            memcpy(packet->frame_begin, &isr_rxData[0], packet->frame_length);
            csp_i2c_rx(&csp_if_i2c, packet, NULL);
            csp_buffer_free(packet);
            HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
            HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
        }
    }
}


/*Initialize CSP*/
int csp_start()
{
    /* Init CSP */
    printf("CSP init\r\n");
    csp_init();

    /* Config I2C */
    csp_i2c_init();

    /* Set route table */
    csp_rtable_set(CSP_DB_ADD, 0, &csp_if_i2c, CSP_NO_VIA_ADDRESS);
    csp_rtable_set(CSP_SLG_ADD, 0, &csp_if_i2c, CSP_SLG_ADD);

    /* Check the CSP config */
    printf("Connection table\r\n");
    csp_print("Connection table\r\n");
	csp_conn_print_table();

	printf("Interfaces\r\n");
	csp_print("Interfaces\r\n");
	csp_iflist_print();

	printf("Route table\r\n");
	csp_print("Route table\r\n");
	csp_rtable_print();

	i2cRxQueue = xQueueCreate(10, sizeof(uint8_t*));

    BaseType_t ret;
    /* Start router Task*/
    ret = xTaskCreate(CSP_Router_Task, "CSP_ROUTER", 1024 , 0, TASK_PRIORITY_CSP_ROUTER, &xTaskHandle_CSP_ROUTER);
	if(ret == pdTRUE)
		printf("Success to create CSP router task!\r\n");
	else
		printf("Fail to create CSP router task! Error code: %d\r\n", (int)ret);

	/* Start Receiving Task */
    ret = xTaskCreate(SLG_I2C_RX, "SLG_RX", 1024 , 0, TASK_PRIORITY_SLG_I2C_RX, &xTaskHandle_SLG_I2C_RX);
    if(ret == pdTRUE)
    	printf("Success to create SLG I2C RX task!\r\n");
    else if(ret != pdTRUE)
        printf("Fail to create SLG I2C RX task! Error code: %d\r\n", (int)ret);
//    if(xTaskCreate(SLG_Data_Receiving, "SLG_DATA", 4096 / sizeof( portSTACK_TYPE ), 0, TASK_PRIORITY_SLG_RECEIVING, &xTaskHandle_SLG_RECEIVING) != pdTRUE)
//        printf("Fail to create SLG data receiving task!\r\n");


    return 1;
}

