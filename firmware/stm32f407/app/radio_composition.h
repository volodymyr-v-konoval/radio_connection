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

#endif /* STM32F407_RADIO_COMPOSITION_H */
