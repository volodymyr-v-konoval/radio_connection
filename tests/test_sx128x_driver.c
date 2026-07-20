#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_sx128x_port.h"
#include "sx128x.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));    \
            return false;                        \
        }                                        \
    } while (0)

static bool setup_driver(
    Sx128x *device,
    Sx128xPort *port,
    FakeSx128xPortContext *context,
    uint32_t busy_timeout_ms
)
{
    return
        fake_sx128x_port_init(port, context) &&
        sx128x_init(
            device,
            port,
            busy_timeout_ms
        ) == SX128X_RESULT_OK;
}

static bool spi_log_matches(
    const FakeSx128xPortContext *context,
    const uint8_t *expected,
    size_t expected_length
)
{
    return
        context->spi_tx_count == expected_length &&
        memcmp(
            context->spi_tx_log,
            expected,
            expected_length
        ) == 0;
}

static bool test_init_validation(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        fake_sx128x_port_init(&port, &context),
        "Fake port should initialize"
    );

    TEST_ASSERT(
        sx128x_init(NULL, &port, 10U) ==
            SX128X_RESULT_INVALID_ARGUMENT,
        "NULL device should be rejected"
    );

    TEST_ASSERT(
        sx128x_init(&device, &port, 0U) ==
            SX128X_RESULT_INVALID_ARGUMENT,
        "Zero BUSY timeout should be rejected"
    );

    port.spi_transfer = NULL;

    TEST_ASSERT(
        sx128x_init(&device, &port, 10U) ==
            SX128X_RESULT_INVALID_PORT,
        "Incomplete port should be rejected"
    );

    return true;
}

static bool test_set_standby_spi_frame(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    TEST_ASSERT(
        sx128x_set_standby(
            &device,
            SX128X_STANDBY_RC
        ) == SX128X_RESULT_OK,
        "STDBY_RC command should succeed"
    );

    static const uint8_t expected[] = {
        0x80U,
        0x00U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "SetStandby SPI frame should match datasheet"
    );

    TEST_ASSERT(
        context.nss_low_calls == 1U,
        "SetStandby should assert NSS once"
    );

    TEST_ASSERT(
        context.nss_high,
        "SetStandby should release NSS"
    );

    return true;
}

static bool test_get_status_spi_frame(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;
    uint8_t status = 0U;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    static const uint8_t response_script[] = {
        0xA4U
    };

    TEST_ASSERT(
        fake_sx128x_port_set_rx_script(
            &context,
            response_script,
            sizeof(response_script)
        ),
        "Status response should be scripted"
    );

    TEST_ASSERT(
        sx128x_get_status(&device, &status) ==
            SX128X_RESULT_OK,
        "GetStatus should succeed"
    );

    static const uint8_t expected[] = {
        0xC0U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "GetStatus should transfer only opcode 0xC0"
    );

    TEST_ASSERT(
        status == 0xA4U,
        "GetStatus should return same-transaction status"
    );

    return true;
}

static bool test_read_command_spi_frame(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;
    uint8_t irq_status[2] = { 0U, 0U };

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    static const uint8_t response_script[] = {
        0x00U,
        0x00U,
        0x12U,
        0x34U
    };

    TEST_ASSERT(
        fake_sx128x_port_set_rx_script(
            &context,
            response_script,
            sizeof(response_script)
        ),
        "IRQ response should be scripted"
    );

    TEST_ASSERT(
        sx128x_read_command(
            &device,
            SX128X_COMMAND_GET_IRQ_STATUS,
            irq_status,
            sizeof(irq_status)
        ) == SX128X_RESULT_OK,
        "GetIrqStatus framing should succeed"
    );

    static const uint8_t expected_tx[] = {
        0x15U,
        0x00U,
        0x00U,
        0x00U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected_tx,
            sizeof(expected_tx)
        ),
        "Read command SPI frame should match datasheet"
    );

    TEST_ASSERT(
        irq_status[0] == 0x12U &&
        irq_status[1] == 0x34U,
        "Read command should return response bytes"
    );

    return true;
}

static bool test_busy_wait_and_timeout(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            4U
        ),
        "Driver should initialize"
    );

    context.busy_release_at_ms = 3U;

    TEST_ASSERT(
        sx128x_set_standby(
            &device,
            SX128X_STANDBY_XOSC
        ) == SX128X_RESULT_OK,
        "Command should wait for BUSY release"
    );

    TEST_ASSERT(
        context.now_ms == 3U,
        "BUSY wait should advance by three milliseconds"
    );

    fake_sx128x_port_clear_spi_log(&context);
    context.busy_stuck_high = true;

    TEST_ASSERT(
        sx128x_set_standby(
            &device,
            SX128X_STANDBY_RC
        ) == SX128X_RESULT_BUSY_TIMEOUT,
        "Stuck BUSY should produce finite timeout"
    );

    TEST_ASSERT(
        context.spi_tx_count == 0U,
        "No SPI bytes should be sent after BUSY timeout"
    );

    return true;
}

