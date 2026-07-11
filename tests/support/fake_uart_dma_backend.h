#ifndef FAKE_UART_DMA_BACKEND_H
#define FAKE_UART_DMA_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t produced_count;
} FakeUartDmaBackend;

bool fake_uart_dma_backend_init(
    FakeUartDmaBackend *backend,
    uint8_t *buffer,
    size_t capacity
);

void fake_uart_dma_backend_write(
    FakeUartDmaBackend *backend,
    const uint8_t *data,
    size_t length
);

uint32_t fake_uart_dma_backend_get_produced_count(
    void *backend_context
);

#endif /* FAKE_UART_DMA_BACKEND_H */
