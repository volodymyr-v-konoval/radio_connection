#ifndef TEST_STUB_STM32F4XX_HAL_H
#define TEST_STUB_STM32F4XX_HAL_H

#include <stdint.h>

typedef enum
{
    HAL_OK = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

#define DMA_NORMAL             0x00000000U
#define DMA_CIRCULAR           0x00000001U
#define DMA_PERIPH_TO_MEMORY   0x00000000U
#define DMA_MEMORY_TO_PERIPH   0x00000001U
#define DMA_CHANNEL_4          0x00000004U
#define DMA_IT_HT              0x00000010U

#define UART_WORDLENGTH_8B     0x00000000U
#define UART_STOPBITS_1        0x00000000U
#define UART_PARITY_NONE       0x00000000U
#define UART_MODE_RX           0x00000004U
#define UART_MODE_TX           0x00000008U
#define UART_MODE_TX_RX        (UART_MODE_TX | UART_MODE_RX)

#define GPIO_PIN_2             0x0004U
#define GPIO_PIN_3             0x0008U
#define GPIO_PIN_9             0x0200U
#define GPIO_PIN_10            0x0400U
#define GPIO_AF7_USART1        7U
#define GPIO_AF7_USART2        7U

#define USART1                 ((void *)0x40011000UL)
#define USART2                 ((void *)0x40004400UL)
#define GPIOA                  ((void *)0x40020000UL)
#define DMA1_Stream5           ((void *)0x40026088UL)
#define DMA1_Stream5_IRQn      16

typedef struct
{
    uint32_t Channel;
    uint32_t Direction;
    uint32_t Mode;
} DMA_InitTypeDef;

typedef struct __DMA_HandleTypeDef
{
    void *Instance;
    DMA_InitTypeDef Init;
    uint32_t disabled_interrupts;
} DMA_HandleTypeDef;

typedef struct
{
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
    uint32_t Mode;
} UART_InitTypeDef;

typedef struct __UART_HandleTypeDef
{
    void *Instance;
    UART_InitTypeDef Init;
    DMA_HandleTypeDef *hdmarx;
    uint32_t ErrorCode;
} UART_HandleTypeDef;

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(
    UART_HandleTypeDef *uart,
    uint8_t *buffer,
    uint16_t size
);

HAL_StatusTypeDef HAL_UART_DMAStop(
    UART_HandleTypeDef *uart
);

HAL_StatusTypeDef HAL_UART_Transmit_IT(
    UART_HandleTypeDef *uart,
    uint8_t *data,
    uint16_t size
);

HAL_StatusTypeDef HAL_UART_Transmit(
    UART_HandleTypeDef *uart,
    uint8_t *data,
    uint16_t size,
    uint32_t timeout
);

uint32_t HAL_GetTick(void);
void fake_stm32f4_hal_data_memory_barrier(void);

#define __DMB() fake_stm32f4_hal_data_memory_barrier()
#define __HAL_DMA_DISABLE_IT(handle, interrupt) \
    ((handle)->disabled_interrupts |= (interrupt))

#endif /* TEST_STUB_STM32F4XX_HAL_H */
