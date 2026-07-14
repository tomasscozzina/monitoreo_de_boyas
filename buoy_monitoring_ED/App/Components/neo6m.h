/******************************************************************************
 * @file    neo6m.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Driver del módulo GPS neo6m de uBlox
 ******************************************************************************/

#ifndef __NEO6M_H__
#define __NEO6M_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Códigos de retorno */
typedef enum {
    GPS_OK 	=  0,   	/* Operación exitosa */
	GPS_ERR_COMMS,		/* Hay problemas en la comunicación con el módulo GPS */
	GPS_ERR_NO_FIX,  	/* El GPS no tiene fix válido (2D o 3D) */
	GPS_ERR_ANTENNA  	/* La antena del GPS está desconectada o sin alimentación */
} GPS_Status_t;

/* Estuctura de datos (fecha UTC) */
typedef struct __attribute__((packed)) {
    int32_t latitude;	/* Latitud  ([°] x 1e7) */
	int32_t longitude;	/* Longitud ([°] x 1e7) */
	uint16_t year;    	/* Año (aaaa) */
    uint8_t month;   	/* Mes (mm) */
    uint8_t day;     	/* Día (dd) */
    uint8_t hour;    	/* Hora (0–23) */
    uint8_t min;     	/* Minutos (0–59) */
    uint8_t sec;     	/* Segundos (0–60) */
} GPS_Data_t;

/* API Pública */
GPS_Status_t GPS_getData(GPS_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __NEO6M_H__ */
