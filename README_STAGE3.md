# Stage 3 — Hardware-Neutral STM32 Platform Adapters

This stage adds STM32-facing adapters without introducing STM32 HAL,
FreeRTOS, UART, DMA, or board dependencies into the radio core.

## Implemented modules

- `stm32_uart_dma_transport.h/.c`
  - implements `RadioTransport`
  - reads from a circular DMA buffer
  - supports byte and block reads
  - handles DMA-buffer wraparound
  - detects consumer overrun
  - keeps the newest bytes after overrun
  - exposes byte, overflow, and dropped-byte statistics

- `stm32_time.h/.c`
  - implements `RadioTime`
  - delegates the current tick to a backend callback
  - handles `uint32_t` tick wraparound through unsigned subtraction

- `stm32_logger.h/.c`
  - implements `RadioLogger`
  - delegates final byte output to a backend callback
  - can later use UART, SWO, SEGGER RTT, or another sink

## DMA backend contract

The board/family backend must provide:

1. a pointer to the circular RX DMA buffer;
2. the buffer capacity;
3. a monotonic `uint32_t` count of all bytes produced by DMA.

The producer count may wrap naturally at `UINT32_MAX`. The adapter uses
unsigned subtraction, so normal counter wraparound is supported.

A monotonic producer count is used instead of only the current DMA write
index because a modulo index alone cannot reliably detect that DMA has
lapped the consumer.

## Tests

Host tests cover:

- empty DMA buffer;
- partial reads;
- circular-buffer wraparound;
- DMA overrun and dropped-byte accounting;
- producer-counter wraparound;
- transport reset;
- STM32 time tick wraparound;
- logger filtering and formatting;
- partial CRSF frame delivery through the unchanged `RcReceiverService`.

## Dependency boundary

`src/radio/` remains platform-independent. No STM32 HAL or FreeRTOS
headers are included by the core, service, or protocol modules.

## Next stage

Add the STM32F407 family backend and the FK407M3-VET6 V1.1 board port:

- circular UART RX DMA setup;
- producer-count maintenance from DMA/IDLE events;
- HAL tick callback;
- UART/SWO/RTT log sink;
- board-specific UART, GPIO, DMA stream/channel, and IRQ selection.
