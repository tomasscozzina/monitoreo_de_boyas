/******************************************************************************
 * @file    system_power.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   [Breve descripción]
 ******************************************************************************/

#ifndef SYSTEM_SYSTEM_POWER_H_
#define SYSTEM_SYSTEM_POWER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

void SystemPower_sleep(uint32_t minuts);
void RTC_notifyTimeOut(void);
bool RTC_getTimeOutEv(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_SYSTEM_POWER_H_ */
