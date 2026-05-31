/*
 ******************************************************************************
 * @file    adxl345.c
 * @brief   Implementación de la librería ADXL345 vía I2C.
 *
 * Comunicación: I2C1 (hi2c1), dirección 0x53 (SDO a GND).
 * Interrupciones:
 *   INT1 (PB1, EXTI1) → Activity (impacto), siempre activa.
 *   INT2 (PB0, EXTI0) → Data Ready, habilitada solo durante la lectura.
 *****************************************************************************/

#include "adxl345.h"
#include <math.h>
#include "config.h"

/* ===========================================================================
 * Dirección I2C y registros
 * =========================================================================*/

#define ADXL345_I2C_ADDR    			0x53    				// TOMI: Dirección I2C (7 bits) del ADXL345
#define ADXL345_I2C_ADDR_SHIFT			ADXL345_I2C_ADDR << 1	// TOMI: Dirección I2C desplazada. Requerido por función de capa HAL

#define REG_DEVID           			0x00
#define REG_THRESH_ACT      			0x24
#define REG_ACT_INACT_CTL   			0x27
#define REG_BW_RATE         			0x2C
#define REG_POWER_CTL       			0x2D
#define REG_INT_ENABLE      			0x2E
#define REG_INT_MAP         			0x2F
#define REG_INT_SOURCE      			0x30
#define REG_DATA_FORMAT     			0x31
#define REG_DATAX0          			0x32

/* ===========================================================================
 * Mascaras
 * =========================================================================*/

#define MASK_DEVID          			0xE5
#define MASK_INT_SOURCE_ACTIVITY		0x10

/* ===========================================================================
 * Valores de configuración (tramas)
 * =========================================================================*/

#define CFG_POWER_CTL_STANDBY   		0x00
#define CFG_POWER_CTL_MEASURE   		0x08
#define CFG_DATA_FORMAT         		0x00
#define CFG_BW_RATE             		0x0A
#define CFG_THRESH_ACT          		0xFF	// TOMI: Escala = 62.5 mg/LSB -> 0xFF ~ 16g
#define CFG_ACT_INACT_CTL       		0xF0
#define CFG_INT_MAP             		0x80	// TOMI: Activity a INT1 - Data Ready a INT2
#define CFG_INT_ENABLE  				0x90

/* ===========================================================================
 * Constantes de conversión y cálculo
 * =========================================================================*/

#define ADXL345_DR_TIMEOUT_MS   		50		// TOMI: Timeout Data Ready
#define ADXL345_AVG_SAMPLES     		10    	// TOMI: Muestras a promediar en cada lectura de inclinación

/* ===========================================================================
 * Variables privadas
 * =========================================================================*/

static volatile uint8_t s_data_ready_flag = 0;
static volatile uint8_t s_impact_flag     = 0;

/* ===========================================================================
 * Funciones privadas
 * =========================================================================*/

/**
 * @brief  Escribe un byte en un registro del ADXL345.
 */
static ADXL345_Status reg_Write(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    if (HAL_I2C_Master_Transmit(&hi2c1, ADXL345_I2C_ADDR_SHIFT, buf, 2, 10) != HAL_OK) {
        return ADXL345_ERR_I2C;
    }
    return ADXL345_OK;
}

/**
 * @brief  Lee uno o más bytes consecutivos desde un registro del ADXL345.
 */
static ADXL345_Status reg_Read(uint8_t reg, uint8_t *data, uint8_t len) {
    if (HAL_I2C_Master_Transmit(&hi2c1, ADXL345_I2C_ADDR_SHIFT, &reg, 1, 10) != HAL_OK) {
        return ADXL345_ERR_I2C;
    }
    if (HAL_I2C_Master_Receive(&hi2c1, ADXL345_I2C_ADDR_SHIFT, data, len, 10) != HAL_OK) {
        return ADXL345_ERR_I2C;
    }
    return ADXL345_OK;
}

/* ===========================================================================
 * API pública
 * =========================================================================*/

