#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include "config.h"

#define VCP_TX_Pin GPIO_PIN_2
#define VCP_TX_GPIO_Port GPIOA
#define RFM95W_RST_Pin GPIO_PIN_8
#define RFM95W_RST_GPIO_Port GPIOA
#define RFM95W_CS_Pin GPIO_PIN_11
#define RFM95W_CS_GPIO_Port GPIOA
#define RFM95W_G0_Pin GPIO_PIN_12
#define RFM95W_G0_GPIO_Port GPIOA
#define RFM95W_G0_EXTI_IRQn EXTI15_10_IRQn
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define VCP_RX_Pin GPIO_PIN_15
#define VCP_RX_GPIO_Port GPIOA
#define RFM95W_SCK_Pin GPIO_PIN_3
#define RFM95W_SCK_GPIO_Port GPIOB
#define RFM95W_MISO_Pin GPIO_PIN_4
#define RFM95W_MISO_GPIO_Port GPIOB
#define RFM95W_MOSI_Pin GPIO_PIN_5
#define RFM95W_MOSI_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
