#include "board_validation.h"

#include "stm32f4xx_hal.h"
#include "usart.h"

enum {
    BOARD_ERROR_CLOCKS = (1UL << 0),
    BOARD_ERROR_USART1 = (1UL << 1),
    BOARD_ERROR_USART2 = (1UL << 2),
    BOARD_ERROR_DMA    = (1UL << 3),
    BOARD_ERROR_NVIC   = (1UL << 4),
};

uint32_t board_validate_configuration(void)
{
    uint32_t errors = 0U;

    if ((HAL_RCC_GetSysClockFreq() != 168000000U) ||
        (HAL_RCC_GetHCLKFreq()     != 168000000U) ||
        (HAL_RCC_GetPCLK1Freq()    !=  42000000U) ||
        (HAL_RCC_GetPCLK2Freq()    !=  84000000U)) {
        errors |= BOARD_ERROR_CLOCKS;
    }

    if ((huart1.Instance            != USART1) ||
        (huart1.Init.BaudRate       != 115200U) ||
        (huart1.Init.WordLength     != UART_WORDLENGTH_8B) ||
        (huart1.Init.StopBits       != UART_STOPBITS_1) ||
        (huart1.Init.Parity         != UART_PARITY_NONE) ||
        (huart1.Init.Mode           != UART_MODE_TX_RX) ||
        (huart1.Init.HwFlowCtl      != UART_HWCONTROL_NONE) ||
        (huart1.Init.OverSampling   != UART_OVERSAMPLING_16)) {
        errors |= BOARD_ERROR_USART1;
    }

    if ((huart2.Instance            != USART2) ||
        (huart2.Init.BaudRate       != 420000U) ||
        (huart2.Init.WordLength     != UART_WORDLENGTH_8B) ||
        (huart2.Init.StopBits       != UART_STOPBITS_1) ||
        (huart2.Init.Parity         != UART_PARITY_NONE) ||
        (huart2.Init.Mode           != UART_MODE_TX_RX) ||
        (huart2.Init.HwFlowCtl      != UART_HWCONTROL_NONE) ||
        (huart2.Init.OverSampling   != UART_OVERSAMPLING_16)) {
        errors |= BOARD_ERROR_USART2;
    }

    if (huart2.hdmarx == NULL) {
        errors |= BOARD_ERROR_DMA;
    } else {
        const DMA_HandleTypeDef *dma = huart2.hdmarx;

        if ((dma->Instance                     != DMA1_Stream5) ||
            (dma->Init.Channel                 != DMA_CHANNEL_4) ||
            (dma->Init.Direction               != DMA_PERIPH_TO_MEMORY) ||
            (dma->Init.PeriphInc               != DMA_PINC_DISABLE) ||
            (dma->Init.MemInc                  != DMA_MINC_ENABLE) ||
            (dma->Init.PeriphDataAlignment     != DMA_PDATAALIGN_BYTE) ||
            (dma->Init.MemDataAlignment        != DMA_MDATAALIGN_BYTE) ||
            (dma->Init.Mode                    != DMA_CIRCULAR) ||
            (dma->Init.Priority                != DMA_PRIORITY_HIGH)) {
            errors |= BOARD_ERROR_DMA;
        }
    }

    if ((NVIC_GetEnableIRQ(DMA1_Stream5_IRQn) == 0U) ||
        (NVIC_GetEnableIRQ(USART2_IRQn)       == 0U)) {
        errors |= BOARD_ERROR_NVIC;
    }

    return errors;
}
