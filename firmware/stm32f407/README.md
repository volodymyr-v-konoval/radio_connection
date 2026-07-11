# STM32F407VET6 hardware port

This directory contains only project-owned board and composition code.
Generate `Core/` and `Drivers/` with STM32CubeMX/CubeIDE and keep generated
vendor code separate from the reusable radio framework.

## Proposed UART allocation

| Purpose | Peripheral | Pins | Configuration |
|---|---|---|---|
| RadioMaster RP2 V2 CRSF | USART2 | PA3 RX, PA2 TX | 420000, 8N1, DMA RX circular |
| Debug logger | USART1 | PA9 TX, PA10 RX | 115200, 8N1 |

PA2/PA3 are selected as a replaceable board policy. Verify the labels and
actual routing on the specific FK407M3-VET6 V1.1 board before wiring.

## CubeMX configuration

### USART2 receiver

- Mode: Asynchronous
- Baud rate: 420000
- Word length: 8 bits
- Parity: None
- Stop bits: 1
- TX/RX: enabled (TX is reserved for future telemetry)
- RX DMA: DMA1 Stream 5, Channel 4
- DMA direction: Peripheral to memory
- DMA mode: Circular
- Peripheral increment: disabled
- Memory increment: enabled
- Data alignment: byte/byte
- USART2 global interrupt: enabled
- DMA1 Stream5 interrupt: enabled

### USART1 logger

- Mode: Asynchronous
- Baud rate: 115200
- Word length: 8 bits
- Parity: None
- Stop bits: 1
- TX enabled

## Application ownership

Create one application-owned instance and DMA buffer:

```c
static Stm32f407RadioComposition g_radio;
static uint8_t g_radio_dma_buffer[FK407M3_RADIO_DMA_BUFFER_SIZE];
```

After `MX_DMA_Init()`, `MX_USART1_UART_Init()` and
`MX_USART2_UART_Init()`, initialize composition:

```c
const Stm32f407RadioCompositionConfig radio_config = {
    .receiver_uart = &huart2,
    .receiver_dma_buffer = g_radio_dma_buffer,
    .receiver_dma_buffer_size = sizeof(g_radio_dma_buffer),
    .logger_uart = &huart1,
    .logger_timeout_ms = 20U,
    .log_level = RADIO_LOG_LEVEL_INFO,
    .failsafe_timeout_ms = FK407M3_RADIO_FAILSAFE_TIMEOUT_MS
};

if (!stm32f407_radio_composition_init(&g_radio, &radio_config)) {
    Error_Handler();
}
```

Call from the main loop:

```c
stm32f407_radio_composition_process(&g_radio);
```

Forward HAL callbacks only; do not parse in interrupt context:

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    stm32f407_radio_composition_on_uart_rx_event(
        &g_radio,
        huart,
        size
    );
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    stm32f407_radio_composition_on_uart_error(&g_radio, huart);
}
```

The RX callback only publishes the DMA producer position. UART restart and
parser reset happen later in the main-loop context.
