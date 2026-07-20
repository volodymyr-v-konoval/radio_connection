#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_sx128x_port.h"
#include "packet_radio_if.h"
#include "sx128x_packet_radio.h"

#define TEST_FRAME_LENGTH 30U

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));    \
            return false;                        \
        }                                        \
    } while (0)

typedef struct
{
    Sx128xPort port;
    FakeSx128xPortContext port_context;
    Sx128x device;
    PacketRadio radio;
    Sx128xPacketRadio adapter;
} TestFixture;

static Sx128xPacketRadioConfig test_config(void)
{
    const Sx128xPacketRadioConfig config = {
        .tx_buffer_offset = 0U,
        .frame_length = TEST_FRAME_LENGTH,
        .tx_timeout_base = SX128X_TIMEOUT_BASE_1_MS,
        .tx_timeout_count = 100U
    };

    return config;
}

static bool setup_fixture(TestFixture *fixture)
{
    if (fixture == NULL) {
        return false;
    }

    memset(fixture, 0, sizeof(*fixture));

    if (!fake_sx128x_port_init(
            &fixture->port,
            &fixture->port_context)) {
        return false;
    }

    if (sx128x_init(
            &fixture->device,
            &fixture->port,
            SX128X_DEFAULT_BUSY_TIMEOUT_MS) !=
        SX128X_RESULT_OK) {
        return false;
    }

    const Sx128xPacketRadioConfig config =
        test_config();

    return sx128x_packet_radio_init(
        &fixture->radio,
        &fixture->adapter,
        &fixture->device,
        &config
    );
}

static void fill_frame(uint8_t *frame)
{
    for (size_t index = 0U;
         index < TEST_FRAME_LENGTH;
         index++) {
        frame[index] = (uint8_t)(0x20U + index);
    }
}

static bool bytes_equal(
    const uint8_t *expected,
    const uint8_t *actual,
    size_t length
)
{
    return memcmp(expected, actual, length) == 0;
}

static bool set_irq_script(
    FakeSx128xPortContext *context,
    uint16_t irq_status
)
{
    const uint8_t script[] = {
        0U,
        0U,
        (uint8_t)(irq_status >> 8U),
        (uint8_t)irq_status,
        0U,
        0U,
        0U
    };

    return fake_sx128x_port_set_rx_script(
        context,
        script,
        sizeof(script)
    );
}

static bool set_rx_done_script(
    FakeSx128xPortContext *context,
    const uint8_t *frame,
    uint8_t payload_length,
    uint8_t start_pointer,
    uint8_t rssi_raw,
    int8_t snr_raw
)
{
    uint8_t script[51U] = { 0U };

    script[2] = 0x00U;
    script[3] = 0x02U;

    script[9] = payload_length;
    script[10] = start_pointer;

    memcpy(
        &script[14],
        frame,
        TEST_FRAME_LENGTH
    );

    script[46] = rssi_raw;
    script[47] = (uint8_t)snr_raw;

    return fake_sx128x_port_set_rx_script(
        context,
        script,
        sizeof(script)
    );
}

static bool test_init_validation(void)
{
    TestFixture fixture;
    memset(&fixture, 0, sizeof(fixture));

    Sx128xPacketRadioConfig config = test_config();

    TEST_ASSERT(
        !sx128x_packet_radio_init(
            NULL,
            &fixture.adapter,
            &fixture.device,
            &config
        ),
        "NULL PacketRadio should be rejected"
    );

    TEST_ASSERT(
        !sx128x_packet_radio_init(
            &fixture.radio,
            &fixture.adapter,
            &fixture.device,
            &config
        ),
        "Uninitialized SX128x should be rejected"
    );

    TEST_ASSERT(
        fake_sx128x_port_init(
            &fixture.port,
            &fixture.port_context
        ),
        "Fake port should initialize"
    );

    TEST_ASSERT(
        sx128x_init(
            &fixture.device,
            &fixture.port,
            SX128X_DEFAULT_BUSY_TIMEOUT_MS
        ) == SX128X_RESULT_OK,
        "SX128x should initialize"
    );

    config.frame_length = 0U;

    TEST_ASSERT(
        !sx128x_packet_radio_init(
            &fixture.radio,
            &fixture.adapter,
            &fixture.device,
            &config
        ),
        "Zero frame length should be rejected"
    );

    config = test_config();
    config.tx_buffer_offset = 250U;

    TEST_ASSERT(
        !sx128x_packet_radio_init(
            &fixture.radio,
            &fixture.adapter,
            &fixture.device,
            &config
        ),
        "TX buffer overflow should be rejected"
    );

    config = test_config();
    config.tx_timeout_count = UINT16_MAX;

    TEST_ASSERT(
        !sx128x_packet_radio_init(
            &fixture.radio,
            &fixture.adapter,
            &fixture.device,
            &config
        ),
        "Continuous TX timeout should be rejected"
    );

    config = test_config();
    config.tx_timeout_base = (Sx128xTimeoutBase)0xFFU;

    TEST_ASSERT(
        !sx128x_packet_radio_init(
            &fixture.radio,
            &fixture.adapter,
            &fixture.device,
            &config
        ),
        "Invalid TX timeout base should be rejected"
    );

    return true;
}

