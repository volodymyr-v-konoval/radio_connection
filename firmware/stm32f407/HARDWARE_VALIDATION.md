# STM32F407 Hardware Validation

## Target

- MCU: STM32F407VET6
- Board: FK407M3-VET6 V1.1
- Development environment: STM32CubeIDE
- Programming interface: ST-LINK over SWD

## Validated functionality

The following STM32F407 firmware bring-up steps were successfully
completed:

- firmware compiled successfully with the ARM embedded toolchain
- firmware programmed successfully through ST-LINK
- MCU started correctly after reset
- startup diagnostics were transmitted through USART1
- USART2 circular DMA reception started successfully
- board configuration passed runtime validation
- host-side framework and architecture tests remained successful

## UART configuration

### USART1 — diagnostics

- purpose: firmware logging
- mode: asynchronous UART
- baud rate: 115200
- format: 8 data bits, no parity, 1 stop bit

### USART2 — radio receiver

- purpose: CRSF receiver communication
- mode: asynchronous UART
- baud rate: 420000
- format: 8 data bits, no parity, 1 stop bit
- RX mode: circular DMA

## Current status

The STM32F407 platform port and firmware bring-up are complete.

Real CRSF reception from the RadioMaster RP2 V2 receiver, channel
validation, failsafe handling, reconnect behavior, and long-running DMA
stability remain part of the next hardware validation stage.
