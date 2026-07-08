#ifndef RC_TYPES_H
#define RC_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define RC_MAX_CHANNELS 18

typedef enum
{
    RADIO_PROTOCOL_NONE = 0,
    RADIO_PROTOCOL_CRSF,
    RADIO_PROTOCOL_SBUS,
    RADIO_PROTOCOL_IBUS
} RadioProtocolType;

typedef struct
{
    uint16_t channels[RC_MAX_CHANNELS];

    uint8_t channel_count;

    bool frame_valid;
    bool failsafe;
    bool frame_lost;

    uint32_t timestamp_ms;

    RadioProtocolType protocol;
} RcInputFrame;

#endif /* RC_TYPES_H */
