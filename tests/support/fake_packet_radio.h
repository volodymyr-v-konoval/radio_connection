#ifndef FAKE_PACKET_RADIO_H
#define FAKE_PACKET_RADIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "packet_radio_if.h"

#define FAKE_PACKET_RADIO_EVENT_CAPACITY 8U

typedef struct
{
    PacketRadioState state;

    bool accept_tx;
    bool accept_rx;
    bool accept_recover;

    uint8_t last_tx_frame[PACKET_RADIO_MAX_FRAME_SIZE];
    size_t last_tx_length;
    uint32_t last_rx_timeout_ms;

    uint32_t tx_start_calls;
    uint32_t rx_start_calls;
    uint32_t process_calls;
    uint32_t recover_calls;
    uint32_t read_rx_frame_calls;

    PacketRadioEvent events[
        FAKE_PACKET_RADIO_EVENT_CAPACITY
    ];

    size_t event_head;
    size_t event_count;

    PacketRadioRxFrame rx_frame;
    bool rx_frame_available;
} FakePacketRadioContext;

bool fake_packet_radio_init(
    PacketRadio *radio,
    FakePacketRadioContext *context
);

bool fake_packet_radio_complete_tx(
    FakePacketRadioContext *context
);

bool fake_packet_radio_deliver_rx(
    FakePacketRadioContext *context,
    const uint8_t *data,
    size_t length,
    int16_t rssi_dbm_x2,
    int16_t snr_db_x4
);

bool fake_packet_radio_trigger_rx_timeout(
    FakePacketRadioContext *context
);

bool fake_packet_radio_trigger_crc_error(
    FakePacketRadioContext *context
);

bool fake_packet_radio_trigger_device_error(
    FakePacketRadioContext *context,
    uint16_t device_error_code
);

#endif /* FAKE_PACKET_RADIO_H */
