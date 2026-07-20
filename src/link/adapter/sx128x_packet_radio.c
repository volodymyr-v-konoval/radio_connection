#include "sx128x_packet_radio.h"

#include <limits.h>
#include <string.h>

static uint16_t sx128x_packet_radio_driver_error(
    Sx128xResult result
)
{
    return (uint16_t)(
        (uint16_t)SX128X_PACKET_RADIO_ERROR_DRIVER_BASE |
        (uint16_t)result
    );
}

static bool sx128x_packet_radio_push_event(
    Sx128xPacketRadio *adapter,
    PacketRadioEventType type,
    uint16_t device_error_code
)
{
    if (adapter->event_count >=
        SX128X_PACKET_RADIO_EVENT_CAPACITY) {
        return false;
    }

    const size_t index =
        (adapter->event_head + adapter->event_count) %
        SX128X_PACKET_RADIO_EVENT_CAPACITY;

    adapter->events[index].type = type;
    adapter->events[index].device_error_code =
        device_error_code;
    adapter->event_count++;

    return true;
}

static void sx128x_packet_radio_enter_error(
    Sx128xPacketRadio *adapter,
    uint16_t error_code
)
{
    adapter->state = PACKET_RADIO_STATE_ERROR;
    adapter->rx_frame_available = false;

    if (!sx128x_packet_radio_push_event(
            adapter,
            PACKET_RADIO_EVENT_DEVICE_ERROR,
            error_code)) {
        adapter->event_head = 0U;
        adapter->event_count = 0U;

        (void)sx128x_packet_radio_push_event(
            adapter,
            PACKET_RADIO_EVENT_DEVICE_ERROR,
            SX128X_PACKET_RADIO_ERROR_EVENT_QUEUE_FULL
        );
    }
}

static bool sx128x_packet_radio_run_command(
    Sx128xPacketRadio *adapter,
    Sx128xResult result
)
{
    if (result == SX128X_RESULT_OK) {
        return true;
    }

    sx128x_packet_radio_enter_error(
        adapter,
        sx128x_packet_radio_driver_error(result)
    );

    return false;
}

static bool sx128x_packet_radio_timeout_base_is_valid(
    Sx128xTimeoutBase timeout_base
)
{
    return
        timeout_base == SX128X_TIMEOUT_BASE_15_625_US ||
        timeout_base == SX128X_TIMEOUT_BASE_62_5_US ||
        timeout_base == SX128X_TIMEOUT_BASE_1_MS ||
        timeout_base == SX128X_TIMEOUT_BASE_4_MS;
}

static bool sx128x_packet_radio_config_is_valid(
    const Sx128xPacketRadioConfig *config
)
{
    return
        config != NULL &&
        config->frame_length > 0U &&
        config->frame_length <= PACKET_RADIO_MAX_FRAME_SIZE &&
        (size_t)config->tx_buffer_offset +
            config->frame_length <= SX128X_BUFFER_SIZE &&
        sx128x_packet_radio_timeout_base_is_valid(
            config->tx_timeout_base
        ) &&
        config->tx_timeout_count > 0U &&
        config->tx_timeout_count < UINT16_MAX;
}

static Sx128xPacketRadio *sx128x_packet_radio_context(
    PacketRadio *self
)
{
    if (self == NULL || self->context == NULL) {
        return NULL;
    }

    Sx128xPacketRadio *adapter =
        (Sx128xPacketRadio *)self->context;

    if (!adapter->initialized || adapter->device == NULL) {
        return NULL;
    }

    return adapter;
}

static bool sx128x_packet_radio_try_start_tx(
    PacketRadio *self,
    const uint8_t *data,
    size_t length
)
{
    Sx128xPacketRadio *adapter =
        sx128x_packet_radio_context(self);

    if (adapter == NULL || data == NULL ||
        adapter->state != PACKET_RADIO_STATE_IDLE ||
        length != adapter->config.frame_length) {
        return false;
    }

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_clear_irq_status(
                adapter->device,
                SX128X_IRQ_ALL
            ))) {
        return false;
    }

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_write_buffer(
                adapter->device,
                adapter->config.tx_buffer_offset,
                data,
                length
            ))) {
        return false;
    }

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_set_tx(
                adapter->device,
                adapter->config.tx_timeout_base,
                adapter->config.tx_timeout_count
            ))) {
        return false;
    }

    adapter->state = PACKET_RADIO_STATE_TX;
    return true;
}

