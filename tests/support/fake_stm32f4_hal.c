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
    g_fake_hal.spi_status = HAL_OK;
    g_fake_hal.gpio_read_state = GPIO_PIN_RESET;
}

FakeStm32f4HalState *fake_stm32f4_hal_state(void)
{
    return &g_fake_hal;
}

void fake_stm32f4_hal_set_spi_rx_script(
    const uint8_t *data,
    size_t length
)
{
    g_fake_hal.spi_rx_script_length = 0U;
    g_fake_hal.spi_rx_script_offset = 0U;

    if (data == NULL) {
        return;
    }

    if (length > sizeof(g_fake_hal.spi_rx_script)) {
        length = sizeof(g_fake_hal.spi_rx_script);
    }

    memcpy(g_fake_hal.spi_rx_script, data, length);
    g_fake_hal.spi_rx_script_length = length;
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

HAL_StatusTypeDef HAL_SPI_TransmitReceive(
    SPI_HandleTypeDef *spi,
    uint8_t *tx_data,
    uint8_t *rx_data,
    uint16_t size,
    uint32_t timeout
)
{
    g_fake_hal.spi_calls++;
    g_fake_hal.last_spi = spi;
    g_fake_hal.last_spi_size = size;
    g_fake_hal.last_spi_timeout = timeout;

    size_t available =
        sizeof(g_fake_hal.spi_tx_capture) -
        g_fake_hal.spi_tx_capture_length;
    size_t to_copy = size;

    if (to_copy > available) {
        to_copy = available;
    }

    if (tx_data != NULL && to_copy > 0U) {
        memcpy(
            &g_fake_hal.spi_tx_capture[
                g_fake_hal.spi_tx_capture_length
            ],
            tx_data,
            to_copy
        );
    }

    g_fake_hal.spi_tx_capture_length += to_copy;

    if (g_fake_hal.spi_status != HAL_OK) {
        return g_fake_hal.spi_status;
    }

    if (rx_data != NULL) {
        for (uint16_t i = 0U; i < size; i++) {
            uint8_t value = 0U;

            if (g_fake_hal.spi_rx_script_offset <
                g_fake_hal.spi_rx_script_length) {
                value = g_fake_hal.spi_rx_script[
                    g_fake_hal.spi_rx_script_offset
                ];
                g_fake_hal.spi_rx_script_offset++;
            }

            rx_data[i] = value;
        }
    }

    return HAL_OK;
}

void HAL_GPIO_WritePin(
    GPIO_TypeDef *gpio,
    uint16_t pin,
    GPIO_PinState state
)
{
    g_fake_hal.gpio_write_calls++;

    if (g_fake_hal.gpio_write_count <
        FAKE_STM32F4_HAL_GPIO_WRITE_CAPACITY) {
        FakeStm32f4GpioWrite *write =
            &g_fake_hal.gpio_writes[g_fake_hal.gpio_write_count];

        write->gpio = gpio;
        write->pin = pin;
        write->state = state;
        g_fake_hal.gpio_write_count++;
    }
}

GPIO_PinState HAL_GPIO_ReadPin(
    GPIO_TypeDef *gpio,
    uint16_t pin
)
{
    g_fake_hal.gpio_read_calls++;
    g_fake_hal.last_gpio_read_port = gpio;
    g_fake_hal.last_gpio_read_pin = pin;
    return g_fake_hal.gpio_read_state;
}

void HAL_Delay(uint32_t delay_ms)
{
    g_fake_hal.delay_calls++;
    g_fake_hal.delay_total_ms += delay_ms;
    g_fake_hal.tick_ms += delay_ms;
}

uint32_t HAL_GetTick(void)
{
    return g_fake_hal.tick_ms;
}

void fake_stm32f4_hal_data_memory_barrier(void)
{
    g_fake_hal.barrier_calls++;
}
