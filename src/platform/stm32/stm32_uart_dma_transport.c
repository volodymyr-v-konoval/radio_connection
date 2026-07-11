#include "stm32_uart_dma_transport.h"

#include <limits.h>
#include <string.h>

static bool stm32_uart_dma_transport_read_byte(
    RadioTransport *self,
    uint8_t *out_byte
);

static size_t stm32_uart_dma_transport_read(
    RadioTransport *self,
    uint8_t *buffer,
    size_t max_len
);

static uint32_t stm32_uart_dma_transport_sync_available(
    Stm32UartDmaTransportContext *context
);

bool stm32_uart_dma_transport_init(
    RadioTransport *transport,
    Stm32UartDmaTransportContext *context,
    const Stm32UartDmaTransportConfig *config
)
{
    if (transport == NULL ||
        context == NULL ||
        config == NULL ||
        config->rx_buffer == NULL ||
        config->rx_buffer_size == 0U ||
        config->rx_buffer_size > UINT32_MAX ||
        config->get_produced_count == NULL) {
        return false;
    }

    memset(context, 0, sizeof(*context));

    context->rx_buffer = config->rx_buffer;
    context->rx_buffer_size = (uint32_t)config->rx_buffer_size;
    context->backend_context = config->backend_context;
    context->get_produced_count = config->get_produced_count;
    context->consumed_count =
        config->get_produced_count(config->backend_context);

    transport->context = context;
    transport->read_byte = stm32_uart_dma_transport_read_byte;
    transport->read = stm32_uart_dma_transport_read;

    return true;
}

void stm32_uart_dma_transport_reset(
    Stm32UartDmaTransportContext *context
)
{
    if (context == NULL || context->get_produced_count == NULL) {
        return;
    }

    context->consumed_count =
        context->get_produced_count(context->backend_context);
    memset(&context->stats, 0, sizeof(context->stats));
}

size_t stm32_uart_dma_transport_available(
    Stm32UartDmaTransportContext *context
)
{
    return (size_t)stm32_uart_dma_transport_sync_available(context);
}

void stm32_uart_dma_transport_get_stats(
    const Stm32UartDmaTransportContext *context,
    Stm32UartDmaTransportStats *out_stats
)
{
    if (context == NULL || out_stats == NULL) {
        return;
    }

    *out_stats = context->stats;
}

static uint32_t stm32_uart_dma_transport_sync_available(
    Stm32UartDmaTransportContext *context
)
{
    if (context == NULL ||
        context->rx_buffer == NULL ||
        context->rx_buffer_size == 0U ||
        context->get_produced_count == NULL) {
        return 0U;
    }

    const uint32_t produced_count =
        context->get_produced_count(context->backend_context);

    uint32_t available = produced_count - context->consumed_count;

    if (available > context->rx_buffer_size) {
        const uint32_t dropped = available - context->rx_buffer_size;

        context->consumed_count = produced_count - context->rx_buffer_size;
        context->stats.overflow_events++;
        context->stats.dropped_bytes += dropped;
        available = context->rx_buffer_size;
    }

    return available;
}

static bool stm32_uart_dma_transport_read_byte(
    RadioTransport *self,
    uint8_t *out_byte
)
{
    if (self == NULL || self->context == NULL || out_byte == NULL) {
        return false;
    }

    Stm32UartDmaTransportContext *context =
        (Stm32UartDmaTransportContext *)self->context;

    if (stm32_uart_dma_transport_sync_available(context) == 0U) {
        return false;
    }

    const uint32_t read_index =
        context->consumed_count % context->rx_buffer_size;

    *out_byte = context->rx_buffer[read_index];
    context->consumed_count++;
    context->stats.bytes_read++;

    return true;
}

static size_t stm32_uart_dma_transport_read(
    RadioTransport *self,
    uint8_t *buffer,
    size_t max_len
)
{
    if (self == NULL ||
        self->context == NULL ||
        buffer == NULL ||
        max_len == 0U) {
        return 0U;
    }

    Stm32UartDmaTransportContext *context =
        (Stm32UartDmaTransportContext *)self->context;

    const uint32_t available =
        stm32_uart_dma_transport_sync_available(context);

    size_t to_read = (size_t)available;

    if (to_read > max_len) {
        to_read = max_len;
    }

    for (size_t i = 0U; i < to_read; i++) {
        const uint32_t read_index =
            context->consumed_count % context->rx_buffer_size;

        buffer[i] = context->rx_buffer[read_index];
        context->consumed_count++;
    }

    context->stats.bytes_read += (uint32_t)to_read;
    return to_read;
}
