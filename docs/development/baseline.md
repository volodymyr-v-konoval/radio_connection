# Verified Project Baseline

## Purpose

This document records the last known-good state of the `radio_connection` project before introducing a repository-owned firmware build system, GitHub Actions CI/CD, firmware artifact packaging, release automation, and complete project documentation.

This baseline is the regression reference for all subsequent infrastructure and build-system changes.

## Source Identity

| Property                     | Value                                      |
| ---------------------------- | ------------------------------------------ |
| Repository                   | `radio_connection`                         |
| Baseline branch              | `master`                                   |
| Baseline commit              | `662ad0ddfa17aca39cb15d89ea2310535a3214e8` |
| Commit subject               | `Add STM32F401 SX128x Duplex Packet Link`  |
| Baseline verification branch | `docs/verified-baseline`                   |
| Baseline verification date   | `2026-07-20`                               |

All results below refer to the exact baseline commit listed above.

## Validation Environment

| Component                     | Version or configuration                       |
| ----------------------------- | ---------------------------------------------- |
| Host operating system         | Ubuntu 24.04 LTS                               |
| STM32CubeIDE                  | 2.2.0                                          |
| STM32CubeIDE headless builder | `/opt/st/stm32cubeide_2.2.0/headless-build.sh` |
| ARM toolchain                 | GNU Tools for STM32 14.3.Rel1                  |
| ARM compiler                  | `arm-none-eabi-gcc 14.3.1`                     |
| Firmware build type           | Debug                                          |
| Host build system             | CMake and CTest                                |
| Firmware build system         | STM32CubeIDE-managed GNU Make                  |
| Firmware architecture         | ARM Cortex-M4                                  |

The exact executable paths and complete tool-version outputs are stored in the local baseline evidence bundle.

## Host Test Baseline

The host-side project was configured from an empty out-of-tree build directory and built successfully.

The complete CTest suite produced:

```text
100% tests passed, 0 tests failed out of 24
```

The verified test suites were:

1. `rc_control_policy_tests`
2. `rc_channel_mapper_tests`
3. `crsf_protocol_tests`
4. `rc_service_crsf_tests`
5. `link_crc16_tests`
6. `link_packet_tests`
7. `packet_radio_if_tests`
8. `duplex_link_service_tests`
9. `sx128x_driver_tests`
10. `sx128x_packet_radio_tests`
11. `stm32_adapter_tests`
12. `rc_service_stm32_dma_tests`
13. `stm32f4_sx128x_port_tests`
14. `stm32f401_link_composition_tests`
15. `stm32f4_backend_tests`
16. `stm32f407_composition_tests`
17. `architecture_dependency_test`
18. `rc_receiver_task_tests`
19. `sbus_protocol_tests`
20. `rc_service_sbus_tests`
21. `protocol_uart_config_tests`
22. `crsf_telemetry_tests`
23. `crsf_telemetry_service_tests`
24. `stm32f4_uart_tx_backend_tests`

The architecture dependency test also passed independently:

```text
100% tests passed, 0 tests failed out of 1
```

## STM32F401 Node A Baseline

| Property              | Value                                                  |
| --------------------- | ------------------------------------------------------ |
| MCU                   | STM32F401CCU6                                          |
| CubeIDE configuration | `Debug_NodeA`                                          |
| Node role             | Initiator                                              |
| Compile definition    | `LORA_NODE_ROLE=NODE_ROLE_INITIATOR`                   |
| ELF output            | `firmware/stm32f401/cubeide/Debug_NodeA/stm32f401.elf` |
| Linker script         | `firmware/stm32f401/cubeide/STM32F401CCUX_FLASH.ld`    |

Verified firmware size:

```text
text     data     bss      dec      hex
30096    92       3396     33584    8330
```

Verified SHA-256:

```text
a3ed7d673654056055e319f22a20e6bab412acef9abbc2dd152421c05a92829e
```

## STM32F401 Node B Baseline

