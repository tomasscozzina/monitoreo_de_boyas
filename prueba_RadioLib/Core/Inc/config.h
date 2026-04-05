#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#include "lorawan_wrapper.h"
#include <stdio.h>

/* Solo en modo DEBUG se habilitan los define de abajo
 * Comentar alguno de ellos si es necesario
 */
#ifdef DEBUG
	#define COMMENTS
	// agregar acá más define que se deben habilitar en DEBUG
#endif

#ifdef COMMENTS
	#define DPRINT(fmt, ...) \
		do { \
			uint32_t tick = HAL_GetTick(); \
			printf("[%04lu.%03lu] " fmt, tick / 1000, tick % 1000, ##__VA_ARGS__); \
		} while (0)
#else
  	  #define DPRINT(...)
#endif

// --- Prototipos y Externs ---
extern const lorawan_credentials_t LORA_CREDENTIALS;

void system_init(void);
void lorawan_setup(void);
void Error_Handler(void);
void SystemClock_Config(void);

#endif
