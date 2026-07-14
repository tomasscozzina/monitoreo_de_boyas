/******************************************************************************
 * @file    ina219.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Driver del sensor de energía INA219 de Texas Instruments
 ******************************************************************************/

#include "ina219.h"
#include <stdbool.h>

/* Direcciones I2C de los 3 módulos (7 bits) */
#define INA219_I2C_ADDR_CH0     	0x40	// LINTERNA		(A1=GND, A0=GND)
#define INA219_I2C_ADDR_CH1     	0x41	// BATERÍA		(A1=GND, A0=VS)
#define INA219_I2C_ADDR_CH2     	0x44	// PANEL SOLAR 	(A1=VS,  A0=GND)

/* Las direcciones I2C deben ser desplazadas a la izquierda para llamar los métodos de Tx y Rx */
#define INA219_I2C_ADDR_CH0_SHIFT	INA219_I2C_ADDR_CH0 << 1
#define INA219_I2C_ADDR_CH1_SHIFT	INA219_I2C_ADDR_CH1 << 1
#define INA219_I2C_ADDR_CH2_SHIFT   INA219_I2C_ADDR_CH2 << 1

/* Registros del INA219 */
#define REG_CONFIGURATION   0x00
#define REG_SHUNT_VOLTAGE   0x01
#define REG_BUS_VOLTAGE     0x02

/* ===========================================================================
 * Valores de configuración
 *
 * CONFIG register (16 bits):
 *   [15]    RST      = 0   	(sin reset)
 *   [14]    —        = 0   	(reservado)
 *   [13]    BRNG     = 1   	(bus voltage range: 32V (26V máx admisible por hardware)
 *   [12:11] PG[1:0]  = 11  	(ganancia shunt: ±320 mV)
 *   [10:7]  BADC     = 1111 	(bus ADC:   128 muestras promediadas)
 *   [6:3]   SADC     = 1111 	(shunt ADC: 128 muestras promediadas)
 *   [2:0]   MODE     = 111  	(shunt + bus, continuo)
 *
 *   Valor: 0b0011_1111_1111_1111 = 0x3FFF
 * =========================================================================*/
#define CFG_CONFIGURATION   0x3FFF

/* Máscaras */
#define MASK_BUS_VOLTAGE_SHIFT  3

/* Reintentos de comunicación por I2C */
#define INA219_COMMS_REINTENTOS	 5

/* Correción de OFFSET en medición de VBUS (mide 12 mV de más) */
#define OFSET_CORRECTION  12

/* Variables privadas */
static const uint8_t s_i2c_addrs[INA219_COUNT] = {
	INA219_I2C_ADDR_CH0_SHIFT,
	INA219_I2C_ADDR_CH1_SHIFT,
	INA219_I2C_ADDR_CH2_SHIFT
};

/* Prototipos de funciones privadas */
static Energy_Status_t reg_write(uint8_t addr, uint8_t reg, uint16_t value);
static Energy_Status_t reg_read(uint8_t addr, uint8_t reg, uint16_t *value);
static Energy_Status_t INA219_getVoltage(INA219_Channel ch, int16_t *voltage_mV);
static Energy_Status_t INA219_getCurrent(INA219_Channel ch, int16_t *current_mA);

/* API pública */
Energy_Status_t INA219_init(INA219_Channel ch) {
	/* Escribir registro de configuración */
	if(reg_write(s_i2c_addrs[ch], REG_CONFIGURATION, CFG_CONFIGURATION) == ENERGY_ERR_COMMS) {
		return ENERGY_ERR_COMMS;
	}
    return ENERGY_OK;
}

Energy_Status_t INA219_getVoltageAndCurrent(INA219_Channel ch, Energy_Data_t *data) {
	int16_t voltage_mV = 0;
	int16_t current_mA = 0;

	if(INA219_getVoltage(ch, &voltage_mV) == ENERGY_OK) {
		if(INA219_getCurrent(ch, &current_mA) == ENERGY_OK) {
			data->voltage_mV = voltage_mV;
			data->current_mA = current_mA;

			return ENERGY_OK;
		}
	}
	return ENERGY_ERR_COMMS;
}

Energy_Status_t INA219_detectAndMeasure(INA219_Channel ch, int16_t *current_mA, int16_t min_voltage_mV) {
    int16_t v1_mV = 0;
    int16_t v2_mV = 0;
    int16_t i_mA = 0;

    // Primera medición de tensión
    if(INA219_getVoltage(ch, &v1_mV) == ENERGY_ERR_COMMS) {
    	return ENERGY_ERR_COMMS;
    }

    HAL_Delay(150);	// Le toma 136ms obtener 2 mediciones nuevas (cada promedio de 120 muestras le toma 68,1 ms)

    // Medición de corriente
    if(INA219_getCurrent(ch, &i_mA) == ENERGY_ERR_COMMS) {
    	return ENERGY_ERR_COMMS;
    }

    HAL_Delay(150); // Le toma 136ms obtener 2 mediciones nuevas (cada promedio de 120 muestras le toma 68,1 ms)

    // Segunda medición de tensión
    if(INA219_getVoltage(ch, &v2_mV) == ENERGY_ERR_COMMS) {
    	return ENERGY_ERR_COMMS;
    }

    bool v1_on = (v1_mV >= min_voltage_mV);
    bool v2_on = (v2_mV >= min_voltage_mV);

    /* Si alguna de las dos mediciones de tensión (o ambas) no superan el mínimo, es porque justo se estaba dando
     * una transición en el estado de la linterna (ON a OFF o viceversa) o porque la linterna se encontraba apagada */
    if (!v1_on || !v2_on) {
        return ENERGY_ERR_BAD_TIMING;
    }

    *current_mA = i_mA;	// Si ambas tensiónes superan el mínimo, se retorna la medición de corriente

    return ENERGY_OK;
}