static bool test_spi_failure_releases_nss(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);
    context.fail_spi_on_call = 2U;

    TEST_ASSERT(
        sx128x_set_standby(
            &device,
            SX128X_STANDBY_RC
        ) == SX128X_RESULT_SPI_ERROR,
        "Parameter transfer failure should be reported"
    );

    TEST_ASSERT(
        context.nss_high,
        "NSS must be released after SPI failure"
    );

    return true;
}

static bool test_hardware_reset_sequence(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    const size_t initial_reset_high_calls =
        context.reset_high_calls;

    TEST_ASSERT(
        sx128x_hardware_reset(&device) ==
            SX128X_RESULT_OK,
        "Hardware reset should succeed"
    );

    TEST_ASSERT(
        context.reset_low_calls == 1U,
        "Reset should be driven low once"
    );

    TEST_ASSERT(
        context.reset_high_calls ==
            initial_reset_high_calls + 1U,
        "Reset should be driven high after pulse"
    );

    TEST_ASSERT(
        context.total_delay_ms ==
            SX128X_RESET_PULSE_MS +
            SX128X_RESET_SETTLE_MS,
        "Reset delays should match driver constants"
    );

    TEST_ASSERT(
        context.reset_high && context.nss_high,
        "Reset and NSS should finish high"
    );

    return true;
}

static bool test_lora_frequency_and_modulation_frames(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    TEST_ASSERT(
        sx128x_set_packet_type(
            &device,
            SX128X_PACKET_TYPE_LORA
        ) == SX128X_RESULT_OK,
        "LoRa packet type should configure"
    );

    TEST_ASSERT(
        sx128x_set_rf_frequency(
            &device,
            2445000000U
        ) == SX128X_RESULT_OK,
        "2445 MHz should configure"
    );

    TEST_ASSERT(
        sx128x_set_lora_modulation_params(
            &device,
            SX128X_LORA_SF6,
            SX128X_LORA_BW_812_5_KHZ,
            SX128X_LORA_CR_4_5
        ) == SX128X_RESULT_OK,
        "SF6 BW812.5 CR4/5 should configure"
    );

    static const uint8_t expected[] = {
        0x8AU, 0x01U,
        0x86U, 0xBCU, 0x13U, 0xB1U,
        0x8BU, 0x60U, 0x18U, 0x01U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "LoRa type, frequency, and modulation frames should match"
    );

    TEST_ASSERT(
        sx128x_set_rf_frequency(
            &device,
            SX128X_RF_FREQUENCY_MIN_HZ - 1U
        ) == SX128X_RESULT_OUT_OF_RANGE,
        "Frequency below 2.4 GHz should fail"
    );

    return true;
}

static bool test_lora_packet_and_tx_parameter_frames(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    TEST_ASSERT(
        sx128x_set_buffer_base_address(
            &device,
            0x00U,
            0x80U
        ) == SX128X_RESULT_OK,
        "Buffer base addresses should configure"
    );

    TEST_ASSERT(
        sx128x_set_lora_packet_params(
            &device,
            12U,
            SX128X_LORA_HEADER_EXPLICIT,
            30U,
            SX128X_LORA_CRC_ON,
            SX128X_LORA_IQ_NORMAL
        ) == SX128X_RESULT_OK,
        "30-byte explicit CRC packet should configure"
    );

    TEST_ASSERT(
        sx128x_set_tx_params(
            &device,
            10,
            SX128X_RAMP_20_US
        ) == SX128X_RESULT_OK,
        "+10 dBm should configure"
    );

    static const uint8_t expected[] = {
        0x8FU, 0x00U, 0x80U,
        0x8CU, 0x0CU, 0x00U, 0x1EU, 0x20U, 0x40U,
        0x8EU, 0x1CU, 0xE0U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "Buffer, packet, and TX parameter frames should match"
    );

    TEST_ASSERT(
        sx128x_set_tx_params(
            &device,
            14,
            SX128X_RAMP_20_US
        ) == SX128X_RESULT_OUT_OF_RANGE,
        "TX power above chip limit should fail"
    );

    return true;
}

