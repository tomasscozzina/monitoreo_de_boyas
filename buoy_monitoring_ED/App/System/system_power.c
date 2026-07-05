/******************************************************************************
 * @file    system_power.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   [Breve descripción]
 ******************************************************************************/

#include "system_power.h"
#include "system_clock.h"

static volatile bool rtc_flag = false;

void SystemPower_sleep(void) {
	HAL_SuspendTick();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
    /* Con la llamada anterior el microcontrolador pasa a STOP 2 */
    /* Al despertar, ejecuta la ISR correspondiente, y continúa con el PC en la línea siguiente */
    SystemClock_Config();
    HAL_ResumeTick();
}

void RTC_notifyTimeOut(void){
	rtc_flag = true;
}

bool RTC_getTimeOutEv(void){
	if(rtc_flag) {
		rtc_flag = false;
		return true;
	}
	return false;
}
