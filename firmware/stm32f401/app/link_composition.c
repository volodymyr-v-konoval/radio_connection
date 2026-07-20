#include "link_composition.h"

#include <string.h>

#include "link_packet.h"
#include "stm32f4_time_backend.h"

#define STM32F401_LINK_DEFAULT_FREQUENCY_HZ 2445000000U
#define STM32F401_LINK_DEFAULT_SPI_TIMEOUT_MS 20U
#define STM32F401_LINK_DEFAULT_BUSY_TIMEOUT_MS 20U
#define STM32F401_LINK_DEFAULT_LOGGER_TIMEOUT_MS 20U
#define STM32F401_LINK_DEFAULT_RESPONSE_TIMEOUT_MS 50U
#define STM32F401_LINK_DEFAULT_OPERATION_TIMEOUT_MS 250U
#define STM32F401_LINK_DEFAULT_TX_TIMEOUT_MS 100U
#define STM32F401_LINK_DEFAULT_MAX_RETRIES 3U
#define STM32F401_LINK_INITIATOR_NODE_ID 1U
#define STM32F401_LINK_RESPONDER_NODE_ID 2U

static bool stm32f401_link_composition_config_is_valid(
    const Stm32f401LinkCompositionConfig *config
)
{
    if (config == NULL ||
        !blackpill_f401cc_sx128x_validate_spi(config->radio_spi) ||
        !blackpill_f401cc_sx128x_validate_debug_uart(
            config->logger_uart) ||
        config->pins.nss_port == NULL ||
        config->pins.nss_pin == 0U ||
        config->pins.busy_port == NULL ||
        config->pins.busy_pin == 0U ||
        config->pins.dio1_port == NULL ||
        config->pins.dio1_pin == 0U ||
        config->pins.reset_port == NULL ||
        config->pins.reset_pin == 0U ||
        config->spi_timeout_ms == 0U ||
        config->busy_timeout_ms == 0U ||
        config->logger_timeout_ms == 0U ||
        config->lora.payload_length != LINK_PACKET_ENCODED_SIZE ||
        config->packet_radio.frame_length != LINK_PACKET_ENCODED_SIZE ||
        config->packet_radio.tx_buffer_offset !=
            config->lora.tx_base_address) {
        return false;
    }

    return true;
}

bool stm32f401_link_composition_config_init_defaults(
    Stm32f401LinkCompositionConfig *config,
    SPI_HandleTypeDef *radio_spi,
    UART_HandleTypeDef *logger_uart,
    DuplexLinkRole role
)
{
    if (config == NULL ||
        !blackpill_f401cc_sx128x_validate_spi(radio_spi) ||
        !blackpill_f401cc_sx128x_validate_debug_uart(logger_uart) ||
        (role != DUPLEX_LINK_ROLE_INITIATOR &&
         role != DUPLEX_LINK_ROLE_RESPONDER)) {
        return false;
    }

    memset(config, 0, sizeof(*config));

    config->radio_spi = radio_spi;
    config->logger_uart = logger_uart;
    blackpill_f401cc_sx128x_get_default_pins(&config->pins);

    config->spi_timeout_ms =
        STM32F401_LINK_DEFAULT_SPI_TIMEOUT_MS;
    config->busy_timeout_ms =
        STM32F401_LINK_DEFAULT_BUSY_TIMEOUT_MS;
    config->logger_timeout_ms =
        STM32F401_LINK_DEFAULT_LOGGER_TIMEOUT_MS;
    config->log_level = RADIO_LOG_LEVEL_INFO;

    config->lora.frequency_hz =
        STM32F401_LINK_DEFAULT_FREQUENCY_HZ;
    config->lora.spreading_factor = SX128X_LORA_SF6;
    config->lora.bandwidth = SX128X_LORA_BW_812_5_KHZ;
    config->lora.coding_rate = SX128X_LORA_CR_4_5;
    config->lora.preamble_symbols = 12U;
    config->lora.header_type = SX128X_LORA_HEADER_EXPLICIT;
    config->lora.payload_length = LINK_PACKET_ENCODED_SIZE;
    config->lora.crc_mode = SX128X_LORA_CRC_ON;
    config->lora.iq_mode = SX128X_LORA_IQ_NORMAL;
    config->lora.tx_power_dbm = 10;
    config->lora.ramp_time = SX128X_RAMP_20_US;
    config->lora.tx_base_address = 0U;
    config->lora.rx_base_address = 128U;
    config->lora.irq_mask = SX128X_DEFAULT_LINK_IRQ_MASK;
    config->lora.dio1_mask = SX128X_DEFAULT_LINK_IRQ_MASK;

    config->packet_radio.tx_buffer_offset =
        config->lora.tx_base_address;
    config->packet_radio.frame_length = LINK_PACKET_ENCODED_SIZE;
    config->packet_radio.tx_timeout_base =
        SX128X_TIMEOUT_BASE_1_MS;
    config->packet_radio.tx_timeout_count =
        STM32F401_LINK_DEFAULT_TX_TIMEOUT_MS;

    config->link.role = role;
    config->link.initial_sequence = 1U;
    config->link.response_timeout_ms =
        STM32F401_LINK_DEFAULT_RESPONSE_TIMEOUT_MS;
    config->link.operation_timeout_ms =
        STM32F401_LINK_DEFAULT_OPERATION_TIMEOUT_MS;
    config->link.max_retries =
        STM32F401_LINK_DEFAULT_MAX_RETRIES;

    if (role == DUPLEX_LINK_ROLE_INITIATOR) {
        config->link.local_node_id =
            STM32F401_LINK_INITIATOR_NODE_ID;
        config->link.peer_node_id =
            STM32F401_LINK_RESPONDER_NODE_ID;
    } else {
        config->link.local_node_id =
            STM32F401_LINK_RESPONDER_NODE_ID;
        config->link.peer_node_id =
            STM32F401_LINK_INITIATOR_NODE_ID;
    }

    return true;
}

