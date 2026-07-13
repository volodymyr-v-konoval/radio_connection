#include "fake_stm32f4_hal.h"

#include <string.h>

static FakeStm32f4HalState g_fake_hal;

void fake_stm32f4_hal_reset(void)
{
    memset(&g_fake_hal, 0, sizeof(g_fake_hal));
    g_fake_hal.receive_status = HAL_OK;
    g_fake_hal.stop_status = HAL_OK;
    g_fake_hal.transmit_status = HAL_OK;
    g_fake_hal.transmit_it_status = HAL_OK;
}

FakeStm32f4HalState *fake_stm32f4_hal_state(void)
{
    return &g_fake_hal;
}

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(
    UART_HandleTypeDef *uart,
    uint8_t *buffer,
    uint16_t size
)
{
    g_fake_hal.receive_calls++;
    g_fake_hal.last_receive_uart = uart;
    g_fake_hal.last_receive_buffer = buffer;
    g_fake_hal.last_receive_size = size;
    return g_fake_hal.receive_status;
}

HAL_StatusTypeDef HAL_UART_DMAStop(
    UART_HandleTypeDef *uart
)
{
    (void)uart;
    g_fake_hal.stop_calls++;
    return g_fake_hal.stop_status;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(
    UART_HandleTypeDef *uart,
    uint8_t *data,
    uint16_t size
)
{
    g_fake_hal.transmit_it_calls++;
    g_fake_hal.last_transmit_it_uart = uart;
    g_fake_hal.last_transmit_it_size = size;

    size_t to_copy = size;
    if (to_copy > sizeof(g_fake_hal.tx_it_capture)) {
        to_copy = sizeof(g_fake_hal.tx_it_capture);
    }

    if (data != NULL && to_copy > 0U) {
        memcpy(g_fake_hal.tx_it_capture, data, to_copy);
    }
    g_fake_hal.tx_it_capture_length = to_copy;

    return g_fake_hal.transmit_it_status;
}

HAL_StatusTypeDef HAL_UART_Transmit(
    UART_HandleTypeDef *uart,
    uint8_t *data,
    uint16_t size,
    uint32_t timeout
)
{
    (void)uart;
    (void)timeout;

    g_fake_hal.transmit_calls++;

    if (g_fake_hal.transmit_status != HAL_OK) {
        return g_fake_hal.transmit_status;
    }

    size_t available =
        sizeof(g_fake_hal.tx_capture) - g_fake_hal.tx_capture_length;
    size_t to_copy = size;

    if (to_copy > available) {
        to_copy = available;
    }

    memcpy(
        &g_fake_hal.tx_capture[g_fake_hal.tx_capture_length],
        data,
        to_copy
    );
    g_fake_hal.tx_capture_length += to_copy;

    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    return g_fake_hal.tick_ms;
}

void fake_stm32f4_hal_data_memory_barrier(void)
{
    g_fake_hal.barrier_calls++;
}
