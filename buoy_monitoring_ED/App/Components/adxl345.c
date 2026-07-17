/******************************************************************************
 * @file    adxl345.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Driver del acelerómetro adxl345 de Analog Devices
 ******************************************************************************/

#include "adxl345.h"
#include "i2c.h"
#include <math.h>

/* Dirección I2C y registros */
#define ADXL345_I2C_ADDR    			0x53    				// Dirección I2C (7 bits) del ADXL345
#define ADXL345_I2C_ADDR_SHIFT			ADXL345_I2C_ADDR << 1	// Dirección I2C desplazada. Requerido por función de capa HAL

#define REG_THRESH_ACT      			0x24
#define REG_ACT_INACT_CTL   			0x27
#define REG_BW_RATE         			0x2C
#define REG_POWER_CTL       			0x2D
#define REG_INT_ENABLE      			0x2E
#define REG_INT_MAP         			0x2F
#define REG_INT_SOURCE      			0x30
#define REG_DATA_FORMAT     			0x31
#define REG_DATAX0          			0x32

/* Mascaras */
#define MASK_INT_SOURCE_ACTIVITY		0x10

/* Valores de configuración (tramas) */
#define CFG_POWER_CTL_STANDBY   		0x00
#define CFG_POWER_CTL_MEASURE   		0x08
#define CFG_DATA_FORMAT         		0x00
#define CFG_BW_RATE             		0x0A
#define CFG_ACT_INACT_CTL       		0xF0
#define CFG_INT_MAP             		0x80	// Activity a INT1 - Data Ready a INT2
#define CFG_INT_ENABLE  				0x90	// IRQ habilitada por Activity y Data Ready

/* Reintentos de comunicación por I2C */
#define ADXL345_COMMS_REINTENTOS		5

/* Constantes de conversión y cálculo */
#define ADXL345_DR_TIMEOUT_MS   		50		// Timeout Data Ready
#define ADXL345_AVG_SAMPLES     		10    	// Muestras a promediar en cada lectura de inclinación

/* Tiempo de espera ante impactos consecutivos */
#define ANTIREBOTE 						1000

/* Variables privadas (static) */
static volatile bool data_ready_flag = false;
static volatile bool impact_flag     = false;
static bool en_espera_antirebote = false;
static uint32_t start = 0;

/* Prototipos de funciones privadas */
static Acel_Status_t reg_write(uint8_t reg, uint8_t value);
static Acel_Status_t reg_read(uint8_t reg, uint8_t *data, uint8_t len);

/* API pública */

