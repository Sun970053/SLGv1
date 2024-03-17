/*
 * slg_task.c
 *
 *  Created on: Nov 6, 2023
 *      Author: spacelab-cute-PC
 */

#include "slg_task.h"

#define INFO_PRINTF	1

//-----STM32------
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

//-----FREERTOS-----
extern TaskHandle_t xTaskHandle_SLG_RECEIVING;
extern QueueHandle_t slg_sfch;

//-----RTC-----
extern void get_Date_Time(void);
extern RTC_TimeTypeDef gTime;

//-----CSP-----
volatile uint8_t status = SLG_HK_Rec_None;
volatile int id = 0;
volatile int fhandle = 0xFF;
csp_socket_t sock = {0};
csp_conn_t *conn;

//-----SD CARD-----
extern FIL file; // file
extern FRESULT fresult; // to store the result
extern uint8_t timer2Flag;
extern uint8_t timer3Flag;

void vTask_SLG_Data_Collection(void * pvParameters)
{
	int filenum = 0;
	status = SLG_HK_Rec_None;
	volatile uint32_t exectime = *((uint32_t*)pvParameters);
    uint32_t seqnum = 0;

    uint32_t second;
	uint32_t tprv = 0, tnow = 0;
	tnow = tprv;
	/* Bind socket to all ports (?) */
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
                	printf("[SLG] packet part A received\r\n");
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
                	printf("[SLG] packet part B received\r\n");
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
    printf("[SLG] LoRa packet receive task off\r\n");
	#endif
    csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_STOP, SLG_TIMEOUT, NULL, 0, NULL, 0);
    xTaskHandle_SLG_RECEIVING = NULL;
    vTaskDelete(0);
}

void vTask_SLG_Forward(void * pvParameters)
{
	//int cspret;
	status = SLG_HK_Rec_None;
	//char filename[128];
    uint32_t second;

	csp_bind(&sock, CSP_ANY);
	csp_listen(&sock, 10);

    slg_hk_a_s slg_hk_a;
    slg_hk_b_s slg_hk_b;
    slg_hk_pkt slg_pkt;
    char* filename = (char*)pvParameters;
    uint8_t bFlag = 0;

    while(1)
    {
		if ((conn = csp_accept(&sock, 10000)) == NULL) {
			continue;
		}

		csp_packet_t *packet;
		while ((packet = csp_read(conn, 50)) != NULL)
        {
			switch (csp_conn_dport(conn))
            {
                case SLG_DATA_A:

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
					#ifdef INFO_PRINTF
					printf("[SLG] packet part A received\r\n");
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
					#endif
                    csp_buffer_free(packet);
                    status |= SLG_HK_Rec_A;
                    break;
                case SLG_DATA_B:
                	memset(&slg_hk_b.rx_b, 0, sizeof(slg_hk_b_s));
                    memcpy(&slg_hk_b.rx_b, packet->data, packet->length);
					#ifdef INFO_PRINTF
                    printf("[SLG] packet part B received\r\n");
                    printf("payload:\r\n");
                    for(int i = 0; i < 248; i++)
                    {
                        printf("%02X ", slg_hk_b.rx_b.payload[i]);
                        if(i%16 == 15){printf("\r\n");}
                    }
                    printf("\r\nchecker: %d\r\n", slg_hk_b.checker);
					#endif
                    csp_buffer_free(packet);
                    status |= SLG_HK_Rec_B;
                    // enter file operation section
                    bFlag = 1;
                    break;
			}
		}
        if(status == SLG_HK_Rec_All && slg_hk_a.checker == slg_hk_b.checker)
        {
           //EXTERNAL_RTC_GetTime(&second);
            slg_pkt.header.fileindex = 0;
            slg_pkt.header.sequence  = 0;
            memcpy(&slg_pkt.header.time, &second, 4);
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

            //xQueueSendToBack(slg_sfch, &slg_pkt.header.fileindex, 1000);
            status = SLG_HK_Rec_None;
        }
		/* Close current connection */
        csp_close(conn);

        /* Store experiment results in SD card file. */
        if(slg_pkt.freq_hz == 923400000 && bFlag == 1)
        {
        	uint8_t buff[100] = {0};
        	/* Refresh Timer2 Interrupt */
			htim2.Instance->CNT &= 0x0;
			HAL_TIM_Base_Start_IT(&htim2);
			/* Stop Timer3 Interrupt */
			htim3.Instance->CNT &= 0x0;
			HAL_TIM_Base_Stop_IT(&htim3);

			/* Update time */
			get_Date_Time();
			/* id, status, mm:ss.ooo */
			int milisec = (1.0f - (float)gTime.SubSeconds / (float)gTime.SecondFraction) * 1000;
			float rssi = (float)slg_pkt.rssi/10;
			float snr = (float)slg_pkt.snr/10;
			if(milisec >= 1000) milisec = 999;
			sprintf((char*)buff, "%d,%d,%02d:%02d.%03d,%.2f,%.2f\n", id++, slg_pkt.crc, gTime.Minutes, gTime.Seconds, milisec, rssi, snr);
			printf("Current time: %02d:%02d.%03d \r\n", gTime.Minutes, gTime.Seconds, milisec);
			/* Updating an existing file */
			fresult = f_open(&file, (char*)filename, FA_OPEN_ALWAYS | FA_WRITE);
			if(fresult == FR_OK) printf("%s opened successfully !\r\n", filename);
			else printf("Fail to open %s !\r\n", filename);
			/* Move to offset to the end to the file */
			fresult = f_lseek(&file, f_size(&file));
			if(fresult == FR_OK) printf("Seek file !\r\n");
			/* Writing text */
			fresult = f_puts((char*)buff, &file);
			if(fresult == FR_OK) printf("Write file !\r\n");
			/* Close file */
			f_close(&file);
			if(fresult == FR_OK) printf("Close file !\r\n");
			bFlag = 0;
        }
        else
        {
        	printf("Frequency: %d Hz, Flag: %d. It doesn't meet the requirements ! \r\n", (int)slg_pkt.freq_hz, bFlag);
        }
    }
}

