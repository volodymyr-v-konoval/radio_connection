#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "rc_receiver_service.h"
#include "crsf_protocol.h"
#include "pc_logger.h"
#include "pc_time.h"
#include "pc_mock_transport.h"

#define TEST_ASSERT(cond, msg)                 \
    do {                                       \
        if (!(cond)) {                         \
            printf("FAIL: %s\n", msg);         \
            return false;                      \
        }                                      \
    } while (0)

static bool test_service_receives_crsf_channels(void)
{
    const uint8_t crsf_sample_frame[] = {
        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0xAD
    };

    RadioLogger logger;
    pc_logger_init(&logger, RADIO_LOG_LEVEL_WARN);

    RadioTime time;
    pc_time_init(&time);

    RadioTransport transport;
    PcMockTransportContext transport_ctx;

    pc_mock_transport_init(
        &transport,
        &transport_ctx,
        crsf_sample_frame,
        sizeof(crsf_sample_frame)
    );

    RadioProtocol protocol;
    CrsfProtocolContext crsf_ctx;
    crsf_protocol_init(&protocol, &crsf_ctx);

    RcReceiverService service;

    TEST_ASSERT(
        rc_receiver_service_init(
            &service,
            &transport,
            &protocol,
            &logger,
            &time,
            100U
        ),
        "Service init should succeed"
    );

    rc_receiver_service_process(&service);

    RcInputFrame frame;

    TEST_ASSERT(
        rc_receiver_service_get_latest_frame(&service, &frame),
        "Service should provide latest frame"
    );

    TEST_ASSERT(frame.protocol == RADIO_PROTOCOL_CRSF, "Protocol should be CRSF");
    TEST_ASSERT(frame.frame_valid == true, "Frame should be valid");
    TEST_ASSERT(frame.failsafe == false, "Failsafe should be false");
    TEST_ASSERT(frame.channel_count == 16U, "Channel count should be 16");

    for (uint8_t i = 0; i < frame.channel_count; i++) {
        TEST_ASSERT(frame.channels[i] == 992U, "Each channel should be 992");
    }

    return true;
}

int main(void)
{
    if (!test_service_receives_crsf_channels()) {
        printf("RC service CRSF test failed\n");
        return 1;
    }

    printf("RC service CRSF test passed\n");
    return 0;
}