/* Definiciones de funciones privadas */
static Energy_Status_t reg_write(uint8_t addr, uint8_t reg, uint16_t value) {
    uint8_t buf[3];
    buf[0] = reg;
    // El INA219 utiliza Big_Endian (MSB First)
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value & 0xFF);

    for(uint8_t i = 0; i < INA219_COMMS_REINTENTOS; i++) {
    	if (HAL_I2C_Master_Transmit(&hi2c1, addr, buf, 3, 10) == HAL_OK) {
			return ENERGY_OK;
		}
    	HAL_Delay(100);
    }
    // Si luego de 5 intentos, la comunicación sigue fallando, se reinicia la interfaz I2C
    HAL_I2C_MspDeInit(&hi2c1);
    HAL_I2C_MspInit(&hi2c1);

    // Se repiten los 5 intentos
    for(uint8_t i = 0; i < INA219_COMMS_REINTENTOS; i++) {
    	if (HAL_I2C_Master_Transmit(&hi2c1, addr, buf, 3, 10) == HAL_OK) {
			return ENERGY_OK;
		}
    	HAL_Delay(100);
    }
    // Luego de los 10 intentos fallidos (habiendo reiniciado la interfaz de por medio) se retorna ENERGY_ERR_COMMS
    return ENERGY_ERR_COMMS;
}

static Energy_Status_t reg_read(uint8_t addr, uint8_t reg, uint16_t *value) {
    uint8_t buf[2];
    for(uint8_t i = 0; i < INA219_COMMS_REINTENTOS; i++) {
    	if (HAL_I2C_Master_Transmit(&hi2c1, addr, &reg, 1, 10) == HAL_OK) {
    		if (HAL_I2C_Master_Receive(&hi2c1, addr, buf, 2, 10) == HAL_OK) {
    			// El INA219 utiliza Big_Endian (MSB First)
    			*value = ((uint16_t)buf[0] << 8) | buf[1];
    			return ENERGY_OK;
			}
    	}
    	HAL_Delay(100);
    }
    // Si luego de 5 intentos, la comunicación sigue fallando, se reinicia la interfaz I2C
    HAL_I2C_MspDeInit(&hi2c1);
    HAL_I2C_MspInit(&hi2c1);

    // Se repiten los 5 intentos
    for(uint8_t i = 0; i < INA219_COMMS_REINTENTOS; i++) {
		if (HAL_I2C_Master_Transmit(&hi2c1, addr, &reg, 1, 10) == HAL_OK) {
			if (HAL_I2C_Master_Receive(&hi2c1, addr, buf, 2, 10) == HAL_OK) {
				// El INA219 utiliza Big_Endian (MSB First)
				*value = ((uint16_t)buf[0] << 8) | buf[1];
				return ENERGY_OK;
			}
		}
		HAL_Delay(100);
	}
    // Luego de los 10 intentos fallidos (habiendo reiniciado la interfaz de por medio) se retorna ENERGY_ERR_COMMS
    return ENERGY_ERR_COMMS;
}

static Energy_Status_t INA219_getVoltage(INA219_Channel ch, int16_t *voltage_mV) {
    uint16_t raw;

    if(reg_read(s_i2c_addrs[ch], REG_BUS_VOLTAGE, &raw) == ENERGY_ERR_COMMS) {
    	return ENERGY_ERR_COMMS;
    }

    uint16_t bus_val = raw >> MASK_BUS_VOLTAGE_SHIFT;	// Bits [15:3] = BUS_VOLTAGE
    *voltage_mV = (bus_val * 4) - OFSET_CORRECTION;		// LSB = 4 mV

    return ENERGY_OK;
}

static Energy_Status_t INA219_getCurrent(INA219_Channel ch, int16_t *current_mA) {
    uint16_t raw;

    if(reg_read(s_i2c_addrs[ch], REG_SHUNT_VOLTAGE, &raw) == ENERGY_ERR_COMMS) {
    	return ENERGY_ERR_COMMS;
    }

   /* La corriente se calcula por software a partir del registro Shunt Voltage:
    *   I = V_shunt / R_shunt
    *   donde:
    *   	R_shunt = 0.1 Ω
    *   	V_shunt = V_shunt_raw * V_shunt LSB = V_shunt_raw * 10 µV/LSB
    *
    *   entonces:
    *   I [mA] = (V_shunt_raw * 10 µV/LSB) / 0.1 Ω
    *   	   =  V_shunt_raw * (10 µV/LSB / 0.1 Ω)
    *   	   =  V_shunt_raw * 0.1 mA/LSB
    *          =  V_shunt_raw / 10
    *
    *   Dado que NO se desean los decimales de mA (0,1 mA), se aprovecha la división entera para eliminarlos.
    *   Pero antes de realizar la división, se redondea al entero más cercano, sumando 5 si el valor es
    *   positivo, o restando 5 si es negativo.
    */
    int16_t current_dmA = (int16_t)raw;
    *current_mA = (current_dmA >= 0) ? (current_dmA + 5) / 10
                                     : (current_dmA - 5) / 10;

    return ENERGY_OK;
}
