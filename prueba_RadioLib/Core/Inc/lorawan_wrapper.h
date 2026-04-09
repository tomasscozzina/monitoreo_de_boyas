#ifndef LORAWAN_WRAPPER_H
#define LORAWAN_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PERIODIC_TRANSMISSION_PORT 	1
#define IMPACT_TRANSMISSION_PORT 	2

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint8_t joinEUI[8];
        uint8_t devEUI[8];
        uint8_t appKey[16];
    } lorawan_credentials_t;

    typedef struct {
        uint8_t data[256];
        uint8_t len;
        uint8_t port;
        bool available;
        uint8_t window;	// 1 = RX1, 2 = RX2
    } lorawan_downlink_t;

    typedef struct {
        const void *payload;    // Puntero genérico (acepta cualquier tipo de dato)
        uint8_t len;            // Tamaño en bytes
        uint8_t port;           // Puerto LoRaWAN (1-223)
        bool confirmed;         // true = Confirmed, false = Unconfirmed
    } lorawan_uplink_t;

    void lorawan_init(void);
    void lorawan_configure(const lorawan_credentials_t *cred);
    void lorawan_session_restore(void);
    void lorawan_join(void);
    void lorawan_session_save(void);
    void lorawan_send(lorawan_uplink_t *uplink, lorawan_downlink_t *downlink);

#ifdef __cplusplus
}
#endif

#endif
