/*
 * csp_task.h
 *
 *  Created on: Nov 8, 2023
 *      Author: e1406
 */

#ifndef INC_CSP_TASK_H_
#define INC_CSP_TASK_H_

#include "slg_task.h"

/** CSP I2C initialize */
int csp_i2c_init();


/** CSP router task */
void CSP_Router_Task(void*);

/** SLG I2C RX
    handle the I2C RX interrupt and pass the data to the CSP */
void SLG_I2C_RX();

/** CSP router task start */
void router_start();

/** CSP router task stop */
void router_stop();

/** CSP initialize */
int csp_start();

#endif /* INC_CSP_TASK_H_ */
