#ifndef STM32_UART_DMA_TRANSPORT_H
#define STM32_UART_DMA_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_transport_if.h"

typedef uint32_t (*Stm32UartDmaGetProducedCountFn)(
    void *backend_context
);

typedef struct
{
    const volatile uint8_t *rx_buffer;
    size_t rx_buffer_size;

    void *backend_context;
    Stm32UartDmaGetProducedCountFn get_produced_count;
} Stm32UartDmaTransportConfig;

typedef struct
{
    uint32_t bytes_read;
    uint32_t overflow_events;
    uint32_t dropped_bytes;
} Stm32UartDmaTransportStats;

typedef struct
{
    const volatile uint8_t *rx_buffer;
    uint32_t rx_buffer_size;

    void *backend_context;
    Stm32UartDmaGetProducedCountFn get_produced_count;

    uint32_t consumed_count;
    Stm32UartDmaTransportStats stats;
} Stm32UartDmaTransportContext;

bool stm32_uart_dma_transport_init(
    RadioTransport *transport,
    Stm32UartDmaTransportContext *context,
    const Stm32UartDmaTransportConfig *config
);

void stm32_uart_dma_transport_reset(
    Stm32UartDmaTransportContext *context
);

size_t stm32_uart_dma_transport_available(
    Stm32UartDmaTransportContext *context
);

void stm32_uart_dma_transport_get_stats(
    const Stm32UartDmaTransportContext *context,
    Stm32UartDmaTransportStats *out_stats
);

#endif /* STM32_UART_DMA_TRANSPORT_H */
