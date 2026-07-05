/******************************************************************************
 * @file    sw1.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   [Breve descripción]
 ******************************************************************************/

#include "sw1.h"
#include "stm32l4xx_hal.h"

#define ANTIREBOTE 300

static volatile bool sw1_flag = false;
static bool en_espera_antirebote = false;
static uint32_t start = 0;

void SW1_notifyPress(void) {
	sw1_flag = true;
}

bool SW1_getPressEv(void){
    if (en_espera_antirebote == true) {
        if (HAL_GetTick() - start >= ANTIREBOTE) {
            en_espera_antirebote = false;
        }
    }
    if (sw1_flag == true) {
        sw1_flag = false;

        if (en_espera_antirebote == false) {
            start = HAL_GetTick();
            en_espera_antirebote = true;
            return true;
        }
    }
    return false;
}
