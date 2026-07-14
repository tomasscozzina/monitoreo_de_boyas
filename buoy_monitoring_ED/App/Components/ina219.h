/******************************************************************************
 * @file    ina219.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Driver del sensor de energía INA219 de Texas Instruments
 ******************************************************************************/

#ifndef __INA219_H__
#define __INA219_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "i2c.h"    /* Expone hi2c1 */

/* Identificadores de canal */
typedef enum {
    INA219_LANTERN = 0,	/* Dirección 0x40 (A1=GND, A0=GND) */
    INA219_BATERY,     	/* Dirección 0x41 (A1=GND, A0=VS)  */
    INA219_SOLAR,     	/* Dirección 0x44 (A1=VS,  A0=GND) */
    INA219_COUNT
} INA219_Channel;

/* Códigos de retorno */
typedef enum {
    ENERGY_OK = 0,   		/* Operación exitosa */
	ENERGY_ERR_COMMS,		/* Hay problemas en la comunicación con el sensor de energía */
	ENERGY_ERR_BAD_TIMING	/* Se llamó a INA219_detectAndMeasure cuando la linterna transicionaba, o estaba apagada */
} Energy_Status_t;

/* Estuctura de datos */
typedef struct __attribute__((packed)) {
    int16_t voltage_mV;		/* Tensión V_BUS */
	int16_t current_mA;		/* Corriente I_SHUNT */
} Energy_Data_t;

/* API pública */

Energy_Status_t INA219_init(INA219_Channel ch);
Energy_Status_t INA219_getVoltageAndCurrent(INA219_Channel ch, Energy_Data_t *data);
Energy_Status_t INA219_detectAndMeasure(INA219_Channel ch, int16_t *current_mA, int16_t min_voltage_mV);

#ifdef __cplusplus
}
#endif

#endif /* __INA219_H__ */