static bool test_irq_routing_status_and_clear(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;
    uint16_t irq_status = 0U;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    TEST_ASSERT(
        sx128x_set_dio_irq_params(
            &device,
            SX128X_DEFAULT_LINK_IRQ_MASK,
            SX128X_DEFAULT_LINK_IRQ_MASK,
            SX128X_IRQ_NONE,
            SX128X_IRQ_NONE
        ) == SX128X_RESULT_OK,
        "DIO1 IRQ routing should configure"
    );

    static const uint8_t irq_response[] = {
        0x00U,
        0x00U,
        0x40U,
        0x42U
    };

    TEST_ASSERT(
        fake_sx128x_port_set_rx_script(
            &context,
            irq_response,
            sizeof(irq_response)
        ),
        "IRQ response should be scripted"
    );

    TEST_ASSERT(
        sx128x_get_irq_status(
            &device,
            &irq_status
        ) == SX128X_RESULT_OK,
        "IRQ status should read"
    );

    TEST_ASSERT(
        irq_status == 0x4042U,
        "IRQ status should decode big-endian"
    );

    TEST_ASSERT(
        sx128x_clear_irq_status(
            &device,
            irq_status
        ) == SX128X_RESULT_OK,
        "IRQ status should clear"
    );

    static const uint8_t expected[] = {
        0x8DU,
        0x40U, 0x43U,
        0x40U, 0x43U,
        0x00U, 0x00U,
        0x00U, 0x00U,
        0x15U, 0x00U, 0x00U, 0x00U,
        0x97U, 0x40U, 0x42U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "IRQ routing, read, and clear frames should match"
    );

    return true;
}

static bool test_buffer_write_and_read_frames(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    static const uint8_t tx_payload[] = {
        0x11U,
        0x22U,
        0x33U
    };

    TEST_ASSERT(
        sx128x_write_buffer(
            &device,
            0x20U,
            tx_payload,
            sizeof(tx_payload)
        ) == SX128X_RESULT_OK,
        "TX buffer write should succeed"
    );

    static const uint8_t rx_script[] = {
        0x00U,
        0x00U,
        0x00U,
        0xA1U,
        0xB2U,
        0xC3U
    };

    TEST_ASSERT(
        fake_sx128x_port_set_rx_script(
            &context,
            rx_script,
            sizeof(rx_script)
        ),
        "RX buffer data should be scripted"
    );

    uint8_t rx_payload[3] = { 0U, 0U, 0U };

    TEST_ASSERT(
        sx128x_read_buffer(
            &device,
            0x40U,
            rx_payload,
            sizeof(rx_payload)
        ) == SX128X_RESULT_OK,
        "RX buffer read should succeed"
    );

    static const uint8_t expected[] = {
        0x1AU, 0x20U, 0x11U, 0x22U, 0x33U,
        0x1BU, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "Buffer SPI frames should match"
    );

    TEST_ASSERT(
        memcmp(
            rx_payload,
            &rx_script[3],
            sizeof(rx_payload)
        ) == 0,
        "RX buffer bytes should be returned"
    );

    TEST_ASSERT(
        sx128x_write_buffer(
            &device,
            255U,
            tx_payload,
            sizeof(tx_payload)
        ) == SX128X_RESULT_INVALID_ARGUMENT,
        "Buffer overflow should be rejected"
    );

    return true;
}

static bool test_rx_buffer_and_packet_diagnostics(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;
    Sx128xRxBufferStatus buffer_status;
    Sx128xLoRaPacketStatus packet_status;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    static const uint8_t rx_buffer_script[] = {
        0x00U,
        0x00U,
        30U,
        0x80U
    };

    TEST_ASSERT(
        fake_sx128x_port_set_rx_script(
            &context,
            rx_buffer_script,
            sizeof(rx_buffer_script)
        ),
        "RX buffer status should be scripted"
    );

    TEST_ASSERT(
        sx128x_get_rx_buffer_status(
            &device,
            &buffer_status
        ) == SX128X_RESULT_OK,
        "RX buffer status should read"
    );

    TEST_ASSERT(
        buffer_status.payload_length == 30U &&
        buffer_status.start_buffer_pointer == 0x80U,
        "RX buffer status should decode"
    );

    fake_sx128x_port_clear_spi_log(&context);

    static const uint8_t packet_status_script[] = {
        0x00U,
        0x00U,
        171U,
        29U,
        0x00U,
        0x00U,
        0x00U
    };

    TEST_ASSERT(
        fake_sx128x_port_set_rx_script(
            &context,
            packet_status_script,
            sizeof(packet_status_script)
        ),
        "Packet status should be scripted"
    );

    TEST_ASSERT(
        sx128x_get_lora_packet_status(
            &device,
            &packet_status
        ) == SX128X_RESULT_OK,
        "LoRa packet status should read"
    );

    TEST_ASSERT(
        packet_status.rssi_dbm_x2 == -171,
        "RSSI should decode in half-dBm units"
    );

    TEST_ASSERT(
        packet_status.snr_db_x4 == 29,
        "SNR should decode in quarter-dB units"
    );

    return true;
}

