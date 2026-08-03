/******************************************************************************
 * @file    main.c
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   Código de inicio
 ******************************************************************************/

#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"
#include "rtc.h"
#include "buoy_app.h"
#include "system_clock.h"
#include "parameters.h"

uint8_t is_boot_retry = 0;

int main(void) {
	HAL_Init();
	CheckReset_Flag();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();
    MX_RTC_Init();
    setvbuf(stdout, NULL, _IONBF, 0);

    is_boot_retry = 0;	// Si se llegó a este punto sin errores de inicialización, se reinicia la bandera

    parameters_init();
    buoyApp_init();

    while(1) {
    	buoyApp_run();
    }
}

int __io_putchar(int ch){
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

/* Esta función se llama si se produce un error en la inicialización de los periféricos del micro */
void Error_Handler(void){
	__disable_irq();

	if(is_boot_retry == 1) {	/* Si se vuelve a producir un error durante la inicialización, se entra a un bucle infinito */
		while(1){}
	}
	NVIC_SystemReset();		/* Si es la primera vez que se produce un error durante la inicialización, se resetea el micro */
}

/* En esta función se revisa si ya se produjo un reinicio por software anteriormente */
void CheckReset_Flag(void) {
	if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
		is_boot_retry = 1;		/* En caso afirmativo, se setea esta bandera */
	}
	__HAL_RCC_CLEAR_RESET_FLAGS();
}
