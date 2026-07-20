#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_packet_radio.h"
#include "packet_radio_if.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));     \
            return false;                        \
        }                                        \
    } while (0)

static bool test_tx_lifecycle_and_event(void)
{
    PacketRadio radio;
    FakePacketRadioContext context;

    TEST_ASSERT(
        fake_packet_radio_init(&radio, &context),
        "Fake packet radio should initialize"
    );

    const uint8_t frame[] = {
        0x4CU, 0x52U, 0x01U,
        0x01U, 0x01U, 0x02U
    };

    TEST_ASSERT(
        packet_radio_get_state(&radio) ==
            PACKET_RADIO_STATE_IDLE,
        "Radio should start idle"
    );

    TEST_ASSERT(
        packet_radio_try_start_tx(
            &radio,
            frame,
            sizeof(frame)
        ),
        "First TX should start"
    );

    TEST_ASSERT(
        packet_radio_get_state(&radio) ==
            PACKET_RADIO_STATE_TX,
        "Radio should enter TX state"
    );

    TEST_ASSERT(
        context.tx_start_calls == 1U,
        "One TX start should be recorded"
    );

    TEST_ASSERT(
        context.last_tx_length == sizeof(frame),
        "TX length should be captured"
    );

    TEST_ASSERT(
        memcmp(
            context.last_tx_frame,
            frame,
            sizeof(frame)
        ) == 0,
        "TX bytes should be copied"
    );

    TEST_ASSERT(
        !packet_radio_try_start_rx(&radio, 25U),
        "RX should be rejected while TX is active"
    );

    TEST_ASSERT(
        fake_packet_radio_complete_tx(&context),
        "TX completion should be accepted"
    );

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(&radio, &event),
        "TX_DONE event should be available"
    );

    TEST_ASSERT(
        event.type == PACKET_RADIO_EVENT_TX_DONE,
        "Event should report TX_DONE"
    );

    TEST_ASSERT(
        packet_radio_get_state(&radio) ==
            PACKET_RADIO_STATE_IDLE,
        "TX completion should return to idle"
    );

    TEST_ASSERT(
        !packet_radio_take_event(&radio, &event),
        "Event queue should be empty"
    );

    return true;
}

static bool test_rx_frame_and_diagnostics(void)
{
    PacketRadio radio;
    FakePacketRadioContext context;

    TEST_ASSERT(
        fake_packet_radio_init(&radio, &context),
        "Fake packet radio should initialize"
    );

    TEST_ASSERT(
        packet_radio_try_start_rx(&radio, 40U),
        "RX should start"
    );

    TEST_ASSERT(
        context.last_rx_timeout_ms == 40U,
        "RX timeout should be captured"
    );

    const uint8_t received[] = {
        0x10U, 0x20U, 0x30U, 0x40U,
        0x50U, 0x60U, 0x70U, 0x80U
    };

    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &context,
            received,
            sizeof(received),
            -171,
            29
        ),
        "RX frame should be delivered"
    );

    PacketRadioEvent event;

    TEST_ASSERT(
        packet_radio_take_event(&radio, &event),
        "RX_DONE event should be available"
    );

    TEST_ASSERT(
        event.type == PACKET_RADIO_EVENT_RX_DONE,
        "Event should report RX_DONE"
    );

    PacketRadioRxFrame frame;

    TEST_ASSERT(
        packet_radio_read_rx_frame(
            &radio,
            &frame
        ),
        "Completed RX frame should be readable"
    );

    TEST_ASSERT(
        frame.length == sizeof(received),
        "RX length should match"
    );

    TEST_ASSERT(
        memcmp(
            frame.data,
            received,
            sizeof(received)
        ) == 0,
        "RX bytes should match"
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
            &radio,
            &frame
        ),
        "RX frame should be consumed once"
    );

    return true;
}

