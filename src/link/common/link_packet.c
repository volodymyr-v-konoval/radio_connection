#include "link_packet.h"

#include <stdbool.h>
#include <string.h>

#include "link_crc16.h"

enum
{
    LINK_PACKET_OFFSET_MAGIC_0 = 0,
    LINK_PACKET_OFFSET_MAGIC_1 = 1,
    LINK_PACKET_OFFSET_VERSION = 2,
    LINK_PACKET_OFFSET_MESSAGE_TYPE = 3,
    LINK_PACKET_OFFSET_SOURCE_ID = 4,
    LINK_PACKET_OFFSET_DESTINATION_ID = 5,
    LINK_PACKET_OFFSET_SEQUENCE = 6,
    LINK_PACKET_OFFSET_ACKNOWLEDGED_SEQUENCE = 8,
    LINK_PACKET_OFFSET_PAYLOAD_LENGTH = 10,
    LINK_PACKET_OFFSET_FLAGS = 11,
    LINK_PACKET_OFFSET_PAYLOAD = 12,
    LINK_PACKET_OFFSET_CRC = 28
};

static bool link_packet_message_type_is_valid(
    uint8_t message_type
)
{
    return
        message_type >= (uint8_t)LINK_MESSAGE_TYPE_DATA &&
        message_type <= (uint8_t)LINK_MESSAGE_TYPE_PONG;
}

static bool link_packet_payload_length_is_valid(
    uint8_t payload_length
)
{
    return
        payload_length >= LINK_PACKET_MIN_PAYLOAD_SIZE &&
        payload_length <= LINK_PACKET_PAYLOAD_SIZE;
}

static void link_packet_write_u16_be(
    uint8_t *destination,
    uint16_t value
)
{
    destination[0] = (uint8_t)(value >> 8U);
    destination[1] = (uint8_t)(value & 0x00FFU);
}

static uint16_t link_packet_read_u16_be(
    const uint8_t *source
)
{
    return (uint16_t)(
        ((uint16_t)source[0] << 8U) |
        (uint16_t)source[1]
    );
}

LinkPacketStatus link_packet_encode(
    const LinkPacket *packet,
    uint8_t *encoded,
    size_t encoded_capacity
)
{
    if (packet == NULL || encoded == NULL) {
        return LINK_PACKET_STATUS_INVALID_ARGUMENT;
    }

    if (encoded_capacity < LINK_PACKET_ENCODED_SIZE) {
        return LINK_PACKET_STATUS_INVALID_FRAME_LENGTH;
    }

    if (!link_packet_message_type_is_valid(
            (uint8_t)packet->message_type)) {
        return LINK_PACKET_STATUS_INVALID_MESSAGE_TYPE;
    }

    if (!link_packet_payload_length_is_valid(
            packet->payload_length)) {
        return LINK_PACKET_STATUS_INVALID_PAYLOAD_LENGTH;
    }

    memset(encoded, 0, LINK_PACKET_ENCODED_SIZE);

    encoded[LINK_PACKET_OFFSET_MAGIC_0] =
        LINK_PACKET_MAGIC_BYTE_0;

    encoded[LINK_PACKET_OFFSET_MAGIC_1] =
        LINK_PACKET_MAGIC_BYTE_1;

    encoded[LINK_PACKET_OFFSET_VERSION] =
        LINK_PACKET_VERSION;

    encoded[LINK_PACKET_OFFSET_MESSAGE_TYPE] =
        (uint8_t)packet->message_type;

    encoded[LINK_PACKET_OFFSET_SOURCE_ID] =
        packet->source_id;

    encoded[LINK_PACKET_OFFSET_DESTINATION_ID] =
        packet->destination_id;

    link_packet_write_u16_be(
        &encoded[LINK_PACKET_OFFSET_SEQUENCE],
        packet->sequence
    );

    link_packet_write_u16_be(
        &encoded[LINK_PACKET_OFFSET_ACKNOWLEDGED_SEQUENCE],
        packet->acknowledged_sequence
    );

    encoded[LINK_PACKET_OFFSET_PAYLOAD_LENGTH] =
        packet->payload_length;

    encoded[LINK_PACKET_OFFSET_FLAGS] =
        packet->flags;

    memcpy(
        &encoded[LINK_PACKET_OFFSET_PAYLOAD],
        packet->payload,
        packet->payload_length
    );

    const uint16_t crc = link_crc16_ccitt_false(
        encoded,
        LINK_PACKET_OFFSET_CRC
    );

    link_packet_write_u16_be(
        &encoded[LINK_PACKET_OFFSET_CRC],
        crc
    );

    return LINK_PACKET_STATUS_OK;
}

