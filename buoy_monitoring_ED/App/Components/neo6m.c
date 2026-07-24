/******************************************************************************
 * @file    neo6m.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Driver del módulo GPS neo6m de uBlox
 ******************************************************************************/

#include "neo6m.h"
#include "usart.h"
#include <string.h>

/* Caracteres de sincronización */
#define UBX_SYNC1   				0xB5	// Letra griega mu en ascii extendido
#define UBX_SYNC2   				0x62	// Letra b en ascii

/* Cabeceras de las clases esperadas en cada respuesta */
#define UBX_MON						0x0A
#define UBX_NAV						0x01
#define UBX_RXM						0x02

/* Cabeceras de los mensajes esperados en cada respuesta */
#define UBX_MON_HW					0x09
#define UBX_NAV_STATUS				0x03
#define UBX_NAV_POSLLH				0x02
#define UBX_NAV_TIMEUTC				0x21
#define UBX_RXM_PMREQ				0x41

/* Tamaños de payload esperados en cada respuesta */
#define UBX_PAYLOAD_MON_HW        	68
#define UBX_PAYLOAD_NAV_STATUS    	16
#define UBX_PAYLOAD_NAV_POSLLH    	28
#define UBX_PAYLOAD_NAV_TIMEUTC   	20

/* Tamaños de las partes de un paquete UBX */
#define UBX_SIZE_SYNCCHAR			1
#define UBX_SIZE_CLASS				1
#define UBX_SIZE_ID					1
#define UBX_SIZE_LENGTH				2
#define UBX_SIZE_CK					2

/* Timeouts */
#define GPS_TIMEOUT_MS   			2000

/* Reintentos de comunicación por UART */
#define GPS_COMMS_REINTENTOS		5

/* Valores válidos de aStatus y aPower en MON-HW */
#define MON_HW_ASTATUS_OK   		2
#define MON_HW_APOWER_ON    		1

/* Valores máximos de agcCnt y jamInd en MON-HW */
#define MON_HW_AGCCNT_MAX  			6550
#define MON_HW_JAMIND_MAX    		10

/* Offsets del payload MON-HW */
#define MON_HW_OFFSET_AGCCNT	   	18
#define MON_HW_OFFSET_ASTATUS   	20
#define MON_HW_OFFSET_APOWER    	21
#define MON_HW_OFFSET_JAMIND    	53

/* Valores válidos de gpsFix en NAV-STATUS */
#define GPS_FIX_2D   				2
#define GPS_FIX_3D   				3
#define GPS_FIX_OK   				(1 << 0)

/* QUERYs hardcodeadas (CLASS ID LEN_L LEN_H PAYLOAD CK_A CK_B) */
static const uint8_t CMD_MON_HW[]      = { 0xB5, 0x62, 0x0A, 0x09, 0x00, 0x00, 0x13, 0x43 };
static const uint8_t CMD_NAV_STATUS[]  = { 0xB5, 0x62, 0x01, 0x03, 0x00, 0x00, 0x04, 0x0D };
static const uint8_t CMD_NAV_POSLLH[]  = { 0xB5, 0x62, 0x01, 0x02, 0x00, 0x00, 0x03, 0x0A };
static const uint8_t CMD_NAV_TIMEUTC[] = { 0xB5, 0x62, 0x01, 0x21, 0x00, 0x00, 0x22, 0x67 };

/* Variables privadas */
static GPS_Data_t s_data = {0};

/* Prototipos de funciones privadas */
static void ubx_calcChecksum(const uint8_t *data, uint16_t len, uint8_t *ck_a, uint8_t *ck_b);
static GPS_Status_t ubx_send(const uint8_t *query, uint16_t query_len);
static GPS_Status_t ubx_receive(uint8_t exp_class, uint8_t exp_id, uint8_t *payload_out, uint16_t exp_len);
static GPS_Status_t ubx_sendReceive(const uint8_t *query, uint16_t query_len, uint8_t exp_class, uint8_t exp_id, uint8_t *payload_out, uint16_t exp_len);
static GPS_Status_t GPS_antenaStatus(void);
static GPS_Status_t GPS_hasValidFix(void);

/* API pública */
GPS_Status_t GPS_getData(GPS_Data_t *data) {
	GPS_Status_t ret;
	ret = GPS_antenaStatus();
	if(ret != GPS_OK) {
		return ret;
	}
	ret = GPS_hasValidFix();
	if(ret != GPS_OK) {
		return ret;
	}
	*data = s_data;
	return ret;
}

/* Definiciones de funciones privadas */
static void ubx_calcChecksum(const uint8_t *data, uint16_t len, uint8_t *ck_a, uint8_t *ck_b) {
    for (uint16_t i = 0; i < len; i++) {
        *ck_a += data[i];
        *ck_b += *ck_a;
    }
}

static GPS_Status_t ubx_send(const uint8_t *query, uint16_t query_len) {
    if (HAL_UART_Transmit(&huart1, query, query_len, GPS_TIMEOUT_MS) != HAL_OK){
    	return GPS_ERR_COMMS;
    }
    return GPS_OK;
}

