#include "app_main.h"

#include "board_validation.h"
#include "main.h"
#include "usart.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CRSF_DMA_BUFFER_SIZE 256U

static uint8_t s_crsf_dma_buffer[CRSF_DMA_BUFFER_SIZE];

static void logger_write(const char *text)
{
    if (text == NULL) {
        return;
    }

    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        1000U
    );
}

void app_main_init(void)
{
    char message[96];

    logger_write("\r\n");
    logger_write("[BOOT] radio_connection STM32F407 Stage 4A\r\n");

    const uint32_t validation_errors = board_validate_configuration();

    if (validation_errors != 0U) {
        (void)snprintf(
            message,
            sizeof(message),
            "[BOARD] validation FAILED: 0x%08lX\r\n",
            (unsigned long)validation_errors
        );

        logger_write(message);
        Error_Handler();
    }

    logger_write("[BOARD] validation OK\r\n");

    if (HAL_UART_Receive_DMA(
            &huart2,
            s_crsf_dma_buffer,
            sizeof(s_crsf_dma_buffer)) != HAL_OK) {
        logger_write("[CRSF] USART2 RX DMA start FAILED\r\n");
        Error_Handler();
    }

    logger_write("[CRSF] USART2 RX circular DMA started\r\n");
    logger_write("[BOOT] Stage 4A initialization complete\r\n");
}

void app_main_poll(void)
{
    /*
     * No protocol processing yet.
     * RP2 is intentionally not connected during Stage 4A.
     */
}
