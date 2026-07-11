#ifndef FAKE_STM32F4_HAL_H
#define FAKE_STM32F4_HAL_H

#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define FAKE_STM32F4_HAL_TX_CAPTURE_SIZE 1024U

typedef struct
{
    HAL_StatusTypeDef receive_status;
    HAL_StatusTypeDef stop_status;
    HAL_StatusTypeDef transmit_status;

    uint32_t tick_ms;
    uint32_t receive_calls;
    uint32_t stop_calls;
    uint32_t transmit_calls;
    uint32_t barrier_calls;

    UART_HandleTypeDef *last_receive_uart;
    uint8_t *last_receive_buffer;
    uint16_t last_receive_size;

    uint8_t tx_capture[FAKE_STM32F4_HAL_TX_CAPTURE_SIZE];
    size_t tx_capture_length;
} FakeStm32f4HalState;

void fake_stm32f4_hal_reset(void);
FakeStm32f4HalState *fake_stm32f4_hal_state(void);

#endif /* FAKE_STM32F4_HAL_H */