static bool sx128x_packet_radio_try_start_rx(
    PacketRadio *self,
    uint32_t timeout_ms
)
{
    Sx128xPacketRadio *adapter =
        sx128x_packet_radio_context(self);

    if (adapter == NULL ||
        adapter->state != PACKET_RADIO_STATE_IDLE ||
        adapter->rx_frame_available ||
        timeout_ms > SX128X_PACKET_RADIO_MAX_TIMED_RX_MS) {
        return false;
    }

    const uint16_t timeout_count =
        timeout_ms == 0U
            ? SX128X_PACKET_RADIO_CONTINUOUS_RX_COUNT
            : (uint16_t)timeout_ms;

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_clear_irq_status(
                adapter->device,
                SX128X_IRQ_ALL
            ))) {
        return false;
    }

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_set_rx(
                adapter->device,
                SX128X_TIMEOUT_BASE_1_MS,
                timeout_count
            ))) {
        return false;
    }

    adapter->state = PACKET_RADIO_STATE_RX;
    return true;
}

static bool sx128x_packet_radio_capture_rx_frame(
    Sx128xPacketRadio *adapter
)
{
    Sx128xRxBufferStatus buffer_status;

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_get_rx_buffer_status(
                adapter->device,
                &buffer_status
            ))) {
        return false;
    }

    if ((size_t)buffer_status.payload_length !=
        adapter->config.frame_length) {
        sx128x_packet_radio_enter_error(
            adapter,
            SX128X_PACKET_RADIO_ERROR_INVALID_RX_LENGTH
        );
        return false;
    }

    PacketRadioRxFrame frame;
    memset(&frame, 0, sizeof(frame));

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_read_buffer(
                adapter->device,
                buffer_status.start_buffer_pointer,
                frame.data,
                buffer_status.payload_length
            ))) {
        return false;
    }

    Sx128xLoRaPacketStatus packet_status;

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_get_lora_packet_status(
                adapter->device,
                &packet_status
            ))) {
        return false;
    }

    frame.length = buffer_status.payload_length;
    frame.rssi_dbm_x2 = packet_status.rssi_dbm_x2;
    frame.snr_db_x4 = packet_status.snr_db_x4;

    adapter->rx_frame = frame;
    adapter->rx_frame_available = true;
    return true;
}

static void sx128x_packet_radio_process_impl(
    PacketRadio *self
)
{
    Sx128xPacketRadio *adapter =
        sx128x_packet_radio_context(self);

    if (adapter == NULL ||
        adapter->state == PACKET_RADIO_STATE_ERROR ||
        !sx128x_take_dio1_event(adapter->device)) {
        return;
    }

    uint16_t irq_status = SX128X_IRQ_NONE;

    if (!sx128x_packet_radio_run_command(
            adapter,
            sx128x_get_irq_status(
                adapter->device,
                &irq_status
            ))) {
        return;
    }

    if (irq_status != SX128X_IRQ_NONE &&
        !sx128x_packet_radio_run_command(
            adapter,
            sx128x_clear_irq_status(
                adapter->device,
                irq_status
            ))) {
        return;
    }

    if ((irq_status & SX128X_IRQ_CRC_ERROR) != 0U) {
        if (adapter->state != PACKET_RADIO_STATE_RX) {
            sx128x_packet_radio_enter_error(
                adapter,
                SX128X_PACKET_RADIO_ERROR_UNEXPECTED_IRQ
            );
            return;
        }

        adapter->state = PACKET_RADIO_STATE_IDLE;
        adapter->rx_frame_available = false;
        (void)sx128x_packet_radio_push_event(
            adapter,
            PACKET_RADIO_EVENT_CRC_ERROR,
            0U
        );
        return;
    }

    if ((irq_status & SX128X_IRQ_RX_TX_TIMEOUT) != 0U) {
        if (adapter->state != PACKET_RADIO_STATE_RX) {
            sx128x_packet_radio_enter_error(
                adapter,
                SX128X_PACKET_RADIO_ERROR_UNEXPECTED_IRQ
            );
            return;
        }

        adapter->state = PACKET_RADIO_STATE_IDLE;
        (void)sx128x_packet_radio_push_event(
            adapter,
            PACKET_RADIO_EVENT_RX_TIMEOUT,
            0U
        );
        return;
    }

    if ((irq_status & SX128X_IRQ_RX_DONE) != 0U) {
        if (adapter->state != PACKET_RADIO_STATE_RX) {
            sx128x_packet_radio_enter_error(
                adapter,
                SX128X_PACKET_RADIO_ERROR_UNEXPECTED_IRQ
            );
            return;
        }

        if (!sx128x_packet_radio_capture_rx_frame(adapter)) {
            return;
        }

        adapter->state = PACKET_RADIO_STATE_IDLE;
        (void)sx128x_packet_radio_push_event(
            adapter,
            PACKET_RADIO_EVENT_RX_DONE,
            0U
        );
        return;
    }

    if ((irq_status & SX128X_IRQ_TX_DONE) != 0U) {
        if (adapter->state != PACKET_RADIO_STATE_TX) {
            sx128x_packet_radio_enter_error(
                adapter,
                SX128X_PACKET_RADIO_ERROR_UNEXPECTED_IRQ
            );
            return;
        }

        adapter->state = PACKET_RADIO_STATE_IDLE;
        (void)sx128x_packet_radio_push_event(
            adapter,
            PACKET_RADIO_EVENT_TX_DONE,
            0U
        );
    }
}

