#ifndef STM32F4_UART_LOGGER_BACKEND_H
#define STM32F4_UART_LOGGER_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

typedef struct
{
    UART_HandleTypeDef *uart;
    uint32_t timeout_ms;
    uint32_t write_calls;
    uint32_t write_errors;
    uint32_t bytes_written;
} Stm32f4UartLoggerBackend;

bool stm32f4_uart_logger_backend_init(
    Stm32f4UartLoggerBackend *backend,
    UART_HandleTypeDef *uart,
    uint32_t timeout_ms
);

/* Blocking bring-up logger. Do not call from an ISR. */
void stm32f4_uart_logger_backend_write(
    void *backend_context,
    const uint8_t *data,
    size_t length
);

#endif /* STM32F4_UART_LOGGER_BACKEND_H */
