#ifndef SBUS_PROTOCOL_H
#define SBUS_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "radio_protocol_if.h"

#define SBUS_FRAME_SIZE                 25U
#define SBUS_HEADER_BYTE                0x0FU
#define SBUS_FOOTER_BYTE                0x00U
#define SBUS_CHANNEL_COUNT              16U
#define SBUS_CHANNEL_DATA_SIZE          22U
#define SBUS_FLAGS_INDEX                23U
#define SBUS_FOOTER_INDEX               24U
#define SBUS_INTER_BYTE_TIMEOUT_MS      5U

#define SBUS_FLAG_DIGITAL_CHANNEL_17    0x01U
#define SBUS_FLAG_DIGITAL_CHANNEL_18    0x02U
#define SBUS_FLAG_FRAME_LOST            0x04U
#define SBUS_FLAG_FAILSAFE              0x08U
#define SBUS_FLAG_RESERVED_MASK         0xF0U

typedef enum
{
    SBUS_PARSER_WAIT_HEADER = 0,
    SBUS_PARSER_READ_FRAME
} SbusParserState;

typedef struct
{
    SbusParserState parser_state;

    uint8_t frame_buffer[SBUS_FRAME_SIZE];
    uint8_t frame_pos;
    uint32_t last_byte_time_ms;

    RcInputFrame latest_frame;
    bool frame_ready;

    uint32_t received_frames;
    uint32_t valid_frames;
    uint32_t malformed_frames;
    uint32_t timeout_resets;
} SbusProtocolContext;

void sbus_protocol_init(
    RadioProtocol *protocol,
    SbusProtocolContext *context
);

#endif /* SBUS_PROTOCOL_H */