| Property              | Value                                                  |
| --------------------- | ------------------------------------------------------ |
| MCU                   | STM32F401CCU6                                          |
| CubeIDE configuration | `Debug_NodeB`                                          |
| Node role             | Responder                                              |
| Compile definition    | `LORA_NODE_ROLE=NODE_ROLE_RESPONDER`                   |
| ELF output            | `firmware/stm32f401/cubeide/Debug_NodeB/stm32f401.elf` |
| Linker script         | `firmware/stm32f401/cubeide/STM32F401CCUX_FLASH.ld`    |

Verified firmware size:

```text
text     data     bss      dec      hex
29864    92       3396     33352    8248
```

Verified SHA-256:

```text
6126d878d33c8e2c74380a684e58ad7cd8d712c219386515d522d9a70beddc98
```

The Node A and Node B firmware binaries were confirmed to be different.

## STM32F407 Regression Baseline

| Property              | Value                                               |
| --------------------- | --------------------------------------------------- |
| MCU                   | STM32F407VET6                                       |
| CubeIDE configuration | `Debug`                                             |
| Compile definition    | `STM32F407xx`                                       |
| ELF output            | `firmware/stm32f407/cubeide/Debug/stm32f407.elf`    |
| Linker script         | `firmware/stm32f407/cubeide/STM32F407VETX_FLASH.ld` |

Verified firmware size:

```text
text     data     bss      dec      hex
36880    92       3692     40664    9ed8
```

Verified SHA-256:

```text
a2f497a132b9583e3484c2dcc6e0febc3b197a1eacb73863122b70c7fe9476cf
```

The STM32F407 build remains the regression firmware target for:

* the protocol-neutral RC receiver service;
* CRSF reception;
* SBUS reception;
* USART2 circular-DMA reception;
* UART error recovery;
* asynchronous UART transmission;
* bidirectional CRSF telemetry;
* STM32F4 platform adapters.

## STM32F401 Architecture Baseline

The STM32F401 radio path is:

```text
Application
    ↓
STM32F401 Link Composition
    ↓
DuplexLinkService
    ↓
PacketRadio
    ↓
Sx128xPacketRadio
    ↓
Generic SX128x Driver
    ↓
STM32F4 SX128x Port
    ↓
STM32 HAL
    ↓
SPI1, GPIO, EXTI and SX128x Hardware
```

The verified SX128x radio configuration is:

| Parameter        | Value      |
| ---------------- | ---------- |
| Frequency        | 2445 MHz   |
| Modulation       | LoRa       |
| Spreading factor | SF6        |
| Bandwidth        | 812.5 kHz  |
| Coding rate      | 4/5        |
| Preamble         | 12 symbols |
| Header mode      | Explicit   |
| Radio CRC        | Enabled    |
| TX power         | +10 dBm    |

The link service supports:

* `DATA` and `ACK`;
* `PING` and `PONG`;
* sequence numbers;
* acknowledgement sequence matching;
* response timeout;
* bounded retries;
* duplicate request suppression;
* cached duplicate responses;
* sequence-number rollover;
* radio error recovery.

## STM32F407 Architecture Baseline

The STM32F407 receive path is:

```text
USART2
    ↓
HAL Receive-to-IDLE Circular DMA
    ↓
STM32F4 UART DMA Backend
    ↓
STM32 UART DMA Transport
    ↓
RadioTransport Interface
    ↓
CRSF or SBUS Protocol
    ↓
RcReceiverService
    ↓
Channel Mapping and Safety Policy
```

The CRSF telemetry transmit path is:

```text
CrsfTelemetryService
    ↓
RadioTx Interface
    ↓
STM32F4 UART TX Backend
    ↓
HAL Asynchronous UART TX
    ↓
USART2
```

The portable protocol and service layers remain independent of STM32 HAL and board-specific definitions.

## Hardware Validation Baseline

The STM32F401 SX128x implementation was previously validated using two physical devices.

Both SX128x radios successfully responded to:

```text
GetStatus=0x45
```

A real bidirectional `DATA` and `ACK` exchange was observed.

Example Node A output:

```text
[TX] DATA started payload=NODE-A-DATA-0001
[INFO] [DUPLEX_LINK] Exchange OK seq=18 ack=18 attempts=1
rtt=18 ms rssi_x2=-98 snr_x4=54
```

Example Node B output:

```text
[RX] request seq=95 len=16 payload=NODE-A-DATA-0001
rssi_x2=-64 snr_x4=52
```

