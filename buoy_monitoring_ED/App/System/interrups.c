/******************************************************************************
 * @file    interrups.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   [Breve descripción]
 ******************************************************************************/

// TOMI2: Podría crear funciones SET para habilitar y deshabilitar IRQs que no necesito que despierten al micro
#include "interrups.h"
#include "main.h"
#include "sw1.h"
#include "adxl345.h"
#include "lorawan_wrapper.h"
#include "system_power.h"
#include "buoy_app.h"

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	switch (GPIO_Pin) {

		case SW1_Pin:
			SW1_notifyPress();
            break;

        case ADXL345_INT1_Pin:
            ADXL345_notifyImpact();
            break;

        case ADXL345_INT2_Pin:
            ADXL345_notifyDataReady();
            break;

        case RFM95W_G0_Pin:
        	RFM95W_notifyG0();
            break;

        default:
            break;
    }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc){
	RTC_notifyTimeOut();
}
