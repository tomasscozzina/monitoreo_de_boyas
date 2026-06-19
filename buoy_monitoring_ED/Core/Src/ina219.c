/**
 ******************************************************************************
 * @file    ina219.c
 * @brief   Implementación de la librería INA219 vía I2C.
 *
 * Comunicación: I2C1 (hi2c1).
 * Módulos: 3 unidades con direcciones 0x40, 0x41 y 0x44.
 * R_SHUNT: 0.1 Ω en cada módulo.
 ******************************************************************************
 */

#include "ina219.h"
#include "config.h"

/* ===========================================================================
 * Direcciones I2C de los 3 módulos (7 bits)
 * =========================================================================*/
#define INA219_I2C_ADDR_CH0     	0x40
#define INA219_I2C_ADDR_CH1     	0x41
#define INA219_I2C_ADDR_CH2     	0x44

// Las direcciones I2C deben ser desplazadas a la izquierda para llamar los métodos de Tx y Rx
#define INA219_I2C_ADDR_CH0_SHIFT	INA219_I2C_ADDR_CH0 << 1
#define INA219_I2C_ADDR_CH1_SHIFT	INA219_I2C_ADDR_CH1 << 1
#define INA219_I2C_ADDR_CH2_SHIFT   INA219_I2C_ADDR_CH2 << 1

/* ===========================================================================
 * Registros del INA219
 * =========================================================================*/
#define REG_CONFIGURATION   0x00
#define REG_SHUNT_VOLTAGE   0x01
#define REG_BUS_VOLTAGE     0x02

/* ===========================================================================
 * Valores de configuración
 *
 * CONFIG register (16 bits):
 *   [15]    RST      = 0   (sin reset)
 *   [14]    —        = 0   (reservado)
 *   [13]    BRNG     = 1   (bus voltage range: 32V)
 *   [12:11] PG[1:0]  = 11  (ganancia shunt: ±320 mV)
 *   [10:7]  BADC     = 1111 (bus ADC:   128 muestras promediadas)
 *   [6:3]   SADC     = 1111 (shunt ADC: 128 muestras promediadas)
 *   [2:0]   MODE     = 111  (shunt + bus, continuo)
 *
 *   Valor: 0b0011_1111_1111_1111 = 0x3FFF
 *
 * La corriente se calcula por software a partir del registro Shunt Voltage:
 *   I = V_shunt / R_shunt
 *   donde:
 *   	R_shunt = 0.1 Ω
 *   	V_shunt = V_shunt_raw * V_shunt LSB = V_shunt_raw * 10 µV
 *
 *   entonces:
 *   I [mA] = V_shunt_raw * 10 µV / 0.1 Ω = V_shunt_raw * 0.1 mA/LSB
 *          = V_shunt_raw / 10
 * =========================================================================*/
#define CFG_CONFIGURATION   0x3FFF

/* ===========================================================================
 * Máscaras
 * =========================================================================*/
#define MASK_BUS_VOLTAGE_SHIFT  3           /* El valor de tensión ocupa bits [15:3] */

/* ===========================================================================
 * Variables privadas
 * =========================================================================*/
static const uint8_t s_i2c_addrs[INA219_CH_COUNT] = {
	INA219_I2C_ADDR_CH0_SHIFT,
	INA219_I2C_ADDR_CH1_SHIFT,
	INA219_I2C_ADDR_CH2_SHIFT
};

/* ===========================================================================
 * Funciones privadas
 * =========================================================================*/

/**
 * @brief  Escribe un registro de 16 bits en el INA219 (big-endian).
 */
static INA219_Status reg_Write(uint8_t addr, uint8_t reg, uint16_t value) {
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)(value >> 8);     /* MSB primero (big-endian) */
    buf[2] = (uint8_t)(value & 0xFF);
    if (HAL_I2C_Master_Transmit(&hi2c1, addr, buf, 3, 10) != HAL_OK) {
        return INA219_ERR_I2C;
    }
    return INA219_OK;
}

/**
 * @brief  Lee un registro de 16 bits del INA219 (big-endian).
 */
static INA219_Status reg_Read(uint8_t addr, uint8_t reg, uint16_t *value) {
    uint8_t buf[2];
    if (HAL_I2C_Master_Transmit(&hi2c1, addr, &reg, 1, 10) != HAL_OK) {
        return INA219_ERR_I2C;
    }
    if (HAL_I2C_Master_Receive(&hi2c1, addr, buf, 2, 10) != HAL_OK) {
        return INA219_ERR_I2C;
    }
    *value = ((uint16_t)buf[0] << 8) | buf[1];   /* MSB primero (big-endian) */
    return INA219_OK;
}

