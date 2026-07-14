/******************************************************************************
 * @file    parameters.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Credenciales de LoRaWAN y otras constantes
 ******************************************************************************/

#ifndef CONFIG_PARAMETERS_H_
#define CONFIG_PARAMETERS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Las dos credenciales siguientes, se deben escribir con el siguiente formato: 0x<CREDENCIAL>ULL */
#define LORAWAN_JOIN_EUI 	0x0000000000000000ULL
#define LORAWAN_DEVICE_EUI 	0x72327bc637227841ULL
/* La siguiente credencial, se debe escribir entre {}, separando con comas los 16 bytes en formato HEX (anteponiendo 0x en c/u) */
#define LORAWAN_APP_KEY		{0xf6, 0x4e, 0xc8, 0x24, 0x2b, 0x0c, 0x12, 0x52, 0x68, 0x37, 0x7c, 0x81, 0x24, 0x27, 0x4c, 0xea}

#define LORAWAN_PORT		1		// Puerto utilizado por LoRaWAN	(1 - 223)

/* Otras constantes */
#define IMPACT_THRESHOLD	0xFF	// Umbral del Impacto. La escala es 62.5 mg/LSB (ej: 0xFF x 62.5 mg = 16g). Si supera esto, se emite una alerta.
#define LANTERN_MIN_MV		3100	// Tensión mínima para la detección de corriente en la linterna, en mV. Si no se supera esto durante LANTERN_PERIOD_MS, se emite una alerta.
#define LANTERN_MIN_MA		4		// Corriente mínima admisible en la linterna, en mA. Si es menor a esto, se emite una alerta.
#define	LANTERN_PERIOD_MS	30000	// Periodo del patrón de destello de la linterna, en ms
#define GPS_TIMECAP_MS		60000	// Máximo tiempo que se le permite al GPS lograr un FIX, en ms. Si no se consigue el FIX en este tiempo, se emite una alerta.
#define TX_PERIOD_S			10		// Tiempo entre transmisiones periodicas, en segundos. Máx = 65535 segundos (Apróx 18 horas). DEFAULT: 900 (15 min)

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_PARAMETERS_H_ */
