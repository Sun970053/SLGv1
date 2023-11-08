/*
 * slg_task.c
 *
 *  Created on: Nov 6, 2023
 *      Author: spacelab-cute-PC
 */

#include "slg_task.h"

#define INFO_PRINTF	1

extern TaskHandle_t xTaskHandle_SLG_RECEIVING;

volatile uint8_t status = SLG_HK_Rec_None;
volatile int fhandle = 0xFF;
csp_socket_t sock = {0};
csp_conn_t *conn;

void vTask_SLG_Data_Collection(void * pvParameters)
{
	int cspret;
	int filenum = 0;
	status = SLG_HK_Rec_None;
	volatile uint32_t exectime = *((uint32_t*)pvParameters);
    uint32_t seqnum = 0;

    uint32_t second;
	uint32_t tprv = 0, tnow = 0;
	tnow = tprv;
	/* Bind socket to all ports (?) */ //TODO
	csp_bind(&sock, CSP_ANY);

	/* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
	csp_listen(&sock, 10);

    slg_hk_a_s slg_hk_a;
    slg_hk_b_s slg_hk_b;
    slg_hk_pkt slg_pkt;


    while(1)  // Add execution time condition
    {
        /* Check executed time and max save packet*/
        if(tnow - tprv >= exectime || seqnum >= SLG_PACKET_MAX)
        {
        	csp_close(conn);
        	break;
        }

		/* Wait for a new connection, 10000 mS timeout */
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
					#ifdef INFO_PRINTF
                	printf("[SLG] packet part A received\n");
					#endif
                    memcpy(&slg_hk_a.time, packet->data, packet->length);
                    /*slg_hk_a.time = csp_ntoh32(slg_hk_a.time);
                    slg_hk_a.count = csp_ntoh32(slg_hk_a.count);
                    slg_hk_a.rx_a.freq_hz = csp_ntoh32(slg_hk_a.rx_a.freq_hz);
                    slg_hk_a.rx_a.count_us = csp_ntoh32(slg_hk_a.rx_a.count_us);
                    slg_hk_a.rx_a.datarate = csp_ntoh32(slg_hk_a.rx_a.datarate);
                    slg_hk_a.rx_a.rssi = csp_ntoh32(slg_hk_a.rx_a.rssi);
                    slg_hk_a.rx_a.snr = csp_ntoh32(slg_hk_a.rx_a.snr);
                    slg_hk_a.rx_a.snr_min = csp_ntoh32(slg_hk_a.rx_a.snr_min);
                    slg_hk_a.rx_a.snr_max = csp_ntoh32(slg_hk_a.rx_a.snr_max);
                    slg_hk_a.rx_a.crc = csp_ntoh16(slg_hk_a.rx_a.crc);
                    slg_hk_a.rx_a.size = csp_ntoh16(slg_hk_a.rx_a.size);*/
                    csp_buffer_free(packet);
                    status |= SLG_HK_Rec_A;
                    break;
                case SLG_DATA_B:
					#ifdef INFO_PRINTF
                	printf("[SLG] packet part B received\n");
					#endif
                	memset(&slg_hk_b.rx_b, 0, sizeof(slg_hk_b_s));
                    memcpy(&slg_hk_b.rx_b, packet->data, packet->length);
                    csp_buffer_free(packet);
                    status |= SLG_HK_Rec_B;
                    break;
			}
		}
        if(status == SLG_HK_Rec_All && slg_hk_a.checker == slg_hk_b.checker)
        {
            /* Copy Port A and Port B data to packet */
            memcpy(&slg_pkt.header.fileindex, &filenum, 4);
            memcpy(&slg_pkt.header.sequence,  &seqnum,  4);  seqnum++;
            memcpy(&slg_pkt.header.time,      &second,  4);
            slg_pkt.header.fileindex = csp_ntoh32(slg_pkt.header.fileindex);
            slg_pkt.header.sequence = csp_ntoh32(slg_pkt.header.sequence);
            slg_pkt.header.time = csp_ntoh32(slg_pkt.header.time);
            slg_pkt.header.millisecond = 0;
            slg_pkt.time = slg_hk_a.time;
            slg_pkt.count = slg_hk_a.count;
            slg_pkt.freq_hz = slg_hk_a.rx_a.freq_hz;
            slg_pkt.if_chain = slg_hk_a.rx_a.if_chain;
            slg_pkt.status = slg_hk_a.rx_a.status;
            slg_pkt.count_us = slg_hk_a.rx_a.count_us;
            slg_pkt.rf_chain = slg_hk_a.rx_a.rf_chain;
            slg_pkt.modulation = slg_hk_a.rx_a.modulation;
            slg_pkt.bandwidth = slg_hk_a.rx_a.bandwidth;
            slg_pkt.datarate = slg_hk_a.rx_a.datarate;
            slg_pkt.coderate = slg_hk_a.rx_a.coderate;
            slg_pkt.rssi = slg_hk_a.rx_a.rssi;
            slg_pkt.snr = slg_hk_a.rx_a.snr;
            slg_pkt.snr_min = slg_hk_a.rx_a.snr_min;
            slg_pkt.snr_max = slg_hk_a.rx_a.snr_max;
            slg_pkt.crc = slg_hk_a.rx_a.crc;
            slg_pkt.size = slg_hk_a.rx_a.size;
            slg_pkt.mhdr = slg_hk_a.rx_a.mhdr;
            memcpy(slg_pkt.devaddr, slg_hk_a.rx_a.devaddr, 4);
            slg_pkt.fctrl = slg_hk_a.rx_a.fctrl;
            memcpy(slg_pkt.fcnt, slg_hk_a.rx_a.fcnt, 2);
            memcpy(slg_pkt.payload, slg_hk_b.rx_b.payload, 284);
            status = SLG_HK_Rec_None;
        }
		/* Close current connection */
        csp_close(conn);
    }

    fhandle = 0xFF;
	#ifdef INFO_PRINTF
    printf("[SLG] LoRa packet receive task off\n");
	#endif
    cspret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_STOP, SLG_TIMEOUT, NULL, 0, NULL, 0);
    xTaskHandle_SLG_RECEIVING = NULL;
    vTaskDelete(0);
}
