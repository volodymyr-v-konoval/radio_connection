#ifndef SX128X_PACKET_RADIO_H
#define SX128X_PACKET_RADIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "packet_radio_if.h"
#include "sx128x.h"

#define SX128X_PACKET_RADIO_EVENT_CAPACITY 4U
#define SX128X_PACKET_RADIO_CONTINUOUS_RX_COUNT UINT16_MAX
#define SX128X_PACKET_RADIO_MAX_TIMED_RX_MS (UINT16_MAX - 1U)

typedef enum
{
    SX128X_PACKET_RADIO_ERROR_NONE = 0,
    SX128X_PACKET_RADIO_ERROR_DRIVER_BASE = 0x0100U,
    SX128X_PACKET_RADIO_ERROR_INVALID_RX_LENGTH = 0x0201U,
    SX128X_PACKET_RADIO_ERROR_UNEXPECTED_IRQ = 0x0202U,
    SX128X_PACKET_RADIO_ERROR_EVENT_QUEUE_FULL = 0x0203U
} Sx128xPacketRadioError;

typedef struct
{
    uint8_t tx_buffer_offset;
    size_t frame_length;
    Sx128xTimeoutBase tx_timeout_base;
    uint16_t tx_timeout_count;
} Sx128xPacketRadioConfig;

typedef struct
{
    Sx128x *device;
    Sx128xPacketRadioConfig config;
    PacketRadioState state;

    PacketRadioEvent events[
        SX128X_PACKET_RADIO_EVENT_CAPACITY
    ];
    size_t event_head;
    size_t event_count;

    PacketRadioRxFrame rx_frame;
    bool rx_frame_available;
    bool initialized;
} Sx128xPacketRadio;

bool sx128x_packet_radio_init(
    PacketRadio *radio,
    Sx128xPacketRadio *adapter,
    Sx128x *device,
    const Sx128xPacketRadioConfig *config
);

#endif /* SX128X_PACKET_RADIO_H */
