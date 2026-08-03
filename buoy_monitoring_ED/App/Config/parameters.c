/******************************************************************************
 * @file    parameters.c
 * @author  Tomás Agustín Scozzina
 * @date    1 ago 2026
 * @brief   Credenciales de LoRaWAN y otras constantes
 ******************************************************************************/
#include "parameters.h"
#include "usart.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>

#define HEADER_MAGIC  0x21474643UL // Marca 'CFG!' en Little-Endian

/* Instancia global en RAM */
parameters_t parameters;

static void led_update_pattern(uint32_t elapsed_ms) {
    static uint32_t last_toggle = 0;
    uint32_t interval = (elapsed_ms >= 25000) ? 500 : 1000;

    if ((HAL_GetTick() - last_toggle) >= interval) {
        HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
        last_toggle = HAL_GetTick();
    }
}

void parameters_init(void) {
    printf("[CFG] Esperando parametros por UART2 (30s)...\r\n");

    // Escuchar por UART2 durante 30 segundos
    if (parameters_receive(30000)) {
        printf("[CFG] Parametros recibidos y guardados con exito.\r\n");
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
        HAL_Delay(2000);
        return;
    }

    // Intentar cargar desde Flash
    if (parameters_load()) {
        printf("[CFG] Parametros cargados correctamente desde Flash NVM.\r\n");
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
        HAL_Delay(2000);
        return;
    }

    // Esperar indefinidamente con titilado constante de 1 segundo
    printf("[CFG] ERROR: Sin datos en Flash. Esperando configuracion indefinidamente...\r\n");
    while (1) {
        // Ejecuta en bloques de 10s con timeout de 10000ms para mantener el loop activo
        if (parameters_receive(10000)) {
            printf("[CFG] Parametros iniciales recibidos. Continuando arranque...\r\n");
            HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
            HAL_Delay(2000);
            return;
        }
    }
}

bool parameters_receive(uint32_t timeout_ms) {
    uint8_t header_buf[4];
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms) {
        // Actualizar el patrón del LED según el tiempo transcurrido
        led_update_pattern(HAL_GetTick() - start_tick);

        // Intentar recibir la cabecera 'CFG!' sin bloquear la CPU por completo
        if (HAL_UART_Receive(&huart2, header_buf, 4, 50) == HAL_OK) {
            uint32_t received_header;
            memcpy(&received_header, header_buf, 4);

            if (received_header == HEADER_MAGIC) {
                parameters_t rx_params;

                if (HAL_UART_Receive(&huart2, (uint8_t *)&rx_params, sizeof(parameters_t), 2000) == HAL_OK) {

                    // Verificar la integridad del PARAMS_MAGIC recibido en el payload
                    if (rx_params.magic == PARAMS_MAGIC) {
                        if (parameters_save(&rx_params)) {
                            HAL_UART_Transmit(&huart2, (uint8_t *)"OK\r\n", 4, 100);
                            return true;
                        } else {
                            HAL_UART_Transmit(&huart2, (uint8_t *)"ERROR_FLASH\r\n", 13, 100);
                        }
                    } else {
                        HAL_UART_Transmit(&huart2, (uint8_t *)"ERROR_MAGIC\r\n", 13, 100);
                    }
                }
            }
        }
    }
    return false;
}

bool parameters_load(void) {
    const parameters_t *flash_ptr = (const parameters_t *)PARAMS_FLASH_ADDR;

    if (flash_ptr->magic != PARAMS_MAGIC) {
        return false;
    }

    memcpy(&parameters, flash_ptr, sizeof(parameters_t));
    return true;
}

bool parameters_save(parameters_t *params) {
    params->magic = PARAMS_MAGIC;

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = PARAMS_FLASH_BANK,
        .Page      = PARAMS_FLASH_PAGE,
        .NbPages   = 1
    };

    uint32_t pageError = 0;
    if (HAL_FLASHEx_Erase(&erase, &pageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    uint8_t *data_ptr = (uint8_t *)params;
    for (size_t i = 0; i < sizeof(parameters_t); i += 8) {
        uint64_t word = 0xFFFFFFFFFFFFFFFFULL;
        memcpy(&word, data_ptr + i, 8);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, PARAMS_FLASH_ADDR + i, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();

    // Sincronizar la variable global en RAM con los nuevos datos
    memcpy(&parameters, params, sizeof(parameters_t));
    return true;
}
