#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "link_packet.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));     \
            return false;                        \
        }                                        \
    } while (0)

static LinkPacket make_packet(
    uint8_t payload_length
)
{
    LinkPacket packet = {
        .message_type = LINK_MESSAGE_TYPE_DATA,
        .source_id = 1U,
        .destination_id = 2U,
        .sequence = 0x1234U,
        .acknowledged_sequence = 0x0102U,
        .payload_length = payload_length,
        .flags = 0xA5U,
        .payload = { 0U }
    };

    for (uint8_t i = 0U;
         i < LINK_PACKET_PAYLOAD_SIZE;
         i++) {
        packet.payload[i] =
            (uint8_t)(0x20U + i);
    }

    return packet;
}

static bool packets_are_equal(
    const LinkPacket *expected,
    const LinkPacket *actual
)
{
    return
        expected->message_type ==
            actual->message_type &&
        expected->source_id ==
            actual->source_id &&
        expected->destination_id ==
            actual->destination_id &&
        expected->sequence ==
            actual->sequence &&
        expected->acknowledged_sequence ==
            actual->acknowledged_sequence &&
        expected->payload_length ==
            actual->payload_length &&
        expected->flags ==
            actual->flags &&
        memcmp(
            expected->payload,
            actual->payload,
            expected->payload_length
        ) == 0;
}

static bool test_encode_decode_16_byte_payload(void)
{
    const LinkPacket source =
        make_packet(LINK_PACKET_PAYLOAD_SIZE);

    uint8_t encoded[LINK_PACKET_ENCODED_SIZE];
    LinkPacket decoded;

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) == LINK_PACKET_STATUS_OK,
        "16-byte packet should encode"
    );

    TEST_ASSERT(
        encoded[0] == LINK_PACKET_MAGIC_BYTE_0,
        "First magic byte should be L"
    );

    TEST_ASSERT(
        encoded[1] == LINK_PACKET_MAGIC_BYTE_1,
        "Second magic byte should be R"
    );

    TEST_ASSERT(
        encoded[2] == LINK_PACKET_VERSION,
        "Packet version should be encoded"
    );

    TEST_ASSERT(
        encoded[6] == 0x12U &&
        encoded[7] == 0x34U,
        "Sequence should use network byte order"
    );

    TEST_ASSERT(
        link_packet_decode(
            encoded,
            sizeof(encoded),
            source.destination_id,
            &decoded
        ) == LINK_PACKET_STATUS_OK,
        "16-byte packet should decode"
    );

    TEST_ASSERT(
        packets_are_equal(&source, &decoded),
        "Decoded packet should match source"
    );

    return true;
}

static bool test_encode_decode_10_byte_payload(void)
{
    const LinkPacket source =
        make_packet(LINK_PACKET_MIN_PAYLOAD_SIZE);

    uint8_t encoded[LINK_PACKET_ENCODED_SIZE];
    LinkPacket decoded;

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) == LINK_PACKET_STATUS_OK,
        "10-byte packet should encode"
    );

    TEST_ASSERT(
        link_packet_decode(
            encoded,
            sizeof(encoded),
            source.destination_id,
            &decoded
        ) == LINK_PACKET_STATUS_OK,
        "10-byte packet should decode"
    );

    TEST_ASSERT(
        packets_are_equal(&source, &decoded),
        "Decoded 10-byte packet should match"
    );

    for (size_t i = LINK_PACKET_MIN_PAYLOAD_SIZE;
         i < LINK_PACKET_PAYLOAD_SIZE;
         i++) {
        TEST_ASSERT(
            decoded.payload[i] == 0U,
            "Unused payload bytes should be zero"
        );
    }

    return true;
}

static bool test_corrupted_crc_is_rejected(void)
{
    const LinkPacket source =
        make_packet(LINK_PACKET_PAYLOAD_SIZE);

    uint8_t encoded[LINK_PACKET_ENCODED_SIZE];
    LinkPacket decoded;

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) == LINK_PACKET_STATUS_OK,
        "Reference packet should encode"
    );

    encoded[12] ^= 0x01U;

    TEST_ASSERT(
        link_packet_decode(
            encoded,
            sizeof(encoded),
            source.destination_id,
            &decoded
        ) == LINK_PACKET_STATUS_CRC_MISMATCH,
        "Corrupted payload should fail CRC"
    );

    return true;
}

