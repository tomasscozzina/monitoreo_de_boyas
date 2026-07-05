/******************************************************************************
 * @file    buoy_app.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Aplicación principal para el monitoreo de boyas
 ******************************************************************************/

#include "buoy_app.h"
#include "adxl345.h"
#include "ina219.h"
#include "neo6m.h"
#include "sw1.h"
#include "lorawan_wrapper.h"
#include "system_power.h"

#include <math.h>

void buoyApp_tests(void){
//    lorawan_setup();
//
//    int32_t lat = 0, lon = 0;
//    GPS_UTCTime utc = {0};
//    lorawan_downlink_t downlink;
//    lorawan_uplink_t uplink;
//    char texto_payload[50];
//
//    /* Inicializar el acelerómetro */
//    if(ADXL345_init() == ADXL345_ERR_COMMS) {
//    	DPRINT("ERROR: ADXL345_Init() = ADXL345_ERR_COMMS \r\n");
//    	while (1) {};
//    }
//
//    DPRINT("Presiona SW1 para leer inclinacion\r\n");
//    ADXL345_Status *ret = ADXL345_OK;
//
//    /* Inicializar los 3 módulos INA219 */
//    if(INA219_initAll() == INA219_ERR_COMMS) {
//    	DPRINT("ERROR: INA219_InitAll() = INA219_ERR_COMMS \r\n");
//    	while (1) {}
//    }
//
//    DPRINT("INA219: 3 modulos inicializados OK\r\n");
//
//    int16_t min_voltage_mV = 3100;
//    int16_t min_current_mA = 4;
//    int16_t voltage_mV;
//    int16_t current_mA;
//    INA219_Channel ch = INA219_LANTERN;
//    int8_t state = 0;
//
//    while (1) {
//    	if (SW1_getPressEv())
//    	{
//    		if(state == 0){
//    			ch = INA219_BATERY;
//    			state = 1;
//    		}
//    		else if(state == 1){
//    			ch = INA219_SOLAR;
//    			state = 2;
//    		}
//    		else if(state == 2){
//    			ch = INA219_LANTERN;
//    			state = 0;
//    		}
//    	}
//
//    	if(ch == INA219_SOLAR || ch == INA219_BATERY) {
//    		if(INA219_getVoltage(ch, &voltage_mV) == INA219_ERR_COMMS) {
//				DPRINT("ERROR: INA219_getVoltage(CH%d) = INA219_ERR_COMMS \r\n", (int)ch);
//				continue;
//			}
//
//			if(INA219_getCurrent(ch, &current_mA) == INA219_ERR_COMMS) {
//				DPRINT("ERROR: INA219_getCurrent(CH%d) = INA219_ERR_COMMS \r\n", (int)ch);
//				continue;
//			}
//
//			HAL_Delay(150);
//			DPRINT("CH%d | %d mV | %d mA \r\n", (int)ch, voltage_mV, current_mA);
//    	}
//    	else{
//			INA219_Status ret = INA219_detectAndMeasure(ch, &current_mA, min_voltage_mV);
//    		switch(ret){
//    			case INA219_ERR_COMMS:
//    				DPRINT("ERROR: INA219_DetectAndMeasure(CH%d) = INA219_ERR_COMMS \r\n", (int)ch);
//    				break;
//    			case INA219_ERR_BAD_TIMING:
////    				DPRINT("LINTERNA APAGADA O EN TRANSICÓN \n\r");
//    				break;
//    			case INA219_OK:
//    				if(current_mA < min_current_mA) {
//						DPRINT("LINTERNA DEFECTUOSA \n\r");
//					}
//					else {
//						DPRINT("LINTERNA OK \n\r");
//					}
//    				break;
//    		}
//    	}
//    }
//}
//
//		if(INA219_detectAndMeasure(ch, &current_mA, min_voltage_mV) == INA219_ERR_COMMS) {
//			DPRINT("ERROR: INA219_DetectAndMeasure(CH%d) = INA219_ERR_COMMS \r\n", (int)ch);
//			continue;
//		}
//
//		DPRINT("CH%d | %ld mA \r\n", (int)ch, current_mA);
//        if (SW1_getPressEv()) {
//        	DPRINT("Leyendo inclinacion...\r\n");
//
//			uint8_t tilt;
//			if (ADXL345_readTilt(&tilt) == ADXL345_OK) {
//
//				DPRINT("Inclinacion total: %d grados \r\n", tilt);
//			}
//			else {
//				DPRINT("ERROR: ADXL345_ReadTilt() = ADXL345_ERR_COMMS \r\n");
//			}
//            DPRINT("INICIANDO\r\n");
//
//            GPS_Status ret = GPS_antenaStatus();
//            while(ret != GPS_OK) {
//                switch(ret){
//                    case GPS_ERR_COMMS:    	DPRINT("ERROR: GPS_ERR_COMMS \r\n");    break;
//                    case GPS_ERR_ANTENNA:   DPRINT("ERROR: GPS_ERR_ANTENNA \r\n");  break;
//                }
//                ret = GPS_antenaStatus();
//            }
//            DPRINT("GPS_OK \r\n");
//
//            ret = GPS_hasValidFix();
//            while(ret != GPS_OK) {
//                switch(ret){
//                	case GPS_ERR_COMMS:    	DPRINT("ERROR: GPS_ERR_COMMS \r\n");    break;
//                    case GPS_ERR_NO_FIX:    DPRINT("ERROR: GPS_ERR_NO_FIX\r\n");    break;
//                }
//                HAL_Delay(1000);
//                ret = GPS_hasValidFix();
//            }
//            DPRINT("FIX_OK\r\n");
//
//            // 2. Leer los datos del GPS
//            GPS_getLatitude(&lat);
//            GPS_getLongitude(&lon);
//            GPS_getUTCTime(&utc);
//
//            DPRINT("LATITUD  : %ld (x1e-7 deg)\r\n", lat);
//            DPRINT("LONGITUD : %ld (x1e-7 deg)\r\n", lon);
//            DPRINT("FECHA    : %04d-%02d-%02d\r\n", utc.year, utc.month, utc.day);
//            DPRINT("HORA UTC : %02d:%02d:%02d\r\n", utc.hour, utc.min, utc.sec);
//
//            // 3. Toda la transmisión LoRaWAN debe ocurrir ACÁ ADENTRO al presionar el botón
//            int len = snprintf(texto_payload, sizeof(texto_payload), "%ld,%ld,%04u,%02u,%02u,%02u,%02u,%02u",
//                    lat, lon, utc.year, utc.month, utc.day, utc.hour, utc.min, utc.sec);
//
//            uplink.payload = texto_payload;
//            uplink.len = (uint8_t)len;
//            uplink.port = PERIODIC_TRANSMISSION_PORT;
//            uplink.confirmed = false;
//
//            lorawan_send(&uplink, &downlink);
//
//            // Parpadeo del LED indicador de transmisión exitosa
//            HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
//            HAL_Delay(500);
//            HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
//        }
//        if(ADXL345_getImpactEv(ret)){
//        	DPRINT("IMPACTO \r\n");
//        	if(*ret == ADXL345_ERR_COMMS){
//        		DPRINT("ERROR: ADXL345_ReadTilt() = ADXL345_ERR_COMMS \r\n");
//        	}
//        }
//    }
}

void buoyApp_init(void) {

}
void buoyApp_run(void) {

}
