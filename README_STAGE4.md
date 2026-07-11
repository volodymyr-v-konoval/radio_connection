# Stage 4 — STM32F407 board port

## Implemented

- STM32F4 HAL UART Receive-to-IDLE circular-DMA backend
- monotonic byte producer count derived from DMA event positions
- ISR-only event publication with no protocol parsing
- deferred UART error recovery in application context
- STM32F4 HAL tick adapter
- blocking UART debug-log sink for bring-up
- FK407M3-VET6 V1.1 board policy and runtime UART validation
- application composition root connecting HAL, DMA transport, CRSF, service,
  logger, and time modules
- host HAL stubs and deterministic backend/composition tests
- automated architecture dependency-boundary test

## Dependency direction

```text
firmware/stm32f407/app/radio_composition
    -> board policy
    -> STM32F4 HAL backends
    -> hardware-neutral STM32 adapters
    -> RcReceiverService / CRSF
```

No file under `src/radio/` includes STM32 HAL or FreeRTOS headers.

## Important runtime rule

`HAL_UARTEx_RxEventCallback()` and `HAL_UART_ErrorCallback()` only forward
information to the backend. They do not decode CRSF, log, allocate memory,
or restart DMA. Recovery and parsing happen in the main loop.
