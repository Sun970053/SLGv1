/*
 * slg_task.h
 *
 *  Created on: Nov 6, 2023
 *      Author: spacelab-cute-PC
 */

#ifndef INC_SLG_TASK_H_
#define INC_SLG_TASK_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f4xx_hal.h"
#include "csp/csp.h"
#include "csp/csp_types.h"
#include "csp/csp_endian.h"
#include "csp/csp_error.h"
#include "csp/interfaces/csp_if_i2c.h"
#include "csp/csp_types.h"

#define SLG_PACKET_MAX  40000
#define SLG_GET_PARAM_RET_LEN 44
#define SLG_GET_REGPARAM_RET_LEN  96
#define SLG_GET_WHITELIST_RET_LEN  44
#define SLG_SF_QUEUE_MAX   60

#define SLG_TIMEOUT						2000 //1 sec timeout

#define CSP_SLG_ADD						6
#define CSP_DB_ADD						7 //TODO

#define CMD_FAIL						0

/* The CSP ports that are configured on the SLG and its associated functions. */
#define SLG_PORT_PING 					1
#define SLG_PORT_START 					2
#define SLG_PORT_STOP 					3
#define SLG_PORT_TRANSMIT 				4
#define SLG_PORT_SYNC_PARAMS 			5
#define SLG_PORT_SYNC_REGIONAL_PARAMS 	6
#define SLG_PORT_SYNC_TX_PARAMS 		7
#define SLG_PORT_SYNC_TX_A 				8
#define SLG_PORT_SYNC_TX_B 				9
#define SLG_PORT_SYNC_WHITELIST 		10
#define SLG_PORT_PRINT_PARAMS 			11
#define SLG_PORT_PRINT_REGIONAL_PARAMS 	12
#define SLG_PORT_PRINT_TX_PARAMS 		13
#define SLG_PORT_PRINT_TX_A 			14
#define SLG_PORT_PRINT_TX_B 			15
#define SLG_PORT_PRINT_WHITELIST 		16
#define SLG_PORT_PRINT_HOUSEKEEPING_A 	17
#define SLG_PORT_PRINT_HOUSEKEEPING_B 	18

/* SLG will send LoRa data packets in 2 segments to 2 different ports,
 * expecting that the OBC to assemble the data to form a complete packet */
#define SLG_DATA_A 						16
#define SLG_DATA_B 						17

/* Radio Frequency product mode */
#define LGW_RADIO_TYPE_SX1257			2

/* LoRa & FSK Bandwidth parameter */
#define BW_500KHz						0x01
#define BW_250KHz						0x02
#define BW_125KHz						0x03
#define BW_62_5KHz						0x04
#define BW_31_2KHz						0x05
#define BW_15_6KHz						0x06
#define BW_7_8KHz						0x07

/* LoRa Spreading Factor (SF) parameter */
#define DR_LORA_SF7 					0x02
#define DR_LORA_SF8						0x04
#define DR_LORA_SF9						0x08
#define DR_LORA_SF10 					0x10
#define DR_LORA_SF11					0x20
#define DR_LORA_SF12					0x40
#define FSK								0x20

/* Code rate */
#define CR_LORA_4_5						0x01
#define CR_LORA_4_6						0x02
#define CR_LORA_4_7						0x03
#define CR_LORA_4_8						0x04

/* Radio Frequency Mode */
#define IMMEDIATE						0
/* Type of radio Frequency modulation */
#define MOD_LORA						0x10

#define TASK_PRIORITY_SLG_RECEIVING 	1

/* SLG Parameters */
typedef struct slg_parameter
{
	uint32_t	epoch;
	uint8_t		stream_mode;
	uint8_t		whitelist_mode;
	char		region[7];
	uint32_t	mac_msb;
	uint32_t	mac_lsb;
	uint16_t	csp_delay_obc;
	uint16_t	csp_delay_gs;
	uint8_t		tx_mode;
	uint16_t	tx_delay;
	uint8_t		tx_count;
	uint16_t	tx_interval;
	uint8_t		repeat_mode;
	uint16_t	repeat_delay;
	uint8_t		repeat_count;
	uint16_t	repeat_interval;
}slg_param_t;

/* Regional Parameters */
typedef struct slg_regional_parameter
{
	uint8_t		lorawan_public;
	uint8_t		clksrc;
	uint8_t		radio_enable[2];
	uint8_t		radio_type[2];
	uint32_t	radio_freq[2];
	uint16_t	radio_rssi_offset[2];
	uint8_t		radio_tx_enable[2];
	uint8_t		chan_multi_enable[8];
	uint8_t		chan_multi_radio[8];
	int32_t 	chan_multi_if[8];
	uint8_t 	chan_std_enable;
	uint8_t 	chan_std_radio;
	int32_t		chan_std_if;
	uint8_t 	chan_std_bw;
	uint8_t 	chan_std_sf;
	uint8_t 	chan_FSK_enable;
	uint8_t		chan_FSK_radio;
	int32_t		chan_FSK_if;
	uint8_t		chan_FSK_bw;
	uint32_t	chan_FSK_dr;
}slg_region_param_t;

/* Transmit Parameters */
typedef struct slg_transmit_parameter
{
	uint32_t	notch_freq;
	uint32_t	freq_hz;
	uint8_t		tx_mode;
	uint32_t	count_us;
	uint8_t		rf_chain;
	int8_t		rf_power;
	uint8_t		modulation;
	uint8_t		bandwidth;
	uint32_t	datarate;
	uint8_t		coderate;
	bool		invert_pol;
	uint8_t		f_dev;
	uint16_t	preamble;
	bool		no_crc;
	bool		no_header;
}slg_tx_param_t;

