#include <stdio.h>
#include <string.h>

#include "fake_stm32f4_hal.h"
#include "link_composition.h"

static int g_failures = 0;
static int g_tests = 0;

#define CHECK(condition)                                                   \
    do {                                                                   \
        if (!(condition)) {                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);   \
            g_failures++;                                                  \
            return;                                                        \
        }                                                                  \
    } while (0)

static SPI_HandleTypeDef make_spi1(void)
{
    SPI_HandleTypeDef spi;
    memset(&spi, 0, sizeof(spi));
    spi.Instance = SPI1;
    return spi;
}

static UART_HandleTypeDef make_usart2(void)
{
    UART_HandleTypeDef uart;
    memset(&uart, 0, sizeof(uart));
    uart.Instance = USART2;
    return uart;
}

static void test_default_configs(void)
{
    g_tests++;

    SPI_HandleTypeDef spi = make_spi1();
    UART_HandleTypeDef uart = make_usart2();
    Stm32f401LinkCompositionConfig initiator;
    Stm32f401LinkCompositionConfig responder;

    CHECK(stm32f401_link_composition_config_init_defaults(
        &initiator,
        &spi,
        &uart,
        DUPLEX_LINK_ROLE_INITIATOR
    ));

    CHECK(stm32f401_link_composition_config_init_defaults(
        &responder,
        &spi,
        &uart,
        DUPLEX_LINK_ROLE_RESPONDER
    ));

    CHECK(initiator.pins.nss_port == GPIOA);
    CHECK(initiator.pins.nss_pin == GPIO_PIN_4);
    CHECK(initiator.pins.busy_port == GPIOB);
    CHECK(initiator.pins.busy_pin == GPIO_PIN_2);
    CHECK(initiator.pins.dio1_port == GPIOB);
    CHECK(initiator.pins.dio1_pin == GPIO_PIN_1);
    CHECK(initiator.pins.reset_port == GPIOB);
    CHECK(initiator.pins.reset_pin == GPIO_PIN_0);

    CHECK(initiator.lora.frequency_hz == 2445000000U);
    CHECK(initiator.lora.spreading_factor == SX128X_LORA_SF6);
    CHECK(initiator.lora.bandwidth == SX128X_LORA_BW_812_5_KHZ);
    CHECK(initiator.lora.coding_rate == SX128X_LORA_CR_4_5);
    CHECK(initiator.lora.payload_length == LINK_PACKET_ENCODED_SIZE);
    CHECK(initiator.packet_radio.frame_length ==
        LINK_PACKET_ENCODED_SIZE);

    CHECK(initiator.link.local_node_id == 1U);
    CHECK(initiator.link.peer_node_id == 2U);
    CHECK(responder.link.local_node_id == 2U);
    CHECK(responder.link.peer_node_id == 1U);
}

static void test_default_config_validation(void)
{
    g_tests++;

    SPI_HandleTypeDef good_spi = make_spi1();
    SPI_HandleTypeDef bad_spi = { .Instance = USART1 };
    UART_HandleTypeDef good_uart = make_usart2();
    UART_HandleTypeDef bad_uart = { .Instance = USART1 };
    Stm32f401LinkCompositionConfig config;

    CHECK(!stm32f401_link_composition_config_init_defaults(
        NULL,
        &good_spi,
        &good_uart,
        DUPLEX_LINK_ROLE_INITIATOR
    ));

    CHECK(!stm32f401_link_composition_config_init_defaults(
        &config,
        &bad_spi,
        &good_uart,
        DUPLEX_LINK_ROLE_INITIATOR
    ));

    CHECK(!stm32f401_link_composition_config_init_defaults(
        &config,
        &good_spi,
        &bad_uart,
        DUPLEX_LINK_ROLE_INITIATOR
    ));

    CHECK(!stm32f401_link_composition_config_init_defaults(
        &config,
        &good_spi,
        &good_uart,
        (DuplexLinkRole)99
    ));
}