bool stm32f401_link_composition_init(
    Stm32f401LinkComposition *composition,
    const Stm32f401LinkCompositionConfig *config
)
{
    if (composition == NULL ||
        !stm32f401_link_composition_config_is_valid(config)) {
        return false;
    }

    memset(composition, 0, sizeof(*composition));

    const Stm32f4Sx128xPortConfig port_config = {
        .spi = config->radio_spi,
        .nss_port = config->pins.nss_port,
        .nss_pin = config->pins.nss_pin,
        .reset_port = config->pins.reset_port,
        .reset_pin = config->pins.reset_pin,
        .busy_port = config->pins.busy_port,
        .busy_pin = config->pins.busy_pin,
        .dio1_pin = config->pins.dio1_pin,
        .spi_timeout_ms = config->spi_timeout_ms
    };

    if (!stm32f4_sx128x_port_init(
            &composition->radio_port_backend,
            &composition->radio_port,
            &port_config)) {
        return false;
    }

    if (sx128x_init(
            &composition->radio_device,
            &composition->radio_port,
            config->busy_timeout_ms) != SX128X_RESULT_OK ||
        sx128x_hardware_reset(
            &composition->radio_device) != SX128X_RESULT_OK ||
        sx128x_configure_lora(
            &composition->radio_device,
            &config->lora) != SX128X_RESULT_OK ||
        !sx128x_packet_radio_init(
            &composition->packet_radio,
            &composition->packet_radio_adapter,
            &composition->radio_device,
            &config->packet_radio) ||
        !stm32f4_uart_logger_backend_init(
            &composition->logger_backend,
            config->logger_uart,
            config->logger_timeout_ms) ||
        !stm32_logger_init(
            &composition->logger,
            &composition->logger_context,
            config->log_level,
            stm32f4_uart_logger_backend_write,
            &composition->logger_backend) ||
        !stm32_time_init(
            &composition->time,
            &composition->time_context,
            stm32f4_time_backend_now_ms,
            NULL) ||
        !duplex_link_service_init(
            &composition->link_service,
            &composition->packet_radio,
            &composition->logger,
            &composition->time,
            &config->link)) {
        memset(composition, 0, sizeof(*composition));
        return false;
    }

    composition->initialized = true;
    return true;
}

void stm32f401_link_composition_process(
    Stm32f401LinkComposition *composition
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    duplex_link_service_process(&composition->link_service);
}

void stm32f401_link_composition_on_dio1_irq(
    Stm32f401LinkComposition *composition,
    uint16_t gpio_pin
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    stm32f4_sx128x_port_on_dio1_irq(
        &composition->radio_port_backend,
        gpio_pin
    );
}

bool stm32f401_link_composition_start_data(
    Stm32f401LinkComposition *composition,
    const uint8_t *payload,
    size_t payload_length
)
{
    return composition != NULL && composition->initialized &&
        duplex_link_service_start_data(
            &composition->link_service,
            payload,
            payload_length
        );
}

bool stm32f401_link_composition_start_ping(
    Stm32f401LinkComposition *composition,
    const uint8_t *payload,
    size_t payload_length
)
{
    return composition != NULL && composition->initialized &&
        duplex_link_service_start_ping(
            &composition->link_service,
            payload,
            payload_length
        );
}

bool stm32f401_link_composition_set_response_payload(
    Stm32f401LinkComposition *composition,
    const uint8_t *payload,
    size_t payload_length
)
{
    return composition != NULL && composition->initialized &&
        duplex_link_service_set_response_payload(
            &composition->link_service,
            payload,
            payload_length
        );
}

bool stm32f401_link_composition_take_event(
    Stm32f401LinkComposition *composition,
    DuplexLinkEvent *event
)
{
    return composition != NULL && composition->initialized &&
        duplex_link_service_take_event(
            &composition->link_service,
            event
        );
}

const DuplexLinkStats *stm32f401_link_composition_get_stats(
    const Stm32f401LinkComposition *composition
)
{
    if (composition == NULL || !composition->initialized) {
        return NULL;
    }

    return duplex_link_service_get_stats(
        &composition->link_service
    );
}
