#ifndef STM32F401_LINK_COMPOSITION_H
#define STM32F401_LINK_COMPOSITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "blackpill_f401cc_sx128x.h"
#include "duplex_link_service.h"
#include "stm32_logger.h"
#include "stm32_time.h"
#include "stm32f4_sx128x_port.h"
#include "stm32f4_uart_logger_backend.h"
#include "sx128x.h"
#include "sx128x_packet_radio.h"

typedef struct
{
    SPI_HandleTypeDef *radio_spi;
    UART_HandleTypeDef *logger_uart;
    BlackpillF401ccSx128xPins pins;

    uint32_t spi_timeout_ms;
    uint32_t busy_timeout_ms;
    uint32_t logger_timeout_ms;
    RadioLogLevel log_level;

    Sx128xLoRaConfig lora;
    Sx128xPacketRadioConfig packet_radio;
    DuplexLinkConfig link;
} Stm32f401LinkCompositionConfig;

typedef struct
{
    Stm32f4Sx128xPort radio_port_backend;
    Sx128xPort radio_port;
    Sx128x radio_device;

    Sx128xPacketRadio packet_radio_adapter;
    PacketRadio packet_radio;

    Stm32f4UartLoggerBackend logger_backend;
    Stm32LoggerContext logger_context;
    RadioLogger logger;

    Stm32TimeContext time_context;
    RadioTime time;

    DuplexLinkService link_service;
    bool initialized;
} Stm32f401LinkComposition;

bool stm32f401_link_composition_config_init_defaults(
    Stm32f401LinkCompositionConfig *config,
    SPI_HandleTypeDef *radio_spi,
    UART_HandleTypeDef *logger_uart,
    DuplexLinkRole role
);

bool stm32f401_link_composition_init(
    Stm32f401LinkComposition *composition,
    const Stm32f401LinkCompositionConfig *config
);

void stm32f401_link_composition_process(
    Stm32f401LinkComposition *composition
);

void stm32f401_link_composition_on_dio1_irq(
    Stm32f401LinkComposition *composition,
    uint16_t gpio_pin
);

bool stm32f401_link_composition_start_data(
    Stm32f401LinkComposition *composition,
    const uint8_t *payload,
    size_t payload_length
);

bool stm32f401_link_composition_start_ping(
    Stm32f401LinkComposition *composition,
    const uint8_t *payload,
    size_t payload_length
);

bool stm32f401_link_composition_set_response_payload(
    Stm32f401LinkComposition *composition,
    const uint8_t *payload,
    size_t payload_length
);

bool stm32f401_link_composition_take_event(
    Stm32f401LinkComposition *composition,
    DuplexLinkEvent *event
);

const DuplexLinkStats *stm32f401_link_composition_get_stats(
    const Stm32f401LinkComposition *composition
);

#endif /* STM32F401_LINK_COMPOSITION_H */
