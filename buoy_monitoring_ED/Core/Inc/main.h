/******************************************************************************
 * @file    main.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Código de inicio
 ******************************************************************************/

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include <stdio.h>

/****************************************************************************************************/
#ifdef DEBUG
	// En modo DEBUG, se habilitan las siguientes definiciones
	#define COMMENTS
#endif

#ifdef COMMENTS
	// Defino DPRINT para imprimir por consola con el formato "[TIME] LOG"
	#define DPRINT(fmt, ...) \
		do { \
			uint32_t tick = HAL_GetTick(); \
			printf("[%04lu.%03lu] " fmt, tick / 1000, tick % 1000, ##__VA_ARGS__); \
		} while (0)
#else
  	  #define DPRINT(...)
#endif
/****************************************************************************************************/

#define RFM95W_MISO_Pin 		GPIO_PIN_4
#define RFM95W_MISO_GPIO_Port 	GPIOB
#define RFM95W_MOSI_Pin 		GPIO_PIN_5
#define RFM95W_MOSI_GPIO_Port 	GPIOB
#define RFM95W_SCK_Pin 			GPIO_PIN_1
#define RFM95W_SCK_GPIO_Port 	GPIOA
#define RFM95W_RST_Pin 			GPIO_PIN_8
#define RFM95W_RST_GPIO_Port 	GPIOA
#define RFM95W_CS_Pin 			GPIO_PIN_11
#define RFM95W_CS_GPIO_Port 	GPIOA
#define RFM95W_G0_Pin 			GPIO_PIN_12
#define RFM95W_G0_GPIO_Port 	GPIOA
#define RFM95W_G0_EXTI_IRQn 	EXTI15_10_IRQn

#define ADXL345_INT2_Pin 		GPIO_PIN_0
#define ADXL345_INT2_GPIO_Port 	GPIOB
#define ADXL345_INT2_EXTI_IRQn 	EXTI0_IRQn
#define ADXL345_INT1_Pin 		GPIO_PIN_1
#define ADXL345_INT1_GPIO_Port 	GPIOB
#define ADXL345_INT1_EXTI_IRQn 	EXTI1_IRQn

#define LD3_Pin 				GPIO_PIN_3
#define LD3_GPIO_Port 			GPIOB

#define SW1_Pin 				GPIO_PIN_3
#define SW1_GPIO_Port 			GPIOA
#define SW1_EXTI_IRQn 			EXTI3_IRQn

#define SWDIO_Pin 				GPIO_PIN_13
#define SWDIO_GPIO_Port 		GPIOA
#define SWCLK_Pin 				GPIO_PIN_14
#define SWCLK_GPIO_Port 		GPIOA

#define VCP_RX_Pin 				GPIO_PIN_15
#define VCP_RX_GPIO_Port 		GPIOA
#define VCP_TX_Pin 				GPIO_PIN_2
#define VCP_TX_GPIO_Port 		GPIOA

/****************************************************************************************************/

void Error_Handler(void);
void CheckReset_Flag(void);

/****************************************************************************************************/
#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
