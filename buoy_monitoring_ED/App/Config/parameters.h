/******************************************************************************
 * @file    parameters.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Credenciales de LoRaWAN y otras constantes
 ******************************************************************************/
#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define PARAMS_MAGIC            0x55AA55AAUL
#define PARAMS_FLASH_PAGE       62U
#define PARAMS_FLASH_BANK       FLASH_BANK_1
#define PARAMS_FLASH_ADDR       0x0801F000UL

typedef struct __attribute__((packed)) {
    uint32_t magic;             // Cabecera de validación
    uint64_t joinEUI;           // 8 bytes
    uint64_t deviceEUI;         // 8 bytes
    uint8_t  appKey[16];        // 16 bytes
    uint8_t  port;              // 1 byte
    uint8_t  impactThreshold;   // 1 byte
    int16_t  lanternMin_mV;     // 2 bytes
    int16_t  lanternMin_mA;     // 2 bytes
    uint8_t  lanternPeriod_s;   // 1 byte
    uint8_t  gpsTimecap_s;      // 1 byte
    uint8_t  txPeriod_min;      // 1 byte
    uint8_t  reserved[3];       // Relleno para alinear a 48 bytes (múltiplo de 8 bytes)
} parameters_t;

/* Variable global accesible desde toda la aplicación */
extern parameters_t parameters;

/* Funciones públicas */
void parameters_init(void);
bool parameters_save(parameters_t *params);
bool parameters_load(void);
bool parameters_receive(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* PARAMETERS_H_ */