static bool test_bad_header_fields_are_rejected(void)
{
    const LinkPacket source =
        make_packet(LINK_PACKET_PAYLOAD_SIZE);

    uint8_t encoded[LINK_PACKET_ENCODED_SIZE];
    LinkPacket decoded;

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) == LINK_PACKET_STATUS_OK,
        "Reference packet should encode"
    );

    encoded[0] = 0U;

    TEST_ASSERT(
        link_packet_decode(
            encoded,
            sizeof(encoded),
            source.destination_id,
            &decoded
        ) == LINK_PACKET_STATUS_INVALID_MAGIC,
        "Bad magic should be rejected"
    );

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) == LINK_PACKET_STATUS_OK,
        "Packet should re-encode"
    );

    encoded[2] =
        (uint8_t)(LINK_PACKET_VERSION + 1U);

    TEST_ASSERT(
        link_packet_decode(
            encoded,
            sizeof(encoded),
            source.destination_id,
            &decoded
        ) == LINK_PACKET_STATUS_UNSUPPORTED_VERSION,
        "Unsupported version should be rejected"
    );

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) == LINK_PACKET_STATUS_OK,
        "Packet should re-encode again"
    );

    TEST_ASSERT(
        link_packet_decode(
            encoded,
            sizeof(encoded),
            3U,
            &decoded
        ) == LINK_PACKET_STATUS_INVALID_DESTINATION,
        "Wrong destination should be rejected"
    );

    return true;
}

static bool test_invalid_frame_length_is_rejected(void)
{
    const LinkPacket source =
        make_packet(LINK_PACKET_PAYLOAD_SIZE);

    uint8_t encoded[LINK_PACKET_ENCODED_SIZE];
    LinkPacket decoded;

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) == LINK_PACKET_STATUS_OK,
        "Reference packet should encode"
    );

    TEST_ASSERT(
        link_packet_decode(
            encoded,
            sizeof(encoded) - 1U,
            source.destination_id,
            &decoded
        ) ==
            LINK_PACKET_STATUS_INVALID_FRAME_LENGTH,
        "Non-30-byte frame should be rejected"
    );

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded) - 1U
        ) ==
            LINK_PACKET_STATUS_INVALID_FRAME_LENGTH,
        "Short output buffer should be rejected"
    );

    return true;
}

static bool test_invalid_payload_lengths_are_rejected(
    void
)
{
    LinkPacket source = make_packet(
        LINK_PACKET_MIN_PAYLOAD_SIZE - 1U
    );

    uint8_t encoded[LINK_PACKET_ENCODED_SIZE];

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) ==
            LINK_PACKET_STATUS_INVALID_PAYLOAD_LENGTH,
        "Payload shorter than 10 should fail"
    );

    source.payload_length =
        LINK_PACKET_PAYLOAD_SIZE + 1U;

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) ==
            LINK_PACKET_STATUS_INVALID_PAYLOAD_LENGTH,
        "Payload longer than 16 should fail"
    );

    return true;
}

static bool test_invalid_message_type_is_rejected(void)
{
    LinkPacket source =
        make_packet(LINK_PACKET_PAYLOAD_SIZE);

    uint8_t encoded[LINK_PACKET_ENCODED_SIZE];

    source.message_type =
        (LinkMessageType)0U;

    TEST_ASSERT(
        link_packet_encode(
            &source,
            encoded,
            sizeof(encoded)
        ) ==
            LINK_PACKET_STATUS_INVALID_MESSAGE_TYPE,
        "Unknown message type should be rejected"
    );

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_encode_decode_16_byte_payload()) {
        failures++;
    }

    if (!test_encode_decode_10_byte_payload()) {
        failures++;
    }

    if (!test_corrupted_crc_is_rejected()) {
        failures++;
    }

    if (!test_bad_header_fields_are_rejected()) {
        failures++;
    }

    if (!test_invalid_frame_length_is_rejected()) {
        failures++;
    }

    if (!test_invalid_payload_lengths_are_rejected()) {
        failures++;
    }

    if (!test_invalid_message_type_is_rejected()) {
        failures++;
    }

    if (failures != 0) {
        printf(
            "%d link packet test(s) failed\n",
            failures
        );
        return 1;
    }

    printf("All link packet tests passed\n");
    return 0;
}
