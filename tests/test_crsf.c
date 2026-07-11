#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "crsf_crc.h"
#include "crsf_protocol.h"

#define TEST_ASSERT(cond, msg)               \
    do {                                     \
        if (!(cond)) {                       \
            printf("FAIL: %s\n", msg);       \
            return false;                    \
        }                                    \
    } while (0)

static bool test_crsf_crc_sample_frame(void)
{
    const uint8_t crc_data[] = {
        0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C
    };

    uint8_t crc = crsf_crc8_dvb_s2(crc_data, sizeof(crc_data));

    TEST_ASSERT(crc == 0xADU, "CRSF CRC should be 0xAD");

    return true;
}

static bool test_crsf_parser_sample_frame(void)
{
    const uint8_t frame[] = {
        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0xAD
    };

    RadioProtocol protocol;
    CrsfProtocolContext context;

    crsf_protocol_init(&protocol, &context);

    RadioParseResult result = RADIO_PARSE_IDLE;

    for (size_t i = 0; i < sizeof(frame); i++) {
        result = protocol.process_byte(&protocol, frame[i], 0U);
    }

    TEST_ASSERT(result == RADIO_PARSE_FRAME_READY, "Parser should return FRAME_READY");

    RcInputFrame rc_frame;

    TEST_ASSERT(protocol.get_frame(&protocol, &rc_frame), "Frame should be available");
    TEST_ASSERT(rc_frame.protocol == RADIO_PROTOCOL_CRSF, "Protocol should be CRSF");
    TEST_ASSERT(rc_frame.frame_valid == true, "Frame should be valid");
    TEST_ASSERT(rc_frame.failsafe == false, "Failsafe should be false");
    TEST_ASSERT(rc_frame.channel_count == 16U, "Channel count should be 16");

    for (uint8_t i = 0; i < rc_frame.channel_count; i++) {
        TEST_ASSERT(rc_frame.channels[i] == 992U, "All channels should be 992");
    }

    return true;
}

static bool test_crsf_bad_crc_rejected(void)
{
    const uint8_t frame[] = {
        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0x00
    };

    RadioProtocol protocol;
    CrsfProtocolContext context;
    crsf_protocol_init(&protocol, &context);

    RadioParseResult result = RADIO_PARSE_IDLE;

    for (size_t i = 0; i < sizeof(frame); i++) {
        result = protocol.process_byte(&protocol, frame[i], 0U);
    }

    TEST_ASSERT(result == RADIO_PARSE_ERROR, "Bad CRC should return PARSE_ERROR");
    TEST_ASSERT(context.crc_errors == 1U, "CRC error counter should be 1");

    RcInputFrame rc_frame;
    TEST_ASSERT(!protocol.get_frame(&protocol, &rc_frame), "Bad frame should not be available");

    return true;
}

static bool test_crsf_garbage_before_frame(void)
{
    const uint8_t stream[] = {
        0x00, 0x11, 0x22, 0x99,
        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0xAD
    };

    RadioProtocol protocol;
    CrsfProtocolContext context;
    crsf_protocol_init(&protocol, &context);

    RadioParseResult result = RADIO_PARSE_IDLE;

    for (size_t i = 0; i < sizeof(stream); i++) {
        result = protocol.process_byte(&protocol, stream[i], 0U);
    }

    TEST_ASSERT(result == RADIO_PARSE_FRAME_READY, "Parser should recover after garbage");

    RcInputFrame frame;
    TEST_ASSERT(protocol.get_frame(&protocol, &frame), "Frame should be available after garbage");
    TEST_ASSERT(frame.channel_count == 16U, "Channel count should be 16");

    return true;
}

static bool test_crsf_wrong_length_rejected(void)
{
    const uint8_t frame[] = {
        0xC8, 0x01, 0x16
    };

    RadioProtocol protocol;
    CrsfProtocolContext context;
    crsf_protocol_init(&protocol, &context);

    RadioParseResult result = RADIO_PARSE_IDLE;

    for (size_t i = 0; i < sizeof(frame); i++) {
        result = protocol.process_byte(&protocol, frame[i], 0U);
    }

    TEST_ASSERT(result == RADIO_PARSE_ERROR, "Wrong length should return PARSE_ERROR");
    TEST_ASSERT(context.length_errors == 1U, "Length error counter should be 1");

    return true;
}

static bool test_crsf_unsupported_frame_type(void)
{
    /*
     * Frame with unsupported type 0x99.
     * Length 0x03 = type + 1 payload byte + CRC.
     * CRC for bytes {0x99, 0x01} is 0xD3.
     */
    const uint8_t frame[] = {
        0xC8, 0x03, 0x99, 0x01, 0xD3
    };

    RadioProtocol protocol;
    CrsfProtocolContext context;
    crsf_protocol_init(&protocol, &context);

    RadioParseResult result = RADIO_PARSE_IDLE;

    for (size_t i = 0; i < sizeof(frame); i++) {
        result = protocol.process_byte(&protocol, frame[i], 0U);
    }

    TEST_ASSERT(result == RADIO_PARSE_IN_PROGRESS, "Unsupported frame should be ignored");
    TEST_ASSERT(context.unsupported_frames == 1U, "Unsupported counter should be 1");

    RcInputFrame rc_frame;
    TEST_ASSERT(!protocol.get_frame(&protocol, &rc_frame), "Unsupported frame should not produce RcInputFrame");

    return true;
}

static bool test_crsf_two_frames_in_row(void)
{
    const uint8_t frame[] = {
        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0xAD,

        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0xAD
    };

    RadioProtocol protocol;
    CrsfProtocolContext context;
    crsf_protocol_init(&protocol, &context);

    uint8_t ready_count = 0U;

    for (size_t i = 0; i < sizeof(frame); i++) {
        RadioParseResult result = protocol.process_byte(&protocol, frame[i], 0U);

        if (result == RADIO_PARSE_FRAME_READY) {
            RcInputFrame rc_frame;
            TEST_ASSERT(protocol.get_frame(&protocol, &rc_frame), "Frame should be readable");
            ready_count++;
        }
    }

    TEST_ASSERT(ready_count == 2U, "Parser should decode two frames in row");
    TEST_ASSERT(context.valid_frames == 2U, "Valid frame counter should be 2");

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_crsf_crc_sample_frame()) {
        failed++;
    }

    if (!test_crsf_parser_sample_frame()) {
        failed++;
    }

    if (failed == 0) {
        printf("All CRSF tests passed\n");
        return 0;
    }

    if (!test_crsf_bad_crc_rejected()) {
        failed++;
    }

    if (!test_crsf_garbage_before_frame()) {
        failed++;
    }

    if (!test_crsf_wrong_length_rejected()) {
        failed++;
    }

    if (!test_crsf_unsupported_frame_type()) {
        failed++;
    }

    if (!test_crsf_two_frames_in_row()) {
        failed++;
    }

    printf("%d CRSF test(s) failed\n", failed);
    return 1;
}
