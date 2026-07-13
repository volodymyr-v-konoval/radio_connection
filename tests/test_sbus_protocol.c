#include "sbus_protocol.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition, message)             \
    do {                                            \
        if (!(condition)) {                         \
            printf("FAIL: %s\n", (message));       \
            return false;                           \
        }                                           \
    } while (0)

static void build_sbus_frame(
    const uint16_t channels[SBUS_CHANNEL_COUNT],
    uint8_t flags,
    uint8_t footer,
    uint8_t out_frame[SBUS_FRAME_SIZE]
)
{
    memset(out_frame, 0, SBUS_FRAME_SIZE);
    out_frame[0] = SBUS_HEADER_BYTE;

    uint32_t bit_buffer = 0U;
    uint8_t bits_in_buffer = 0U;
    size_t out_pos = 1U;

    for (uint8_t channel = 0U;
         channel < SBUS_CHANNEL_COUNT;
         channel++) {
        bit_buffer |=
            ((uint32_t)(channels[channel] & 0x07FFU)) <<
            bits_in_buffer;

        bits_in_buffer =
            (uint8_t)(bits_in_buffer + 11U);

        while (bits_in_buffer >= 8U) {
            out_frame[out_pos] =
                (uint8_t)(bit_buffer & 0xFFU);

            out_pos++;
            bit_buffer >>= 8U;
            bits_in_buffer =
                (uint8_t)(bits_in_buffer - 8U);
        }
    }

    out_frame[SBUS_FLAGS_INDEX] = flags;
    out_frame[SBUS_FOOTER_INDEX] = footer;
}

static RadioParseResult feed_bytes(
    RadioProtocol *protocol,
    const uint8_t *data,
    size_t size,
    uint32_t now_ms
)
{
    RadioParseResult result = RADIO_PARSE_IDLE;

    for (size_t i = 0U; i < size; i++) {
        result = protocol->process_byte(
            protocol,
            data[i],
            now_ms
        );
    }

    return result;
}

static bool test_initialization_and_uart_config(void)
{
    RadioProtocol protocol;
    SbusProtocolContext context;

    sbus_protocol_init(&protocol, &context);

    TEST_ASSERT(
        protocol.type == RADIO_PROTOCOL_SBUS,
        "Protocol type should be SBUS"
    );

    TEST_ASSERT(
        protocol.process_byte != NULL &&
        protocol.get_frame != NULL &&
        protocol.get_uart_config != NULL,
        "SBUS interface callbacks should be installed"
    );

    RadioUartConfig config;

    TEST_ASSERT(
        radio_protocol_get_uart_config(
            &protocol,
            &config),
        "SBUS UART configuration should be available"
    );

    TEST_ASSERT(
        config.baud_rate == 100000U,
        "SBUS baud rate should be 100000"
    );

    TEST_ASSERT(
        config.data_bits == 8U,
        "SBUS should use eight data bits"
    );

    TEST_ASSERT(
        config.parity == RADIO_UART_PARITY_EVEN,
        "SBUS should use even parity"
    );

    TEST_ASSERT(
        config.stop_bits == RADIO_UART_STOP_BITS_2,
        "SBUS should use two stop bits"
    );

    TEST_ASSERT(
        config.signal_inverted,
        "SBUS signal should be inverted"
    );

    return true;
}

