/******************************************************************************
 * @file    lorawan_credentials.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Credenciales de LoRaWAN
 ******************************************************************************/

#ifndef CONFIG_LORAWAN_CREDENTIALS_H_
#define CONFIG_LORAWAN_CREDENTIALS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
/* Las dos credenciales siguientes, se deben escribir con el siguiente formato: 0x<CREDENCIAL>ULL */
#define LORAWAN_JOIN_EUI 	0x0000000000000000ULL
#define LORAWAN_DEVICE_EUI 	0x72327bc637227841ULL

/* La siguiente credencial, se debe escribir entre {}, separando con comas
 * los 16 bytes en formato HEX (anteponiendo 0x en c/u) */
#define LORAWAN_APP_KEY		{0xf6, 0x4e, 0xc8, 0x24, 0x2b, 0x0c, 0x12, 0x52, 0x68, 0x37, 0x7c, 0x81, 0x24, 0x27, 0x4c, 0xea}
/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_LORAWAN_CREDENTIALS_H_ */
