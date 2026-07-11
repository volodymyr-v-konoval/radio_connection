#include "stm32f4_uart_dma_backend.h"

#include <limits.h>
#include <string.h>

#include "stm32f4xx_hal.h"

static bool stm32f4_uart_dma_backend_configuration_valid(
    const Stm32f4UartDmaBackend *backend
);

static bool stm32f4_uart_dma_backend_begin_receive(
    Stm32f4UartDmaBackend *backend
);

bool stm32f4_uart_dma_backend_init(
    Stm32f4UartDmaBackend *backend,
    const Stm32f4UartDmaBackendConfig *config
)
{
    if (backend == NULL ||
        config == NULL ||
        config->uart == NULL ||
        config->rx_buffer == NULL ||
        config->rx_buffer_size == 0U ||
        config->rx_buffer_size > UINT16_MAX) {
        return false;
    }

    memset(backend, 0, sizeof(*backend));

    backend->uart = config->uart;
    backend->rx_buffer = config->rx_buffer;
    backend->rx_buffer_size = (uint32_t)config->rx_buffer_size;
    backend->disable_half_transfer_irq =
        config->disable_half_transfer_irq;

    return true;
}

bool stm32f4_uart_dma_backend_start(
    Stm32f4UartDmaBackend *backend
)
{
    if (!stm32f4_uart_dma_backend_configuration_valid(backend)) {
        return false;
    }

    backend->last_dma_position = 0U;
    backend->recovery_requested = false;

    return stm32f4_uart_dma_backend_begin_receive(backend);
}

bool stm32f4_uart_dma_backend_stop(
    Stm32f4UartDmaBackend *backend
)
{
    if (backend == NULL || backend->uart == NULL) {
        return false;
    }

    const HAL_StatusTypeDef status = HAL_UART_DMAStop(backend->uart);
    backend->started = false;

    return status == HAL_OK;
}

bool stm32f4_uart_dma_backend_process(
    Stm32f4UartDmaBackend *backend
)
{
    if (backend == NULL || !backend->recovery_requested) {
        return false;
    }

    backend->stats.recovery_attempts++;

    (void)HAL_UART_DMAStop(backend->uart);
    backend->started = false;
    backend->last_dma_position = 0U;

    if (stm32f4_uart_dma_backend_begin_receive(backend)) {
        backend->recovery_requested = false;
        backend->stats.recovery_successes++;
        return true;
    }

    backend->stats.recovery_failures++;
    return false;
}

void stm32f4_uart_dma_backend_on_rx_event(
    Stm32f4UartDmaBackend *backend,
    UART_HandleTypeDef *uart,
    uint16_t dma_position
)
{
    if (backend == NULL ||
        !backend->started ||
        uart != backend->uart ||
        dma_position == 0U ||
        (uint32_t)dma_position > backend->rx_buffer_size) {
        if (backend != NULL) {
            backend->stats.invalid_events++;
        }
        return;
    }

    uint32_t current_position = (uint32_t)dma_position;

    if (current_position == backend->rx_buffer_size) {
        current_position = 0U;
    }

    uint32_t produced_delta = 0U;

    if (current_position >= backend->last_dma_position) {
        produced_delta = current_position - backend->last_dma_position;
    } else {
        produced_delta =
            (backend->rx_buffer_size - backend->last_dma_position) +
            current_position;
    }

    backend->stats.rx_events++;

    if (produced_delta == 0U) {
        backend->stats.duplicate_events++;
        return;
    }

    /* Publish the DMA-written bytes before advancing the producer count. */
    __DMB();
    backend->produced_count += produced_delta;
    backend->last_dma_position = current_position;
}

void stm32f4_uart_dma_backend_on_error(
    Stm32f4UartDmaBackend *backend,
    UART_HandleTypeDef *uart
)
{
    if (backend == NULL || uart != backend->uart) {
        if (backend != NULL) {
            backend->stats.invalid_events++;
        }
        return;
    }

    backend->stats.uart_error_events++;
    backend->stats.last_uart_error = uart->ErrorCode;
    backend->recovery_requested = true;
    backend->started = false;
}

uint32_t stm32f4_uart_dma_backend_get_produced_count(
    void *backend_context
)
{
    const Stm32f4UartDmaBackend *backend =
        (const Stm32f4UartDmaBackend *)backend_context;

    if (backend == NULL) {
        return 0U;
    }

    const uint32_t produced_count = backend->produced_count;

    /* Acquire DMA buffer writes published by the ISR-side producer. */
    __DMB();
    return produced_count;
}

bool stm32f4_uart_dma_backend_is_started(
    const Stm32f4UartDmaBackend *backend
)
{
    return backend != NULL && backend->started;
}

void stm32f4_uart_dma_backend_get_stats(
    const Stm32f4UartDmaBackend *backend,
    Stm32f4UartDmaBackendStats *out_stats
)
{
    if (backend == NULL || out_stats == NULL) {
        return;
    }

    *out_stats = backend->stats;
}

static bool stm32f4_uart_dma_backend_configuration_valid(
    const Stm32f4UartDmaBackend *backend
)
{
    return backend != NULL &&
        backend->uart != NULL &&
        backend->uart->hdmarx != NULL &&
        backend->uart->hdmarx->Init.Mode == DMA_CIRCULAR &&
        backend->rx_buffer != NULL &&
        backend->rx_buffer_size > 0U &&
        backend->rx_buffer_size <= UINT16_MAX;
}

static bool stm32f4_uart_dma_backend_begin_receive(
    Stm32f4UartDmaBackend *backend
)
{
    const HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(
        backend->uart,
        backend->rx_buffer,
        (uint16_t)backend->rx_buffer_size
    );

    if (status != HAL_OK) {
        backend->started = false;
        return false;
    }

    if (backend->disable_half_transfer_irq) {
        __HAL_DMA_DISABLE_IT(backend->uart->hdmarx, DMA_IT_HT);
    }

    backend->started = true;
    return true;
}