void vTask_No_Forward(void* pvParameters)
{
	char* filename = (char*)pvParameters;
	while(1)
	{
		/* If it exceeded threshold, triggering Timer interrupt */
		if(timer2Flag | timer3Flag)
		{
			uint8_t buff[100] = {0};
			/* Receive data fail */
			/* Update time */
			get_Date_Time();
			printf("Rx didn't receive the predicted signal !\r\n");
			/* id, status, mm:ss.ooo */
			int milisec = (1.0f - (float)gTime.SubSeconds / (float)gTime.SecondFraction) * 1000;
			if(milisec >= 1000) milisec = 999;
			sprintf((char*)buff, "%d,%d,%02d:%02d.%03d\n", id++, -1, gTime.Minutes, gTime.Seconds, milisec);
			printf("Current time: %02d:%02d.%03d \r\n", gTime.Minutes, gTime.Seconds, milisec);
			/* Updating an existing file */
			fresult = f_open(&file, (char*)filename, FA_OPEN_ALWAYS | FA_WRITE);
			if(fresult == FR_OK) printf("%s opened successfully !\r\n", filename);
			else printf("Fail to open %s !\r\n", filename);
			/* Move to offset to the end to the file */
			fresult = f_lseek(&file, f_size(&file));
			if(fresult == FR_OK) printf("Seek file !\r\n");
			/* Writing text */
			fresult = f_puts((char*)buff, &file);
			if(fresult == FR_OK) printf("Write file !\r\n");
			/* Close file */
			fresult = f_close(&file);
			if(fresult == FR_OK) printf("Close file !\r\n");

			timer2Flag = 0;
			HAL_TIM_Base_Stop_IT(&htim2);
			timer3Flag = 0;

			htim3.Instance->CNT &= 0x0;
			HAL_TIM_Base_Start_IT(&htim3);
		}
	}
}


