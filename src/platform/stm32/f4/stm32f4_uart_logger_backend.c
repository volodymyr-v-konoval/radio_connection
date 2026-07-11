#include "stm32f4_uart_logger_backend.h"

#include <limits.h>
#include <string.h>

#include "stm32f4xx_hal.h"

bool stm32f4_uart_logger_backend_init(
    Stm32f4UartLoggerBackend *backend,
    UART_HandleTypeDef *uart,
    uint32_t timeout_ms
)
{
    if (backend == NULL || uart == NULL) {
        return false;
    }

    memset(backend, 0, sizeof(*backend));
    backend->uart = uart;
    backend->timeout_ms = timeout_ms;

    return true;
}

void stm32f4_uart_logger_backend_write(
    void *backend_context,
    const uint8_t *data,
    size_t length
)
{
    Stm32f4UartLoggerBackend *backend =
        (Stm32f4UartLoggerBackend *)backend_context;

    if (backend == NULL ||
        backend->uart == NULL ||
        data == NULL ||
        length == 0U) {
        return;
    }

    size_t offset = 0U;

    while (offset < length) {
        size_t chunk_size = length - offset;

        if (chunk_size > UINT16_MAX) {
            chunk_size = UINT16_MAX;
        }

        const HAL_StatusTypeDef status = HAL_UART_Transmit(
            backend->uart,
            (uint8_t *)&data[offset],
            (uint16_t)chunk_size,
            backend->timeout_ms
        );

        backend->write_calls++;

        if (status != HAL_OK) {
            backend->write_errors++;
            return;
        }

        backend->bytes_written += (uint32_t)chunk_size;
        offset += chunk_size;
    }
}
