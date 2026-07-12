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

## RadioMaster RP2 V2 CRSF Validation

### Hardware

- receiver: RadioMaster RP2 V2
- protocol: CRSF
- STM32 interface: USART2 RX
- baud rate: 420000
- UART format: 8 data bits, no parity, 1 stop bit
- receive mode: Receive-to-IDLE circular DMA
- debug output: USART1 at 115200 baud

### Validated functionality

The RadioMaster RP2 V2 receiver was successfully bound to the
ExpressLRS transmitter and tested with the STM32F407 firmware.

The following functionality was validated on real hardware:

- stable CRSF byte reception through USART2
- circular DMA position event handling
- CRSF frame synchronization
- frame length validation
- CRC validation
- decoding of all 16 RC channels
- detection of active RC link
- failsafe activation after transmitter shutdown
- invalidation of stale RC channel data during failsafe
- automatic link recovery after transmitter restart
- continued operation without resetting the STM32
- recovery after receiver signal loss
- stable operation without DMA overflow or byte loss

### Observed results

A representative validation run produced:

- more than 1,100,000 received and processed bytes
- more than 43,000 parsed CRSF frames
- more than 41,000 valid RC channel frames
- zero CRC errors
- zero frame length errors
- zero DMA overruns
- zero dropped bytes
- zero UART errors after replacing unreliable wiring
- zero UART recovery attempts during stable operation

Unsupported frame counters represent valid non-RC CRSF frame types
which are intentionally ignored by the current receiver implementation.

### Failsafe behavior

When the transmitter was powered off:

- CRSF frame reception stopped
- the receiver service entered failsafe
- the latest frame was marked invalid and lost
- stale channel values remained available only for diagnostics
- channel data was no longer considered usable for control

When the transmitter was powered on again:

- the CRSF link recovered automatically
- valid channel frames resumed
- failsafe was cleared
- no STM32 reset was required

### Result

The STM32F407 and RadioMaster RP2 V2 CRSF hardware integration is
considered successfully validated.
