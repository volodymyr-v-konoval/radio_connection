#ifndef CRSF_PROTOCOL_H
#define CRSF_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "radio_protocol_if.h"

#define CRSF_MAX_FRAME_SIZE        64U
#define CRSF_MAX_PAYLOAD_SIZE      60U

#define CRSF_ADDRESS_FLIGHT_CTRL   0xC8U
#define CRSF_ADDRESS_CRSF_RECEIVER 0xECU

#define CRSF_FRAME_TYPE_RC_CHANNELS_PACKED 0x16U
#define CRSF_FRAME_TYPE_PARAMETER_PING       0x28U
#define CRSF_EXTENDED_PING_FRAME_LENGTH      4U

typedef enum
{
    CRSF_PARSER_WAIT_ADDRESS = 0,
    CRSF_PARSER_WAIT_LENGTH,
    CRSF_PARSER_READ_PAYLOAD
} CrsfParserState;

typedef struct
{
    CrsfParserState parser_state;

    uint8_t frame_buffer[CRSF_MAX_FRAME_SIZE];
    uint8_t frame_pos;
    uint8_t expected_length;

    RcInputFrame latest_frame;
    bool frame_ready;
    bool device_ping_pending;
    uint8_t device_ping_origin;

    uint32_t received_frames;
    uint32_t valid_frames;
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t unsupported_frames;
} CrsfProtocolContext;

void crsf_protocol_init(
    RadioProtocol *protocol,
    CrsfProtocolContext *context
);


bool crsf_protocol_take_device_ping(
    CrsfProtocolContext *context,
    uint8_t *out_origin
);

#endif /* CRSF_PROTOCOL_H */