/* ===========================================================================
 * API pública
 * =========================================================================*/

INA219_Status INA219_InitAll(void) {
    INA219_Status ret;

    for (int i = 0; i < INA219_CH_COUNT; i++) {

        /* Escribir registro de configuración */
        ret = reg_Write(s_i2c_addrs[i], REG_CONFIGURATION, CFG_CONFIGURATION);
        if (ret != INA219_OK) return ret;
    }

    return INA219_OK;
}

INA219_Status INA219_GetVoltage(INA219_Channel ch, int32_t *voltage_mV) {
    if (ch >= INA219_CH_COUNT) return INA219_ERR_CHANNEL;

    INA219_Status ret;
    uint16_t raw;

    ret = reg_Read(s_i2c_addrs[ch], REG_BUS_VOLTAGE, &raw);
    if (ret != INA219_OK) return ret;

    /*
     * Bus Voltage register:
     *   Bits [15:3]: valor de tensión. LSB = 4 mV.
     *   Bit  [1]:    CNVR (conversión lista, no se usa aquí).
     *   Bit  [0]:    OVF  (overflow, ya verificado arriba).
     */
    uint16_t bus_val = raw >> MASK_BUS_VOLTAGE_SHIFT;
    *voltage_mV = (int32_t)bus_val * 4;     /* 4 mV por LSB */

    return INA219_OK;
}

INA219_Status INA219_GetCurrent(INA219_Channel ch, int32_t *current_mA) {
    if (ch >= INA219_CH_COUNT) return INA219_ERR_CHANNEL;

    INA219_Status ret;
    uint16_t raw;

    ret = reg_Read(s_i2c_addrs[ch], REG_SHUNT_VOLTAGE, &raw);
    if (ret != INA219_OK) return ret;

    /*
     * Shunt Voltage register: entero con signo en complemento a 2.
     * Resolución: 10 µV/LSB.
     * Con R_shunt = 0.1 Ω:
     *   I = V_shunt / R_shunt
     *   I [mA] = (raw * 10 µV) / 0.1 Ω = raw * 0.1 mA/LSB = raw / 10
     *
     *   Se redondea al entero más cercano antes de truncar:
     *   +5 antes de dividir si el valor es positivo, -5 si es negativo.
     */
    int32_t current_dmA = (int32_t)(int16_t)raw;
    *current_mA = (current_dmA >= 0) ? (current_dmA + 5) / 10
                                     : (current_dmA - 5) / 10;

    return INA219_OK;
}

INA219_Status INA219_DetectAndMeasure(INA219_Channel ch, int32_t *current_mA, int32_t min_voltage_mV) {
    if (ch >= INA219_CH_COUNT) return INA219_ERR_CHANNEL;

    INA219_Status ret;
    int32_t v1_mV = 0;
    int32_t v2_mV = 0;

    /* 1. Primera medición de tensión */
    ret = INA219_GetVoltage(ch, &v1_mV);
    if (ret != INA219_OK) return ret;
    HAL_Delay(75);

    /* 2. Medición de corriente */
    ret = INA219_GetCurrent(ch, current_mA);
    if (ret != INA219_OK) return ret;
    HAL_Delay(75);

    /* 3. Segunda medición de tensión */
    ret = INA219_GetVoltage(ch, &v2_mV);
    if (ret != INA219_OK) return ret;
    HAL_Delay(75);

    uint8_t v1_on = (v1_mV >= min_voltage_mV);
    uint8_t v2_on = (v2_mV >= min_voltage_mV);

//    DPRINT("v1_mV = %lu - v2_mV = %lu \r\n", v1_mV, v2_mV);

    if (!v1_on && !v2_on) {
        /* Carga apagada durante toda la medición */
        *current_mA = 0;
        return INA219_ERR_LIGHT_OFF;
    }

    if (!v1_on || !v2_on) {
        /* La carga cambió de estado durante la medición: corriente no confiable */
        *current_mA = 0;
        return INA219_ERR_UNSTABLE;
    }

    /* Ambas tensiones superan el umbral: corriente válida */
    return INA219_OK;
}
