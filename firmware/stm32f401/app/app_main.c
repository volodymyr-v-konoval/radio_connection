#include "app_main.h"

#include <stddef.h>
#include <string.h>

#include "board_validation.h"
#include "link_composition.h"
#include "node_config.h"
#include "spi.h"
#include "usart.h"

#define APP_UART_TIMEOUT_MS 100U
#define APP_TX_INTERVAL_MS 1000U

static Stm32f401LinkComposition g_link_composition;
static bool g_app_initialized = false;
static uint32_t g_last_tx_ms = 0U;

static const uint8_t g_initiator_payload[16] = {
    'N', 'O', 'D', 'E',
    '-', 'A', '-',
    'D', 'A', 'T', 'A',
    '-', '0', '0', '0', '1'
};

static const uint8_t g_responder_payload[16] = {
    0x42U, 0x2DU, 0x52U, 0x45U,
    0x53U, 0x50U, 0x4FU, 0x4EU,
    0x53U, 0x45U, 0x2DU, 0x44U,
    0x41U, 0x54U, 0x41U, 0x21U
};

static void app_uart_write(const char *text)
{
    if (text == NULL) {
        return;
    }

    const size_t length = strlen(text);

    if (length == 0U || length > UINT16_MAX) {
        return;
    }

    (void)HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        (uint16_t)length,
        APP_UART_TIMEOUT_MS
    );
}

static char app_hex_digit(uint8_t value)
{
    value &= 0x0FU;

    if (value < 10U) {
        return (char)('0' + value);
    }

    return (char)('A' + (value - 10U));
}

static void app_uart_write_radio_status(uint8_t status)
{
    char line[] = "[SX128X] GetStatus=0x00\r\n";

    line[21] = app_hex_digit((uint8_t)(status >> 4U));
    line[22] = app_hex_digit(status);

    app_uart_write(line);
}

static void app_uart_write_bytes(
    const uint8_t *data,
    size_t length
)
{
    if (data == NULL || length == 0U || length > UINT16_MAX) {
        return;
    }

    (void)HAL_UART_Transmit(
        &huart2,
        (uint8_t *)data,
        (uint16_t)length,
        APP_UART_TIMEOUT_MS
    );
}