static bool test_tx_rx_timeout_frames_and_dio_event(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    TEST_ASSERT(
        sx128x_set_tx(
            &device,
            SX128X_TIMEOUT_BASE_1_MS,
            100U
        ) == SX128X_RESULT_OK,
        "Timed TX should start"
    );

    TEST_ASSERT(
        sx128x_set_rx(
            &device,
            SX128X_TIMEOUT_BASE_1_MS,
            0xFFFFU
        ) == SX128X_RESULT_OK,
        "Continuous RX encoding should start"
    );

    static const uint8_t expected[] = {
        0x83U, 0x02U, 0x00U, 0x64U,
        0x82U, 0x02U, 0xFFU, 0xFFU
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "TX and RX timeout frames should match"
    );

    TEST_ASSERT(
        !sx128x_take_dio1_event(&device),
        "DIO1 should initially be clear"
    );

    context.dio1_pending = true;

    TEST_ASSERT(
        sx128x_take_dio1_event(&device),
        "Pending DIO1 should be consumed"
    );

    TEST_ASSERT(
        !sx128x_take_dio1_event(&device),
        "DIO1 event should only be consumed once"
    );

    return true;
}

static bool test_complete_lora_configuration_sequence(void)
{
    Sx128x device;
    Sx128xPort port;
    FakeSx128xPortContext context;

    TEST_ASSERT(
        setup_driver(
            &device,
            &port,
            &context,
            10U
        ),
        "Driver should initialize"
    );

    fake_sx128x_port_clear_spi_log(&context);

    const Sx128xLoRaConfig config = {
        .frequency_hz = 2445000000U,
        .spreading_factor = SX128X_LORA_SF6,
        .bandwidth = SX128X_LORA_BW_812_5_KHZ,
        .coding_rate = SX128X_LORA_CR_4_5,
        .preamble_symbols = 12U,
        .header_type = SX128X_LORA_HEADER_EXPLICIT,
        .payload_length = 30U,
        .crc_mode = SX128X_LORA_CRC_ON,
        .iq_mode = SX128X_LORA_IQ_NORMAL,
        .tx_power_dbm = 10,
        .ramp_time = SX128X_RAMP_20_US,
        .tx_base_address = 0x00U,
        .rx_base_address = 0x80U,
        .irq_mask = SX128X_DEFAULT_LINK_IRQ_MASK,
        .dio1_mask = SX128X_DEFAULT_LINK_IRQ_MASK
    };

    TEST_ASSERT(
        sx128x_configure_lora(
            &device,
            &config
        ) == SX128X_RESULT_OK,
        "Complete LoRa configuration should succeed"
    );

    static const uint8_t expected[] = {
        0x80U, 0x00U,
        0x8AU, 0x01U,
        0x86U, 0xBCU, 0x13U, 0xB1U,
        0x8FU, 0x00U, 0x80U,
        0x8BU, 0x60U, 0x18U, 0x01U,
        0x8CU, 0x0CU, 0x00U, 0x1EU, 0x20U, 0x40U,
        0x8EU, 0x1CU, 0xE0U,
        0x8DU,
        0x40U, 0x43U,
        0x40U, 0x43U,
        0x00U, 0x00U,
        0x00U, 0x00U
    };

    TEST_ASSERT(
        spi_log_matches(
            &context,
            expected,
            sizeof(expected)
        ),
        "Complete SF6/BW812.5 configuration should match"
    );

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_init_validation()) {
        failures++;
    }

    if (!test_set_standby_spi_frame()) {
        failures++;
    }

    if (!test_get_status_spi_frame()) {
        failures++;
    }

    if (!test_read_command_spi_frame()) {
        failures++;
    }

    if (!test_busy_wait_and_timeout()) {
        failures++;
    }

    if (!test_spi_failure_releases_nss()) {
        failures++;
    }

    if (!test_hardware_reset_sequence()) {
        failures++;
    }

    if (!test_lora_frequency_and_modulation_frames()) {
        failures++;
    }

    if (!test_lora_packet_and_tx_parameter_frames()) {
        failures++;
    }

    if (!test_irq_routing_status_and_clear()) {
        failures++;
    }

    if (!test_buffer_write_and_read_frames()) {
        failures++;
    }

    if (!test_rx_buffer_and_packet_diagnostics()) {
        failures++;
    }

    if (!test_tx_rx_timeout_frames_and_dio_event()) {
        failures++;
    }

    if (!test_complete_lora_configuration_sequence()) {
        failures++;
    }

    if (failures != 0) {
        printf(
            "%d SX128x driver test(s) failed\n",
            failures
        );

        return 1;
    }

    printf("All SX128x driver tests passed\n");
    return 0;
}
