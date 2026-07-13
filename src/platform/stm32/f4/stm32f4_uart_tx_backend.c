#include "stm32f4_uart_tx_backend.h"

#include <limits.h>
#include <string.h>

#include "stm32f4xx_hal.h"

static bool stm32f4_uart_tx_try_write(
    RadioTx *self,
    const uint8_t *data,
    size_t length
);

static bool stm32f4_uart_tx_is_busy(
    RadioTx *self
);

bool stm32f4_uart_tx_backend_init(
    Stm32f4UartTxBackend *backend,
    RadioTx *tx,
    UART_HandleTypeDef *uart
)
{
    if (backend == NULL || tx == NULL || uart == NULL) {
        return false;
    }

    memset(backend, 0, sizeof(*backend));
    memset(tx, 0, sizeof(*tx));

    backend->uart = uart;

    tx->context = backend;
    tx->try_write = stm32f4_uart_tx_try_write;
    tx->is_busy = stm32f4_uart_tx_is_busy;

    return true;
}

void stm32f4_uart_tx_backend_on_tx_complete(
    Stm32f4UartTxBackend *backend,
    UART_HandleTypeDef *uart
)
{
    if (backend == NULL || uart != backend->uart || !backend->busy) {
        return;
    }

    backend->busy = false;
    backend->tx_length = 0U;
    backend->stats.completed_frames++;
}

void stm32f4_uart_tx_backend_on_error(
    Stm32f4UartTxBackend *backend,
    UART_HandleTypeDef *uart
)
{
    if (backend == NULL || uart != backend->uart) {
        return;
    }

    backend->stats.uart_errors++;
    backend->busy = false;
    backend->tx_length = 0U;
}

bool stm32f4_uart_tx_backend_is_busy(
    const Stm32f4UartTxBackend *backend
)
{
    return backend != NULL && backend->busy;
}

void stm32f4_uart_tx_backend_get_stats(
    const Stm32f4UartTxBackend *backend,
    Stm32f4UartTxBackendStats *out_stats
)
{
    if (backend == NULL || out_stats == NULL) {
        return;
    }

    *out_stats = backend->stats;
}

static bool stm32f4_uart_tx_try_write(
    RadioTx *self,
    const uint8_t *data,
    size_t length
)
{
    if (self == NULL || self->context == NULL) {
        return false;
    }

    Stm32f4UartTxBackend *backend =
        (Stm32f4UartTxBackend *)self->context;

    backend->stats.start_attempts++;

    if (data == NULL || length == 0U ||
        length > sizeof(backend->tx_buffer) ||
        length > UINT16_MAX) {
        backend->stats.invalid_requests++;
        return false;
    }

    if (backend->busy) {
        backend->stats.busy_rejections++;
        return false;
    }

    memcpy(backend->tx_buffer, data, length);
    backend->tx_length = (uint16_t)length;
    backend->busy = true;

    const HAL_StatusTypeDef status = HAL_UART_Transmit_IT(
        backend->uart,
        backend->tx_buffer,
        backend->tx_length
    );

    if (status != HAL_OK) {
        backend->busy = false;
        backend->tx_length = 0U;
        backend->stats.hal_errors++;
        return false;
    }

    backend->stats.started_frames++;
    backend->stats.bytes_started += (uint32_t)length;
    return true;
}

static bool stm32f4_uart_tx_is_busy(
    RadioTx *self
)
{
    if (self == NULL || self->context == NULL) {
        return false;
    }

    const Stm32f4UartTxBackend *backend =
        (const Stm32f4UartTxBackend *)self->context;

    return backend->busy;
}
