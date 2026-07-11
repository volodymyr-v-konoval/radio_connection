#ifndef STM32F4_UART_DMA_BACKEND_H
#define STM32F4_UART_DMA_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

typedef struct
{
    UART_HandleTypeDef *uart;
    uint8_t *rx_buffer;
    size_t rx_buffer_size;
    bool disable_half_transfer_irq;
} Stm32f4UartDmaBackendConfig;

typedef struct
{
    uint32_t rx_events;
    uint32_t duplicate_events;
    uint32_t invalid_events;
    uint32_t uart_error_events;
    uint32_t recovery_attempts;
    uint32_t recovery_successes;
    uint32_t recovery_failures;
    uint32_t last_uart_error;
} Stm32f4UartDmaBackendStats;

typedef struct
{
    UART_HandleTypeDef *uart;
    uint8_t *rx_buffer;
    uint32_t rx_buffer_size;
    bool disable_half_transfer_irq;

    volatile uint32_t produced_count;
    volatile bool recovery_requested;

    uint32_t last_dma_position;
    bool started;

    Stm32f4UartDmaBackendStats stats;
} Stm32f4UartDmaBackend;

bool stm32f4_uart_dma_backend_init(
    Stm32f4UartDmaBackend *backend,
    const Stm32f4UartDmaBackendConfig *config
);

bool stm32f4_uart_dma_backend_start(
    Stm32f4UartDmaBackend *backend
);

bool stm32f4_uart_dma_backend_stop(
    Stm32f4UartDmaBackend *backend
);

/*
 * Call from the application/main-loop context.
 * Returns true only when a deferred UART recovery succeeded.
 */
bool stm32f4_uart_dma_backend_process(
    Stm32f4UartDmaBackend *backend
);

/*
 * Forward HAL_UARTEx_RxEventCallback() here.
 * This function is ISR-safe and performs no parsing.
 */
void stm32f4_uart_dma_backend_on_rx_event(
    Stm32f4UartDmaBackend *backend,
    UART_HandleTypeDef *uart,
    uint16_t dma_position
);

/*
 * Forward HAL_UART_ErrorCallback() here.
 * Recovery is deferred to stm32f4_uart_dma_backend_process().
 */
void stm32f4_uart_dma_backend_on_error(
    Stm32f4UartDmaBackend *backend,
    UART_HandleTypeDef *uart
);

uint32_t stm32f4_uart_dma_backend_get_produced_count(
    void *backend_context
);

bool stm32f4_uart_dma_backend_is_started(
    const Stm32f4UartDmaBackend *backend
);

void stm32f4_uart_dma_backend_get_stats(
    const Stm32f4UartDmaBackend *backend,
    Stm32f4UartDmaBackendStats *out_stats
);

#endif /* STM32F4_UART_DMA_BACKEND_H */
