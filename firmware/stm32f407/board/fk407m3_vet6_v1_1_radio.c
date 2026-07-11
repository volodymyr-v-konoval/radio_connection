#include "fk407m3_vet6_v1_1_radio.h"

#include <stddef.h>

bool fk407m3_vet6_v1_1_validate_receiver_uart(
    const UART_HandleTypeDef *uart
)
{
    return uart != NULL &&
        uart->Instance == FK407M3_RADIO_UART_INSTANCE &&
        uart->Init.BaudRate == FK407M3_RADIO_UART_BAUDRATE &&
        uart->Init.WordLength == UART_WORDLENGTH_8B &&
        uart->Init.StopBits == UART_STOPBITS_1 &&
        uart->Init.Parity == UART_PARITY_NONE &&
        (uart->Init.Mode & UART_MODE_RX) != 0U &&
        uart->hdmarx != NULL &&
        uart->hdmarx->Instance == FK407M3_RADIO_DMA_INSTANCE &&
        uart->hdmarx->Init.Channel == FK407M3_RADIO_DMA_CHANNEL &&
        uart->hdmarx->Init.Direction == DMA_PERIPH_TO_MEMORY &&
        uart->hdmarx->Init.Mode == DMA_CIRCULAR;
}

bool fk407m3_vet6_v1_1_validate_debug_uart(
    const UART_HandleTypeDef *uart
)
{
    return uart != NULL &&
        uart->Instance == FK407M3_DEBUG_UART_INSTANCE &&
        uart->Init.BaudRate == FK407M3_DEBUG_UART_BAUDRATE &&
        uart->Init.WordLength == UART_WORDLENGTH_8B &&
        uart->Init.StopBits == UART_STOPBITS_1 &&
        uart->Init.Parity == UART_PARITY_NONE &&
        (uart->Init.Mode & UART_MODE_TX) != 0U;
}