static bool sx128x_packet_radio_take_event_impl(
    PacketRadio *self,
    PacketRadioEvent *event
)
{
    Sx128xPacketRadio *adapter =
        sx128x_packet_radio_context(self);

    if (adapter == NULL || event == NULL ||
        adapter->event_count == 0U) {
        return false;
    }

    *event = adapter->events[adapter->event_head];
    adapter->event_head =
        (adapter->event_head + 1U) %
        SX128X_PACKET_RADIO_EVENT_CAPACITY;
    adapter->event_count--;

    return true;
}

static bool sx128x_packet_radio_read_rx_frame_impl(
    PacketRadio *self,
    PacketRadioRxFrame *frame
)
{
    Sx128xPacketRadio *adapter =
        sx128x_packet_radio_context(self);

    if (adapter == NULL || frame == NULL ||
        !adapter->rx_frame_available) {
        return false;
    }

    *frame = adapter->rx_frame;
    adapter->rx_frame_available = false;
    return true;
}

static bool sx128x_packet_radio_recover_impl(
    PacketRadio *self
)
{
    Sx128xPacketRadio *adapter =
        sx128x_packet_radio_context(self);

    if (adapter == NULL) {
        return false;
    }

    adapter->event_head = 0U;
    adapter->event_count = 0U;
    adapter->rx_frame_available = false;

    Sx128xResult result = sx128x_set_standby(
        adapter->device,
        SX128X_STANDBY_RC
    );

    if (result == SX128X_RESULT_OK) {
        result = sx128x_clear_irq_status(
            adapter->device,
            SX128X_IRQ_ALL
        );
    }

    if (result != SX128X_RESULT_OK) {
        sx128x_packet_radio_enter_error(
            adapter,
            sx128x_packet_radio_driver_error(result)
        );
        return false;
    }

    adapter->state = PACKET_RADIO_STATE_IDLE;
    return true;
}

static PacketRadioState sx128x_packet_radio_get_state_impl(
    PacketRadio *self
)
{
    const Sx128xPacketRadio *adapter =
        sx128x_packet_radio_context(self);

    if (adapter == NULL) {
        return PACKET_RADIO_STATE_UNINITIALIZED;
    }

    return adapter->state;
}

bool sx128x_packet_radio_init(
    PacketRadio *radio,
    Sx128xPacketRadio *adapter,
    Sx128x *device,
    const Sx128xPacketRadioConfig *config
)
{
    if (radio == NULL || adapter == NULL || device == NULL ||
        !device->initialized ||
        !sx128x_packet_radio_config_is_valid(config)) {
        return false;
    }

    memset(radio, 0, sizeof(*radio));
    memset(adapter, 0, sizeof(*adapter));

    adapter->device = device;
    adapter->config = *config;
    adapter->state = PACKET_RADIO_STATE_IDLE;
    adapter->initialized = true;

    radio->context = adapter;
    radio->try_start_tx = sx128x_packet_radio_try_start_tx;
    radio->try_start_rx = sx128x_packet_radio_try_start_rx;
    radio->process = sx128x_packet_radio_process_impl;
    radio->take_event = sx128x_packet_radio_take_event_impl;
    radio->read_rx_frame =
        sx128x_packet_radio_read_rx_frame_impl;
    radio->recover = sx128x_packet_radio_recover_impl;
    radio->get_state = sx128x_packet_radio_get_state_impl;

    return true;
}
