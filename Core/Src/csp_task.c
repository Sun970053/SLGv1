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

void router_start()
{
    if(xTaskCreate(CSP_Router_Task, "CSP_ROUTER", 4096*4 / sizeof( portSTACK_TYPE ), 0, TASK_PRIORITY_CSP_ROUTER, &xTaskHandle_CSP_ROUTER) != pdTRUE)
        printf("Fail to create CSP router task!\r\n");
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
	// dynamic array
	uint8_t* process_data = (uint8_t*)malloc(rxcount*sizeof(uint8_t));
	memcpy(process_data, isr_rxData, rxcount);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xQueueSendFromISR(i2cRxQueue, process_data, &xHigherPriorityTaskWoken);
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
void SLG_I2C_RX()
{
	uint8_t new_data[SLAVE_RX_BUFFER_SIZE] = {0};
	portBASE_TYPE result = pdFALSE;
    csp_packet_t * packet = csp_buffer_get(SLAVE_RX_BUFFER_SIZE);

    while(1)
    {
    	result = xQueueReceive(i2cRxQueue, &new_data[0], CSP_MAX_TIMEOUT);
//    	result = uxQueueMessagesWaitingFromISR(i2cRxQueue);
    	if(result == pdPASS)
        {
            csp_id_setup_rx((csp_packet_t *)packet);
            packet->frame_length = rxcount;
            memcpy(packet->frame_begin, &isr_rxData[0], packet->frame_length);
            csp_i2c_rx(&csp_if_i2c, packet, NULL);
            csp_buffer_free(packet);
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

    /* Start router Task*/
    router_start();

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

    /* Start Receiving Task */

	slg_sfch = xQueueCreate(5, sizeof(slg_hk_pkt*));
    i2cRxQueue = xQueueCreate(5, sizeof(uint8_t*));
    if(xTaskCreate(SLG_I2C_RX, "SLG_RX", 4096 / sizeof( portSTACK_TYPE ), 0, TASK_PRIORITY_SLG_I2C_RX, &xTaskHandle_SLG_I2C_RX) != pdTRUE)
        printf("Fail to create SLG I2C RX task!\r\n");
//    if(xTaskCreate(SLG_Data_Receiving, "SLG_DATA", 4096 / sizeof( portSTACK_TYPE ), 0, TASK_PRIORITY_SLG_RECEIVING, &xTaskHandle_SLG_RECEIVING) != pdTRUE)
//        printf("Fail to create SLG data receiving task!\r\n");


    return 1;
}

