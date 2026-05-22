/**
 ******************************************************************************
 * @file    gps.c
 * @brief   Implementación de la librería GPS NEO-6M (protocolo UBX, sin DMA).
 *
 * Todos los mensajes siguen la estructura UBX:
 *   [0xB5][0x62][CLASS][ID][LEN_L][LEN_H][PAYLOAD...][CK_A][CK_B]
 *
 * El checksum UBX (Fletcher-8) se calcula sobre CLASS, ID, LEN y PAYLOAD.
 ******************************************************************************
 */

#include "gps.h"
#include <string.h>
#include <stdint.h>
#include "usart.h"
#include "config.h"

/* ===========================================================================
 * Definiciones internas
 * =========================================================================*/

#define UBX_SYNC1   0xB5	// Letra griega mu en ascii extendido
#define UBX_SYNC2   0x62	// Letra b en ascii

/* Clases esperadas en cada respuesta */
#define UBX_MON		0x0A
#define UBX_NAV		0x01
#define UBX_RXM		0x02

/* Mensajes esperados en cada respuesta */
#define UBX_MON_HW			0x09
#define UBX_NAV_STATUS		0x03
#define UBX_NAV_POSLLH		0x02
#define UBX_NAV_TIMEUTC		0x21
#define UBX_RXM_PMREQ		0x41

/* Tamaños de payload esperados en cada respuesta */
#define UBX_PAYLOAD_MON_HW        68
#define UBX_PAYLOAD_NAV_STATUS    16
#define UBX_PAYLOAD_NAV_POSLLH    28
#define UBX_PAYLOAD_NAV_TIMEUTC   20

/* Timeouts */
#define GPS_TIMEOUT_MS   2000

/* Valores válidos de aStatus y aPower en MON-HW */
#define MON_HW_ASTATUS_OK   2
#define MON_HW_APOWER_ON    1

/* Offsets del payload MON-HW */
#define MON_HW_OFFSET_ASTATUS   20
#define MON_HW_OFFSET_APOWER    21

/* Valores válidos de gpsFix en NAV-STATUS */
#define GPS_FIX_2D   2
#define GPS_FIX_3D   3
#define GPS_FIX_OK   (1 << 0)

/* ===========================================================================
 * Tramas hardcodeadas (CLASS ID LEN_L LEN_H PAYLOAD CK_A CK_B)
 * =========================================================================*/

static const uint8_t CMD_WAKE[]        = { 0xFF };
static const uint8_t CMD_MON_HW[]      = { 0xB5, 0x62, 0x0A, 0x09, 0x00, 0x00, 0x13, 0x43 };
static const uint8_t CMD_NAV_STATUS[]  = { 0xB5, 0x62, 0x01, 0x03, 0x00, 0x00, 0x04, 0x0D };
static const uint8_t CMD_NAV_POSLLH[]  = { 0xB5, 0x62, 0x01, 0x02, 0x00, 0x00, 0x03, 0x0A };
static const uint8_t CMD_NAV_TIMEUTC[] = { 0xB5, 0x62, 0x01, 0x21, 0x00, 0x00, 0x22, 0x67 };
static const uint8_t CMD_RXM_PMREQ[]   = { 0xB5, 0x62, 0x02, 0x41, 0x08, 0x00,
                                            0x00, 0x00, 0x00, 0x00,   /* duration = 0 (infinito) */
                                            0x02, 0x00, 0x00, 0x00,   /* flags: bit1 = backup    */
                                            0x4D, 0x3B };

/* ===========================================================================
 * Variables privadas
 * =========================================================================*/

static int32_t    s_latitude  = 0;
static int32_t    s_longitude = 0;
static GPS_UTCTime s_utcTime  = {0};

/* ===========================================================================
 * Funciones privadas
 * =========================================================================*/

/**
 * @brief  Calcula el checksum UBX Fletcher-8.
 *
 * Se aplica sobre CLASS, ID, LEN_L, LEN_H y PAYLOAD (el puntero debe
 * apuntar al byte CLASS y len debe incluir los 4 bytes de cabecera + payload).
 */
static void ubx_CalcChecksum(const uint8_t *data, uint16_t len, uint8_t *ck_a, uint8_t *ck_b) {
    *ck_a = 0;
    *ck_b = 0;
    for (uint16_t i = 0; i < len; i++) {
        *ck_a += data[i];
        *ck_b += *ck_a;
    }
}

/**
 * @brief  Envía una trama hardcodeada por UART.
 *
 * @param[in] frame  Puntero a la trama completa.
 * @param[in] len    Longitud en bytes.
 */
static void ubx_Send(const uint8_t *frame, uint16_t len) {
    HAL_UART_Transmit(&huart1, (uint8_t *)frame, len, GPS_TIMEOUT_MS);
}