LinkPacketStatus link_packet_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    uint8_t expected_destination_id,
    LinkPacket *packet
)
{
    if (encoded == NULL || packet == NULL) {
        return LINK_PACKET_STATUS_INVALID_ARGUMENT;
    }

    if (encoded_length != LINK_PACKET_ENCODED_SIZE) {
        return LINK_PACKET_STATUS_INVALID_FRAME_LENGTH;
    }

    if (encoded[LINK_PACKET_OFFSET_MAGIC_0] !=
            LINK_PACKET_MAGIC_BYTE_0 ||
        encoded[LINK_PACKET_OFFSET_MAGIC_1] !=
            LINK_PACKET_MAGIC_BYTE_1) {
        return LINK_PACKET_STATUS_INVALID_MAGIC;
    }

    if (encoded[LINK_PACKET_OFFSET_VERSION] !=
        LINK_PACKET_VERSION) {
        return LINK_PACKET_STATUS_UNSUPPORTED_VERSION;
    }

    if (!link_packet_message_type_is_valid(
            encoded[LINK_PACKET_OFFSET_MESSAGE_TYPE])) {
        return LINK_PACKET_STATUS_INVALID_MESSAGE_TYPE;
    }

    if (encoded[LINK_PACKET_OFFSET_DESTINATION_ID] !=
        expected_destination_id) {
        return LINK_PACKET_STATUS_INVALID_DESTINATION;
    }

    if (!link_packet_payload_length_is_valid(
            encoded[LINK_PACKET_OFFSET_PAYLOAD_LENGTH])) {
        return LINK_PACKET_STATUS_INVALID_PAYLOAD_LENGTH;
    }

    const uint16_t expected_crc = link_packet_read_u16_be(
        &encoded[LINK_PACKET_OFFSET_CRC]
    );

    const uint16_t calculated_crc =
        link_crc16_ccitt_false(
            encoded,
            LINK_PACKET_OFFSET_CRC
        );

    if (expected_crc != calculated_crc) {
        return LINK_PACKET_STATUS_CRC_MISMATCH;
    }

    LinkPacket decoded = { 0 };

    decoded.message_type =
        (LinkMessageType)
            encoded[LINK_PACKET_OFFSET_MESSAGE_TYPE];

    decoded.source_id =
        encoded[LINK_PACKET_OFFSET_SOURCE_ID];

    decoded.destination_id =
        encoded[LINK_PACKET_OFFSET_DESTINATION_ID];

    decoded.sequence = link_packet_read_u16_be(
        &encoded[LINK_PACKET_OFFSET_SEQUENCE]
    );

    decoded.acknowledged_sequence =
        link_packet_read_u16_be(
            &encoded[
                LINK_PACKET_OFFSET_ACKNOWLEDGED_SEQUENCE
            ]
        );

    decoded.payload_length =
        encoded[LINK_PACKET_OFFSET_PAYLOAD_LENGTH];

    decoded.flags =
        encoded[LINK_PACKET_OFFSET_FLAGS];

    memcpy(
        decoded.payload,
        &encoded[LINK_PACKET_OFFSET_PAYLOAD],
        LINK_PACKET_PAYLOAD_SIZE
    );

    *packet = decoded;

    return LINK_PACKET_STATUS_OK;
}
