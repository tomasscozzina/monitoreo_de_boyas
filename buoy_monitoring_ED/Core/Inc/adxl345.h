/**
 ******************************************************************************
 * @file    adxl345.h
 * @brief   Librería para el acelerómetro ADXL345 vía I2C.
 *
 * El sensor cumple dos funciones:
 *   1. Detección de impactos (Activity interrupt) → INT1 → EXTI1, siempre activa.
 *   2. Medición de inclinación → lectura bajo demanda, sincronizada con
 *      Data Ready interrupt → INT2 → EXTI0, habilitada solo durante la lectura.
 *
 * Flujo de uso típico:
 *   1. ADXL345_Init()          → configura el sensor al inicio del programa.
 *   2. (el micro duerme)
 *   3. Al despertar por RTC:
 *        ADXL345_ReadTilt()    → habilita Data Ready, espera INT2, lee y calcula.
 *   4. (el micro vuelve a dormir)
 *   5. Si INT1 dispara mientras duerme:
 *        ADXL345_HandleImpact() → limpia la interrupción en el sensor.
 ******************************************************************************
 */

#ifndef __ADXL345_H__
#define __ADXL345_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "i2c.h"    // Expone hi2c1

/* ===========================================================================
 * Códigos de retorno
 * =========================================================================*/
typedef enum {
    ADXL345_OK          	=  0,   // Operación exitosa
    ADXL345_ERR_I2C     	= -1,   // Error en la comunicación I2C
    ADXL345_ERR_DEVID   	= -2,   // El Device ID leído no coincide con 0xE5
    ADXL345_ERR_TIMEOUT 	= -3,   // No llegó la interrupción Data Ready a tiempo
	ADXL345_ERR_INTERRUPT 	= -4	// Se activó la interrupción INT1 pero no por Activity
} ADXL345_Status;

/* ===========================================================================
 * Estructura de resultado de inclinación
 * =========================================================================*/
typedef struct {
    float tilt_deg;     /* Ángulo de inclinación total respecto de la vertical en grados */
    float tilt_xz_deg;  /* Ángulo respecto del eje Z sobre el plano Y=0 (inclinación en X) */
    float tilt_yz_deg;  /* Ángulo respecto del eje Z sobre el plano X=0 (inclinación en Y) */
} ADXL345_TiltData;

/* ===========================================================================
 * API pública
 * =========================================================================*/

/**
 * @brief  Inicializa y configura el ADXL345.
 *
 * Verifica el Device ID, configura DATA_FORMAT, BW_RATE, THRESH_ACT,
 * ACT_INACT_CTL, INT_MAP e INT_ENABLE, y activa measurement mode.
 * Debe llamarse una sola vez en la inicialización del sistema, después
 * de MX_I2C1_Init().
 *
 * @retval ADXL345_OK        Sensor inicializado correctamente.
 * @retval ADXL345_ERR_I2C   Error de comunicación.
 * @retval ADXL345_ERR_DEVID Device ID incorrecto (sensor no encontrado).
 */
ADXL345_Status ADXL345_Init(void);

/**
 * @brief  Lee la inclinación actual de la boya.
 *
 * Habilita la interrupción Data Ready, espera que INT2 se active,
 * lee los 6 bytes de datos de aceleración en burst, deshabilita
 * Data Ready y calcula el ángulo de inclinación.
 *
 * @param[out] data  Puntero a estructura donde se escriben los resultados.
 *
 * @retval ADXL345_OK           Lectura y cálculo exitosos.
 * @retval ADXL345_ERR_I2C      Error de comunicación.
 * @retval ADXL345_ERR_TIMEOUT  No llegó la interrupción Data Ready.
 */
ADXL345_Status ADXL345_ReadTilt(ADXL345_TiltData *data);

/**
 * @brief  Limpia la interrupción de impacto en el sensor.
 *
 * Debe llamarse desde el main cuando se detecta el flag de impacto,
 * antes de armar la trama de emergencia. Lee INT_SOURCE para que el
 * ADXL345 baje la línea INT1.
 *
 * @retval ADXL345_OK       Interrupción limpiada.
 * @retval ADXL345_ERR_I2C  Error de comunicación.
 */
ADXL345_Status ADXL345_HandleImpact(void);

/**
 * @brief  Notifica al driver que INT2 (Data Ready) se activó.
 *
 * Debe llamarse desde HAL_GPIO_EXTI_Callback() cuando GPIO_Pin == ADXL345_INT2_Pin.
 * No hace operaciones I2C; solo setea un flag interno.
 */
void ADXL345_NotifyDataReady(void);

/**
 * @brief  Notifica al driver que INT1 (Activity/impacto) se activó.
 *
 * Debe llamarse desde HAL_GPIO_EXTI_Callback() cuando GPIO_Pin == ADXL345_INT1_Pin.
 * No hace operaciones I2C; solo setea un flag interno.
 */
void ADXL345_NotifyImpact(void);

/**
 * @brief  Devuelve y limpia el flag de impacto.
 *
 * @retval 1  Se detectó un impacto desde la última llamada.
 * @retval 0  No hubo impacto.
 */
uint8_t ADXL345_GetAndClearImpactFlag(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADXL345_H__ */
