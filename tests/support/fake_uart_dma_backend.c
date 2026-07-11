#include "fake_uart_dma_backend.h"

#include <limits.h>
#include <string.h>

bool fake_uart_dma_backend_init(
    FakeUartDmaBackend *backend,
    uint8_t *buffer,
    size_t capacity
)
{
    if (backend == NULL ||
        buffer == NULL ||
        capacity == 0U ||
        capacity > UINT32_MAX) {
        return false;
    }

    memset(buffer, 0, capacity);
    backend->buffer = buffer;
    backend->capacity = (uint32_t)capacity;
    backend->produced_count = 0U;

    return true;
}

void fake_uart_dma_backend_write(
    FakeUartDmaBackend *backend,
    const uint8_t *data,
    size_t length
)
{
    if (backend == NULL ||
        backend->buffer == NULL ||
        backend->capacity == 0U ||
        data == NULL) {
        return;
    }

    for (size_t i = 0U; i < length; i++) {
        const uint32_t write_index =
            backend->produced_count % backend->capacity;

        backend->buffer[write_index] = data[i];
        backend->produced_count++;
    }
}

uint32_t fake_uart_dma_backend_get_produced_count(
    void *backend_context
)
{
    if (backend_context == NULL) {
        return 0U;
    }

    const FakeUartDmaBackend *backend =
        (const FakeUartDmaBackend *)backend_context;

    return backend->produced_count;
}
