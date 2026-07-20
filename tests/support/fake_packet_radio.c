#include "fake_packet_radio.h"

#include <string.h>

static bool fake_packet_radio_push_event(
    FakePacketRadioContext *context,
    PacketRadioEventType type,
    uint16_t device_error_code
)
{
    if (context == NULL ||
        context->event_count >=
            FAKE_PACKET_RADIO_EVENT_CAPACITY) {
        return false;
    }

    const size_t event_index =
        (context->event_head + context->event_count) %
        FAKE_PACKET_RADIO_EVENT_CAPACITY;

    context->events[event_index].type = type;

    context->events[event_index].device_error_code =
        device_error_code;

    context->event_count++;

    return true;
}

static bool fake_packet_radio_try_start_tx(
    PacketRadio *self,
    const uint8_t *data,
    size_t length
)
{
    if (self == NULL ||
        self->context == NULL ||
        data == NULL) {
        return false;
    }

    FakePacketRadioContext *context =
        (FakePacketRadioContext *)self->context;

    context->tx_start_calls++;

    if (!context->accept_tx ||
        context->state != PACKET_RADIO_STATE_IDLE ||
        length == 0U ||
        length > sizeof(context->last_tx_frame)) {
        return false;
    }

    memcpy(
        context->last_tx_frame,
        data,
        length
    );

    context->last_tx_length = length;
    context->state = PACKET_RADIO_STATE_TX;

    return true;
}

static bool fake_packet_radio_try_start_rx(
    PacketRadio *self,
    uint32_t timeout_ms
)
{
    if (self == NULL || self->context == NULL) {
        return false;
    }

    FakePacketRadioContext *context =
        (FakePacketRadioContext *)self->context;

    context->rx_start_calls++;

    if (!context->accept_rx ||
        context->state != PACKET_RADIO_STATE_IDLE) {
        return false;
    }

    context->last_rx_timeout_ms = timeout_ms;
    context->rx_frame_available = false;
    context->state = PACKET_RADIO_STATE_RX;

    return true;
}

static void fake_packet_radio_process_impl(
    PacketRadio *self
)
{
    if (self == NULL || self->context == NULL) {
        return;
    }

    FakePacketRadioContext *context =
        (FakePacketRadioContext *)self->context;

    context->process_calls++;
}

static bool fake_packet_radio_take_event_impl(
    PacketRadio *self,
    PacketRadioEvent *event
)
{
    if (self == NULL ||
        self->context == NULL ||
        event == NULL) {
        return false;
    }

    FakePacketRadioContext *context =
        (FakePacketRadioContext *)self->context;

    if (context->event_count == 0U) {
        return false;
    }

    *event = context->events[context->event_head];

    context->event_head =
        (context->event_head + 1U) %
        FAKE_PACKET_RADIO_EVENT_CAPACITY;

    context->event_count--;

    return true;
}

static bool fake_packet_radio_read_rx_frame_impl(
    PacketRadio *self,
    PacketRadioRxFrame *frame
)
{
    if (self == NULL ||
        self->context == NULL ||
        frame == NULL) {
        return false;
    }

    FakePacketRadioContext *context =
        (FakePacketRadioContext *)self->context;

    context->read_rx_frame_calls++;

    if (!context->rx_frame_available) {
        return false;
    }

    *frame = context->rx_frame;
    context->rx_frame_available = false;

    return true;
}

static bool fake_packet_radio_recover_impl(
    PacketRadio *self
)
{
    if (self == NULL || self->context == NULL) {
        return false;
    }

    FakePacketRadioContext *context =
        (FakePacketRadioContext *)self->context;

    context->recover_calls++;

    if (!context->accept_recover) {
        return false;
    }

    context->state = PACKET_RADIO_STATE_IDLE;
    context->event_head = 0U;
    context->event_count = 0U;
    context->rx_frame_available = false;

    return true;
}