Acel_Status_t ADXL345_init(uint8_t impact_threshold) {

    // Poner en standby antes de configurar
    if(reg_write(REG_POWER_CTL, CFG_POWER_CTL_STANDBY) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // Formato de datos: 10-bit, +-2g, right-justified
    if(reg_write(REG_DATA_FORMAT, CFG_DATA_FORMAT) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // ODR 100 Hz, normal power mode
    if(reg_write(REG_BW_RATE, CFG_BW_RATE) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // Threshold de actividad: 16g (ajustar experimentalmente)
    if(reg_write(REG_THRESH_ACT, impact_threshold) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // Activity AC-coupled, ejes X+Y+Z
    if(reg_write(REG_ACT_INACT_CTL, CFG_ACT_INACT_CTL) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // Data Ready a INT2, Activity a INT1
    if(reg_write(REG_INT_MAP, CFG_INT_MAP) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // Limpiar interrupciones pendientes de Activity leyendo el registro INT_SOURCE
    uint8_t int_source;
    if(reg_read(REG_INT_SOURCE, &int_source, 1) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // Limpiar interrupciones pendientes de Data Ready leyendo los registros DATA
    uint8_t raw[6];
	if(reg_read(REG_DATAX0, raw, 6) == ACEL_ERR_COMMS) {
		return ACEL_ERR_COMMS;
	}

    // Deshabilito la IRQ (NUCLEO) por Data Ready. Cuando quiera leer las aceleraciónes, se habilita temporalmente.
    HAL_NVIC_DisableIRQ(EXTI0_IRQn);

    // Habilitar interrupciones por Activity y Data Ready
    if(reg_write(REG_INT_ENABLE, CFG_INT_ENABLE) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    // Activar measurement mode
    if(reg_write(REG_POWER_CTL, CFG_POWER_CTL_MEASURE) == ACEL_ERR_COMMS) {
    	return ACEL_ERR_COMMS;
    }

    return ACEL_OK;
}

Acel_Status_t ADXL345_getTilt(uint8_t *tilt) {
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;

    /* EXPLICACIÓN:
     * Leo los registros DATA para que baje la flag externa (ADXL345), limpio la flag interna (NUCLEO) y habilito la
     * interrupción EXTI0 (NUCLEO), así con la próxima interrupción se leerá un dato nuevo. Esto se hace así, para asegurarse
     * que durante la primera lectura, no se esté escribiendo, en ese instante, un dato nuevo en el registro REG_DATAX0, lo que
     * podría resultar en una lectura de un dato erróneo (parte nueva y parte antigüa) */

    HAL_NVIC_DisableIRQ(EXTI0_IRQn);

    uint8_t discard[6];
    if(reg_read(REG_DATAX0, discard, 6) == ACEL_ERR_COMMS){
    	return ACEL_ERR_COMMS;
    }

    data_ready_flag = false;
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    for (int i = 0; i < ADXL345_AVG_SAMPLES; i++) {

        uint32_t start = HAL_GetTick();
        while (!data_ready_flag) {
            if (HAL_GetTick() - start >= 200) {
                HAL_NVIC_DisableIRQ(EXTI0_IRQn);
                return ACEL_ERR_COMMS;
            }
            // Se repite este bucle hasta que se lanze la IRQ por data ready, o se superen los 200 ms
        }
        data_ready_flag = false;

        uint8_t raw[6];
        if(reg_read(REG_DATAX0, raw, 6) == ACEL_ERR_COMMS){
        	HAL_NVIC_DisableIRQ(EXTI0_IRQn);
        	return ACEL_ERR_COMMS;
        }
        // El ADXL345 utiliza Little_Endian (LSB First)
        sum_x += (int16_t)(raw[0] | ((uint16_t)raw[1] << 8));
        sum_y += (int16_t)(raw[2] | ((uint16_t)raw[3] << 8));
        sum_z += (int16_t)(raw[4] | ((uint16_t)raw[5] << 8));
    }

    HAL_NVIC_DisableIRQ(EXTI0_IRQn);

    // Promediar
    float avg_x = sum_x / (float)ADXL345_AVG_SAMPLES;
    float avg_y = sum_y / (float)ADXL345_AVG_SAMPLES;
    float avg_z = sum_z / (float)ADXL345_AVG_SAMPLES;

    // Ángulo total respecto de la vertical
    float norm = sqrtf(avg_x * avg_x + avg_y * avg_y + avg_z * avg_z);
    if (norm > 0.0f) {
        *tilt = acosf(avg_z / norm) * (180.0f / (float)M_PI);
    } else {
    	return ACEL_ERR_COMMS;
    }

    return ACEL_OK;
}

void ADXL345_notifyDataReady(void) {
	data_ready_flag = true;
}

void ADXL345_notifyImpact(void) {
	impact_flag = true;
}

bool ADXL345_getImpactEv(Acel_Status_t *ret) {
    if (en_espera_antirebote == true) {
        if (HAL_GetTick() - start >= ANTIREBOTE) {
            en_espera_antirebote = false;
        }
    }
    if (impact_flag == true) {
    	impact_flag = false;

    	// Limpiar la interrupcion de Activity leyendo el registro INT_SOURCE
        uint8_t int_source;
        *ret = reg_read(REG_INT_SOURCE, &int_source, 1);

        if (en_espera_antirebote == false) {
            start = HAL_GetTick();
            en_espera_antirebote = true;
            return true;
        }
    }
    return false;
}

/* Definiciones de funciones privadas */
static Acel_Status_t reg_write(uint8_t reg, uint8_t value) {
	uint8_t buf[2] = {reg, value};
    for (uint8_t i = 0; i < ADXL345_COMMS_REINTENTOS; i++) {
    	if (HAL_I2C_Master_Transmit(&hi2c1, ADXL345_I2C_ADDR_SHIFT, buf, 2, 10) == HAL_OK) {
    	    return ACEL_OK;
    	}
		HAL_Delay(100);
	}
    // Si luego de 5 intentos, la comunicación sigue fallando, se reinicia la interfaz I2C
    HAL_I2C_DeInit(&hi2c1);
    HAL_I2C_Init(&hi2c1);

    // Se repiten los 5 intentos
    for (uint8_t i = 0; i < ADXL345_COMMS_REINTENTOS; i++) {
    	if (HAL_I2C_Master_Transmit(&hi2c1, ADXL345_I2C_ADDR_SHIFT, buf, 2, 10) == HAL_OK) {
    	    return ACEL_OK;
    	}
		HAL_Delay(100);
	}
    // Luego de los 10 intentos fallidos (habiendo reiniciado la interfaz de por medio) se retorna ACEL_ERR_COMMS
    return ACEL_ERR_COMMS;
}

static Acel_Status_t reg_read(uint8_t reg, uint8_t *data, uint8_t len) {
	for (uint8_t i = 0; i < ADXL345_COMMS_REINTENTOS; i++) {
		if (HAL_I2C_Master_Transmit(&hi2c1, ADXL345_I2C_ADDR_SHIFT, &reg, 1, 10) == HAL_OK) {
			if (HAL_I2C_Master_Receive(&hi2c1, ADXL345_I2C_ADDR_SHIFT, data, len, 10) == HAL_OK) {
				return ACEL_OK;
			}
		}
		HAL_Delay(100);
	}
    // Si luego de 5 intentos, la comunicación sigue fallando, se reinicia la interfaz I2C
	HAL_I2C_DeInit(&hi2c1);
	HAL_I2C_Init(&hi2c1);

    // Se repiten los 5 intentos
	for (uint8_t i = 0; i < ADXL345_COMMS_REINTENTOS; i++) {
		if (HAL_I2C_Master_Transmit(&hi2c1, ADXL345_I2C_ADDR_SHIFT, &reg, 1, 10) == HAL_OK) {
			if (HAL_I2C_Master_Receive(&hi2c1, ADXL345_I2C_ADDR_SHIFT, data, len, 10) == HAL_OK) {
				return ACEL_OK;
			}
		}
		HAL_Delay(100);
	}
	// Luego de los 10 intentos fallidos (habiendo reiniciado la interfaz de por medio) se retorna ACEL_ERR_COMMS
    return ACEL_ERR_COMMS;
}
