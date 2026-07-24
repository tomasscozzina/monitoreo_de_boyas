/******************************************************************************
 * @file    buoy_app.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Aplicación principal para el monitoreo de boyas
 ******************************************************************************/
#include "buoy_app.h"
#include <string.h>

#define LORAWAN_CONFIRMED_RETRIES 10

uint32_t tx_period_s = TX_PERIOD_S;

lorawan_credentials_t credentials = {
    .joinEUI   = LORAWAN_JOIN_EUI,
    .deviceEUI = LORAWAN_DEVICE_EUI,
    .appKey    = LORAWAN_APP_KEY
};

/* Prototipos de funciones privadas */
void take_measurements(payloadUp_t *payload);
void transmit_data(payloadUp_t *payload);
void goto_sleep(void);

/* Definiciones de funciones públicas */
void buoyApp_init(void) {
	DPRINT("INICIANDO EL SISTEMA ... \n\r");
	HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
	while(lorawan_init(&credentials) != LORA_JOIN_OK) {
		DPRINT("JOIN FALLÓ, SE INTENTARÁ NUEVAMENTE EN EL PRÓXIMO CICLO \n\r");
		goto_sleep();
	}
	DPRINT("JOIN OK \n\r");
}

void buoyApp_run(void) {
	payloadUp_t payload = {0};
	take_measurements(&payload);
	transmit_data(&payload);
	goto_sleep();
}

