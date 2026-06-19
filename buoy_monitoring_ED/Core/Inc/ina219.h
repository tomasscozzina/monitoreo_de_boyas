/**
 ******************************************************************************
 * @file    ina219.h
 * @brief   Librería para el sensor de tensión y corriente INA219 vía I2C.
 *
 * El sistema utiliza 3 módulos INA219, cada uno con una R_SHUNT de 0.1 Ω
 * y una dirección I2C única configurada por hardware:
 *   - INA219_ADDR_0: 0x40  (A1=GND, A0=GND)
 *   - INA219_ADDR_1: 0x41  (A1=GND, A0=VS)
 *   - INA219_ADDR_2: 0x44  (A1=VS,  A0=GND)
 *
 * Configuración del sensor:
 *   - Rango de tensión de bus: 32V (26V como máximo admisible por hardware)
 *   - Ganancia del shunt: ±320 mV (rango completo)
 *   - Promedio ADC: 128 muestras (bus y shunt)
 *   - Modo: continuo (shunt + bus)
 *
 * Flujo de uso típico:
 *   1. INA219_InitAll()           → configura y calibra los 3 módulos.
 *   2. INA219_GetVoltage(canal)   → retorna tensión de bus en mV.
 *   3. INA219_GetCurrent(canal)   → retorna corriente en mA.
 *   4. INA219_DetectAndMeasure(canal, ...) → mide corriente solo si ambas
 *                                            lecturas de tensión superan el
 *                                            umbral mínimo indicado.
 ******************************************************************************
 */

#ifndef __INA219_H__
#define __INA219_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "i2c.h"    /* Expone hi2c1 */

/* ===========================================================================
 * Identificadores de canal
 * =========================================================================*/
typedef enum {
    INA219_CH0 = 0,     /* Dirección 0x40 */
    INA219_CH1 = 1,     /* Dirección 0x41 */
    INA219_CH2 = 2,     /* Dirección 0x44 */
    INA219_CH_COUNT = 3
} INA219_Channel;

/* ===========================================================================
 * Códigos de retorno
 * =========================================================================*/
typedef enum {
    INA219_OK           	=  0,   /* Operación exitosa */
    INA219_ERR_I2C      	= -1,   /* Error en la comunicación I2C */
    INA219_ERR_CHANNEL  	= -2,   /* Canal inválido */
    INA219_ERR_UNSTABLE   	= -4,   /* Tensión inestable durante la medición de corriente */
    INA219_ERR_LIGHT_OFF  	= -5,   /* Tensión por debajo del umbral: carga apagada */
} INA219_Status;

/* ===========================================================================
 * API pública
 * =========================================================================*/

/**
 * @brief  Inicializa y calibra los 3 módulos INA219.
 *
 * Escribe el registro de configuración y el registro de calibración
 * en cada uno de los 3 sensores. Debe llamarse una sola vez durante
 * la inicialización del sistema, después de MX_I2C1_Init().
 *
 * @retval INA219_OK        Todos los módulos inicializados correctamente.
 * @retval INA219_ERR_I2C   Error de comunicación con alguno de los módulos.
 */
INA219_Status INA219_InitAll(void);

/**
 * @brief  Lee la tensión del bus de un canal específico.
 *
 * Lee el registro Bus Voltage del módulo indicado y convierte el valor
 * a milivoltios. La resolución es 4 mV/LSB.
 *
 * @param[in]  ch       Canal a leer (INA219_CH0, INA219_CH1 o INA219_CH2).
 * @param[out] voltage_mV  Puntero donde se escribe la tensión en mV.
 *
 * @retval INA219_OK            Lectura exitosa.
 * @retval INA219_ERR_I2C       Error de comunicación.
 * @retval INA219_ERR_CHANNEL   Canal fuera de rango.
 * @retval INA219_ERR_OVERFLOW  Bit OVF activo en el registro de tensión.
 */
INA219_Status INA219_GetVoltage(INA219_Channel ch, int32_t *voltage_mV);

/**
 * @brief  Lee la corriente de un canal específico.
 *
 * Lee el registro Shunt Voltage del módulo indicado y calcula la corriente
 * por software: I = V_shunt / R_shunt, con R_shunt = 0.1 Ω.
 * El registro Shunt Voltage tiene una resolución de 10 µV/LSB.
 *
 * @param[in]  ch         Canal a leer (INA219_CH0, INA219_CH1 o INA219_CH2).
 * @param[out] current_mA Puntero donde se escribe la corriente en mA.
 *                        Puede ser negativo (corriente inversa).
 *
 * @retval INA219_OK          Lectura exitosa.
 * @retval INA219_ERR_I2C     Error de comunicación.
 * @retval INA219_ERR_CHANNEL Canal fuera de rango.
 */
INA219_Status INA219_GetCurrent(INA219_Channel ch, int32_t *current_mA);

/**
 * @brief  Mide la corriente de una carga solo si esta se encuentra encendida.
 *
 * Realiza tres operaciones en secuencia:
 *   1. Mide la tensión de bus (V1).
 *   2. Mide la corriente.
 *   3. Mide la tensión de bus nuevamente (V2).
 *
 * La medición de corriente se considera válida únicamente si tanto V1
 * como V2 superan el umbral mínimo indicado en min_voltage_mV. Esto
 * garantiza que la carga estaba encendida durante toda la ventana de
 * medición del sensor.
 *
 * Si alguna de las dos tensiones es menor al umbral, se escribe 0 en
 * *current_mA y se retorna INA219_ERR_LIGHT_OFF o INA219_ERR_UNSTABLE
 * según el caso.
 *
 * @param[in]  ch             Canal a leer (INA219_CH0, INA219_CH1 o INA219_CH2).
 * @param[out] current_mA     Puntero donde se escribe la corriente en mA,
 *                            o 0 si la carga no estaba encendida.
 * @param[in]  min_voltage_mV Tensión mínima en mV que debe superar la carga
 *                            para considerarse encendida. Se recomienda usar
 *                            un #define con el valor nominal menos un margen.
 *
 * @retval INA219_OK            Carga encendida y estable, corriente válida.
 * @retval INA219_ERR_LIGHT_OFF Ambas tensiones por debajo del umbral: apagada.
 * @retval INA219_ERR_UNSTABLE  V1 supera el umbral pero V2 no (o viceversa):
 *                              la carga cambió de estado durante la medición.
 * @retval INA219_ERR_I2C       Error de comunicación.
 * @retval INA219_ERR_CHANNEL   Canal fuera de rango.
 * @retval INA219_ERR_OVERFLOW  Overflow en alguna de las lecturas de tensión.
 */
INA219_Status INA219_DetectAndMeasure(INA219_Channel ch, int32_t *current_mA, int32_t min_voltage_mV);

#ifdef __cplusplus
}
#endif

#endif /* __INA219_H__ */