/**
 * @brief  Recibe y valida un mensaje UBX esperado.
 *
 * Sincroniza con el header 0xB5 0x62, lee CLASS, ID, LEN y PAYLOAD,
 * verifica el checksum y copia el payload al buffer del llamador.
 * Descarta bytes previos al header sin procesarlos.
 *
 * @param[in]  exp_class    Clase UBX esperada.
 * @param[in]  exp_id       ID UBX esperado.
 * @param[out] payload_out  Buffer donde se copia el payload.
 * @param[in]  exp_len      Longitud de payload esperada.
 * @param[in]  timeout_ms   Tiempo máximo de espera en ms.
 *
 * @retval GPS_OK            Mensaje recibido y checksum válido.
 * @retval GPS_ERR_TIMEOUT   No llegó la trama completa a tiempo.
 * @retval GPS_ERR_CHECKSUM  Checksum incorrecto.
 * @retval GPS_ERR_BADHEADER Clase, ID o longitud no coinciden.
 */
static GPS_Status ubx_Receive(uint8_t exp_class, uint8_t exp_id, uint8_t *payload_out, uint16_t exp_len, uint32_t timeout_ms) {
    uint8_t byte;
    HAL_StatusTypeDef hal_ret;
    uint32_t start = HAL_GetTick();

    /* Sincronizar con SYNC1 */
    do {
        hal_ret = HAL_UART_Receive(&huart1, &byte, 1, 10);
        if (HAL_GetTick() - start >= timeout_ms) return GPS_ERR_TIMEOUT;
    } while (hal_ret != HAL_OK || byte != UBX_SYNC1);

    /* Sincronizar con SYNC2 */
    do {
        hal_ret = HAL_UART_Receive(&huart1, &byte, 1, 10);
        if (HAL_GetTick() - start >= timeout_ms) return GPS_ERR_TIMEOUT;
    } while (hal_ret != HAL_OK || byte != UBX_SYNC2);

    /* Leer CLASS, ID, LEN_L, LEN_H */
    uint8_t header[4];
    uint32_t remaining = timeout_ms - (HAL_GetTick() - start);
    hal_ret = HAL_UART_Receive(&huart1, header, 4, remaining);
    if (hal_ret != HAL_OK) return GPS_ERR_TIMEOUT;

    uint8_t  msg_class   = header[0];
    uint8_t  msg_id      = header[1];
    uint16_t payload_len = (uint16_t)header[2] | ((uint16_t)header[3] << 8);

    if (msg_class != exp_class || msg_id != exp_id || payload_len != exp_len) {
        return GPS_ERR_BADHEADER;
    }

    /* Leer payload */
    remaining = timeout_ms - (HAL_GetTick() - start);
    hal_ret = HAL_UART_Receive(&huart1, payload_out, payload_len, remaining);
    if (hal_ret != HAL_OK) return GPS_ERR_TIMEOUT;

    /* Leer checksum recibido */
    uint8_t recv_ck[2];
    remaining = timeout_ms - (HAL_GetTick() - start);
    hal_ret = HAL_UART_Receive(&huart1, recv_ck, 2, remaining);
    if (hal_ret != HAL_OK) return GPS_ERR_TIMEOUT;

    /* Calcular checksum esperado sobre header[0..3] + payload */
    uint8_t ck_buf[4 + payload_len];
    memcpy(ck_buf, header, 4);
    memcpy(ck_buf + 4, payload_out, payload_len);

    uint8_t ck_a, ck_b;
    ubx_CalcChecksum(ck_buf, 4 + payload_len, &ck_a, &ck_b);

    if (recv_ck[0] != ck_a || recv_ck[1] != ck_b) {
        return GPS_ERR_CHECKSUM;
    }

    return GPS_OK;
}

/* ===========================================================================
 * API pública
 * =========================================================================*/

uint32_t TTFF_begin = 0;

GPS_Status GPS_WakeUp(void) {
    /* Despertar el módulo con actividad en la línea serie */
    ubx_Send(CMD_WAKE, sizeof(CMD_WAKE));

    TTFF_begin = HAL_GetTick();
    /* Espera mínima para que el UART interno del módulo esté listo */
    HAL_Delay(2000);

    /* Solicitar MON-HW para confirmar que el módulo está activo */
    ubx_Send(CMD_MON_HW, sizeof(CMD_MON_HW));

    uint8_t payload[UBX_PAYLOAD_MON_HW];
    GPS_Status ret = ubx_Receive(UBX_MON, UBX_MON_HW, payload, UBX_PAYLOAD_MON_HW, GPS_TIMEOUT_MS);
    if (ret != GPS_OK) return ret;

    /*
     * Payload MON-HW, offsets relevantes:
     *   [20] aStatus  — 0:init, 1:unknown, 2:OK, 3:short, 4:open
     *   [21] aPower   — 0:off, 1:on
     */
    uint8_t aStatus = payload[MON_HW_OFFSET_ASTATUS];
    uint8_t aPower  = payload[MON_HW_OFFSET_APOWER];

    if (aStatus != MON_HW_ASTATUS_OK || aPower != MON_HW_APOWER_ON) {
        return GPS_ERR_ANTENNA;
    }

    return GPS_OK;
}