static bool test_decodes_sixteen_channels(void)
{
    const uint16_t expected[SBUS_CHANNEL_COUNT] = {
        172U, 1811U, 992U, 0U,
        2047U, 350U, 700U, 1050U,
        1400U, 50U, 1900U, 500U,
        1000U, 1500U, 2000U, 1234U
    };

    uint8_t frame_bytes[SBUS_FRAME_SIZE];

    build_sbus_frame(
        expected,
        0U,
        SBUS_FOOTER_BYTE,
        frame_bytes
    );

    RadioProtocol protocol;
    SbusProtocolContext context;
    sbus_protocol_init(&protocol, &context);

    TEST_ASSERT(
        feed_bytes(
            &protocol,
            frame_bytes,
            sizeof(frame_bytes),
            100U) == RADIO_PARSE_FRAME_READY,
        "Valid SBUS frame should become ready"
    );

    RcInputFrame frame;

    TEST_ASSERT(
        protocol.get_frame(&protocol, &frame),
        "Decoded SBUS frame should be available"
    );

    TEST_ASSERT(
        frame.protocol == RADIO_PROTOCOL_SBUS,
        "Decoded frame protocol should be SBUS"
    );

    TEST_ASSERT(
        frame.channel_count == SBUS_CHANNEL_COUNT,
        "SBUS should publish sixteen channels"
    );

    TEST_ASSERT(
        frame.frame_valid &&
        !frame.frame_lost &&
        !frame.failsafe,
        "Normal SBUS frame should be valid and active"
    );

    for (uint8_t i = 0U;
         i < SBUS_CHANNEL_COUNT;
         i++) {
        TEST_ASSERT(
            frame.channels[i] == expected[i],
            "Decoded SBUS channel should match source"
        );
    }

    TEST_ASSERT(
        context.received_frames == 1U &&
        context.valid_frames == 1U,
        "SBUS counters should record one valid frame"
    );

    TEST_ASSERT(
        !protocol.get_frame(&protocol, &frame),
        "Frame should be consumed only once"
    );

    return true;
}

static bool test_frame_lost_and_failsafe_flags(void)
{
    uint16_t channels[SBUS_CHANNEL_COUNT];

    for (uint8_t i = 0U;
         i < SBUS_CHANNEL_COUNT;
         i++) {
        channels[i] = 992U;
    }

    uint8_t frame_bytes[SBUS_FRAME_SIZE];

    build_sbus_frame(
        channels,
        SBUS_FLAG_FRAME_LOST | SBUS_FLAG_FAILSAFE,
        SBUS_FOOTER_BYTE,
        frame_bytes
    );

    RadioProtocol protocol;
    SbusProtocolContext context;
    sbus_protocol_init(&protocol, &context);

    TEST_ASSERT(
        feed_bytes(
            &protocol,
            frame_bytes,
            sizeof(frame_bytes),
            200U) == RADIO_PARSE_FRAME_READY,
        "Flagged SBUS frame should still parse"
    );

    RcInputFrame frame;

    TEST_ASSERT(
        protocol.get_frame(&protocol, &frame),
        "Flagged frame should be available"
    );

    TEST_ASSERT(
        frame.frame_valid,
        "Structurally correct flagged frame should remain valid"
    );

    TEST_ASSERT(
        frame.frame_lost,
        "Frame-lost flag should propagate"
    );

    TEST_ASSERT(
        frame.failsafe,
        "Failsafe flag should propagate"
    );

    return true;
}

static bool test_ignores_garbage_before_header(void)
{
    const uint8_t garbage[] = {
        0x00U, 0xFFU, 0x55U, 0xAAU, 0xF0U
    };

    RadioProtocol protocol;
    SbusProtocolContext context;
    sbus_protocol_init(&protocol, &context);

    TEST_ASSERT(
        feed_bytes(
            &protocol,
            garbage,
            sizeof(garbage),
            10U) == RADIO_PARSE_IDLE,
        "Garbage should be ignored while waiting for header"
    );

    TEST_ASSERT(
        context.received_frames == 0U,
        "Garbage should not count as a frame"
    );

    return true;
}

static bool test_rejects_invalid_footer(void)
{
    uint16_t channels[SBUS_CHANNEL_COUNT] = { 0U };
    uint8_t frame_bytes[SBUS_FRAME_SIZE];

    build_sbus_frame(
        channels,
        0U,
        0x55U,
        frame_bytes
    );

    RadioProtocol protocol;
    SbusProtocolContext context;
    sbus_protocol_init(&protocol, &context);

    TEST_ASSERT(
        feed_bytes(
            &protocol,
            frame_bytes,
            sizeof(frame_bytes),
            100U) == RADIO_PARSE_ERROR,
        "Invalid footer should be rejected"
    );

    RcInputFrame frame;

    TEST_ASSERT(
        !protocol.get_frame(&protocol, &frame),
        "Malformed frame should not be published"
    );

    TEST_ASSERT(
        context.received_frames == 1U &&
        context.malformed_frames == 1U,
        "Malformed frame counter should increment"
    );

    return true;
}