static bool test_tx_start_and_tx_done(void)
{
    TestFixture fixture;
    uint8_t frame[TEST_FRAME_LENGTH];

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    fill_frame(frame);
    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        packet_radio_try_start_tx(
            &fixture.radio,
            frame,
            sizeof(frame)
        ),
        "TX should start"
    );

    TEST_ASSERT(
        packet_radio_get_state(&fixture.radio) ==
            PACKET_RADIO_STATE_TX,
        "Adapter should enter TX state"
    );

    uint8_t expected[39U] = {
        0x97U, 0xFFU, 0xFFU,
        0x1AU, 0x00U
    };

    memcpy(&expected[5], frame, sizeof(frame));
    expected[35] = 0x83U;
    expected[36] = 0x02U;
    expected[37] = 0x00U;
    expected[38] = 0x64U;

    TEST_ASSERT(
        fixture.port_context.spi_tx_count ==
            sizeof(expected),
        "TX SPI frame length should match"
    );

    TEST_ASSERT(
        bytes_equal(
            expected,
            fixture.port_context.spi_tx_log,
            sizeof(expected)
        ),
        "TX SPI commands should match"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        set_irq_script(
            &fixture.port_context,
            SX128X_IRQ_TX_DONE
        ),
        "TX_DONE script should load"
    );

    fixture.port_context.dio1_pending = true;
    packet_radio_process(&fixture.radio);

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(
            &fixture.radio,
            &event
        ),
        "TX_DONE event should be available"
    );

    TEST_ASSERT(
        event.type == PACKET_RADIO_EVENT_TX_DONE,
        "Event should report TX_DONE"
    );

    TEST_ASSERT(
        packet_radio_get_state(&fixture.radio) ==
            PACKET_RADIO_STATE_IDLE,
        "TX_DONE should return adapter to idle"
    );

    return true;
}

static bool test_rx_timeout_encoding(void)
{
    TestFixture fixture;

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        packet_radio_try_start_rx(
            &fixture.radio,
            40U
        ),
        "Timed RX should start"
    );

    const uint8_t expected_timed[] = {
        0x97U, 0xFFU, 0xFFU,
        0x82U, 0x02U, 0x00U, 0x28U
    };

    TEST_ASSERT(
        fixture.port_context.spi_tx_count ==
            sizeof(expected_timed) &&
        bytes_equal(
            expected_timed,
            fixture.port_context.spi_tx_log,
            sizeof(expected_timed)
        ),
        "Timed RX should use a 1 ms base"
    );

    TEST_ASSERT(
        packet_radio_recover(&fixture.radio),
        "Adapter should recover to idle"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        packet_radio_try_start_rx(
            &fixture.radio,
            0U
        ),
        "Continuous RX should start"
    );

    const uint8_t expected_continuous[] = {
        0x97U, 0xFFU, 0xFFU,
        0x82U, 0x02U, 0xFFU, 0xFFU
    };

    TEST_ASSERT(
        fixture.port_context.spi_tx_count ==
            sizeof(expected_continuous) &&
        bytes_equal(
            expected_continuous,
            fixture.port_context.spi_tx_log,
            sizeof(expected_continuous)
        ),
        "Continuous RX should use count 0xFFFF"
    );

    return true;
}

