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
#define LORAWAN_JOIN_EUI 	0x28aff1fc6cfbb1a8ULL
#define LORAWAN_DEVICE_EUI 	0xecc37c14215eaec2ULL
/* La siguiente credencial, se debe escribir entre {}, separando con comas los 16 bytes en formato HEX (anteponiendo 0x en c/u) */
#define LORAWAN_APP_KEY		{0xc1, 0x83, 0xa9, 0x4a, 0x6a, 0xc9, 0x7a, 0x56, 0x59, 0xba, 0xab, 0xa5, 0x50, 0xeb, 0xb6, 0xa9}

#define LORAWAN_PORT		1		// Puerto utilizado por LoRaWAN	(1 - 223)

/* Otras constantes */
#define IMPACT_THRESHOLD	0xFF	// Umbral del Impacto. La escala es 62.5 mg/LSB (ej: 0xFF x 62.5 mg = 16g). Si supera esto, se emite una alerta.
#define LANTERN_MIN_MV		3000	// Tensión mínima para la detección de corriente en la linterna, en mV. Si no se supera esto durante LANTERN_PERIOD_MS, se emite una alerta.
#define LANTERN_MIN_MA		4		// Corriente mínima admisible en la linterna, en mA. Si es menor a esto, se emite una alerta.
#define	LANTERN_PERIOD_MS	16000	// Periodo del patrón de destello de la linterna, en ms
#define GPS_TIMECAP_MS		60000	// Máximo tiempo que se le permite al GPS lograr un FIX, en ms. Si no se consigue el FIX en este tiempo, se emite una alerta.
#define TX_PERIOD_MIN		1		// Tiempo entre transmisiones periodicas, en minutos. Máx = 127 minutos (Apróx 2 horas). DEFAULT: 15 min

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_PARAMETERS_H_ */