static bool test_timeout_resynchronizes_parser(void)
{
    uint16_t channels[SBUS_CHANNEL_COUNT];

    for (uint8_t i = 0U;
         i < SBUS_CHANNEL_COUNT;
         i++) {
        channels[i] = (uint16_t)(400U + i);
    }

    uint8_t frame_bytes[SBUS_FRAME_SIZE];

    build_sbus_frame(
        channels,
        0U,
        SBUS_FOOTER_BYTE,
        frame_bytes
    );

    RadioProtocol protocol;
    SbusProtocolContext context;
    sbus_protocol_init(&protocol, &context);

    TEST_ASSERT(
        protocol.process_byte(
            &protocol,
            SBUS_HEADER_BYTE,
            10U) == RADIO_PARSE_IN_PROGRESS,
        "Header should start a frame"
    );

    TEST_ASSERT(
        protocol.process_byte(
            &protocol,
            0x11U,
            10U) == RADIO_PARSE_IN_PROGRESS,
        "Partial frame should remain in progress"
    );

    TEST_ASSERT(
        feed_bytes(
            &protocol,
            frame_bytes,
            sizeof(frame_bytes),
            20U) == RADIO_PARSE_FRAME_READY,
        "Parser should recover after an inter-byte timeout"
    );

    TEST_ASSERT(
        context.timeout_resets == 1U,
        "Timeout reset counter should increment"
    );

    return true;
}

static bool test_two_frames_back_to_back(void)
{
    uint16_t channels_a[SBUS_CHANNEL_COUNT];
    uint16_t channels_b[SBUS_CHANNEL_COUNT];

    for (uint8_t i = 0U;
         i < SBUS_CHANNEL_COUNT;
         i++) {
        channels_a[i] = 600U;
        channels_b[i] = 1400U;
    }

    uint8_t frame_a[SBUS_FRAME_SIZE];
    uint8_t frame_b[SBUS_FRAME_SIZE];

    build_sbus_frame(
        channels_a,
        0U,
        SBUS_FOOTER_BYTE,
        frame_a
    );

    build_sbus_frame(
        channels_b,
        0U,
        SBUS_FOOTER_BYTE,
        frame_b
    );

    RadioProtocol protocol;
    SbusProtocolContext context;
    sbus_protocol_init(&protocol, &context);

    TEST_ASSERT(
        feed_bytes(
            &protocol,
            frame_a,
            sizeof(frame_a),
            100U) == RADIO_PARSE_FRAME_READY,
        "First SBUS frame should parse"
    );

    RcInputFrame decoded;

    TEST_ASSERT(
        protocol.get_frame(&protocol, &decoded),
        "First frame should be available"
    );

    TEST_ASSERT(
        decoded.channels[0] == 600U,
        "First frame should contain first values"
    );

    TEST_ASSERT(
        feed_bytes(
            &protocol,
            frame_b,
            sizeof(frame_b),
            101U) == RADIO_PARSE_FRAME_READY,
        "Second SBUS frame should parse"
    );

    TEST_ASSERT(
        protocol.get_frame(&protocol, &decoded),
        "Second frame should be available"
    );

    TEST_ASSERT(
        decoded.channels[0] == 1400U,
        "Second frame should contain second values"
    );

    TEST_ASSERT(
        context.received_frames == 2U &&
        context.valid_frames == 2U,
        "Both back-to-back frames should be counted"
    );

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_initialization_and_uart_config()) {
        failed++;
    }

    if (!test_decodes_sixteen_channels()) {
        failed++;
    }

    if (!test_frame_lost_and_failsafe_flags()) {
        failed++;
    }

    if (!test_ignores_garbage_before_header()) {
        failed++;
    }

    if (!test_rejects_invalid_footer()) {
        failed++;
    }

    if (!test_timeout_resynchronizes_parser()) {
        failed++;
    }

    if (!test_two_frames_back_to_back()) {
        failed++;
    }

    if (failed != 0) {
        printf("%d SBUS protocol test(s) failed\n", failed);
        return 1;
    }

    printf("All SBUS protocol tests passed\n");
    return 0;
}