Normal observed round-trip time was approximately:

```text
18–19 ms
```

Retry recovery was also observed:

```text
Retry seq=19 retry=1/3
Exchange OK seq=19 ack=19 attempts=2
```

This hardware validation confirms the previously verified operation of:

* SPI1 communication;
* SX128x reset and BUSY handling;
* deferred DIO1 interrupt processing;
* packet transmission and reception;
* DATA and ACK exchange;
* sequence matching;
* response timeout;
* retry recovery;
* duplicate suppression support;
* SX128x packet CRC;
* application-level CRC16;
* RSSI and SNR reporting.

## Demonstration Payload

The current STM32F401 demonstration firmware uses a fixed application payload:

```text
NODE-A-DATA-0001
```

Node B uses the fixed response payload:

```text
B-RESPONSE-DATA!
```

These fixed values belong only to the demonstration application layer.

The reusable link and packet-radio layers accept arbitrary binary buffers and are not coupled to these strings.

The currently supported application payload length is:

```text
Minimum: 10 bytes
Maximum: 16 bytes
```

## Architecture Boundaries

The required dependency direction is:

```text
Portable Protocol and Service Logic
                ↓
Platform-Independent Interfaces
                ↓
STM32 Platform Adapters
                ↓
Firmware Composition
                ↓
STM32 HAL and Generated Hardware Code
```

Portable modules must not directly depend on:

* STM32 HAL;
* MCU-specific registers;
* CubeIDE-generated application globals;
* board pin definitions;
* firmware-specific composition state;
* FreeRTOS outside explicit wrapper modules.

The `architecture_dependency_test` enforces the current portable-layer dependency rules.

## Known Baseline Limitations

The following limitations are intentionally present in this baseline:

1. Firmware builds depend on STM32CubeIDE-generated Makefiles.
2. Firmware cannot yet be built from a completely clean Git checkout without invoking STM32CubeIDE.
3. Node A and Node B depend on separate generated build outputs.
4. Both STM32F401 configurations currently generate a directory named `Debug`, which must be isolated or renamed during sequential headless builds.
5. The STM32F401 demonstration application uses fixed application payloads.
6. Hardware-in-the-loop validation is not automated.
7. GitHub Actions workflows are not yet present.
8. Firmware artifacts are not yet packaged by repository-owned scripts.
9. GitHub Release automation is not yet present.
10. Complete user-facing documentation is not yet present.
11. Generated API documentation is not yet present.
12. STM32CubeIDE headless builds may modify generated language-settings metadata, which must not be included in baseline documentation commits.

## Regression Requirements

Future changes must preserve all of the following unless a deliberate functional change is documented and approved:

* all 24 host-side CTest suites;
* `architecture_dependency_test`;
* STM32F401 Node A firmware build;
* STM32F401 Node B firmware build;
* distinct Node A and Node B compile-time role definitions;
* distinct Node A and Node B binaries;
* STM32F407 regression firmware build;
* protocol-neutral portable architecture;
* non-blocking deferred SX128x interrupt processing;
* bounded SX128x BUSY timeouts;
* DATA and ACK exchange;
* PING and PONG exchange;
* retry behavior;
* duplicate suppression;
* CRSF receive support;
* SBUS receive support;
* circular-DMA UART receive behavior;
* asynchronous UART transmission;
* bidirectional CRSF telemetry.

## Baseline Acceptance Result

The baseline verification completed with:

```text
[PASS] Source commit identity verified
[PASS] Verification branch created
[PASS] Host CMake configuration completed
[PASS] Host build completed
[PASS] 24 of 24 CTest suites passed
[PASS] Architecture dependency test passed
[PASS] STM32F401 Node A clean headless build completed
[PASS] STM32F401 Node A initiator role verified
[PASS] STM32F401 Node B clean headless build completed
[PASS] STM32F401 Node B responder role verified
[PASS] Node A and Node B binaries are different
[PASS] STM32F407 regression clean headless build completed
[PASS] Firmware section sizes recorded
[PASS] Firmware SHA-256 hashes recorded
[PASS] Firmware ELF architecture verified as ARM
[PASS] No generated firmware artifacts are tracked
[PASS] Git working tree was clean before adding this document
```
