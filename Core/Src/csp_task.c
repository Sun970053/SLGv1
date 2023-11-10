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
#define SLAVE_RX_BUFFER_SIZE 64

TaskHandle_t xTaskHandle_CSP_ROUTER = NULL;
TaskHandle_t xTaskHandle_SLG_I2C_RX = NULL;
TaskHandle_t xTaskHandle_SLG_RECEIVING = NULL;

QueueHandle_t i2cRxQueue;
/** Interface definition */
csp_iface_t csp_if_i2c =
{
	.name = "I2C",
	.nexthop = csp_i2c_tx,
    .addr = CSP_DB_ADD,
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

void SLG_I2C_RX()
{
    uint8_t new_data[SLAVE_RX_BUFFER_SIZE];

    csp_packet_t * packet = csp_buffer_get(SLAVE_RX_BUFFER_SIZE);

    while(1)
    {
        if(xQueueReceive(i2cRxQueue, &new_data[0], CSP_MAX_TIMEOUT) == pdPASS)
        {
            csp_id_setup_rx(packet);
            packet->frame_length = new_data[0];
            memcpy(packet->frame_begin, &new_data[1], packet->frame_length);
            csp_i2c_rx(&csp_if_i2c, packet, NULL);
            csp_buffer_free(packet);
        }
    }
}

void SLG_Data_Receiving()
{
    //printf("Activate SLG data receiving task\r\n");
    //uint8_t temp[SLAVE_RX_BUFFER_SIZE];

    /* Create socket with no specific socket options */
	csp_socket_t sock = {0};

	/* Bind socket to all ports (?) */ //TODO
	csp_bind(&sock, CSP_ANY);

	/* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
	csp_listen(&sock, 10);

    slg_hk_a_s slg_hk_a;
    slg_hk_b_s slg_hk_b;

    while(1)
    {
		/* Wait for a new connection, 10000 mS timeout */
		csp_conn_t *conn;
		if ((conn = csp_accept(&sock, 10000)) == NULL) {
			/* timeout */
			continue;
		}

		/* Read packets on connection, timout is 100 mS */
		csp_packet_t *packet;
		while ((packet = csp_read(conn, 50)) != NULL)
        {
			switch (csp_conn_dport(conn))
            {
                case SLG_DATA_A:
                    /* Process packet here */
                    memcpy(&slg_hk_a.time, packet->data, packet->length);
                    slg_hk_a.time = csp_ntoh32(slg_hk_a.time);
                    slg_hk_a.count = csp_ntoh32(slg_hk_a.count);
                    slg_hk_a.rx_a.freq_hz = csp_ntoh32(slg_hk_a.rx_a.freq_hz);
                    slg_hk_a.rx_a.count_us = csp_ntoh32(slg_hk_a.rx_a.count_us);
                    slg_hk_a.rx_a.datarate = csp_ntoh32(slg_hk_a.rx_a.datarate);
                    slg_hk_a.rx_a.rssi = csp_ntoh32(slg_hk_a.rx_a.rssi);
                    slg_hk_a.rx_a.snr = csp_ntoh32(slg_hk_a.rx_a.snr);
                    slg_hk_a.rx_a.snr_min = csp_ntoh32(slg_hk_a.rx_a.snr_min);
                    slg_hk_a.rx_a.snr_max = csp_ntoh32(slg_hk_a.rx_a.snr_max);
                    slg_hk_a.rx_a.crc = csp_ntoh16(slg_hk_a.rx_a.crc);
                    slg_hk_a.rx_a.size = csp_ntoh16(slg_hk_a.rx_a.size);

                    printf("time:       %ld\r\n", slg_hk_a.time);
                    printf("count:      %ld\r\n", slg_hk_a.count);
                    printf("freq_hz:    %ld\r\n", slg_hk_a.rx_a.freq_hz);
                    printf("if_chain:   %d\r\n", slg_hk_a.rx_a.if_chain);
                    printf("status:     %d\r\n", slg_hk_a.rx_a.status);
                    printf("count_us:   %ld\r\n", slg_hk_a.rx_a.count_us);
                    printf("rf_chain:   %d\r\n", slg_hk_a.rx_a.rf_chain);
                    printf("modulation: %d\r\n", slg_hk_a.rx_a.modulation);
                    printf("bandwidth:  %d\r\n", slg_hk_a.rx_a.bandwidth);
                    printf("datarate:   %ld\r\n", slg_hk_a.rx_a.datarate);
                    printf("coderate:   %d\r\n", slg_hk_a.rx_a.coderate);
                    printf("rssi:       %ld\r\n", slg_hk_a.rx_a.rssi);
                    printf("snr:        %ld\r\n", slg_hk_a.rx_a.snr);
                    printf("snr_min:    %ld\r\n", slg_hk_a.rx_a.snr_min);
                    printf("snr_max:    %ld\r\n", slg_hk_a.rx_a.snr_max);
                    printf("crc:        %d\r\n", slg_hk_a.rx_a.crc);
                    printf("size:       %d\r\n", slg_hk_a.rx_a.size);
                    printf("mhdr:       %d\r\n", slg_hk_a.rx_a.mhdr);
                    printf("devaddr:    %d, %d, %d, %d\r\n", slg_hk_a.rx_a.devaddr[0], slg_hk_a.rx_a.devaddr[1], slg_hk_a.rx_a.devaddr[2], slg_hk_a.rx_a.devaddr[3]);
                    printf("fctrl:      %d\r\n", slg_hk_a.rx_a.fctrl);
                    printf("fcnt:       %d, %d\r\n", slg_hk_a.rx_a.fcnt[0], slg_hk_a.rx_a.fcnt[1]);
                    printf("checker:    %d\r\n", slg_hk_a.checker);
                    csp_buffer_free(packet);
                    break;
                case SLG_DATA_B:
                    /* Process packet here */
                    memcpy(&slg_hk_b.rx_b, packet->data, packet->length);
                    printf("payload:\r\n");
                    for(int i = 0; i < 248; i++)
                    {
                        printf("%02X ", slg_hk_b.rx_b.payload[i]);
                        if(i%16 == 15){printf("\r\n");}
                    }
                    printf("\r\nchecker: %d\r\n", slg_hk_b.checker);

                    csp_buffer_free(packet);
                    break;
			}
		}

		/* Close current connection */
		csp_close(conn);
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

    i2cRxQueue = xQueueCreate(5, sizeof(uint8_t*));
    if(xTaskCreate(SLG_I2C_RX, "SLG_RX", 4096 / sizeof( portSTACK_TYPE ), 0, TASK_PRIORITY_SLG_I2C_RX, &xTaskHandle_SLG_I2C_RX) != pdTRUE)
        printf("Fail to create SLG I2C RX task!\r\n");
    if(xTaskCreate(SLG_Data_Receiving, "SLG_DATA", 4096 / sizeof( portSTACK_TYPE ), 0, TASK_PRIORITY_SLG_RECEIVING, &xTaskHandle_SLG_RECEIVING) != pdTRUE)
        printf("Fail to create SLG data receiving task!\r\n");


    return 1;
}