/* Definiciones de funciones privadas */
void take_measurements(payloadUp_t *payload) {
	static bool acel_status = true;
	static bool solarSens_status = true;
	static bool baterySens_status = true;
	static bool lanternSens_status = true;

	/* Acelerómetro */
	if(acel_status) {
		if(ADXL345_init(IMPACT_THRESHOLD) == ACEL_OK) {
			acel_status = false;
			HAL_Delay(500); // Delay para la estabilización del sensor, antes de solicitar una medición
			DPRINT("ACEL: CONFIG \n\r");
		}
		else {
			acel_status = true; // Ya era true, pero por las dudas
			payload->acel_status = true;
			payload->tilt = 0;
			DPRINT("ACEL: ERR COMMS \n\r");
		}
	}
	if(!acel_status) {
		uint8_t tilt_temp;
		if(ADXL345_getTilt(&tilt_temp) == ACEL_OK) {
			acel_status = false; // Ya era false, pero por las dudas
			payload->acel_status = false;
			payload->tilt = tilt_temp;
			DPRINT("ACEL: %d°\n\r", tilt_temp);
		}
		else {
			acel_status = true;
			payload->acel_status = true;
			payload->tilt = 0;
			DPRINT("ACEL: ERR COMMS \n\r");
		}
	}

	/* Sensor de energía del panel solar */
	if(solarSens_status) {
		if(INA219_init(INA219_SOLAR) == ENERGY_OK) {
			solarSens_status = false;
			HAL_Delay(500); // Delay para la estabilización del sensor, antes de solicitar una medición
			DPRINT("SOLAR: CONFIG \n\r");
		}
		else {
			solarSens_status = true; // Ya era true, pero por las dudas
			payload->solarSens_status = true;
			memset(&(payload->solarP), 0, sizeof(Energy_Data_t));
			DPRINT("SOLAR: ERR COMMS \n\r");
		}
	}
	if(!solarSens_status) {
		Energy_Data_t solarP_temp;
		if(INA219_getVoltageAndCurrent(INA219_SOLAR, &solarP_temp) == ENERGY_OK) {
			solarSens_status = false; // Ya era false, pero por las dudas
			payload->solarSens_status = false;
			payload->solarP = solarP_temp;
			DPRINT("SOLAR: %d mV - %d mA \n\r", solarP_temp.voltage_mV, solarP_temp.current_mA);
		}
		else {
			solarSens_status = true;
			payload->solarSens_status = true;
			memset(&(payload->solarP), 0, sizeof(Energy_Data_t));
			DPRINT("SOLAR: ERR COMMS \n\r");
		}
	}

	/* Sensor de energía de la batería */
	if(baterySens_status) {
		if(INA219_init(INA219_BATERY) == ENERGY_OK) {
			baterySens_status = false;
			HAL_Delay(500); // Delay para la estabilización del sensor, antes de solicitar una medición
			DPRINT("BATERY: CONFIG \n\r");
		}
		else {
			baterySens_status = true; // Ya era true, pero por las dudas
			payload->baterySens_status = true;
			memset(&(payload->batery), 0, sizeof(Energy_Data_t));
			DPRINT("BATERY: ERR COMMS \n\r");
		}
	}
	if(!baterySens_status) {
		Energy_Data_t batery_temp;
		if(INA219_getVoltageAndCurrent(INA219_BATERY, &batery_temp) == ENERGY_OK) {
			baterySens_status = false; // Ya era false, pero por las dudas
			payload->baterySens_status = false;
			payload->batery = batery_temp;
			DPRINT("BATERY: %d mV - %d mA \n\r", batery_temp.voltage_mV, batery_temp.current_mA);
		}
		else {
			baterySens_status = true;
			payload->baterySens_status = true;
			memset(&(payload->batery), 0, sizeof(Energy_Data_t));
			DPRINT("BATERY: ERR COMMS \n\r");
		}
	}

	/* Sensor de energía de la linterna */
	if(lanternSens_status) {
		if(INA219_init(INA219_LANTERN) == ENERGY_OK) {
			lanternSens_status = false;
			HAL_Delay(500); // Delay para la estabilización del sensor, antes de solicitar una medición
			DPRINT("LANTERN: CONFIG \n\r");
		}
		else {
			lanternSens_status = true; // Ya era true, pero por las dudas
			payload->lanternSens_status = true;
			payload->lantern_failure = false; // No puedo confirmar que tenga error
			payload->flasher_failure = false; // No puedo confirmar que tenga error
			DPRINT("LANTERN: ERR COMMS \n\r");
		}
	}
	if(!lanternSens_status) {
		int16_t current_mA;
		Energy_Status_t Energy_Status;
		Energy_Status = INA219_detectAndMeasure(INA219_LANTERN, &current_mA, LANTERN_MIN_MV);

		uint32_t start = HAL_GetTick();
		while(Energy_Status == ENERGY_ERR_BAD_TIMING) {
			Energy_Status = INA219_detectAndMeasure(INA219_LANTERN, &current_mA, LANTERN_MIN_MV);
			if(HAL_GetTick() - start > LANTERN_PERIOD_MS) {
				break;
			}
		}
		if(Energy_Status == ENERGY_OK) {
			lanternSens_status = false; // Ya era false, pero por las dudas
			payload->lanternSens_status = false;
			payload->flasher_failure = false;

			if(current_mA < LANTERN_MIN_MA) {
				payload->lantern_failure = true; // La corriente por la linterna es menor a la mínima aceptable
				DPRINT("LANTERN: LANTERN FAILURE \n\r");
			}
			else {
				payload->lantern_failure = false; // La corriente por la linterna es mayor o igual a la mínima aceptable
				DPRINT("LANTERN: LANTERN OK \n\r");
			}
		}
		else if(Energy_Status == ENERGY_ERR_COMMS) {
			lanternSens_status = true;
			payload->lanternSens_status = true;
			payload->lantern_failure = false;	// No puedo confirmar que tenga error
			payload->flasher_failure = false; 	// No puedo confirmar que tenga error
			DPRINT("LANTERN: ERR COMMS \n\r");
		}
		else if(Energy_Status == ENERGY_ERR_BAD_TIMING) {
			/* No se logró medir una tensión superior a LANTERN_MIN_MV dentro del periodo LANTERN_PERIOD_MS especificado */
			lanternSens_status = false;
			payload->lanternSens_status = false;
			payload->lantern_failure = false;
			payload->flasher_failure = true; // Falló el destellador
			DPRINT("LANTERN: FLASHER FAILURE \n\r");
		}
	}

	/* GPS */
	GPS_Data_t GPS_Data;
	GPS_Status_t GPS_Status;
	GPS_Status = GPS_getData(&GPS_Data);

	uint32_t start = HAL_GetTick();
	while(GPS_Status == GPS_ERR_NO_FIX || GPS_Status == GPS_ERR_ANTENNA) {
		HAL_Delay(1000); // El periodo de actualización del GPS es de 1 Hz
		GPS_Status = GPS_getData(&GPS_Data);
		if(HAL_GetTick() - start > GPS_TIMECAP_MS) {
			break;
		}
	}
	switch(GPS_Status) {
		case GPS_OK:
			payload->gps_status = 0;
			payload->gps = GPS_Data;
			DPRINT("GPS: OK \n\r");
			break;
		case GPS_ERR_COMMS:
			payload->gps_status = 1;
			memset(&(payload->gps), 0, sizeof(GPS_Data_t));
			DPRINT("GPS: ERR COMMS \n\r");
			break;
		case GPS_ERR_NO_FIX:
			payload->gps_status = 2;
			memset(&(payload->gps), 0, sizeof(GPS_Data_t));
			DPRINT("GPS: ERR NO FIX \n\r");
			break;
		case GPS_ERR_ANTENNA:
			payload->gps_status = 3;
			memset(&(payload->gps), 0, sizeof(GPS_Data_t));
			DPRINT("GPS: ERR ANTENNA \n\r");
			break;
	}

	/* Bandera de IMPACTO */
	Acel_Status_t Impact_Status;
	if(ADXL345_getImpactEv(&Impact_Status)) {
		payload->impact = true;
		DPRINT("IMPACTO DETECTADO \n\r");
	}
	else {
		payload->impact = false;
		DPRINT("SIN IMPACTO DETECTADO \n\r");
	}
	if (Impact_Status == ACEL_ERR_COMMS) {
		DPRINT("ACEL: ERR COMMS. RECONFIG \n\r");
		/* Vuelvo a configurar el acelerómetro, para intentar solucionar el problema y por ende, limpiar la bandera de Activity (Impacto) */
		if(ADXL345_init(IMPACT_THRESHOLD) == ACEL_OK) {  // Se pudo configurar el acelerómetro con éxito
			DPRINT("ACEL: ERR COMMS. RECONFIG OK \n\r");
			HAL_Delay(500); // Delay para la estabilización del sensor, antes de solicitar una lectura
			ADXL345_getImpactEv(&Impact_Status);
			if(Impact_Status == ACEL_OK) {
				acel_status = false; // Se pudo limpiar la bandera de Activity con exito
				DPRINT("ACEL: ERR COMMS. FLAG CLEAR \n\r");
			}
			else {
				acel_status = true;	// No se pudo limpiar la bandera de Activity
				payload->acel_status = true;
				payload->tilt = 0; // No puedo enviar un dato que, aunque válido, contradiga el estado de falla del acelerómetro
			}
		}
		else {	// No se pudo configurar el acelerómetro
			DPRINT("ACEL: ERR COMMS. RECONFIG FAIL \n\r");
			acel_status = true;
			payload->acel_status = true;
			payload->tilt = 0; // No puedo enviar un dato que, aunque válido, contradiga el estado de falla del acelerómetro
		}
	} // Cualquiera de los 2 casos de falla (en configuración y en el clear de la bandera), se reintentará corregir en el proximo ciclo

	/* Bandera de GPS-SET (por SW1) */
	if(SW1_getPressEv()) {
		DPRINT("GPS-SET (SW1 PULSADO) \n\r");
		payload->gps_set = true;
	}
	else {
		DPRINT("SIN GPS-SET (SW1 NO PULSADO) \n\r");
		payload->gps_set = false;
	}

	/* Bandera de TRANSMISIÓN PERIÓDICA (wake up por RTC) */
	if(RTC_getTimeOutEv()) {
		DPRINT("RTC WAKEUP (TX PERIÓDICA) \n\r");
		payload->periodic_TX = true;
	}
	else {
		DPRINT("SIN RTC WAKEUP (NO TX PERIÓDICA) \n\r");
		payload->periodic_TX = false;
	}
}

