#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_stm32f4_hal.h"
#include "stm32f4_time_backend.h"
#include "stm32f4_uart_dma_backend.h"
#include "stm32f4_uart_logger_backend.h"

#define TEST_ASSERT(condition, message)       \
    do {                                      \
        if (!(condition)) {                   \
            printf("FAIL: %s\n", message);   \
            return false;                     \
        }                                     \
    } while (0)

static void init_uart(
    UART_HandleTypeDef *uart,
    DMA_HandleTypeDef *dma,
    uint32_t dma_mode
)
{
    memset(uart, 0, sizeof(*uart));
    memset(dma, 0, sizeof(*dma));
    dma->Init.Mode = dma_mode;
    uart->hdmarx = dma;
}

static bool test_backend_requires_circular_dma(void)
{
    fake_stm32f4_hal_reset();

    UART_HandleTypeDef uart;
    DMA_HandleTypeDef dma;
    uint8_t buffer[64];
    Stm32f4UartDmaBackend backend;

    init_uart(&uart, &dma, DMA_NORMAL);

    const Stm32f4UartDmaBackendConfig config = {
        .uart = &uart,
        .rx_buffer = buffer,
        .rx_buffer_size = sizeof(buffer),
        .disable_half_transfer_irq = false
    };

    TEST_ASSERT(
        stm32f4_uart_dma_backend_init(&backend, &config),
        "Backend init should accept structural configuration"
    );
    TEST_ASSERT(
        !stm32f4_uart_dma_backend_start(&backend),
        "Backend start should reject non-circular DMA"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->receive_calls == 0U,
        "HAL receive must not start with invalid DMA mode"
    );

    return true;
}

static bool test_backend_tracks_dma_positions(void)
{
    fake_stm32f4_hal_reset();

    UART_HandleTypeDef uart;
    DMA_HandleTypeDef dma;
    uint8_t buffer[64];
    Stm32f4UartDmaBackend backend;

    init_uart(&uart, &dma, DMA_CIRCULAR);

    const Stm32f4UartDmaBackendConfig config = {
        .uart = &uart,
        .rx_buffer = buffer,
        .rx_buffer_size = sizeof(buffer),
        .disable_half_transfer_irq = false
    };

    TEST_ASSERT(
        stm32f4_uart_dma_backend_init(&backend, &config) &&
        stm32f4_uart_dma_backend_start(&backend),
        "Circular DMA backend should start"
    );

    stm32f4_uart_dma_backend_on_rx_event(&backend, &uart, 10U);
    stm32f4_uart_dma_backend_on_rx_event(&backend, &uart, 32U);
    stm32f4_uart_dma_backend_on_rx_event(&backend, &uart, 64U);
    stm32f4_uart_dma_backend_on_rx_event(&backend, &uart, 7U);

    TEST_ASSERT(
        stm32f4_uart_dma_backend_get_produced_count(&backend) == 71U,
        "Produced count should accumulate across DMA wrap"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->barrier_calls == 5U,
        "Producer publication and consumer acquisition should use barriers"
    );

    stm32f4_uart_dma_backend_on_rx_event(&backend, &uart, 7U);

    Stm32f4UartDmaBackendStats stats;
    stm32f4_uart_dma_backend_get_stats(&backend, &stats);

    TEST_ASSERT(stats.rx_events == 5U, "Five RX events should be counted");
    TEST_ASSERT(
        stats.duplicate_events == 1U,
        "Repeated DMA position should be counted as duplicate"
    );

    return true;
}

static bool test_backend_defers_error_recovery(void)
{
    fake_stm32f4_hal_reset();

    UART_HandleTypeDef uart;
    DMA_HandleTypeDef dma;
    uint8_t buffer[64];
    Stm32f4UartDmaBackend backend;

    init_uart(&uart, &dma, DMA_CIRCULAR);
    uart.ErrorCode = 0x1234U;

    const Stm32f4UartDmaBackendConfig config = {
        .uart = &uart,
        .rx_buffer = buffer,
        .rx_buffer_size = sizeof(buffer),
        .disable_half_transfer_irq = false
    };

    TEST_ASSERT(
        stm32f4_uart_dma_backend_init(&backend, &config) &&
        stm32f4_uart_dma_backend_start(&backend),
        "Backend should start before recovery test"
    );

    stm32f4_uart_dma_backend_on_error(&backend, &uart);

    TEST_ASSERT(
        fake_stm32f4_hal_state()->stop_calls == 0U,
        "ISR error callback must not restart DMA"
    );
    TEST_ASSERT(
        stm32f4_uart_dma_backend_process(&backend),
        "Main-loop processing should recover DMA"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->stop_calls == 1U,
        "Deferred recovery should stop DMA once"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->receive_calls == 2U,
        "Deferred recovery should restart reception"
    );

    Stm32f4UartDmaBackendStats stats;
    stm32f4_uart_dma_backend_get_stats(&backend, &stats);

    TEST_ASSERT(stats.uart_error_events == 1U, "UART error should be counted");
    TEST_ASSERT(stats.last_uart_error == 0x1234U, "Error code should be saved");
    TEST_ASSERT(stats.recovery_successes == 1U, "Recovery should succeed once");

    return true;
}

static bool test_f4_time_and_uart_logger_backends(void)
{
    fake_stm32f4_hal_reset();
    fake_stm32f4_hal_state()->tick_ms = 12345U;

    TEST_ASSERT(
        stm32f4_time_backend_now_ms(NULL) == 12345U,
        "F4 time backend should expose HAL tick"
    );

    UART_HandleTypeDef uart;
    DMA_HandleTypeDef dma;
    init_uart(&uart, &dma, DMA_CIRCULAR);

    Stm32f4UartLoggerBackend logger;
    TEST_ASSERT(
        stm32f4_uart_logger_backend_init(&logger, &uart, 20U),
        "UART logger backend init should succeed"
    );

    const uint8_t message[] = "radio ready\n";
    stm32f4_uart_logger_backend_write(
        &logger,
        message,
        sizeof(message) - 1U
    );

    TEST_ASSERT(logger.write_calls == 1U, "Logger should perform one HAL write");
    TEST_ASSERT(logger.write_errors == 0U, "Logger should have no error");
    TEST_ASSERT(
        fake_stm32f4_hal_state()->tx_capture_length == sizeof(message) - 1U,
        "Logger output should be captured"
    );
    TEST_ASSERT(
        memcmp(
            fake_stm32f4_hal_state()->tx_capture,
            message,
            sizeof(message) - 1U) == 0,
        "Logger should preserve output bytes"
    );

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_backend_requires_circular_dma()) {
        failed++;
    }
    if (!test_backend_tracks_dma_positions()) {
        failed++;
    }
    if (!test_backend_defers_error_recovery()) {
        failed++;
    }
    if (!test_f4_time_and_uart_logger_backends()) {
        failed++;
    }

    if (failed == 0) {
        printf("All STM32F4 backend tests passed (4 tests)\n");
        return 0;
    }

    printf("%d STM32F4 backend test(s) failed\n", failed);
    return 1;
}