/* Whitelist Parameters */
typedef struct slg_white_list
{
	uint32_t	address[10];
	uint8_t		count;
}slg_whitelist_t;

/* When SLG receives a packet, the LoRa packets are presented to OBC in the following structure: */
typedef struct slg_rx_s {
	uint32_t	freq_hz;		/*!> central frequency of the IF chain */
	uint8_t		if_chain; 		/*!> by which IF chain was packet received */
	uint8_t		status; 		/*!> status of the received packet */
	uint32_t	count_us; 		/*!> internal concentrator counter for timestamping, 1 microsecond resolution */
	uint8_t		rf_chain; 		/*!> through which RF chain the packet was received */
	uint8_t		modulation; 	/*!> modulation used by the packet */
	uint8_t		bandwidth; 		/*!> modulation bandwidth (LoRa only) */
	uint32_t	datarate; 		/*!> RX datarate of the packet (SF for LoRa) */
	uint8_t		coderate; 		/*!> error-correcting code of the packet (LoRa only) */
	float		rssi; 			/*!> average packet RSSI in dB */
	float		snr; 			/*!> average packet SNR, in dB (LoRa only) */
	float		snr_min; 		/*!> minimum packet SNR, in dB (LoRa only) */
	float		snr_max; 		/*!> maximum packet SNR, in dB (LoRa only) */
	uint16_t	crc; 			/*!> CRC that was received in the payload */
	uint16_t	size; 			/*!> payload size in bytes */
	uint8_t		payload[256]; 	/*!> buffer containing the payload */
} slg_rx_s;

/* A received packet is split into two sections, A and B. */
typedef struct slg_rx_a_s {
	uint32_t	freq_hz; 		/*!> central frequency of the IF chain */
	uint8_t		if_chain; 		/*!> by which IF chain was packet received */
	uint8_t		status; 		/*!> status of the received packet */
	uint32_t	count_us; 		/*!> internal concentrator counter for timestamping, 1 microsecond resolution */
	uint8_t		rf_chain; 		/*!> through which RF chain the packet was received */
	uint8_t		modulation; 	/*!> modulation used by the packet */
	uint8_t		bandwidth; 		/*!> modulation bandwidth (LoRa only) */
	uint32_t	datarate; 		/*!> RX datarate of the packet (SF for LoRa) */
	uint8_t		coderate; 		/*!> error-correcting code of the packet (LoRa only) */
	int32_t		rssi; 			/*!> average packet RSSI in dB multiplied by 10 */
	int32_t		snr; 			/*!> average packet SNR, in dB multiplied by 10 (LoRa only) */
	int32_t		snr_min; 		/*!> minimum packet SNR, in dB multiplied by 10 (LoRa only) */
	int32_t		snr_max; 		/*!> maximum packet SNR, in dB multiplied by 10 (LoRa only) */
	uint16_t	crc; 			/*!> CRC that was received in the payload */
	uint16_t	size; 			/*!> payload size in bytes */
	uint8_t		mhdr; 			/*!> LoRaWAN MAC header */
	uint8_t		devaddr[4]; 	/*!> LoRaWAN Device Address */
	uint8_t		fctrl; 			/*!> LoRaWAN Frame Control */
	uint8_t		fcnt[2]; 		/*!> LoRaWAN Frame Counter */
} slg_rx_a_s;

typedef struct slg_hk_a_s {
	uint32_t time;
	uint32_t count;
	slg_rx_a_s rx_a;
	uint8_t checker; 			/*!> random checker code to glue part a and b together */
} slg_hk_a_s;

typedef struct slg_rx_b_s {
	uint8_t payload[248]; 		/*!> buffer containing the payload (except MHDR, DevAddr, FCtrl, and FCnt)*/
} slg_rx_b_s;

typedef struct slg_hk_b_s {
	slg_rx_b_s rx_b;
	uint8_t checker; 			/*!> random checker code to glue part a and b together */
} slg_hk_b_s;

enum SLG_hk_rec_status
{
	SLG_HK_Rec_None,
	SLG_HK_Rec_A,
	SLG_HK_Rec_B,
	SLG_HK_Rec_All,
};

typedef struct __attribute__((__packed__)) slg_pkt_header
{
    uint32_t fileindex;
    uint32_t sequence;
    uint32_t time;
    uint32_t millisecond;
} slg_pkt_header;

typedef struct  __attribute__((__packed__)) slg_hk_pkt
{
    slg_pkt_header header;
    uint32_t time;
    uint32_t count;
    uint32_t freq_hz;
    uint8_t  if_chain;
    uint8_t  status;
    uint32_t count_us;
    uint8_t  rf_chain;
    uint8_t  modulation;
    uint8_t  bandwidth;
    uint32_t datarate;
    uint8_t  coderate;
    int32_t  rssi;
    int32_t  snr;
    int32_t  snr_min;
    int32_t  snr_max;
    uint16_t crc;
    uint16_t size;
    uint8_t mhdr;
    uint8_t devaddr[4];
    uint8_t fctrl;
    uint8_t fcnt[2];
    uint8_t payload[248];
} slg_hk_pkt;

/* Initialize SLG parameters. */
slg_param_t init_slg_param(void);

/* Initialize Regional Parameters */
slg_region_param_t init_slg_region_param(void);

/* Initialize Transmit Parameters */
slg_tx_param_t init_slg_tx_param(void);

/* Initialize Whitelist Parameters */
slg_whitelist_t init_slg_whitelist(void);

/* SLG interaction command line */
int cmd_slg_handle(int);

void vTask_SLG_Data_Collection(void* pvParameter);

#endif /* INC_SLG_TASK_H_ */
