#ifndef LORAWAN_WRAPPER_H
#define LORAWAN_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* Códigos de retorno */
	typedef enum {
		LORA_JOIN_OK  =  0,   	/* La activación (JOIN) fue exitosa */
		LORA_UP_OK,				/* Se transmitió Uplink SIN recepción de Downlink */
		LORA_DOWN_AVAILABLE,	/* Se transmitió Uplink CON recepción de Downlink */
		LORA_ERR_COMMS,			/* La comunicación con el Network Server (NS) falló */
	} LoRaWAN_Status_t;

    typedef struct {
        const void *payload;    /* Puntero genérico (acepta cualquier tipo de dato) */
        uint8_t len;			/* Tamaño del payload, en bytes */
        uint8_t port;           /* Puerto LoRaWAN (1-223) */
        bool confirmed;         /* true = Confirmed, false = Unconfirmed */
    } lorawan_uplink_t;

    typedef struct {
    	bool confirming;		/* Acuse del recibo del Uplink confirmed anterior: true = Confirming, false = Unconfirming */
    	bool available;			/* Hay payload dispobible en el Downlink */
        uint8_t data[242];		/* Arreglo de RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE bytes para la recepción del Dowlink */
    } lorawan_downlink_t;

    LoRaWAN_Status_t lorawan_init(uint64_t joinEUI, uint64_t deviceEUI, uint8_t* appKey);
    LoRaWAN_Status_t lorawan_sendReceive(lorawan_uplink_t *uplink, lorawan_downlink_t *downlink);
    void lorawan_setADR(bool enable);
    void lorawan_setDataRate(uint8_t dr);
    int8_t lorawan_getSNR(void);
    int8_t lorawan_getRSSI(void);
    void RFM95W_notifyG0(void);
    void lorawan_sleep(void);

#ifdef __cplusplus
}
#endif

#endif
