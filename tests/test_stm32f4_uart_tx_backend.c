#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_stm32f4_hal.h"
#include "stm32f4_uart_tx_backend.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));   \
            return false;                        \
        }                                        \
    } while (0)

static bool test_nonblocking_transmit_lifecycle(void)
{
    fake_stm32f4_hal_reset();

    UART_HandleTypeDef uart;
    memset(&uart, 0, sizeof(uart));
    uart.Instance = USART2;

    Stm32f4UartTxBackend backend;
    RadioTx tx;

    TEST_ASSERT(
        stm32f4_uart_tx_backend_init(&backend, &tx, &uart),
        "TX backend should initialize"
    );

    const uint8_t frame[] = { 0xC8U, 0x04U, 0x0BU, 0x00U, 0xC8U, 0xEDU };

    TEST_ASSERT(
        radio_tx_try_write(&tx, frame, sizeof(frame)),
        "First asynchronous transmission should start"
    );
    TEST_ASSERT(radio_tx_is_busy(&tx), "Backend should become busy");

    FakeStm32f4HalState *state = fake_stm32f4_hal_state();
    TEST_ASSERT(state->transmit_it_calls == 1U,
        "HAL interrupt transmit should be called once");
    TEST_ASSERT(state->last_transmit_it_uart == &uart,
        "Transmit should use USART2");
    TEST_ASSERT(state->last_transmit_it_size == sizeof(frame),
        "Transmit size should match the CRSF frame");
    TEST_ASSERT(memcmp(state->tx_it_capture, frame, sizeof(frame)) == 0,
        "Backend should copy the frame before starting TX");

    TEST_ASSERT(
        !radio_tx_try_write(&tx, frame, sizeof(frame)),
        "Second write should be rejected while busy"
    );

    stm32f4_uart_tx_backend_on_tx_complete(&backend, &uart);
    TEST_ASSERT(!radio_tx_is_busy(&tx),
        "TX complete callback should release the backend");

    Stm32f4UartTxBackendStats stats;
    stm32f4_uart_tx_backend_get_stats(&backend, &stats);

    TEST_ASSERT(stats.started_frames == 1U,
        "One frame should have started");
    TEST_ASSERT(stats.completed_frames == 1U,
        "One frame should have completed");
    TEST_ASSERT(stats.busy_rejections == 1U,
        "Busy rejection should be counted");

    return true;
}

static bool test_hal_error_and_uart_error_recovery(void)
{
    fake_stm32f4_hal_reset();

    UART_HandleTypeDef uart;
    memset(&uart, 0, sizeof(uart));
    uart.Instance = USART2;

    Stm32f4UartTxBackend backend;
    RadioTx tx;
    TEST_ASSERT(stm32f4_uart_tx_backend_init(&backend, &tx, &uart),
        "TX backend should initialize");

    const uint8_t frame[] = { 1U, 2U, 3U };

    fake_stm32f4_hal_state()->transmit_it_status = HAL_ERROR;
    TEST_ASSERT(
        !radio_tx_try_write(&tx, frame, sizeof(frame)),
        "HAL start error should be returned"
    );
    TEST_ASSERT(!radio_tx_is_busy(&tx),
        "HAL start error must not leave backend busy");

    fake_stm32f4_hal_state()->transmit_it_status = HAL_OK;
    TEST_ASSERT(radio_tx_try_write(&tx, frame, sizeof(frame)),
        "Backend should recover after a HAL start error");

    stm32f4_uart_tx_backend_on_error(&backend, &uart);
    TEST_ASSERT(!radio_tx_is_busy(&tx),
        "UART error callback should release the TX backend");

    Stm32f4UartTxBackendStats stats;
    stm32f4_uart_tx_backend_get_stats(&backend, &stats);

    TEST_ASSERT(stats.hal_errors == 1U,
        "HAL start error should be counted");
    TEST_ASSERT(stats.uart_errors == 1U,
        "UART runtime error should be counted");

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_nonblocking_transmit_lifecycle()) failures++;
    if (!test_hal_error_and_uart_error_recovery()) failures++;

    if (failures != 0) {
        printf("%d STM32F4 UART TX backend test(s) failed\n", failures);
        return 1;
    }

    printf("All STM32F4 UART TX backend tests passed\n");
    return 0;
}
