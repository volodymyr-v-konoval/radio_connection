#ifndef FK407M3_VET6_V1_1_RADIO_H
#define FK407M3_VET6_V1_1_RADIO_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define FK407M3_RADIO_UART_INSTANCE        USART2
#define FK407M3_RADIO_UART_BAUDRATE        420000U
#define FK407M3_RADIO_UART_RX_PORT         GPIOA
#define FK407M3_RADIO_UART_RX_PIN          GPIO_PIN_3
#define FK407M3_RADIO_UART_TX_PORT         GPIOA
#define FK407M3_RADIO_UART_TX_PIN          GPIO_PIN_2
#define FK407M3_RADIO_UART_GPIO_AF         GPIO_AF7_USART2

#define FK407M3_RADIO_DMA_INSTANCE         DMA1_Stream5
#define FK407M3_RADIO_DMA_CHANNEL          DMA_CHANNEL_4
#define FK407M3_RADIO_DMA_IRQ              DMA1_Stream5_IRQn

#define FK407M3_DEBUG_UART_INSTANCE        USART1
#define FK407M3_DEBUG_UART_BAUDRATE        115200U
#define FK407M3_DEBUG_UART_TX_PORT         GPIOA
#define FK407M3_DEBUG_UART_TX_PIN          GPIO_PIN_9
#define FK407M3_DEBUG_UART_RX_PORT         GPIOA
#define FK407M3_DEBUG_UART_RX_PIN          GPIO_PIN_10
#define FK407M3_DEBUG_UART_GPIO_AF         GPIO_AF7_USART1

#define FK407M3_RADIO_DMA_BUFFER_SIZE      256U
#define FK407M3_RADIO_FAILSAFE_TIMEOUT_MS  100U

bool fk407m3_vet6_v1_1_validate_receiver_uart(
    const UART_HandleTypeDef *uart
);

bool fk407m3_vet6_v1_1_validate_debug_uart(
    const UART_HandleTypeDef *uart
);

#endif /* FK407M3_VET6_V1_1_RADIO_H */