static void test_initiator_init_and_start_tx(void)
{
    g_tests++;
    fake_stm32f4_hal_reset();

    SPI_HandleTypeDef spi = make_spi1();
    UART_HandleTypeDef uart = make_usart2();
    Stm32f401LinkCompositionConfig config;
    Stm32f401LinkComposition composition;

    CHECK(stm32f401_link_composition_config_init_defaults(
        &config,
        &spi,
        &uart,
        DUPLEX_LINK_ROLE_INITIATOR
    ));

    CHECK(stm32f401_link_composition_init(
        &composition,
        &config
    ));

    CHECK(composition.initialized);
    CHECK(composition.radio_device.initialized);
    CHECK(packet_radio_get_state(&composition.packet_radio) ==
        PACKET_RADIO_STATE_IDLE);
    CHECK(duplex_link_service_get_state(&composition.link_service) ==
        DUPLEX_LINK_STATE_IDLE);

    FakeStm32f4HalState *hal = fake_stm32f4_hal_state();
    CHECK(hal->delay_total_ms ==
        SX128X_RESET_PULSE_MS + SX128X_RESET_SETTLE_MS);
    CHECK(hal->spi_calls > 0U);
    CHECK(hal->transmit_calls > 0U);

    uint8_t payload[LINK_PACKET_MIN_PAYLOAD_SIZE];
    for (size_t i = 0U; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(0x30U + i);
    }

    CHECK(stm32f401_link_composition_start_data(
        &composition,
        payload,
        sizeof(payload)
    ));

    stm32f401_link_composition_process(&composition);

    CHECK(packet_radio_get_state(&composition.packet_radio) ==
        PACKET_RADIO_STATE_TX);
    CHECK(duplex_link_service_is_busy(&composition.link_service));
}

static void test_responder_init_and_start_rx(void)
{
    g_tests++;
    fake_stm32f4_hal_reset();

    SPI_HandleTypeDef spi = make_spi1();
    UART_HandleTypeDef uart = make_usart2();
    Stm32f401LinkCompositionConfig config;
    Stm32f401LinkComposition composition;

    CHECK(stm32f401_link_composition_config_init_defaults(
        &config,
        &spi,
        &uart,
        DUPLEX_LINK_ROLE_RESPONDER
    ));

    CHECK(stm32f401_link_composition_init(
        &composition,
        &config
    ));

    uint8_t response[LINK_PACKET_PAYLOAD_SIZE];
    memset(response, 0xA5, sizeof(response));

    CHECK(stm32f401_link_composition_set_response_payload(
        &composition,
        response,
        sizeof(response)
    ));

    CHECK(!stm32f401_link_composition_start_data(
        &composition,
        response,
        sizeof(response)
    ));

    stm32f401_link_composition_process(&composition);

    CHECK(packet_radio_get_state(&composition.packet_radio) ==
        PACKET_RADIO_STATE_RX);
    CHECK(duplex_link_service_get_state(&composition.link_service) ==
        DUPLEX_LINK_STATE_RESPONDER_RX);
}

static void test_invalid_runtime_config_is_rejected(void)
{
    g_tests++;
    fake_stm32f4_hal_reset();

    SPI_HandleTypeDef spi = make_spi1();
    UART_HandleTypeDef uart = make_usart2();
    Stm32f401LinkCompositionConfig config;
    Stm32f401LinkComposition composition;

    CHECK(stm32f401_link_composition_config_init_defaults(
        &config,
        &spi,
        &uart,
        DUPLEX_LINK_ROLE_INITIATOR
    ));

    config.lora.payload_length =
        (uint8_t)(LINK_PACKET_ENCODED_SIZE - 1U);

    CHECK(!stm32f401_link_composition_init(
        &composition,
        &config
    ));
}

static void test_dio1_irq_is_deferred(void)
{
    g_tests++;
    fake_stm32f4_hal_reset();

    SPI_HandleTypeDef spi = make_spi1();
    UART_HandleTypeDef uart = make_usart2();
    Stm32f401LinkCompositionConfig config;
    Stm32f401LinkComposition composition;

    CHECK(stm32f401_link_composition_config_init_defaults(
        &config,
        &spi,
        &uart,
        DUPLEX_LINK_ROLE_INITIATOR
    ));

    CHECK(stm32f401_link_composition_init(
        &composition,
        &config
    ));

    FakeStm32f4HalState *hal = fake_stm32f4_hal_state();
    const uint32_t spi_calls_before = hal->spi_calls;
    const uint32_t gpio_writes_before = hal->gpio_write_calls;

    stm32f401_link_composition_on_dio1_irq(
        &composition,
        GPIO_PIN_1
    );

    CHECK(hal->spi_calls == spi_calls_before);
    CHECK(hal->gpio_write_calls == gpio_writes_before);

    Stm32f4Sx128xPortStats stats;
    memset(&stats, 0, sizeof(stats));
    stm32f4_sx128x_port_get_stats(
        &composition.radio_port_backend,
        &stats
    );

    CHECK(stats.dio1_irq_events == 1U);
}

int main(void)
{
    test_default_configs();
    test_default_config_validation();
    test_initiator_init_and_start_tx();
    test_responder_init_and_start_rx();
    test_invalid_runtime_config_is_rejected();
    test_dio1_irq_is_deferred();

    if (g_failures != 0) {
        printf(
            "STM32F401 link composition tests failed: %d/%d\n",
            g_failures,
            g_tests
        );
        return 1;
    }

    printf(
        "All STM32F401 link composition tests passed (%d tests)\n",
        g_tests
    );
    return 0;
}