static bool test_rx_done_frame_and_diagnostics(void)
{
    TestFixture fixture;
    uint8_t expected_frame[TEST_FRAME_LENGTH];

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    fill_frame(expected_frame);

    TEST_ASSERT(
        packet_radio_try_start_rx(
            &fixture.radio,
            25U
        ),
        "RX should start"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        set_rx_done_script(
            &fixture.port_context,
            expected_frame,
            TEST_FRAME_LENGTH,
            0x80U,
            171U,
            29
        ),
        "RX_DONE script should load"
    );

    fixture.port_context.dio1_pending = true;
    packet_radio_process(&fixture.radio);

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(
            &fixture.radio,
            &event
        ) &&
        event.type == PACKET_RADIO_EVENT_RX_DONE,
        "RX_DONE event should be reported"
    );

    PacketRadioRxFrame frame;

    TEST_ASSERT(
        packet_radio_read_rx_frame(
            &fixture.radio,
            &frame
        ),
        "Received frame should be readable"
    );

    TEST_ASSERT(
        frame.length == TEST_FRAME_LENGTH,
        "RX frame length should match"
    );

    TEST_ASSERT(
        bytes_equal(
            expected_frame,
            frame.data,
            TEST_FRAME_LENGTH
        ),
        "RX payload should match"
    );

    TEST_ASSERT(
        frame.rssi_dbm_x2 == -171,
        "RSSI should preserve half-dBm units"
    );

    TEST_ASSERT(
        frame.snr_db_x4 == 29,
        "SNR should preserve quarter-dB units"
    );

    TEST_ASSERT(
        !packet_radio_read_rx_frame(
            &fixture.radio,
            &frame
        ),
        "RX frame should be consumed once"
    );

    TEST_ASSERT(
        packet_radio_get_state(&fixture.radio) ==
            PACKET_RADIO_STATE_IDLE,
        "RX_DONE should return adapter to idle"
    );

    return true;
}

static bool test_crc_error_has_priority(void)
{
    TestFixture fixture;

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    TEST_ASSERT(
        packet_radio_try_start_rx(
            &fixture.radio,
            10U
        ),
        "RX should start"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        set_irq_script(
            &fixture.port_context,
            (uint16_t)(
                SX128X_IRQ_RX_DONE |
                SX128X_IRQ_CRC_ERROR
            )
        ),
        "CRC error script should load"
    );

    fixture.port_context.dio1_pending = true;
    packet_radio_process(&fixture.radio);

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(
            &fixture.radio,
            &event
        ) &&
        event.type == PACKET_RADIO_EVENT_CRC_ERROR,
        "CRC error should take priority over RX_DONE"
    );

    PacketRadioRxFrame frame;

    TEST_ASSERT(
        !packet_radio_read_rx_frame(
            &fixture.radio,
            &frame
        ),
        "CRC error should not expose a frame"
    );

    return true;
}

static bool test_rx_timeout_event(void)
{
    TestFixture fixture;

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    TEST_ASSERT(
        packet_radio_try_start_rx(
            &fixture.radio,
            10U
        ),
        "RX should start"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        set_irq_script(
            &fixture.port_context,
            SX128X_IRQ_RX_TX_TIMEOUT
        ),
        "Timeout script should load"
    );

    fixture.port_context.dio1_pending = true;
    packet_radio_process(&fixture.radio);

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(
            &fixture.radio,
            &event
        ) &&
        event.type == PACKET_RADIO_EVENT_RX_TIMEOUT,
        "RX timeout event should be reported"
    );

    TEST_ASSERT(
        packet_radio_get_state(&fixture.radio) ==
            PACKET_RADIO_STATE_IDLE,
        "RX timeout should return adapter to idle"
    );

    return true;
}

static bool test_driver_error_and_recovery(void)
{
    TestFixture fixture;
    uint8_t frame[TEST_FRAME_LENGTH];

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    fill_frame(frame);
    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );
    fixture.port_context.fail_spi_on_call = 1U;

    TEST_ASSERT(
        !packet_radio_try_start_tx(
            &fixture.radio,
            frame,
            sizeof(frame)
        ),
        "SPI failure should reject TX"
    );

    TEST_ASSERT(
        packet_radio_get_state(&fixture.radio) ==
            PACKET_RADIO_STATE_ERROR,
        "SPI failure should enter error state"
    );

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(
            &fixture.radio,
            &event
        ) &&
        event.type == PACKET_RADIO_EVENT_DEVICE_ERROR &&
        event.device_error_code ==
            (uint16_t)(
                SX128X_PACKET_RADIO_ERROR_DRIVER_BASE |
                SX128X_RESULT_SPI_ERROR
            ),
        "Driver error code should be preserved"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        packet_radio_recover(&fixture.radio),
        "Adapter should recover"
    );

    const uint8_t expected[] = {
        0x80U, 0x00U,
        0x97U, 0xFFU, 0xFFU
    };

    TEST_ASSERT(
        fixture.port_context.spi_tx_count ==
            sizeof(expected) &&
        bytes_equal(
            expected,
            fixture.port_context.spi_tx_log,
            sizeof(expected)
        ),
        "Recovery should enter standby and clear IRQs"
    );

    TEST_ASSERT(
        packet_radio_get_state(&fixture.radio) ==
            PACKET_RADIO_STATE_IDLE,
        "Recovery should return adapter to idle"
    );

    return true;
}

