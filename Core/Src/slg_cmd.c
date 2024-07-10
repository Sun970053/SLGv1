/*
 * slg.c
 *
 *  Created on: Nov 2, 2023
 *      Author: spacelab-cute-PC
 */

#include "slg_task.h"
#include "HashTable.h"

/* -----STM32------ */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

/* -----FREERTOS----- */
extern TaskHandle_t xTaskHandle_SLG_RECEIVING;
TaskHandle_t xTaskHandle_SLG_FORWARD = NULL;
TaskHandle_t xTaskHandle_NO_FORWARD = NULL;

/* -----HASHTABLE----- */
extern Hash_Table* ht;

/* -----SDCARD----- */
extern volatile int id;
char filename[30] = {0};

/* -----LORA----- */
extern uint32_t cr_sf_array[4][4];
extern uint8_t sf_setting;
extern uint8_t cr_setting;

/* -----RTC----- */
extern void init_Date_Time(void);
extern void init_DS1307_Date_Time(void);

/* Initialize SLG parameters. */
/* The default parameters loaded onto SLG upon bootup are also listed */
slg_param_t init_slg_param(void)
{
	slg_param_t slg_param;
	/* Initialize SLG Parameters */
	slg_param.epoch = 1640995200; // 1 January 2022, 12:00 AM UTC+0
	slg_param.stream_mode = 0;
	slg_param.whitelist_mode = 0;
	sprintf(slg_param.region, "AS2");
	slg_param.mac_msb = 0xABCDABCD;
	slg_param.mac_lsb = 0xABCDABCD;
	slg_param.csp_delay_obc = 500;
	slg_param.csp_delay_gs = 500;
	slg_param.tx_mode = 0;
	slg_param.tx_delay = 1000;
	slg_param.tx_count = 1;
	slg_param.tx_interval = 5000;
	slg_param.repeat_mode = 0;
	slg_param.repeat_delay = 1000;
	slg_param.repeat_count = 1;
	slg_param.repeat_interval = 500;

	slg_param.epoch = csp_hton32(slg_param.epoch);
	slg_param.mac_msb = csp_hton32(slg_param.mac_msb);
	slg_param.mac_lsb = csp_hton32(slg_param.mac_lsb);
	slg_param.csp_delay_obc = csp_hton16(slg_param.csp_delay_obc);
	slg_param.csp_delay_gs = csp_hton16(slg_param.csp_delay_gs);
	slg_param.tx_delay = csp_hton16(slg_param.tx_delay);
	slg_param.tx_interval = csp_hton16(slg_param.tx_interval);
	slg_param.repeat_delay = csp_hton16(slg_param.repeat_delay);
	slg_param.repeat_interval = csp_hton16(slg_param.repeat_interval);

	return slg_param;
}

