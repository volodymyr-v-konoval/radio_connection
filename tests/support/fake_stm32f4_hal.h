#ifndef FAKE_STM32F4_HAL_H
#define FAKE_STM32F4_HAL_H

#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define FAKE_STM32F4_HAL_TX_CAPTURE_SIZE 1024U
#define FAKE_STM32F4_HAL_SPI_CAPTURE_SIZE 2048U
#define FAKE_STM32F4_HAL_GPIO_WRITE_CAPACITY 32U

typedef struct
{
    GPIO_TypeDef *gpio;
    uint16_t pin;
    GPIO_PinState state;
} FakeStm32f4GpioWrite;

typedef struct
{
    HAL_StatusTypeDef receive_status;
    HAL_StatusTypeDef stop_status;
    HAL_StatusTypeDef transmit_status;
    HAL_StatusTypeDef transmit_it_status;
    HAL_StatusTypeDef spi_status;

    uint32_t tick_ms;
    uint32_t receive_calls;
    uint32_t stop_calls;
    uint32_t transmit_calls;
    uint32_t transmit_it_calls;
    uint32_t spi_calls;
    uint32_t gpio_write_calls;
    uint32_t gpio_read_calls;
    uint32_t delay_calls;
    uint32_t delay_total_ms;
    uint32_t barrier_calls;

    UART_HandleTypeDef *last_receive_uart;
    uint8_t *last_receive_buffer;
    uint16_t last_receive_size;

    UART_HandleTypeDef *last_transmit_it_uart;
    uint16_t last_transmit_it_size;
    uint8_t tx_it_capture[FAKE_STM32F4_HAL_TX_CAPTURE_SIZE];
    size_t tx_it_capture_length;

    uint8_t tx_capture[FAKE_STM32F4_HAL_TX_CAPTURE_SIZE];
    size_t tx_capture_length;

    SPI_HandleTypeDef *last_spi;
    uint16_t last_spi_size;
    uint32_t last_spi_timeout;
    uint8_t spi_tx_capture[FAKE_STM32F4_HAL_SPI_CAPTURE_SIZE];
    size_t spi_tx_capture_length;
    uint8_t spi_rx_script[FAKE_STM32F4_HAL_SPI_CAPTURE_SIZE];
    size_t spi_rx_script_length;
    size_t spi_rx_script_offset;

    FakeStm32f4GpioWrite gpio_writes[
        FAKE_STM32F4_HAL_GPIO_WRITE_CAPACITY
    ];
    size_t gpio_write_count;

    GPIO_TypeDef *last_gpio_read_port;
    uint16_t last_gpio_read_pin;
    GPIO_PinState gpio_read_state;
} FakeStm32f4HalState;

void fake_stm32f4_hal_reset(void);
FakeStm32f4HalState *fake_stm32f4_hal_state(void);

void fake_stm32f4_hal_set_spi_rx_script(
    const uint8_t *data,
    size_t length
);

#endif /* FAKE_STM32F4_HAL_H */
