/*
 * slg_task.h
 *
 *  Created on: Nov 6, 2023
 *      Author: spacelab-cute-PC
 */

#ifndef INC_SLG_TASK_H_
#define INC_SLG_TASK_H_

#include "slg.h"
#include <csp/csp.h>
#include <csp/csp_types.h>
#include <csp/csp_error.h>
#include <csp/interfaces/csp_if_i2c.h>
#include <csp/csp_types.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SLG_PACKET_MAX  40000
#define SLG_GET_PARAM_RET_LEN 44
#define SLG_GET_REGPARAM_RET_LEN  96
#define SLG_GET_WHITELIST_RET_LEN  44
#define SLG_SF_QUEUE_MAX   60

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

void vTask_SLG_Data_Collection(void* pvParameter);

#endif /* INC_SLG_TASK_H_ */