static void app_uart_write_u32(uint32_t value)
{
    char buffer[10];
    size_t position = sizeof(buffer);

    do {
        buffer[--position] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    app_uart_write_bytes(
        (const uint8_t *)&buffer[position],
        sizeof(buffer) - position
    );
}

static void app_uart_write_i32(int32_t value)
{
    if (value < 0) {
        app_uart_write("-");
        app_uart_write_u32((uint32_t)(-value));
        return;
    }

    app_uart_write_u32((uint32_t)value);
}

static void app_process_link_events(void)
{
    DuplexLinkEvent event;

    while (stm32f401_link_composition_take_event(
            &g_link_composition,
            &event)) {
        switch (event.type) {
            case DUPLEX_LINK_EVENT_REQUEST_RECEIVED:
                app_uart_write("[RX] request seq=");
                app_uart_write_u32(event.packet.sequence);

                app_uart_write(" len=");
                app_uart_write_u32(event.packet.payload_length);

                app_uart_write(" payload=");
                app_uart_write_bytes(
                    event.packet.payload,
                    event.packet.payload_length
                );

                app_uart_write(" rssi_x2=");
                app_uart_write_i32(event.rssi_dbm_x2);

                app_uart_write(" snr_x4=");
                app_uart_write_i32(event.snr_db_x4);

                app_uart_write("\r\n");
                break;

            case DUPLEX_LINK_EVENT_EXCHANGE_SUCCEEDED:
                app_uart_write("[APP] exchange OK ack=");
                app_uart_write_u32(
                    event.packet.acknowledged_sequence
                );

                app_uart_write(" attempts=");
                app_uart_write_u32(event.attempts);

                app_uart_write(" rtt_ms=");
                app_uart_write_u32(event.rtt_ms);

                app_uart_write(" rssi_x2=");
                app_uart_write_i32(event.rssi_dbm_x2);

                app_uart_write(" snr_x4=");
                app_uart_write_i32(event.snr_db_x4);

                app_uart_write("\r\n");
                break;

            case DUPLEX_LINK_EVENT_EXCHANGE_FAILED:
                app_uart_write("[APP] exchange FAILED seq=");
                app_uart_write_u32(event.packet.sequence);

                app_uart_write(" attempts=");
                app_uart_write_u32(event.attempts);

                app_uart_write("\r\n");
                break;

            case DUPLEX_LINK_EVENT_DUPLICATE_SUPPRESSED:
                app_uart_write("[RX] duplicate suppressed seq=");
                app_uart_write_u32(event.packet.sequence);
                app_uart_write("\r\n");
                break;

            case DUPLEX_LINK_EVENT_INVALID_PACKET:
                app_uart_write("[RX] invalid packet reason=");
                app_uart_write_u32(event.invalid_reason);
                app_uart_write("\r\n");
                break;

            case DUPLEX_LINK_EVENT_DEVICE_ERROR:
                app_uart_write("[SX128X] device error=");
                app_uart_write_u32(event.device_error_code);
                app_uart_write("\r\n");
                break;

            case DUPLEX_LINK_EVENT_NONE:
            default:
                break;
        }
    }
}

bool app_main_init(void)
{
    Stm32f401LinkCompositionConfig config;

    g_app_initialized = false;

    app_uart_write("\r\n[BOOT] STM32F401 SX128x link\r\n");
    app_uart_write("[BOOT] role=" NODE_CONFIG_NAME "\r\n");

    if (!stm32f401_board_validation_run()) {
        app_uart_write("[BOARD] validation FAILED\r\n");
        return false;
    }

    app_uart_write("[BOARD] validation OK\r\n");

    if (!stm32f401_link_composition_config_init_defaults(
            &config,
            &hspi1,
            &huart2,
            NODE_CONFIG_DUPLEX_ROLE)) {
        app_uart_write("[LINK] config initialization FAILED\r\n");
        return false;
    }

    app_uart_write("[SX128X] initialization started\r\n");

    if (!stm32f401_link_composition_init(
            &g_link_composition,
            &config)) {
        app_uart_write("[SX128X] initialization FAILED\r\n");
        return false;
    }

    uint8_t radio_status = 0U;

    if (sx128x_get_status(
            &g_link_composition.radio_device,
            &radio_status) != SX128X_RESULT_OK) {
        app_uart_write("[SX128X] GetStatus FAILED\r\n");
        return false;
    }

    app_uart_write_radio_status(radio_status);

#if LORA_NODE_ROLE == NODE_ROLE_RESPONDER
    if (!stm32f401_link_composition_set_response_payload(
            &g_link_composition,
            g_responder_payload,
            sizeof(g_responder_payload))) {
        app_uart_write("[LINK] response payload setup FAILED\r\n");
        return false;
    }
#else
    (void)g_responder_payload;
#endif

    g_last_tx_ms = HAL_GetTick();
    g_app_initialized = true;

    app_uart_write(
        "[SX128X] init OK freq=2445000000 sf=6 bw=812500\r\n"
    );
    app_uart_write("[LINK] service ready\r\n");

    return true;
}

void app_main_process(void)
{
    if (!g_app_initialized) {
        return;
    }

    stm32f401_link_composition_process(
        &g_link_composition
    );

    app_process_link_events();

#if LORA_NODE_ROLE == NODE_ROLE_INITIATOR
    const uint32_t now_ms = HAL_GetTick();

    if ((uint32_t)(now_ms - g_last_tx_ms) >=
        APP_TX_INTERVAL_MS) {
        if (stm32f401_link_composition_start_data(
                &g_link_composition,
                g_initiator_payload,
                sizeof(g_initiator_payload))) {
            g_last_tx_ms = now_ms;
            app_uart_write(
                "[TX] DATA started payload=NODE-A-DATA-0001\r\n"
            );
        }
    }
#endif
}

void app_main_on_gpio_exti(uint16_t gpio_pin)
{
    if (!g_app_initialized) {
        return;
    }

    stm32f401_link_composition_on_dio1_irq(
        &g_link_composition,
        gpio_pin
    );
}