void transmit_data(payloadUp_t *payload) {
	lorawan_downlink_t downlink = {0};
	lorawan_uplink_t uplink = {0};
	/* Reestructuro el payload del Downlink, para mayor practicidad */
	payloadDown_t *payloadDown = (payloadDown_t*) downlink.data;

	bool system_reboot = false;
	bool confirmed = (payload->gps_set || payload->impact);

	uplink.payload = payload;
	uplink.len = sizeof(payloadUp_t);
	uplink.port = LORAWAN_PORT;
	uplink.confirmed = confirmed;

	LoRaWAN_Status_t LoRaWAN_Status;
	uint8_t retries = LORAWAN_CONFIRMED_RETRIES;
	do {
		DPRINT("LORAWAN: SEND \n\r");
		LoRaWAN_Status = lorawan_sendReceive(&uplink, &downlink);
		if(LoRaWAN_Status == LORA_ERR_COMMS) {
			DPRINT("LORAWAN: SE PRODUJERON ERRORES EN EL UPLINK. REINICIANDO EL JOIN \n\r");
			if(lorawan_init(&credentials) == LORA_ERR_COMMS) {
				DPRINT("LORAWAN: SE PRODUJERON ERRORES EN EL JOIN. REINICIANDO EL SISTEMA \n\r");
				Error_Handler();
			}
			DPRINT("LORAWAN: JOIN OK \n\r");
		}
		else if(LoRaWAN_Status == LORA_DOWN_AVAILABLE) {
			if(downlink.available){
				DPRINT("LORAWAN: PAYLOAD RECIBIDO EN EL DOWNLINK \n\r");
				if(payloadDown->tx_period_s > 0) {
					DPRINT("LORAWAN: COMANDO PARA EL AJUSTE DEL PERIODO DE TX \n\r");
					tx_period_s = (uint32_t)payloadDown->tx_period_s;
				}
				if(payloadDown->system_reboot) {
				    system_reboot = true;
				}
			}
		}
		/* Si el Uplink era confirmed y el Downlink no lo confirma (o no se recibió Downlink (LORA_UP_OK)
		 * o falló la transmisión del Uplink (LORA_ERR_COMMS)) se reintenta LORAWAN_CONFIRMED_RETRIES veces */
		if(confirmed && !(downlink.confirming) && (retries > 0)) {
			DPRINT("LORAWAN: UPLINK CONFIRMED SIN ACUSE DE RECIBO. REINTENTANDO EN 5 SEGUNDOS \n\r");
			HAL_Delay(5000);
		}
		retries--;
	} while(confirmed && !(downlink.confirming) && (retries > 0));

	if(system_reboot) {
		DPRINT("LORAWAN: COMANDO PARA EL REINICIO DEL SISTEMA. REINICIANDO ... \n\r");
		Error_Handler();
	}
}

void goto_sleep(void) {
	DPRINT("SLEEP \n\r");
	DPRINT("------------------------------ \n\r");	/* Separador de paquetes en consola */
	HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
	lorawan_sleep();
	SystemPower_sleep(tx_period_s);
	HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
	DPRINT("WAKE UP \n\r");
}