GPS_Status GPS_HasValidFix(void) {
    GPS_Status ret;

    /* --- NAV-STATUS --- */
    ubx_Send(CMD_NAV_STATUS, sizeof(CMD_NAV_STATUS));

    uint8_t status_payload[UBX_PAYLOAD_NAV_STATUS];
    ret = ubx_Receive(UBX_NAV, UBX_NAV_STATUS, status_payload, UBX_PAYLOAD_NAV_STATUS, GPS_TIMEOUT_MS);
    if (ret != GPS_OK) return ret;

    /*
     * Payload NAV-STATUS:
     *   [0..3] iTOW    	uint32
     *   [4]    gpsFix  	uint8  - 2=2D fix, 3=3D fix
     *   [5]    flags       uint8  - bit 0 = gpsFixOk
     */
    uint8_t gpsFix = status_payload[4];
    uint8_t fixFlags = status_payload[5];

    DPRINT("GPS_FIX_STATUS: %d \n\r", (fixFlags & GPS_FIX_OK));
    DPRINT("GPS_FIX_TYPE: %d \n\r", gpsFix);

    if ((gpsFix != GPS_FIX_2D && gpsFix != GPS_FIX_3D) || !(fixFlags & GPS_FIX_OK)) {
        return GPS_ERR_NO_FIX;
    }
    uint32_t TTFF = HAL_GetTick() - TTFF_begin;
    DPRINT("TTFF: %lu.%03lu seg \n\r", TTFF / 1000, TTFF % 1000);

    /* --- NAV-POSLLH --- */
    ubx_Send(CMD_NAV_POSLLH, sizeof(CMD_NAV_POSLLH));

    uint8_t posllh_payload[UBX_PAYLOAD_NAV_POSLLH];
    ret = ubx_Receive(UBX_NAV, UBX_NAV_POSLLH, posllh_payload, UBX_PAYLOAD_NAV_POSLLH, GPS_TIMEOUT_MS);
    if (ret != GPS_OK) return ret;

    /*
     * Payload NAV-POSLLH:
     *   [0..3]  iTOW   uint32
     *   [4..7]  lon    int32  — grados * 1e7
     *   [8..11] lat    int32  — grados * 1e7
     */
    s_longitude = (int32_t)( posllh_payload[4]
                | ((uint32_t)posllh_payload[5] << 8)
                | ((uint32_t)posllh_payload[6] << 16)
                | ((uint32_t)posllh_payload[7] << 24));

    s_latitude  = (int32_t)( posllh_payload[8]
                | ((uint32_t)posllh_payload[9]  << 8)
                | ((uint32_t)posllh_payload[10] << 16)
                | ((uint32_t)posllh_payload[11] << 24));


    /* --- NAV-TIMEUTC --- */
    ubx_Send(CMD_NAV_TIMEUTC, sizeof(CMD_NAV_TIMEUTC));

    uint8_t timeutc_payload[UBX_PAYLOAD_NAV_TIMEUTC];
    ret = ubx_Receive(UBX_NAV, UBX_NAV_TIMEUTC, timeutc_payload, UBX_PAYLOAD_NAV_TIMEUTC, GPS_TIMEOUT_MS);
    if (ret != GPS_OK) return ret;

    /*
     * Payload NAV-TIMEUTC:
     *   [12..13] year   uint16
     *   [14]     month  uint8
     *   [15]     day    uint8
     *   [16]     hour   uint8
     *   [17]     min    uint8
     *   [18]     sec    uint8
     */
    s_utcTime.year  = (uint16_t)timeutc_payload[12] | ((uint16_t)timeutc_payload[13] << 8);
    s_utcTime.month = timeutc_payload[14];
    s_utcTime.day   = timeutc_payload[15];
    s_utcTime.hour  = timeutc_payload[16];
    s_utcTime.min   = timeutc_payload[17];
    s_utcTime.sec   = timeutc_payload[18];

    return GPS_OK;
}

void GPS_GetLatitude(int32_t *lat) {
    *lat = s_latitude;
}

void GPS_GetLongitude(int32_t *lon) {
    *lon = s_longitude;
}

void GPS_GetUTCTime(GPS_UTCTime *utc) {
    *utc = s_utcTime;
}

void GPS_Sleep(void) {
    ubx_Send(CMD_RXM_PMREQ, sizeof(CMD_RXM_PMREQ));
}
