/******************************************************************************
 * @file    buoy_app.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Aplicación principal para el monitoreo de boyas
 ******************************************************************************/

#ifndef APPLICATION_BUOY_APP_H_
#define APPLICATION_BUOY_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "parameters.h"
#include "adxl345.h"
#include "ina219.h"
#include "neo6m.h"
#include "sw1.h"
#include "lorawan_wrapper.h"
#include "system_power.h"

/* Tamaño del payload: 26 bytes */
typedef struct __attribute__((packed)) {
	GPS_Data_t gps;					 /* 15 bytes */
	uint8_t tilt;					 /* 1  bytes */
	Energy_Data_t solarP;			 /* 4  bytes */
	Energy_Data_t batery;			 /* 4  bytes */
	/* Flags */						 /* 2  bytes */
	uint16_t gps_status 		:2;	 /* 0 = GPS_OK - 1 = GPS_ERR_COMMS - 2 = GPS_ERR_NO_FIX - 3 = GPS_ERR_ANTENNA */
	uint16_t acel_status		:1;	 /* 0 = ACEL_OK - 1 = ACEL_ERR_COMMS */
	uint16_t solarSens_status 	:1;	 /* 0 = ENERGY_OK - 1 = ENERGY_ERR_COMMS */
	uint16_t baterySens_status 	:1;	 /* 0 = ENERGY_OK - 1 = ENERGY_ERR_COMMS */
	uint16_t lanternSens_status :1;	 /* 0 = ENERGY_OK - 1 = ENERGY_ERR_COMMS */
	uint16_t periodic_TX		:1;	 /* 0 = NO_PERIODIC_TX - 1 = PERIODIC_TX */
	uint16_t gps_set			:1;	 /* 0 = NO_SET - 1 = SET */
	uint16_t impact				:1;	 /* 0 = NO_IMPACT - 1 = IMPACT */
	uint16_t lantern_failure	:1;	 /* 0 = NO_FAILURE - 1 = FAILURE */
	uint16_t flasher_failure	:1;  /* 0 = NO_FAILURE - 1 = FAILURE */
	uint16_t padding			:5;	 /* RESERVED BITS */
}payload_t;

void buoyApp_init(void);
void buoyApp_run(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_BUOY_APP_H_ */
