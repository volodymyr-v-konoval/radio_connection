#include "pc_logger.h"
#include "pc_mock_transport.h"
#include "pc_time.h"
#include "rc_receiver_service.h"
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
    out_frame[SBUS_FOOTER_INDEX] = SBUS_FOOTER_BYTE;
}

static bool run_service_test(
    uint8_t flags,
    bool expected_service_failsafe
)
{
    uint16_t expected[SBUS_CHANNEL_COUNT];

    for (uint8_t i = 0U;
         i < SBUS_CHANNEL_COUNT;
         i++) {
        expected[i] = (uint16_t)(172U + (uint16_t)(i * 90U));
    }

    uint8_t frame_bytes[SBUS_FRAME_SIZE];
    build_sbus_frame(expected, flags, frame_bytes);

    RadioLogger logger;
    pc_logger_init(&logger, RADIO_LOG_LEVEL_WARN);

    RadioTime time;
    pc_time_init(&time);

    RadioTransport transport;
    PcMockTransportContext transport_context;

    pc_mock_transport_init(
        &transport,
        &transport_context,
        frame_bytes,
        sizeof(frame_bytes)
    );

    RadioProtocol protocol;
    SbusProtocolContext sbus_context;
    sbus_protocol_init(&protocol, &sbus_context);

    RcReceiverService service;

    TEST_ASSERT(
        rc_receiver_service_init(
            &service,
            &transport,
            &protocol,
            &logger,
            &time,
            100U),
        "Receiver service should initialize with SBUS"
    );

    rc_receiver_service_process(&service);

    RcInputFrame frame;

    TEST_ASSERT(
        rc_receiver_service_get_latest_frame(
            &service,
            &frame),
        "Receiver service should publish an SBUS frame"
    );

    TEST_ASSERT(
        frame.protocol == RADIO_PROTOCOL_SBUS,
        "Receiver service should preserve SBUS protocol type"
    );

    TEST_ASSERT(
        frame.channel_count == SBUS_CHANNEL_COUNT,
        "Receiver service should publish sixteen SBUS channels"
    );

    for (uint8_t i = 0U;
         i < SBUS_CHANNEL_COUNT;
         i++) {
        TEST_ASSERT(
            frame.channels[i] == expected[i],
            "Receiver service should preserve decoded SBUS channels"
        );
    }

    TEST_ASSERT(
        rc_receiver_service_is_failsafe(&service) ==
            expected_service_failsafe,
        "Receiver service failsafe state should follow SBUS flags"
    );

    return true;
}

static bool test_service_receives_normal_sbus_frame(void)
{
    return run_service_test(0U, false);
}

static bool test_service_treats_frame_lost_as_failsafe(void)
{
    return run_service_test(
        SBUS_FLAG_FRAME_LOST,
        true
    );
}

static bool test_service_treats_sbus_failsafe_as_failsafe(void)
{
    return run_service_test(
        SBUS_FLAG_FAILSAFE,
        true
    );
}

int main(void)
{
    int failed = 0;

    if (!test_service_receives_normal_sbus_frame()) {
        failed++;
    }

    if (!test_service_treats_frame_lost_as_failsafe()) {
        failed++;
    }

    if (!test_service_treats_sbus_failsafe_as_failsafe()) {
        failed++;
    }

    if (failed != 0) {
        printf("%d RC service SBUS test(s) failed\n", failed);
        return 1;
    }

    printf("All RC service SBUS tests passed\n");
    return 0;
}
