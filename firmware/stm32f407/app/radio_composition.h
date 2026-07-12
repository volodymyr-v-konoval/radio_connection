#ifndef STM32F407_RADIO_COMPOSITION_H
#define STM32F407_RADIO_COMPOSITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crsf_protocol.h"
#include "rc_receiver_service.h"
#include "stm32_logger.h"
#include "stm32_time.h"
#include "stm32_uart_dma_transport.h"
#include "stm32f4_uart_dma_backend.h"
#include "stm32f4_uart_logger_backend.h"

typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

typedef struct
{
    UART_HandleTypeDef *receiver_uart;
    uint8_t *receiver_dma_buffer;
    size_t receiver_dma_buffer_size;

    UART_HandleTypeDef *logger_uart;
    uint32_t logger_timeout_ms;
    RadioLogLevel log_level;

    uint32_t failsafe_timeout_ms;
} Stm32f407RadioCompositionConfig;

typedef struct
{
    Stm32f4UartDmaBackend uart_dma_backend;
    Stm32UartDmaTransportContext transport_context;
    Stm32f4UartLoggerBackend logger_backend;
    Stm32LoggerContext logger_context;
    Stm32TimeContext time_context;
    CrsfProtocolContext crsf_context;

    RadioTransport transport;
    RadioLogger logger;
    RadioTime time;
    RadioProtocol protocol;
    RcReceiverService service;

    bool initialized;
} Stm32f407RadioComposition;

typedef struct
{
    /*
     * UART/DMA byte flow.
     */
    uint32_t received_bytes;
    uint32_t processed_bytes;

    /*
     * Protocol parser results.
     */
    uint32_t received_frames;
    uint32_t valid_frames;
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t unsupported_frames;

    /*
     * DMA transport state.
     */
    uint32_t dma_rx_events;
    uint32_t dma_duplicate_events;
    uint32_t dma_invalid_events;
    uint32_t dma_overrun_events;
    uint32_t dma_dropped_bytes;

    /*
     * UART error and deferred recovery state.
     */
    uint32_t uart_error_events;
    uint32_t uart_recovery_attempts;
    uint32_t uart_recovery_successes;
    uint32_t uart_recovery_failures;
    uint32_t last_uart_error;
} Stm32f407RadioDiagnostics;

bool stm32f407_radio_composition_init(
    Stm32f407RadioComposition *composition,
    const Stm32f407RadioCompositionConfig *config
);

void stm32f407_radio_composition_process(
    Stm32f407RadioComposition *composition
);

void stm32f407_radio_composition_on_uart_rx_event(
    Stm32f407RadioComposition *composition,
    UART_HandleTypeDef *uart,
    uint16_t dma_position
);

void stm32f407_radio_composition_on_uart_error(
    Stm32f407RadioComposition *composition,
    UART_HandleTypeDef *uart
);

bool stm32f407_radio_composition_get_latest_frame(
    Stm32f407RadioComposition *composition,
    RcInputFrame *out_frame
);

bool stm32f407_radio_composition_is_failsafe(
    Stm32f407RadioComposition *composition
);

bool stm32f407_radio_composition_get_diagnostics(
    const Stm32f407RadioComposition *composition,
    Stm32f407RadioDiagnostics *out_diagnostics
);
#endif /* STM32F407_RADIO_COMPOSITION_H */