static bool test_timeout_crc_error_and_recovery(void)
{
    PacketRadio radio;
    FakePacketRadioContext context;
    PacketRadioEvent event;

    TEST_ASSERT(
        fake_packet_radio_init(&radio, &context),
        "Fake packet radio should initialize"
    );

    TEST_ASSERT(
        packet_radio_try_start_rx(&radio, 10U),
        "Timed RX should start"
    );

    TEST_ASSERT(
        fake_packet_radio_trigger_rx_timeout(
            &context
        ),
        "RX timeout should be generated"
    );

    TEST_ASSERT(
        packet_radio_take_event(
            &radio,
            &event
        ) &&
        event.type ==
            PACKET_RADIO_EVENT_RX_TIMEOUT,
        "RX timeout event should be reported"
    );

    TEST_ASSERT(
        packet_radio_try_start_rx(&radio, 0U),
        "Continuous RX should start"
    );

    TEST_ASSERT(
        fake_packet_radio_trigger_crc_error(
            &context
        ),
        "CRC error should be generated"
    );

    TEST_ASSERT(
        packet_radio_take_event(
            &radio,
            &event
        ) &&
        event.type ==
            PACKET_RADIO_EVENT_CRC_ERROR,
        "CRC error event should be reported"
    );

    TEST_ASSERT(
        fake_packet_radio_trigger_device_error(
            &context,
            0x1234U
        ),
        "Device error should be generated"
    );

    TEST_ASSERT(
        packet_radio_take_event(
            &radio,
            &event
        ) &&
        event.type ==
            PACKET_RADIO_EVENT_DEVICE_ERROR &&
        event.device_error_code == 0x1234U,
        "Device error code should be preserved"
    );

    TEST_ASSERT(
        packet_radio_get_state(&radio) ==
            PACKET_RADIO_STATE_ERROR,
        "Device error should enter error state"
    );

    TEST_ASSERT(
        packet_radio_recover(&radio),
        "Radio should recover"
    );

    TEST_ASSERT(
        packet_radio_get_state(&radio) ==
            PACKET_RADIO_STATE_IDLE,
        "Recovery should return to idle"
    );

    TEST_ASSERT(
        context.recover_calls == 1U,
        "Recovery call should be counted"
    );

    return true;
}

static bool
test_deferred_process_and_argument_checks(void)
{
    PacketRadio radio;
    FakePacketRadioContext context;
    PacketRadioEvent event;
    PacketRadioRxFrame frame;

    const uint8_t byte = 0xAAU;

    TEST_ASSERT(
        fake_packet_radio_init(&radio, &context),
        "Fake packet radio should initialize"
    );

    packet_radio_process(&radio);
    packet_radio_process(&radio);

    TEST_ASSERT(
        context.process_calls == 2U,
        "Deferred process calls should be forwarded"
    );

    TEST_ASSERT(
        !packet_radio_try_start_tx(
            NULL,
            &byte,
            1U
        ),
        "NULL radio TX should fail"
    );

    TEST_ASSERT(
        !packet_radio_try_start_tx(
            &radio,
            NULL,
            1U
        ),
        "NULL TX data should fail"
    );

    TEST_ASSERT(
        !packet_radio_try_start_tx(
            &radio,
            &byte,
            0U
        ),
        "Zero-length TX should fail"
    );

    TEST_ASSERT(
        !packet_radio_take_event(
            NULL,
            &event
        ),
        "NULL radio event read should fail"
    );

    TEST_ASSERT(
        !packet_radio_take_event(
            &radio,
            NULL
        ),
        "NULL event output should fail"
    );

    TEST_ASSERT(
        !packet_radio_read_rx_frame(
            NULL,
            &frame
        ),
        "NULL radio frame read should fail"
    );

    TEST_ASSERT(
        !packet_radio_read_rx_frame(
            &radio,
            NULL
        ),
        "NULL frame output should fail"
    );

    TEST_ASSERT(
        packet_radio_get_state(NULL) ==
            PACKET_RADIO_STATE_UNINITIALIZED,
        "NULL radio should be uninitialized"
    );

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_tx_lifecycle_and_event()) {
        failures++;
    }

    if (!test_rx_frame_and_diagnostics()) {
        failures++;
    }

    if (!test_timeout_crc_error_and_recovery()) {
        failures++;
    }

    if (!test_deferred_process_and_argument_checks()) {
        failures++;
    }

    if (failures != 0) {
        printf(
            "%d packet radio interface test(s) failed\n",
            failures
        );

        return 1;
    }

    printf(
        "All packet radio interface tests passed\n"
    );

    return 0;
}
