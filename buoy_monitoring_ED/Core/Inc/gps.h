/**
 ******************************************************************************
 * @file    gps.h
 * @brief   Librería para gestionar el módulo GPS NEO-6M via protocolo UBX.
 *
 * El módulo está configurado en modo "solo responde ante solicitudes" (no emite
 * tramas periódicas). La comunicación usa USART1 con lecturas/escrituras
 * bloqueantes (sin DMA).
 *
 * Flujo de uso típico:
 *   1. GPS_WakeUp()        → despierta el módulo y verifica antena (MON-HW)
 *   2. GPS_HasValidFix()   → consulta NAV-STATUS; retorna error si no hay fix
 *   3. GPS_GetLatitude()   → devuelve latitud en grados * 1e7
 *   4. GPS_GetLongitude()  → devuelve longitud en grados * 1e7
 *   5. GPS_GetUTCTime()    → devuelve la fecha y hora UTC
 *   6. GPS_Sleep()         → manda el módulo a dormir (PMREQ duración infinita)
 ******************************************************************************
 */

#ifndef __GPS_H__
#define __GPS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "usart.h"   /* expone huart1 */

/* ===========================================================================
 * Códigos de retorno
 * =========================================================================*/
typedef enum {
    GPS_OK              =  0,   /* Operación exitosa */
    GPS_ERR_TIMEOUT     = -1,   /* No llegó respuesta a tiempo */
    GPS_ERR_CHECKSUM    = -2,   /* Checksum del paquete recibido incorrecto */
    GPS_ERR_BADHEADER   = -3,   /* Clase o ID del paquete no coinciden con lo esperado */
    GPS_ERR_NO_FIX      = -4,   /* El GPS no tiene fix válido (2D o 3D) */
    GPS_ERR_ANTENNA     = -5,   /* Antena desconectada o sin alimentación */
} GPS_Status;

/* ===========================================================================
 * Estructura de fecha/hora UTC (NAV-TIMEUTC)
 * =========================================================================*/
typedef struct {
    uint16_t year;    /* Año (ej. 2026)   */
    uint8_t  month;   /* Mes  (1–12)      */
    uint8_t  day;     /* Día  (1–31)      */
    uint8_t  hour;    /* Hora (0–23) UTC  */
    uint8_t  min;     /* Minutos (0–59)   */
    uint8_t  sec;     /* Segundos (0–60)  */
} GPS_UTCTime;

/* ===========================================================================
 * API pública
 * =========================================================================*/

/**
 * @brief  Despierta el módulo GPS y verifica que está operativo.
 *
 * Envía 0xFF para salir del modo backup, espera 50 ms y consulta MON-HW.
 * Verifica que la antena esté conectada (aStatus == 2) y con alimentación
 * (aPower == 1). Si cualquiera de las dos condiciones falla, retorna
 * GPS_ERR_ANTENNA.
 *
 * @retval GPS_OK           Módulo despierto, antena OK.
 * @retval GPS_ERR_TIMEOUT  No se recibió respuesta MON-HW.
 * @retval GPS_ERR_CHECKSUM Checksum incorrecto en la respuesta.
 * @retval GPS_ERR_ANTENNA  Antena desconectada o sin alimentación.
 */
GPS_Status GPS_WakeUp(void);

/**
 * @brief  Consulta NAV-STATUS y verifica fix válido (2D o 3D).
 *
 * Si gpsFix es 2 o 3, solicita también NAV-POSLLH y NAV-TIMEUTC y guarda
 * los datos internamente para que los getters puedan devolverlos sin
 * nuevas transacciones UART.
 *
 * @retval GPS_OK            Fix válido. Datos de posición y tiempo cargados.
 * @retval GPS_ERR_TIMEOUT   No se recibió alguna respuesta.
 * @retval GPS_ERR_CHECKSUM  Checksum incorrecto en alguna respuesta.
 * @retval GPS_ERR_NO_FIX    gpsFix no es 2 ni 3.
 */
GPS_Status GPS_HasValidFix(void);

/**
 * @brief  Devuelve la latitud obtenida en la última llamada exitosa a GPS_HasValidFix().
 * @param[out] lat  Latitud en grados * 1e7. Ej: -330194022 = -33.0194022°
 */
void GPS_GetLatitude(int32_t *lat);

/**
 * @brief  Devuelve la longitud obtenida en la última llamada exitosa a GPS_HasValidFix().
 * @param[out] lon  Longitud en grados * 1e7. Ej: -603956556 = -60.3956556°
 */
void GPS_GetLongitude(int32_t *lon);

/**
 * @brief  Devuelve la fecha y hora UTC obtenida en la última llamada exitosa a GPS_HasValidFix().
 * @param[out] utc  Puntero a GPS_UTCTime donde se escribe el resultado.
 */
void GPS_GetUTCTime(GPS_UTCTime *utc);

/**
 * @brief  Envía RXM-PMREQ para dormir el módulo indefinidamente.
 *
 * El módulo no envía ACK para este comando. Se despierta enviando
 * cualquier byte por UART (0xFF).
 */
void GPS_Sleep(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPS_H__ */