static bool test_invalid_rx_length_is_device_error(void)
{
    TestFixture fixture;
    uint8_t frame[TEST_FRAME_LENGTH];

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    fill_frame(frame);

    TEST_ASSERT(
        packet_radio_try_start_rx(
            &fixture.radio,
            10U
        ),
        "RX should start"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    TEST_ASSERT(
        set_rx_done_script(
            &fixture.port_context,
            frame,
            TEST_FRAME_LENGTH - 1U,
            0x80U,
            171U,
            29
        ),
        "Invalid-length script should load"
    );

    fixture.port_context.dio1_pending = true;
    packet_radio_process(&fixture.radio);

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(
            &fixture.radio,
            &event
        ) &&
        event.type == PACKET_RADIO_EVENT_DEVICE_ERROR &&
        event.device_error_code ==
            SX128X_PACKET_RADIO_ERROR_INVALID_RX_LENGTH,
        "Invalid RX length should be reported"
    );

    TEST_ASSERT(
        packet_radio_get_state(&fixture.radio) ==
            PACKET_RADIO_STATE_ERROR,
        "Invalid RX length should enter error state"
    );

    return true;
}

static bool test_processing_is_deferred_until_dio1(void)
{
    TestFixture fixture;

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    fake_sx128x_port_clear_spi_log(
        &fixture.port_context
    );

    packet_radio_process(&fixture.radio);

    TEST_ASSERT(
        fixture.port_context.spi_transfer_calls == 0U,
        "No SPI should run without a DIO1 event"
    );

    return true;
}

static bool test_argument_and_state_checks(void)
{
    TestFixture fixture;
    uint8_t frame[TEST_FRAME_LENGTH];

    TEST_ASSERT(
        setup_fixture(&fixture),
        "Fixture should initialize"
    );

    fill_frame(frame);

    TEST_ASSERT(
        !packet_radio_try_start_tx(
            &fixture.radio,
            frame,
            TEST_FRAME_LENGTH - 1U
        ),
        "Wrong TX frame length should fail"
    );

    TEST_ASSERT(
        !packet_radio_try_start_rx(
            &fixture.radio,
            SX128X_PACKET_RADIO_MAX_TIMED_RX_MS + 1U
        ),
        "Oversized RX timeout should fail"
    );

    TEST_ASSERT(
        packet_radio_try_start_tx(
            &fixture.radio,
            frame,
            sizeof(frame)
        ),
        "Valid TX should start"
    );

    TEST_ASSERT(
        !packet_radio_try_start_rx(
            &fixture.radio,
            10U
        ),
        "RX should be rejected while TX is active"
    );

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_init_validation()) {
        failures++;
    }

    if (!test_tx_start_and_tx_done()) {
        failures++;
    }

    if (!test_rx_timeout_encoding()) {
        failures++;
    }

    if (!test_rx_done_frame_and_diagnostics()) {
        failures++;
    }

    if (!test_crc_error_has_priority()) {
        failures++;
    }

    if (!test_rx_timeout_event()) {
        failures++;
    }

    if (!test_driver_error_and_recovery()) {
        failures++;
    }

    if (!test_invalid_rx_length_is_device_error()) {
        failures++;
    }

    if (!test_processing_is_deferred_until_dio1()) {
        failures++;
    }

    if (!test_argument_and_state_checks()) {
        failures++;
    }

    if (failures != 0) {
        printf(
            "%d SX128x PacketRadio test(s) failed\n",
            failures
        );
        return 1;
    }

    printf("All SX128x PacketRadio tests passed\n");
    return 0;
}
