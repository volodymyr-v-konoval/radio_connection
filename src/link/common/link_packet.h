#ifndef LINK_PACKET_H
#define LINK_PACKET_H

#include <stddef.h>
#include <stdint.h>

#define LINK_PACKET_MAGIC_BYTE_0 0x4CU
#define LINK_PACKET_MAGIC_BYTE_1 0x52U

#define LINK_PACKET_VERSION 1U

#define LINK_PACKET_MIN_PAYLOAD_SIZE 10U
#define LINK_PACKET_PAYLOAD_SIZE 16U
#define LINK_PACKET_ENCODED_SIZE 30U

#define LINK_PACKET_FLAG_NONE 0x00U

typedef enum
{
    LINK_MESSAGE_TYPE_DATA = 1,
    LINK_MESSAGE_TYPE_ACK = 2,
    LINK_MESSAGE_TYPE_PING = 3,
    LINK_MESSAGE_TYPE_PONG = 4
} LinkMessageType;

typedef enum
{
    LINK_PACKET_STATUS_OK = 0,
    LINK_PACKET_STATUS_INVALID_ARGUMENT,
    LINK_PACKET_STATUS_INVALID_FRAME_LENGTH,
    LINK_PACKET_STATUS_INVALID_MAGIC,
    LINK_PACKET_STATUS_UNSUPPORTED_VERSION,
    LINK_PACKET_STATUS_INVALID_MESSAGE_TYPE,
    LINK_PACKET_STATUS_INVALID_DESTINATION,
    LINK_PACKET_STATUS_INVALID_PAYLOAD_LENGTH,
    LINK_PACKET_STATUS_CRC_MISMATCH
} LinkPacketStatus;

typedef struct
{
    LinkMessageType message_type;

    uint8_t source_id;
    uint8_t destination_id;

    uint16_t sequence;
    uint16_t acknowledged_sequence;

    uint8_t payload_length;
    uint8_t flags;

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE];
} LinkPacket;

LinkPacketStatus link_packet_encode(
    const LinkPacket *packet,
    uint8_t *encoded,
    size_t encoded_capacity
);

LinkPacketStatus link_packet_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    uint8_t expected_destination_id,
    LinkPacket *packet
);

#endif /* LINK_PACKET_H */
