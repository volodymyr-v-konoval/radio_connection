#ifndef RC_CHANNEL_MAPPER_H
#define RC_CHANNEL_MAPPER_H

#include <stdbool.h>
#include <stdint.h>

#include "rc_types.h"

#define RC_CHANNEL_MAPPER_MAX_ENTRIES      16U
#define RC_CHANNEL_MAPPER_MAX_CHANNELS     18U

#define RC_NORMALIZED_MIN                 (-1000)
#define RC_NORMALIZED_CENTER                  0
#define RC_NORMALIZED_MAX                  1000

#define RC_PULSE_MIN_US                    1000U
#define RC_PULSE_CENTER_US                 1500U
#define RC_PULSE_MAX_US                    2000U

typedef enum
{
    RC_FUNCTION_NONE = 0,

    RC_FUNCTION_ROLL,
    RC_FUNCTION_PITCH,
    RC_FUNCTION_THROTTLE,
    RC_FUNCTION_YAW,

    RC_FUNCTION_ARM,
    RC_FUNCTION_MODE,

    RC_FUNCTION_AUX1,
    RC_FUNCTION_AUX2,
    RC_FUNCTION_AUX3,
    RC_FUNCTION_AUX4,
    RC_FUNCTION_AUX5,
    RC_FUNCTION_AUX6,
    RC_FUNCTION_AUX7,
    RC_FUNCTION_AUX8,

    RC_FUNCTION_COUNT
} RcLogicalFunction;

typedef enum
{
    /*
     * Typical stick:
     * raw_min ... raw_center ... raw_max
     * becomes:
     * -1000  ... 0          ... +1000
     */
    RC_CHANNEL_MODE_CENTERED = 0,

    /*
     * Typical throttle:
     * raw_min ... raw_max
     * becomes:
     * 0       ... 1000
     */
    RC_CHANNEL_MODE_UNIPOLAR,

    /*
     * Two-position switch:
     * low  -> 0
     * high -> 1000
     */
    RC_CHANNEL_MODE_SWITCH_2POS,

    /*
     * Three-position switch:
     * low    -> -1000
     * center -> 0
     * high   -> +1000
     */
    RC_CHANNEL_MODE_SWITCH_3POS
} RcChannelMode;

typedef struct
{
    /*
     * Human-readable channel number:
     * 1 means frame.channels[0].
     */
    uint8_t source_channel;

    RcLogicalFunction function;
    RcChannelMode mode;

    uint16_t raw_min;
    uint16_t raw_center;
    uint16_t raw_max;

    uint16_t deadband;

    bool reversed;
} RcChannelMapEntry;

typedef struct
{
    RcChannelMapEntry entries[RC_CHANNEL_MAPPER_MAX_ENTRIES];
    uint8_t entry_count;

    bool initialized;
} RcChannelMapper;

typedef struct
{
    /*
     * Values are indexed by RcLogicalFunction.
     *
     * CENTERED and SWITCH_3POS:
     *   -1000 ... 0 ... +1000
     *
     * UNIPOLAR and SWITCH_2POS:
     *   0 ... 1000
     */
    int16_t normalized[RC_FUNCTION_COUNT];

    /*
     * Equivalent 1000–2000 µs representation.
     */
    uint16_t pulse_us[RC_FUNCTION_COUNT];

    /*
     * Bit N indicates that logical function N is available.
     */
    uint32_t available_mask;

    bool frame_valid;
    bool frame_lost;
    bool failsafe;

    uint32_t timestamp_ms;
    RadioProtocolType protocol;
} RcMappedInput;

bool rc_channel_mapper_init(
    RcChannelMapper *mapper,
    const RcChannelMapEntry *entries,
    uint8_t entry_count
);

bool rc_channel_mapper_map(
    const RcChannelMapper *mapper,
    const RcInputFrame *input,
    RcMappedInput *output
);

bool rc_mapped_input_get(
    const RcMappedInput *input,
    RcLogicalFunction function,
    int16_t *normalized,
    uint16_t *pulse_us
);

#endif /* RC_CHANNEL_MAPPER_H */