ADXL345_Status ADXL345_Init(void) {
    ADXL345_Status ret;
    uint8_t devid;

    // Verificar comunicación y que el sensor sea el correcto
    ret = reg_Read(REG_DEVID, &devid, 1);
    if (ret != ADXL345_OK) return ret;
    if (devid != MASK_DEVID) return ADXL345_ERR_DEVID;

    // Poner en standby antes de configurar
    ret = reg_Write(REG_POWER_CTL, CFG_POWER_CTL_STANDBY);
    if (ret != ADXL345_OK) return ret;

    // Formato de datos: 10-bit, +-2g, right-justified
    ret = reg_Write(REG_DATA_FORMAT, CFG_DATA_FORMAT);
    if (ret != ADXL345_OK) return ret;

    // ODR 100 Hz, normal power mode
    ret = reg_Write(REG_BW_RATE, CFG_BW_RATE);
    if (ret != ADXL345_OK) return ret;

    // Threshold de actividad: 16g (ajustar experimentalmente)
    ret = reg_Write(REG_THRESH_ACT, CFG_THRESH_ACT);
    if (ret != ADXL345_OK) return ret;

    // Activity AC-coupled, ejes X+Y+Z
    ret = reg_Write(REG_ACT_INACT_CTL, CFG_ACT_INACT_CTL);
    if (ret != ADXL345_OK) return ret;

    // Data Ready a INT2, Activity a INT1
    ret = reg_Write(REG_INT_MAP, CFG_INT_MAP);
    if (ret != ADXL345_OK) return ret;

    // Limpiar interrupciones pendientes
    uint8_t int_source;
    ret = reg_Read(REG_INT_SOURCE, &int_source, 1);
    if (ret != ADXL345_OK) return ret;

    // Habilitar solo Activity. Data Ready se habilita solo durante la lectura
    ret = reg_Write(REG_INT_ENABLE, CFG_INT_ENABLE);	// TOMI: Prueba, habilité ambas int al inicio
    if (ret != ADXL345_OK) return ret;

    // Activar measurement mode
    ret = reg_Write(REG_POWER_CTL, CFG_POWER_CTL_MEASURE);
    if (ret != ADXL345_OK) return ret;

    return ADXL345_OK;
}

ADXL345_Status ADXL345_ReadTilt(ADXL345_TiltData *data) {
    ADXL345_Status ret;
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;

    HAL_NVIC_DisableIRQ(EXTI0_IRQn);

    uint8_t discard[6];
    ret = reg_Read(REG_DATAX0, discard, 6);
    if (ret != ADXL345_OK) return ret;

    s_data_ready_flag = 0;

    // Habilitar EXTI0: el próximo pulso en INT2 será dato fresco
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    for (int i = 0; i < ADXL345_AVG_SAMPLES; i++) {

        uint32_t start = HAL_GetTick();
        while (!s_data_ready_flag) {
            if (HAL_GetTick() - start >= 200) {
                HAL_NVIC_DisableIRQ(EXTI0_IRQn);
                return ADXL345_ERR_TIMEOUT;
            }
        }

        s_data_ready_flag = 0;

        uint8_t raw[6];
        ret = reg_Read(REG_DATAX0, raw, 6);
        if (ret != ADXL345_OK) {
            HAL_NVIC_DisableIRQ(EXTI0_IRQn);
            return ret;
        }

        sum_x += (int16_t)(raw[0] | ((uint16_t)raw[1] << 8));
        sum_y += (int16_t)(raw[2] | ((uint16_t)raw[3] << 8));
        sum_z += (int16_t)(raw[4] | ((uint16_t)raw[5] << 8));
    }

    // Deshabilitar EXTI0 al terminar
    HAL_NVIC_DisableIRQ(EXTI0_IRQn);

    // Promediar
    float avg_x = sum_x / (float)ADXL345_AVG_SAMPLES;
    float avg_y = sum_y / (float)ADXL345_AVG_SAMPLES;
    float avg_z = sum_z / (float)ADXL345_AVG_SAMPLES;

    // Ángulo total respecto de la vertical
    float norm = sqrtf(avg_x * avg_x + avg_y * avg_y + avg_z * avg_z);
    if (norm > 0.0f) {
        data->tilt_deg = acosf(avg_z / norm) * (180.0f / (float)M_PI);
    } else {
        data->tilt_deg = 0.0f;
    }

    // Ángulo respecto del eje Z sobre el plano Y=0
    data->tilt_xz_deg = atan2f(avg_x, avg_z) * (180.0f / (float)M_PI);

    // Ángulo respecto del eje Z sobre el plano X=0
    data->tilt_yz_deg = atan2f(avg_y, avg_z) * (180.0f / (float)M_PI);

    return ADXL345_OK;
}

void ADXL345_NotifyDataReady(void) {
    s_data_ready_flag = 1;
}

void ADXL345_NotifyImpact(void) {
    s_impact_flag = 1;
}

uint8_t ADXL345_GetAndClearImpactFlag(void) {
    if (s_impact_flag) {
        s_impact_flag = 0;
        return 1;
    }
    return 0;
}

ADXL345_Status ADXL345_HandleImpact(void) {
    // Leer INT_SOURCE baja la línea INT1 y permite nuevas detecciones
    uint8_t int_source;
    ADXL345_Status ret = reg_Read(REG_INT_SOURCE, &int_source, 1);
    if (ret != ADXL345_OK) return ret;

    // Si se lanzó la interrupción en INT1, pero no fue por Activity, retorna error.
    // INT1 comparte el pin con el resto de interrupciones, y aunque activity sea la única activada, vale la pena verificar.
    if(!(int_source & MASK_INT_SOURCE_ACTIVITY)) return ADXL345_ERR_INTERRUPT;

    return ADXL345_OK;
}