/* Initialize Regional Parameters */
slg_region_param_t init_slg_region_param(void)
{
	slg_region_param_t slg_region_param;

	slg_region_param.lorawan_public = true;
	slg_region_param.clksrc = 1;
	slg_region_param.radio_enable[0] = false;
	slg_region_param.radio_enable[1] = true;
	slg_region_param.radio_type[0] = LGW_RADIO_TYPE_SX1257;
	slg_region_param.radio_type[1] = LGW_RADIO_TYPE_SX1257;
	slg_region_param.radio_freq[0] = 923400000;
	slg_region_param.radio_freq[1] = 922000000;
	slg_region_param.radio_rssi_offset[0] = -166 * 10;
	slg_region_param.radio_rssi_offset[1] = -166 * 10;
	slg_region_param.radio_tx_enable[0] = true;
	slg_region_param.radio_tx_enable[1] = false;
	slg_region_param.chan_multi_enable[0] = false;
	slg_region_param.chan_multi_enable[1] = false;
	slg_region_param.chan_multi_enable[2] = false;
	slg_region_param.chan_multi_enable[3] = false;
	slg_region_param.chan_multi_enable[4] = false;
	slg_region_param.chan_multi_enable[5] = false;
	slg_region_param.chan_multi_enable[6] = false;
	slg_region_param.chan_multi_enable[7] = false;
	slg_region_param.chan_multi_radio[0] = 0;
	slg_region_param.chan_multi_radio[1] = 0;
	slg_region_param.chan_multi_radio[2] = 0;
	slg_region_param.chan_multi_radio[3] = 0;
	slg_region_param.chan_multi_radio[4] = 0;
	slg_region_param.chan_multi_radio[5] = 0;
	slg_region_param.chan_multi_radio[6] = 0;
	slg_region_param.chan_multi_radio[7] = 0;
	slg_region_param.chan_multi_if[0] = 0;
	slg_region_param.chan_multi_if[1] = 400000;
	slg_region_param.chan_multi_if[2] = 0;
	slg_region_param.chan_multi_if[3] = 0;
	slg_region_param.chan_multi_if[4] = 0;
	slg_region_param.chan_multi_if[5] = 0;
	slg_region_param.chan_multi_if[6] = 0;
	slg_region_param.chan_multi_if[7] = 0;
	slg_region_param.chan_std_enable = true;
	slg_region_param.chan_std_radio = 1;
	slg_region_param.chan_std_if = 100000;
	slg_region_param.chan_std_bw = BW_125KHz;
	slg_region_param.chan_std_sf = DR_LORA_SF10;
	slg_region_param.chan_FSK_enable = true;
	slg_region_param.chan_FSK_radio = 1;
	slg_region_param.chan_FSK_if = -200000;
	slg_region_param.chan_FSK_bw = BW_125KHz;
	slg_region_param.chan_FSK_dr = 50000;

	slg_region_param.radio_freq[0] = csp_hton32(slg_region_param.radio_freq[0]);
	slg_region_param.radio_freq[1] = csp_hton32(slg_region_param.radio_freq[1]);
	slg_region_param.radio_rssi_offset[0] = csp_hton16(slg_region_param.radio_rssi_offset[0]);
	slg_region_param.radio_rssi_offset[1] = csp_hton16(slg_region_param.radio_rssi_offset[1]);
	slg_region_param.chan_multi_if[0] = csp_hton32(slg_region_param.chan_multi_if[0]);
	slg_region_param.chan_multi_if[1] = csp_hton32(slg_region_param.chan_multi_if[1]);
	slg_region_param.chan_multi_if[2] = csp_hton32(slg_region_param.chan_multi_if[2]);
	slg_region_param.chan_multi_if[3] = csp_hton32(slg_region_param.chan_multi_if[3]);
	slg_region_param.chan_multi_if[4] = csp_hton32(slg_region_param.chan_multi_if[4]);
	slg_region_param.chan_multi_if[5] = csp_hton32(slg_region_param.chan_multi_if[5]);
	slg_region_param.chan_multi_if[6] = csp_hton32(slg_region_param.chan_multi_if[6]);
	slg_region_param.chan_multi_if[7] = csp_hton32(slg_region_param.chan_multi_if[7]);
	slg_region_param.chan_std_if = csp_hton32(slg_region_param.chan_std_if);
	slg_region_param.chan_FSK_if = csp_hton32(slg_region_param.chan_FSK_if);
	slg_region_param.chan_FSK_dr = csp_hton32(slg_region_param.chan_FSK_dr);

	return slg_region_param;
}

/* Initialize Whitelist Parameters */
slg_whitelist_t init_slg_whitelist(void)
{
	slg_whitelist_t slg_whitelist;

	slg_whitelist.address[0] = 0x26000000; // The Things Network
	slg_whitelist.address[1] = 0x27000000; // The Things Network
	slg_whitelist.address[2] = 0x30000000; // Loriot
	slg_whitelist.address[3] = 0x31000000; // Loriot
	slg_whitelist.count = 4;

	slg_whitelist.address[0] = csp_hton32(slg_whitelist.address[0]);
	slg_whitelist.address[1] = csp_hton32(slg_whitelist.address[1]);
	slg_whitelist.address[2] = csp_hton32(slg_whitelist.address[2]);
	slg_whitelist.address[3] = csp_hton32(slg_whitelist.address[3]);

	return slg_whitelist;
}

