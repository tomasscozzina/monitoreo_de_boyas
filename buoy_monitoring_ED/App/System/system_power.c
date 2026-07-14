/******************************************************************************
 * @file    system_power.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   [Breve descripción]
 ******************************************************************************/

#include "system_power.h"
#include "system_clock.h"
#include "rtc.h"

static volatile bool rtc_flag = false;

void SystemPower_sleep(uint32_t seconds) {
	if(seconds > 65535) {
		seconds = 65535; // Tiempo máximo del contador. Equivale a 18 horas, 12 minutos y 15 segundos
	}
	if(HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0) != HAL_OK) {
		Error_Handler();
	}
	HAL_SuspendTick();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
    /* Con la llamada anterior el microcontrolador pasa a STOP 2 */
    /* --------------------- SYSTEM SLEEP -----------------------*/
    /* Al despertar, ejecuta la ISR correspondiente, y continúa con el PC en la línea siguiente */
    SystemClock_Config();
    HAL_ResumeTick();
    if(HAL_RTCEx_DeactivateWakeUpTimer(&hrtc) != HAL_OK) {
    	Error_Handler();
    }
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
