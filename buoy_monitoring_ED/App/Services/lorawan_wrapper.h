#ifndef LORAWAN_WRAPPER_H
#define LORAWAN_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct {
		uint64_t joinEUI;
		uint64_t deviceEUI;
		uint8_t appKey[16];
	} lorawan_credentials_t;

    typedef struct {
        uint8_t data[256];
        uint8_t len;
        uint8_t port;
        bool available;
        uint8_t window;	// 1 = RX1, 2 = RX2
        bool confirming;
    } lorawan_downlink_t;

    typedef struct {
        const void *payload;    // Puntero genérico (acepta cualquier tipo de dato)
        uint8_t len;            // Tamaño en bytes
        uint8_t port;           // Puerto LoRaWAN (1-223)
        bool confirmed;         // true = Confirmed, false = Unconfirmed
    } lorawan_uplink_t;

    void lorawan_init(lorawan_credentials_t* credentials);
    void lorawan_begin(void);
    void lorawan_configure(lorawan_credentials_t* credentials);
    void lorawan_sessionRestore(void);
    void lorawan_join(void);
    void lorawan_sessionSave(void);
    void lorawan_sendReceive(lorawan_uplink_t *uplink, lorawan_downlink_t *downlink);
    void lorawan_setADR(bool enable);
    void lorawan_setDataRate(uint8_t dr);
    int8_t lorawan_getSNR();
    int8_t lorawan_getRSSI();
    void RFM95W_notifyG0(void);
    void lorawan_sleep();

#ifdef __cplusplus
}
#endif

#endif
