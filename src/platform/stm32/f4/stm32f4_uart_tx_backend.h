#ifndef STM32F4_UART_TX_BACKEND_H
#define STM32F4_UART_TX_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_tx_if.h"

#define STM32F4_UART_TX_BACKEND_MAX_FRAME_SIZE 64U

typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

typedef struct
{
    uint32_t start_attempts;
    uint32_t started_frames;
    uint32_t completed_frames;
    uint32_t busy_rejections;
    uint32_t invalid_requests;
    uint32_t hal_errors;
    uint32_t uart_errors;
    uint32_t bytes_started;
} Stm32f4UartTxBackendStats;

typedef struct
{
    UART_HandleTypeDef *uart;
    uint8_t tx_buffer[STM32F4_UART_TX_BACKEND_MAX_FRAME_SIZE];
    uint16_t tx_length;
    volatile bool busy;
    Stm32f4UartTxBackendStats stats;
} Stm32f4UartTxBackend;

bool stm32f4_uart_tx_backend_init(
    Stm32f4UartTxBackend *backend,
    RadioTx *tx,
    UART_HandleTypeDef *uart
);

void stm32f4_uart_tx_backend_on_tx_complete(
    Stm32f4UartTxBackend *backend,
    UART_HandleTypeDef *uart
);

void stm32f4_uart_tx_backend_on_error(
    Stm32f4UartTxBackend *backend,
    UART_HandleTypeDef *uart
);

bool stm32f4_uart_tx_backend_is_busy(
    const Stm32f4UartTxBackend *backend
);

void stm32f4_uart_tx_backend_get_stats(
    const Stm32f4UartTxBackend *backend,
    Stm32f4UartTxBackendStats *out_stats
);

#endif /* STM32F4_UART_TX_BACKEND_H */