static PacketRadioState
fake_packet_radio_get_state_impl(
    PacketRadio *self
)
{
    if (self == NULL || self->context == NULL) {
        return PACKET_RADIO_STATE_UNINITIALIZED;
    }

    const FakePacketRadioContext *context =
        (const FakePacketRadioContext *)self->context;

    return context->state;
}

bool fake_packet_radio_init(
    PacketRadio *radio,
    FakePacketRadioContext *context
)
{
    if (radio == NULL || context == NULL) {
        return false;
    }

    memset(radio, 0, sizeof(*radio));
    memset(context, 0, sizeof(*context));

    context->state = PACKET_RADIO_STATE_IDLE;
    context->accept_tx = true;
    context->accept_rx = true;
    context->accept_recover = true;

    radio->context = context;
    radio->try_start_tx =
        fake_packet_radio_try_start_tx;
    radio->try_start_rx =
        fake_packet_radio_try_start_rx;
    radio->process =
        fake_packet_radio_process_impl;
    radio->take_event =
        fake_packet_radio_take_event_impl;
    radio->read_rx_frame =
        fake_packet_radio_read_rx_frame_impl;
    radio->recover =
        fake_packet_radio_recover_impl;
    radio->get_state =
        fake_packet_radio_get_state_impl;

    return true;
}

bool fake_packet_radio_complete_tx(
    FakePacketRadioContext *context
)
{
    if (context == NULL ||
        context->state != PACKET_RADIO_STATE_TX ||
        !fake_packet_radio_push_event(
            context,
            PACKET_RADIO_EVENT_TX_DONE,
            0U
        )) {
        return false;
    }

    context->state = PACKET_RADIO_STATE_IDLE;

    return true;
}

bool fake_packet_radio_deliver_rx(
    FakePacketRadioContext *context,
    const uint8_t *data,
    size_t length,
    int16_t rssi_dbm_x2,
    int16_t snr_db_x4
)
{
    if (context == NULL ||
        data == NULL ||
        context->state != PACKET_RADIO_STATE_RX ||
        length == 0U ||
        length > sizeof(context->rx_frame.data) ||
        !fake_packet_radio_push_event(
            context,
            PACKET_RADIO_EVENT_RX_DONE,
            0U
        )) {
        return false;
    }

    memset(
        &context->rx_frame,
        0,
        sizeof(context->rx_frame)
    );

    memcpy(
        context->rx_frame.data,
        data,
        length
    );

    context->rx_frame.length = length;
    context->rx_frame.rssi_dbm_x2 = rssi_dbm_x2;
    context->rx_frame.snr_db_x4 = snr_db_x4;
    context->rx_frame_available = true;
    context->state = PACKET_RADIO_STATE_IDLE;

    return true;
}

bool fake_packet_radio_trigger_rx_timeout(
    FakePacketRadioContext *context
)
{
    if (context == NULL ||
        context->state != PACKET_RADIO_STATE_RX ||
        !fake_packet_radio_push_event(
            context,
            PACKET_RADIO_EVENT_RX_TIMEOUT,
            0U
        )) {
        return false;
    }

    context->state = PACKET_RADIO_STATE_IDLE;

    return true;
}

bool fake_packet_radio_trigger_crc_error(
    FakePacketRadioContext *context
)
{
    if (context == NULL ||
        context->state != PACKET_RADIO_STATE_RX ||
        !fake_packet_radio_push_event(
            context,
            PACKET_RADIO_EVENT_CRC_ERROR,
            0U
        )) {
        return false;
    }

    context->state = PACKET_RADIO_STATE_IDLE;

    return true;
}

bool fake_packet_radio_trigger_device_error(
    FakePacketRadioContext *context,
    uint16_t device_error_code
)
{
    if (context == NULL ||
        !fake_packet_radio_push_event(
            context,
            PACKET_RADIO_EVENT_DEVICE_ERROR,
            device_error_code
        )) {
        return false;
    }

    context->state = PACKET_RADIO_STATE_ERROR;

    return true;
}
