#include "config.h"

#include "stm32l4xx_hal.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

const lorawan_credentials_t LORA_CREDENTIALS = {
    .joinEUI = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .devEUI  = {0x72, 0x32, 0x7B, 0xC6, 0x37, 0x22, 0x78, 0x41},
    .appKey  = {0xF6, 0x4E, 0xC8, 0x24, 0x2B, 0x0C, 0x12, 0x52,
                0x68, 0x37, 0x7C, 0x81, 0x24, 0x27, 0x4C, 0xEA}
};

bool sw1_flag = false;

void system_init(void){
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_SPI1_Init();
	MX_USART2_UART_Init();
	setvbuf(stdout, NULL, _IONBF, 0);
}

void lorawan_setup(void){

	lorawan_init();
	lorawan_configure(&LORA_CREDENTIALS);
	lorawan_session_restore();
	lorawan_join();
	lorawan_session_save();
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_10;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

void Error_Handler(void){
  __disable_irq();
  while (1){}
}

// Redefinición de función weak, necesaria para el método printf()
int __io_putchar(int ch){
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

// Redefinición de callback weak
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if(GPIO_Pin == SW1_Pin){
		sw1_flag = true;
	}
}

bool get_sw1PressEv(void){
	if(sw1_flag){
		sw1_flag = false;
		return true;
	}
	return false;
}