static GPS_Status_t ubx_receive(uint8_t exp_class, uint8_t exp_id, uint8_t *payload_out, uint16_t exp_len) {
    uint32_t start = HAL_GetTick();
    uint8_t byte;
    uint8_t sync_state = 0;
    uint32_t timeout_ms = GPS_TIMEOUT_MS;

    // Primera lectura: Se busca la secuencia SYNC1 + SYNC2 usando switch-case
    while (sync_state < 2) {

        if ((HAL_GetTick() - start) >= timeout_ms) {
            return GPS_ERR_COMMS;
        }

        if (HAL_UART_Receive(&huart1, &byte, UBX_SIZE_SYNCCHAR, 5) != HAL_OK) {
            continue;
        }

        switch (sync_state) {
            case 0:
                if (byte == UBX_SYNC1) {
                    sync_state = 1;
                }
                break;

            case 1:
                if (byte == UBX_SYNC2) {
                    sync_state = 2;
                }
                if (byte != UBX_SYNC2) {
                    if (byte == UBX_SYNC1) {
                        sync_state = 1;
                    }
                    if (byte != UBX_SYNC1) {
                        sync_state = 0;
                    }
                }
                break;
        }
    }

    uint32_t elapsed = HAL_GetTick() - start;
	if(elapsed >= timeout_ms){
		return GPS_ERR_COMMS;
	}
	uint32_t timeout_rem = timeout_ms - elapsed;
    uint16_t remaining_bytes = UBX_SIZE_CLASS + UBX_SIZE_ID + UBX_SIZE_LENGTH + exp_len + UBX_SIZE_CK;
    uint8_t rx_buf[remaining_bytes];

    // Segunda lectura: Se reciben el resto de bytes de la trama UBX
    if (HAL_UART_Receive(&huart1, rx_buf, remaining_bytes, timeout_rem) != HAL_OK) {
        return GPS_ERR_COMMS;
    }

    // Se valida la Clase, el ID y la longitud de trama
    uint16_t parsed_len = (uint16_t)rx_buf[2] | ((uint16_t)rx_buf[3] << 8);
    if ((rx_buf[0] != exp_class) || (rx_buf[1] != exp_id) || (parsed_len != exp_len)) {
    	return GPS_ERR_COMMS;
    }

    // Se validaa el Checksum
    uint8_t ck_a = 0, ck_b = 0;
    ubx_calcChecksum(rx_buf, UBX_SIZE_CLASS + UBX_SIZE_ID + UBX_SIZE_LENGTH + exp_len, &ck_a, &ck_b);
    uint8_t *recv_ck = &rx_buf[UBX_SIZE_CLASS + UBX_SIZE_ID + UBX_SIZE_LENGTH + exp_len];
    if ((recv_ck[0] != ck_a) || (recv_ck[1] != ck_b)) {
    	return GPS_ERR_COMMS;
    }

    // Se copia el payload al buffer de salida
    memcpy(payload_out, &rx_buf[4], exp_len);

    return GPS_OK;
}

static GPS_Status_t ubx_sendReceive(const uint8_t *query, uint16_t query_len, uint8_t exp_class, uint8_t exp_id, uint8_t *payload_out, uint16_t exp_len) {

    for (uint8_t i = 0; i < GPS_COMMS_REINTENTOS; i++) {

		if (ubx_send(query, query_len) == GPS_OK) {
			if (ubx_receive(exp_class, exp_id, payload_out, exp_len) == GPS_OK) {
				return GPS_OK;
			}
		}
		HAL_Delay(100);
	}
    // Si luego de 5 intentos, la comunicación sigue fallando, se reinicia la interfaz UART
    HAL_UART_DeInit(&huart1);
    HAL_UART_Init(&huart1);

	// Se repiten los 5 intentos
    for (uint8_t i = 0; i < GPS_COMMS_REINTENTOS; i++) {

		if (ubx_send(query, query_len) == GPS_OK) {
			if (ubx_receive(exp_class, exp_id, payload_out, exp_len) == GPS_OK) {
				return GPS_OK;
			}
		}
		HAL_Delay(100);
	}
    // Luego de los 10 intentos fallidos (habiendo reiniciado la interfaz de por medio) se retorna GPS_ERR_COMMS
    return GPS_ERR_COMMS;
}

