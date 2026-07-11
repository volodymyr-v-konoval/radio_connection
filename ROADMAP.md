# Radio Receiver Framework Roadmap

## Stage 1 — Core framework

Status: done

Implemented:

- universal RC types
- protocol, transport, logger, and time interfaces
- generic receiver service
- PC platform and mock transport
- CMake host build

## Stage 2 — CRSF support

Status: done

Implemented:

- CRC8 DVB-S2
- CRSF byte-stream parser
- exact RC-frame length validation
- packed 16-channel decoder
- parser timeout and recovery
- protocol statistics
- failsafe timeout and recovery tests
- CTest protocol and service integration tests

Checkpoint:

- tag: `v0.2.0-crsf`

## Stage 3 — Hardware-neutral STM32 adapters

Status: done

Implemented:

- callback-based STM32 UART DMA transport
- circular-buffer wraparound handling
- DMA overrun detection and statistics
- callback-based STM32 logger
- callback-based STM32 time provider
- fake DMA backend for host tests
- partial CRSF-over-DMA service integration test
- no HAL or FreeRTOS dependency in `src/radio/`

Not run in the host environment:

- optional ARM cross-build (`arm-none-eabi-gcc` unavailable)

## Stage 4 — STM32F407 board port

Status: next

Tasks:

- add STM32F4 HAL backend
- create STM32F407VET6 CubeMX/CubeIDE firmware project
- select board UART pins, DMA stream/channel, and IRQ
- maintain monotonic DMA producer count
- connect HAL tick to `stm32_time`
- connect UART/SWO/RTT sink to `stm32_logger`
- run a bare-metal receiver processing loop

## Stage 5 — RadioMaster RP2 V2 CRSF integration

Status: planned

Tasks:

- add replaceable receiver profile
- configure CRSF UART at 420000 baud, 8N1, non-inverted
- connect RP2 V2 TX to STM32 RX
- validate all channels, failsafe, reconnect, and long-running DMA reception

Checkpoint after hardware validation:

- tag: `v0.3.0-f407-rp2`

## Stage 6 — FreeRTOS wrapper

Status: planned

Tasks:

- add `rc_receiver_task.h/.c`
- keep parsing out of ISR context
- keep FreeRTOS out of core framework files

## Stage 7 — SBUS support

Status: planned

Tasks:

- add isolated SBUS parser and decoder
- add SBUS unit and service tests
- switch protocol through application composition only

Checkpoint:

- tag: `v0.4.0-multiprotocol`

## Stage 8 — CRSF telemetry

Status: planned

Tasks:

- add a separate byte-writer interface
- add CRSF telemetry encoder
- add STM32 UART TX backend
- connect STM32 TX to RP2 V2 RX

## Stage 9 — Production hardening

Status: planned

Tasks:

- UART-noise and overrun tests
- receiver brownout and restart tests
- transmitter-loss and reconnect tests
- watchdog, stack, and memory validation
- long-duration soak test
- static analysis and warnings-as-errors builds
- hardware-in-the-loop release validation

Release checkpoint:

- tag: `v1.0.0`