/* SLG interaction command line */
int cmd_slg_handle(int param, char* param2)
{
	int ret = 0;
	slg_hk_a_s slg_hk_a;
	slg_hk_b_s slg_hk_b;
	uint32_t params;

	int param2Code = HT_searchKey(ht, param2);

	switch(param)
	{
		case CMD_SLG_PORT_PING:
		{

			ret = csp_ping(CSP_SLG_ADD, SLG_TIMEOUT, 1, CSP_O_NONE);
			if(ret) printf("SLG_PORT_PING: \'%d\' ---> success\r\n", SLG_PORT_PING);
			break;
		}
		case CMD_SLG_PORT_START:
		{
			params = 100;
			if(xTaskCreate(vTask_SLG_Data_Collection, "SLG_DATA_RX", 4096 / sizeof( portSTACK_TYPE ), (void*)&params, TASK_PRIORITY_SLG_RECEIVING, &xTaskHandle_SLG_RECEIVING) != pdTRUE)
				printf("Fail to create SLG data receiving task!\r\n");
			ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_START, SLG_TIMEOUT, NULL, 0, NULL, 0);
			if(ret) printf("SLG_PORT_START: \'%d\' ---> success\r\n", SLG_PORT_START);
			break;
		}
		case CMD_SLG_PORT_STOP:
		{
			ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_STOP, SLG_TIMEOUT, NULL, 0, NULL, 0);
			if(ret) printf("SLG_PORT_STOP: \'%d\' ---> success\r\n", SLG_PORT_STOP);

			if(xTaskHandle_SLG_RECEIVING != NULL)
			{
				vTaskDelete(xTaskHandle_SLG_RECEIVING);
				xTaskHandle_SLG_RECEIVING = NULL;
			}

			break;
		}
		case CMD_SLG_SYNC:
		{
			if(param2Code == CMD_PARAM_SLG_PARAMS)
			{
				slg_param_t slg_param = init_slg_param();
				ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_SYNC_PARAMS,
						SLG_TIMEOUT, &slg_param.epoch, sizeof(slg_param_t), NULL, 0);
				if(ret) printf("SLG_PORT_SYNC_PARAMS: \'%d\' ---> success\r\n", SLG_PORT_SYNC_PARAMS);
			}
			else if(param2Code == CMD_PARAM_SLG_REGIONAL_PARAMS)
			{
				slg_region_param_t slg_region_param = init_slg_region_param();
				ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_SYNC_REGIONAL_PARAMS,
						SLG_TIMEOUT, &slg_region_param.lorawan_public, sizeof(slg_region_param_t), NULL, 0);
				if(ret) printf("SLG_PORT_SYNC_REGIONAL_PARAMS: \'%d\' ---> success\r\n", SLG_PORT_SYNC_REGIONAL_PARAMS);
			}
			else if(param2Code == CMD_PARAM_SLG_WHITELIST)
			{
				slg_whitelist_t slg_whitelist = init_slg_whitelist();
				ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_SYNC_WHITELIST,
						SLG_TIMEOUT, &slg_whitelist.address[0], sizeof(slg_whitelist_t), NULL, 0);
				if(ret) printf("SLG_PORT_SYNC_WHITELIST: \'%d\' ---> success\r\n", SLG_PORT_SYNC_WHITELIST);
			}
			else
			{
				printf("\r\nInput error parameter. Please input correct parameter again.\r\n");
			}
			break;
		}
		case CMD_SLG_PRINT:
		{
			if(param2Code == CMD_PARAM_SLG_PARAMS)
			{
				slg_param_t slg_param;
				ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_PRINT_PARAMS, SLG_TIMEOUT, NULL, 0,
						&slg_param.epoch, sizeof(slg_param_t));
				if(ret) printf("SLG_PORT_PRINT_PARAMS: \'%d\' ---> success\r\n", SLG_PORT_PRINT_PARAMS);
				slg_param.epoch = csp_ntoh32(slg_param.epoch);
				slg_param.mac_msb = csp_ntoh32(slg_param.mac_msb);
				slg_param.mac_lsb = csp_ntoh32(slg_param.mac_lsb);
				slg_param.csp_delay_obc = csp_ntoh16(slg_param.csp_delay_obc);
				slg_param.csp_delay_gs = csp_ntoh16(slg_param.csp_delay_gs);
				slg_param.tx_delay = csp_ntoh16(slg_param.tx_delay);
				slg_param.tx_interval = csp_ntoh16(slg_param.tx_interval);
				slg_param.repeat_delay = csp_ntoh16(slg_param.repeat_delay);
				slg_param.repeat_interval = csp_ntoh16(slg_param.repeat_interval);

				printf("epoch:           %ld\r\n", slg_param.epoch);
				printf("stream mode:     %d\r\n", slg_param.stream_mode);
				printf("whitelist mode:  %d\r\n", slg_param.whitelist_mode);
				printf("region:          %s\r\n", slg_param.region);
				printf("mac_msb:         %x\r\n", (unsigned int)(slg_param.mac_msb));
				printf("mac_lsb:         %x\r\n", (unsigned int)(slg_param.mac_lsb));
				printf("csp delay obc:   %ld\r\n", (long int)(slg_param.csp_delay_obc));
				printf("csp delay gs:    %ld\r\n", (long int)(slg_param.csp_delay_gs));
				printf("tx mode:         %d\r\n", slg_param.tx_mode);
				printf("tx delay:        %ld\r\n", (long int)(slg_param.tx_delay));
				printf("tx count:        %d\r\n", slg_param.tx_count);
				printf("tx interval:     %ld\r\n", (long int)(slg_param.tx_interval));
				printf("repeat mode:     %d\r\n", slg_param.repeat_mode);
				printf("repeat delay:    %ld\r\n", (long int)(slg_param.repeat_delay));
				printf("repeat count:    %d\r\n", slg_param.repeat_count);
				printf("repeat interval: %ld\r\n", (long int)(slg_param.repeat_interval));
			}
			else if(param2Code == CMD_PARAM_SLG_REGIONAL_PARAMS)
			{
				slg_region_param_t slg_region_param;
				ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_PRINT_REGIONAL_PARAMS, SLG_TIMEOUT, NULL, 0,
						&slg_region_param.lorawan_public, sizeof(slg_region_param_t));
				if(ret) printf("SLG_PORT_PRINT_REGIONAL_PARAMS: \'%d\' ---> success\r\n", SLG_PORT_PRINT_REGIONAL_PARAMS);
				slg_region_param.radio_freq[0] = csp_ntoh32(slg_region_param.radio_freq[0]);
				slg_region_param.radio_freq[1] = csp_ntoh32(slg_region_param.radio_freq[1]);
				slg_region_param.radio_rssi_offset[0] = csp_ntoh16(slg_region_param.radio_rssi_offset[0]);
				slg_region_param.radio_rssi_offset[1] = csp_ntoh16(slg_region_param.radio_rssi_offset[1]);
				slg_region_param.chan_multi_if[0] = csp_ntoh32(slg_region_param.chan_multi_if[0]);
				slg_region_param.chan_multi_if[1] = csp_ntoh32(slg_region_param.chan_multi_if[1]);
				slg_region_param.chan_multi_if[2] = csp_ntoh32(slg_region_param.chan_multi_if[2]);
				slg_region_param.chan_multi_if[3] = csp_ntoh32(slg_region_param.chan_multi_if[3]);
				slg_region_param.chan_multi_if[4] = csp_ntoh32(slg_region_param.chan_multi_if[4]);
				slg_region_param.chan_multi_if[5] = csp_ntoh32(slg_region_param.chan_multi_if[5]);
				slg_region_param.chan_multi_if[6] = csp_ntoh32(slg_region_param.chan_multi_if[6]);
				slg_region_param.chan_multi_if[7] = csp_ntoh32(slg_region_param.chan_multi_if[7]);
				slg_region_param.chan_std_if = csp_ntoh32(slg_region_param.chan_std_if);
				slg_region_param.chan_FSK_if = csp_ntoh32(slg_region_param.chan_FSK_if);
				slg_region_param.chan_FSK_dr = csp_ntoh32(slg_region_param.chan_FSK_dr);

				printf("lorawan public:    %d\r\n", slg_region_param.lorawan_public);
				printf("clksrc:            %d\r\n", slg_region_param.clksrc);
				printf("radio enable:      %d, %d\r\n", slg_region_param.radio_enable[0], slg_region_param.radio_enable[1]);
				printf("radio type:        %d, %d\r\n", slg_region_param.radio_type[0], slg_region_param.radio_type[1]);
				printf("radio freq:        %ld, %ld\r\n", (long int)slg_region_param.radio_freq[0], (long int)slg_region_param.radio_freq[1]);
				printf("radio rssi offset: %ld, %ld\r\n", (long int)slg_region_param.radio_rssi_offset[0], (long int)slg_region_param.radio_rssi_offset[1]);
				printf("radio tx enable:   %d, %d\r\n", slg_region_param.radio_tx_enable[0], slg_region_param.radio_tx_enable[1]);
				printf("chan multi enable: %d, %d, %d, %d, %d, %d, %d, %d\r\n", slg_region_param.chan_multi_enable[0], slg_region_param.chan_multi_enable[1], slg_region_param.chan_multi_enable[2], slg_region_param.chan_multi_enable[3], slg_region_param.chan_multi_enable[4], slg_region_param.chan_multi_enable[5], slg_region_param.chan_multi_enable[6], slg_region_param.chan_multi_enable[7]);
				printf("chan multi radio:  %d, %d, %d, %d, %d, %d, %d, %d\r\n", slg_region_param.chan_multi_radio[0], slg_region_param.chan_multi_radio[1], slg_region_param.chan_multi_radio[2], slg_region_param.chan_multi_radio[3], slg_region_param.chan_multi_radio[4], slg_region_param.chan_multi_radio[5], slg_region_param.chan_multi_radio[6], slg_region_param.chan_multi_radio[7]);
				printf("chan multi if:     %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld\r\n", slg_region_param.chan_multi_if[0], slg_region_param.chan_multi_if[1], slg_region_param.chan_multi_if[2], slg_region_param.chan_multi_if[3], slg_region_param.chan_multi_if[4], slg_region_param.chan_multi_if[5], slg_region_param.chan_multi_if[6], slg_region_param.chan_multi_if[7]);
				printf("chan std enable:   %d\r\n", slg_region_param.chan_std_enable);
				printf("chan std radio:    %d\r\n", slg_region_param.chan_std_radio);
				printf("chan std if:       %ld\r\n", slg_region_param.chan_std_if);
				printf("chan std bw:       %d\r\n", slg_region_param.chan_std_bw);
				printf("chan std sf:       %d\r\n", slg_region_param.chan_std_sf);
				printf("chan FSK enable:   %d\r\n", slg_region_param.chan_FSK_enable);
				printf("chan FSK radio:    %d\r\n", slg_region_param.chan_FSK_radio);
				printf("chan FSK if:       %ld\r\n", slg_region_param.chan_FSK_if);
				printf("chan FSK bw:       %d\r\n", slg_region_param.chan_FSK_bw);
				printf("chan FSK dr:       %ld\r\n", slg_region_param.chan_FSK_dr);
			}
			else if(param2Code == CMD_PARAM_SLG_WHITELIST)
			{
				slg_whitelist_t slg_whitelist;
				ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_PRINT_WHITELIST, SLG_TIMEOUT, NULL, 0,
						&slg_whitelist.address[0], sizeof(slg_whitelist));
				if(ret) printf("SLG_PORT_PRINT_WHITELIST: \'%d\' ---> success\r\n", SLG_PORT_PRINT_WHITELIST);
				slg_whitelist.address[0] = csp_ntoh32(slg_whitelist.address[0]);
				slg_whitelist.address[1] = csp_ntoh32(slg_whitelist.address[1]);
				slg_whitelist.address[2] = csp_ntoh32(slg_whitelist.address[2]);
				slg_whitelist.address[3] = csp_ntoh32(slg_whitelist.address[3]);

				printf("address 0: %x\r\n", (unsigned int)slg_whitelist.address[0]);
				printf("address 1: %x\r\n", (unsigned int)slg_whitelist.address[1]);
				printf("address 2: %x\r\n", (unsigned int)slg_whitelist.address[2]);
				printf("address 3: %x\r\n", (unsigned int)slg_whitelist.address[3]);
				printf("address 4: %x\r\n", (unsigned int)slg_whitelist.address[4]);
				printf("address 5: %x\r\n", (unsigned int)slg_whitelist.address[5]);
				printf("address 6: %x\r\n", (unsigned int)slg_whitelist.address[6]);
				printf("address 7: %x\r\n", (unsigned int)slg_whitelist.address[7]);
				printf("address 8: %x\r\n", (unsigned int)slg_whitelist.address[8]);
				printf("address 9: %x\r\n", (unsigned int)slg_whitelist.address[9]);
				printf("count:     %d\r\n", (unsigned int)slg_whitelist.count);
			}
			else
			{
				printf("\r\nInput error parameter. Please input correct parameter again.\r\n");
			}
			break;
		}
		case CMD_SLG_PRINT_HOUSEKEEPING_A:
		{
			ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_PRINT_HOUSEKEEPING_A, SLG_TIMEOUT, NULL, 0,
								&slg_hk_a.time, sizeof(slg_hk_a_s));
			if(ret) printf("SLG_PORT_PRINT_HOUSEKEEPING_A: \'%d\' ---> success\r\n", SLG_PORT_PRINT_HOUSEKEEPING_A);
			slg_hk_a.time = csp_ntoh32(slg_hk_a.time);
			slg_hk_a.count = csp_ntoh32(slg_hk_a.count);
			slg_hk_a.rx_a.freq_hz = csp_ntoh32(slg_hk_a.rx_a.freq_hz);
			slg_hk_a.rx_a.if_chain = csp_ntoh32(slg_hk_a.rx_a.if_chain);
			slg_hk_a.rx_a.status = csp_ntoh32(slg_hk_a.rx_a.status);
			slg_hk_a.rx_a.count_us = csp_ntoh32(slg_hk_a.rx_a.count_us);
			slg_hk_a.rx_a.rf_chain = csp_ntoh32(slg_hk_a.rx_a.rf_chain);
			slg_hk_a.rx_a.modulation = csp_ntoh32(slg_hk_a.rx_a.modulation);
			slg_hk_a.rx_a.bandwidth = csp_ntoh32(slg_hk_a.rx_a.bandwidth);
			slg_hk_a.rx_a.datarate = csp_ntoh32(slg_hk_a.rx_a.datarate);
			slg_hk_a.rx_a.coderate = csp_ntoh32(slg_hk_a.rx_a.coderate);
			slg_hk_a.rx_a.rssi = csp_ntoh32(slg_hk_a.rx_a.rssi);
			slg_hk_a.rx_a.snr = csp_ntoh32(slg_hk_a.rx_a.snr);
			slg_hk_a.rx_a.snr_min = csp_ntoh32(slg_hk_a.rx_a.snr_min);
			slg_hk_a.rx_a.snr_max = csp_ntoh32(slg_hk_a.rx_a.snr_max);
			slg_hk_a.rx_a.crc = csp_ntoh32(slg_hk_a.rx_a.crc);
			slg_hk_a.rx_a.size = csp_ntoh32(slg_hk_a.rx_a.size);
			slg_hk_a.rx_a.mhdr = csp_ntoh32(slg_hk_a.rx_a.mhdr);
			slg_hk_a.rx_a.devaddr[0] = csp_ntoh32(slg_hk_a.rx_a.devaddr[0]);
			slg_hk_a.rx_a.devaddr[1] = csp_ntoh32(slg_hk_a.rx_a.devaddr[1]);
			slg_hk_a.rx_a.devaddr[2] = csp_ntoh32(slg_hk_a.rx_a.devaddr[2]);
			slg_hk_a.rx_a.devaddr[3] = csp_ntoh32(slg_hk_a.rx_a.devaddr[3]);
			slg_hk_a.rx_a.fctrl = csp_ntoh32(slg_hk_a.rx_a.fctrl);
			slg_hk_a.rx_a.fcnt[0] = csp_ntoh32(slg_hk_a.rx_a.fcnt[0]);
			slg_hk_a.rx_a.fcnt[1] = csp_ntoh32(slg_hk_a.rx_a.fcnt[1]);
			slg_hk_a.checker = csp_ntoh32(slg_hk_a.checker);

			printf("time:       %ld\r\n", (long int)slg_hk_a.time);
			printf("count:      %ld\r\n", (long int)slg_hk_a.count);
			printf("freq_hz:    %ld\r\n", (long int)slg_hk_a.rx_a.freq_hz);
			printf("if_chain:   %d\r\n", slg_hk_a.rx_a.if_chain);
			printf("status:     %d\r\n", slg_hk_a.rx_a.status);
			printf("count_us:   %ld\r\n", (long int)slg_hk_a.rx_a.count_us);
			printf("rf_chain:   %d\r\n", slg_hk_a.rx_a.rf_chain);
			printf("modulation: %d\r\n", slg_hk_a.rx_a.modulation);
			printf("bandwidth:  %d\r\n", slg_hk_a.rx_a.bandwidth);
			printf("datarate:   %ld\r\n", (long int)slg_hk_a.rx_a.datarate);
			printf("coderate:   %d\r\n", slg_hk_a.rx_a.coderate);
			printf("rssi:       %ld\r\n", (long int)slg_hk_a.rx_a.rssi);
			printf("snr:        %ld\r\n", (long int)slg_hk_a.rx_a.snr);
			printf("snr_min:    %ld\r\n", (long int)slg_hk_a.rx_a.snr_min);
			printf("snr_max:    %ld\r\n", (long int)slg_hk_a.rx_a.snr_max);
			printf("crc:        %d\r\n", slg_hk_a.rx_a.crc);
			printf("size:       %d\r\n", slg_hk_a.rx_a.size);
			printf("mhdr:       %d\r\n", slg_hk_a.rx_a.mhdr);
			printf("devaddr:    %d, %d, %d, %d\r\n", slg_hk_a.rx_a.devaddr[0], slg_hk_a.rx_a.devaddr[1], slg_hk_a.rx_a.devaddr[2], slg_hk_a.rx_a.devaddr[3]);
			printf("fctrl:      %d\r\n", slg_hk_a.rx_a.fctrl);
			printf("fcnt:       %d, %d\r\n", slg_hk_a.rx_a.fcnt[0], slg_hk_a.rx_a.fcnt[1]);
			printf("checker:    %d\r\n", slg_hk_a.checker);
			break;
		}
		case CMD_SLG_PRINT_HOUSEKEEPING_B:
		{
			ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_PRINT_HOUSEKEEPING_B, SLG_TIMEOUT, NULL, 0,
											&slg_hk_b.rx_b.payload[0], sizeof(slg_hk_b_s));
			if(ret) printf("SLG_PORT_PRINT_HOUSEKEEPING_B: \'%d\' ---> success\r\n", SLG_PORT_PRINT_HOUSEKEEPING_B);
			for(int i = 0;i < 248;i++)
			{
				printf("%02x ", slg_hk_b.rx_b.payload[i]);
				if(i%16 == 15) printf("\r\n");
			}
			printf("\r\nchecker:    %d\r\n", slg_hk_a.checker);
			break;
		}
		case CMD_SLG_FORWARD_ON:
		{
			ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_START, SLG_TIMEOUT, NULL, 0, NULL, 0);
			if(ret) printf("SLG_PORT_START: \'%d\' ---> success\r\n", SLG_PORT_START);
			memcpy(filename ,param2, strlen(param2));

			if(strlen(param2) != 0)
			{
				int result;
				if(xTaskHandle_SLG_FORWARD == NULL)
				{
					result = xTaskCreate(vTask_SLG_Forward, "SLG_FORWARD", 2048, (void*)filename, TASK_PRIORITY_SLG_FORWARD, &xTaskHandle_SLG_FORWARD);
					if(result == pdTRUE)
						printf("Success to create SLG data forward task ! \r\n");
					else
						printf("Fail to create SLG data forward task! Error Code: %d\r\n", result);
				}
				else
					printf("vTask_SLG_Forward is already exist ! \r\n");

				if(xTaskHandle_NO_FORWARD == NULL)
				{
					result = xTaskCreate(vTask_No_Forward, "NO_FORWARD", 1024, (void*)filename, TASK_PRIORITY_NO_FORWARD, &xTaskHandle_NO_FORWARD);
					if(result == pdTRUE)
						printf("Success to create No data forward task ! \r\n");
					else
						printf("Fail to create No data forward task! Error Code: %d\r\n", result);
				}
				else
					printf("vTask_No_Forward is already exist ! \r\n");

				/* If we create those task successfully, Timer interrupt and RTC time recorder startup to alignment time */
				if(xTaskHandle_SLG_FORWARD != NULL && xTaskHandle_NO_FORWARD != NULL)
				{
					/* restart the timer */
					htim2.Instance->CNT &= 0x0;
					HAL_TIM_Base_Start_IT(&htim2);
					/* initialize RTC to 00:00 */
					init_Date_Time();
					/* initialize DS1307 to 00:00 */
					init_DS1307_Date_Time();
					/* According to the parameter, define counter period value (AutoReload Register). */
					uint8_t row, col; // row: cr, column: sf
					row = cr_setting - 1;
					col = sf_setting - 7;
					printf("Delay time: Matrix[%d][%d] -> %lu\r\n", row, col, cr_sf_array[row][col]);
					htim3.Instance->ARR = cr_sf_array[row][col] * 10 - 1;
				}
				else
				{
					printf("Fail to restart the timer ! \r\n");
				}
			}
			else
			{
				printf("Invalid file name ! \r\n");
			}

			break;
		}
		case CMD_SLG_FORWARD_OFF:
		{
			ret = csp_transaction(CSP_PRIO_NORM, CSP_SLG_ADD, SLG_PORT_STOP, SLG_TIMEOUT, NULL, 0, NULL, 0);
			if(ret) printf("SLG_PORT_STOP: \'%d\' ---> success\r\n", SLG_PORT_STOP);
			memset(filename, '\0', strlen(filename));

			if(xTaskHandle_SLG_FORWARD != NULL)
			{
				vTaskDelete(xTaskHandle_SLG_FORWARD);
				xTaskHandle_SLG_FORWARD = NULL;
				printf("SLG data forward task was deleted !\r\n");
			}
			else
			{
				printf("SLG data forward task isn't exist !\r\n");
			}

			if(xTaskHandle_NO_FORWARD != NULL)
			{
				vTaskDelete(xTaskHandle_NO_FORWARD);
				xTaskHandle_NO_FORWARD = NULL;
				printf("No data forward task was deleted !\r\n");
			}
			else
			{
				printf("No data forward task isn't exist !\r\n");
			}

			/* Reset id number */
			id = 0;
			/* reset the timer */
			htim2.Instance->CNT &= 0x0;
			HAL_TIM_Base_Stop_IT(&htim2);
			htim3.Instance->CNT &= 0x0;
			HAL_TIM_Base_Stop_IT(&htim3);

			break;
		}
		default:
		{
			printf("\r\nInput error parameter. Please input correct parameter again.\r\n");
		}
	}
	return 0;
}

