#ifndef PACKET_RADIO_IF_H
#define PACKET_RADIO_IF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PACKET_RADIO_MAX_FRAME_SIZE 255U

typedef enum
{
    PACKET_RADIO_STATE_UNINITIALIZED = 0,
    PACKET_RADIO_STATE_IDLE,
    PACKET_RADIO_STATE_TX,
    PACKET_RADIO_STATE_RX,
    PACKET_RADIO_STATE_ERROR
} PacketRadioState;

typedef enum
{
    PACKET_RADIO_EVENT_NONE = 0,
    PACKET_RADIO_EVENT_TX_DONE,
    PACKET_RADIO_EVENT_RX_DONE,
    PACKET_RADIO_EVENT_RX_TIMEOUT,
    PACKET_RADIO_EVENT_CRC_ERROR,
    PACKET_RADIO_EVENT_DEVICE_ERROR
} PacketRadioEventType;

typedef struct
{
    PacketRadioEventType type;
    uint16_t device_error_code;
} PacketRadioEvent;

typedef struct
{
    uint8_t data[PACKET_RADIO_MAX_FRAME_SIZE];
    size_t length;

    /*
     * RSSI in half-dBm units.
     * Example: -171 means -85.5 dBm.
     */
    int16_t rssi_dbm_x2;

    /*
     * SNR in quarter-dB units.
     * Example: 29 means 7.25 dB.
     */
    int16_t snr_db_x4;
} PacketRadioRxFrame;

typedef struct PacketRadio PacketRadio;

struct PacketRadio
{
    void *context;

    bool (*try_start_tx)(
        PacketRadio *self,
        const uint8_t *data,
        size_t length
    );

    /*
     * timeout_ms == 0U requests continuous receive mode.
     */
    bool (*try_start_rx)(
        PacketRadio *self,
        uint32_t timeout_ms
    );

    /*
     * Runs deferred device processing outside interrupt context.
     */
    void (*process)(
        PacketRadio *self
    );

    /*
     * Removes one pending event from the implementation queue.
     */
    bool (*take_event)(
        PacketRadio *self,
        PacketRadioEvent *event
    );

    /*
     * Reads one completed RX frame after an RX_DONE event.
     */
    bool (*read_rx_frame)(
        PacketRadio *self,
        PacketRadioRxFrame *frame
    );

    /*
     * Returns the implementation to a known idle state.
     */
    bool (*recover)(
        PacketRadio *self
    );

    PacketRadioState (*get_state)(
        PacketRadio *self
    );
};

static inline bool packet_radio_try_start_tx(
    PacketRadio *radio,
    const uint8_t *data,
    size_t length
)
{
    return radio != NULL &&
        radio->try_start_tx != NULL &&
        data != NULL &&
        length > 0U &&
        length <= PACKET_RADIO_MAX_FRAME_SIZE &&
        radio->try_start_tx(radio, data, length);
}

static inline bool packet_radio_try_start_rx(
    PacketRadio *radio,
    uint32_t timeout_ms
)
{
    return radio != NULL &&
        radio->try_start_rx != NULL &&
        radio->try_start_rx(radio, timeout_ms);
}

static inline void packet_radio_process(
    PacketRadio *radio
)
{
    if (radio != NULL && radio->process != NULL) {
        radio->process(radio);
    }
}

static inline bool packet_radio_take_event(
    PacketRadio *radio,
    PacketRadioEvent *event
)
{
    return radio != NULL &&
        radio->take_event != NULL &&
        event != NULL &&
        radio->take_event(radio, event);
}

static inline bool packet_radio_read_rx_frame(
    PacketRadio *radio,
    PacketRadioRxFrame *frame
)
{
    return radio != NULL &&
        radio->read_rx_frame != NULL &&
        frame != NULL &&
        radio->read_rx_frame(radio, frame);
}

static inline bool packet_radio_recover(
    PacketRadio *radio
)
{
    return radio != NULL &&
        radio->recover != NULL &&
        radio->recover(radio);
}

static inline PacketRadioState packet_radio_get_state(
    PacketRadio *radio
)
{
    if (radio == NULL || radio->get_state == NULL) {
        return PACKET_RADIO_STATE_UNINITIALIZED;
    }

    return radio->get_state(radio);
}

#endif /* PACKET_RADIO_IF_H */