static GPS_Status_t GPS_antenaStatus(void) {

	/* Solicitud de MON_HW */
    uint8_t hw_payload[UBX_PAYLOAD_MON_HW];
    if(ubx_sendReceive(CMD_MON_HW, sizeof(CMD_MON_HW),UBX_MON, UBX_MON_HW, hw_payload, UBX_PAYLOAD_MON_HW) == GPS_ERR_COMMS){
    	return GPS_ERR_COMMS;
    }

    uint16_t agcCnt = (uint16_t)hw_payload[MON_HW_OFFSET_AGCCNT] |
                      ((uint16_t)hw_payload[MON_HW_OFFSET_AGCCNT + 1] << 8);	// [18] agcCnt   — rango (0,8191)
    uint8_t aStatus = hw_payload[MON_HW_OFFSET_ASTATUS];						// [20] aStatus  — 0:init, 1:unknown, 2:OK, 3:short, 4:open
    uint8_t aPower  = hw_payload[MON_HW_OFFSET_APOWER];							// [21] aPower   — 0:off, 1:on
    uint8_t jamInd  = hw_payload[MON_HW_OFFSET_JAMIND];							// [53] jamInd	 — rango (0,255)

//    DPRINT("ANTENA_STATUS: %d \n\r", aStatus);
//    DPRINT("ANTENA_POWER: %d \n\r", aPower);

    if (aStatus != MON_HW_ASTATUS_OK || aPower != MON_HW_APOWER_ON || ((agcCnt > MON_HW_AGCCNT_MAX) && (jamInd > MON_HW_JAMIND_MAX))) {
        return GPS_ERR_ANTENNA;
    }

    return GPS_OK;
}

static GPS_Status_t GPS_hasValidFix(void) {

    /* Solicitud de NAV-STATUS */
    uint8_t status_payload[UBX_PAYLOAD_NAV_STATUS];
    if(ubx_sendReceive(CMD_NAV_STATUS, sizeof(CMD_NAV_STATUS), UBX_NAV, UBX_NAV_STATUS, status_payload, UBX_PAYLOAD_NAV_STATUS) == GPS_ERR_COMMS){
    	return GPS_ERR_COMMS;
    }

    uint8_t gpsFix = status_payload[4];		// [4] gpsFix uint8 - 2=2D fix, 3=3D fix
    uint8_t fixFlags = status_payload[5];	// [5] flags  uint8 - bit 0 = gpsFixOk

//    DPRINT("GPS_FIX_STATUS: %d \n\r", (fixFlags & GPS_FIX_OK));
//    DPRINT("GPS_FIX_TYPE: %d \n\r", gpsFix);
//
//    uint8_t flags2 = status_payload[7];
//    uint8_t psmState = flags2 & 0x03;
//
//    switch(psmState) {
//        case 0: // ACQUISITION
//            DPRINT("ESTADO MEF: ACQUISITION \n\r");
//            break;
//        case 1: // TRACKING
//            DPRINT("ESTADO MEF: TRACKING \n\r");
//            break;
//        case 2: // POT
//            DPRINT("ESTADO MEF: POT (POWER OPTIMIZED TRACKING) \n\r");
//            break;
//        case 3: // INACTIVE
//            DPRINT("ESTADO MEF: INACTIVE FOR SEARCH \n\r");
//            break;
//    }

    if ((gpsFix != GPS_FIX_2D && gpsFix != GPS_FIX_3D) || !(fixFlags & GPS_FIX_OK)) {
        return GPS_ERR_NO_FIX;
    }

    /* Solicitud de NAV-POSLLH */
    uint8_t posllh_payload[UBX_PAYLOAD_NAV_POSLLH];
    if(ubx_sendReceive(CMD_NAV_POSLLH, sizeof(CMD_NAV_POSLLH), UBX_NAV, UBX_NAV_POSLLH, posllh_payload, UBX_PAYLOAD_NAV_POSLLH) == GPS_ERR_COMMS){
    	return GPS_ERR_COMMS;
    }

    s_data.longitude = (int32_t)( posllh_payload[4]			// [4..7] lon int32 — grados * 1e7
                     | ((uint32_t)posllh_payload[5] << 8)
                     | ((uint32_t)posllh_payload[6] << 16)
                     | ((uint32_t)posllh_payload[7] << 24));

    s_data.latitude  = (int32_t)( posllh_payload[8]			// [8..11] lat int32 — grados * 1e7
                     | ((uint32_t)posllh_payload[9]  << 8)
                     | ((uint32_t)posllh_payload[10] << 16)
                     | ((uint32_t)posllh_payload[11] << 24));

    /* Solicitud de NAV-TIMEUTC */
    uint8_t timeutc_payload[UBX_PAYLOAD_NAV_TIMEUTC];
    if(ubx_sendReceive(CMD_NAV_TIMEUTC, sizeof(CMD_NAV_TIMEUTC), UBX_NAV, UBX_NAV_TIMEUTC, timeutc_payload, UBX_PAYLOAD_NAV_TIMEUTC) == GPS_ERR_COMMS){
    	return GPS_ERR_COMMS;
    }

    s_data.year  = (uint16_t)timeutc_payload[12] | ((uint16_t)timeutc_payload[13] << 8);
    s_data.month = timeutc_payload[14];
    s_data.day   = timeutc_payload[15];
    s_data.hour  = timeutc_payload[16];
    s_data.min   = timeutc_payload[17];
    s_data.sec   = timeutc_payload[18];

    return GPS_OK;
}
