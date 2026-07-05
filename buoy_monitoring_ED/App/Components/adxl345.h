/******************************************************************************
 * @file    adxl345.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Driver del acelerómetro adxl345 de Analog Devices
 ******************************************************************************/

#ifndef __ADXL345_H__
#define __ADXL345_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Códigos de retorno */
typedef enum {
    ACEL_OK  =  0,   	/* Operación exitosa */
	ACEL_ERR_COMMS,		/* Hay problemas en la comunicación con el acelerómetro */
} Acel_Status;

/* API Pública */

Acel_Status ADXL345_init(void);
Acel_Status ADXL345_getTilt(uint8_t *tilt);
void ADXL345_notifyDataReady(void);
void ADXL345_notifyImpact(void);
bool ADXL345_getImpactEv(Acel_Status *ret);

#ifdef __cplusplus
}
#endif

#endif /* __ADXL345_H__ */
