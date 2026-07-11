#include <stdio.h>

#include "rc_receiver_service.h"
#include "pc_logger.h"
#include "pc_time.h"
#include "pc_mock_transport.h"
#include "crsf_protocol.h"

int main(void)
{
    /*
     * Valid CRSF RC_CHANNELS_PACKED frame.
     *
     * Address: 0xC8
     * Length:  0x18 = type + 22 payload bytes + CRC
     * Type:    0x16 = RC_CHANNELS_PACKED
     *
     * Channels are packed as 16 x 11-bit values.
     * This sample contains all channels around 992.
     */
    const uint8_t crsf_sample_frame[] = {
        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0xAD
    };

    RadioLogger logger;
    pc_logger_init(&logger, RADIO_LOG_LEVEL_DEBUG);

    RadioTime time;
    pc_time_init(&time);

    RadioTransport transport;
    PcMockTransportContext transport_context;

    pc_mock_transport_init(
        &transport,
        &transport_context,
        crsf_sample_frame,
        sizeof(crsf_sample_frame)
    );

    RadioProtocol protocol;
    CrsfProtocolContext crsf_context;

    crsf_protocol_init(
        &protocol,
        &crsf_context
    );

    RcReceiverService service;

    if (!rc_receiver_service_init(
            &service,
            &transport,
            &protocol,
            &logger,
            &time,
            100U
        )) {
        printf("Failed to init RC receiver service\n");
        return 1;
    }

    rc_receiver_service_process(&service);

    RcInputFrame frame;

    if (rc_receiver_service_get_latest_frame(&service, &frame)) {
        printf("Latest CRSF frame:\n");
        printf("  protocol: %d\n", frame.protocol);
        printf("  channels: %u\n", frame.channel_count);
        printf("  failsafe: %d\n", frame.failsafe);
        printf("  frame_valid: %d\n", frame.frame_valid);

        for (uint8_t i = 0; i < frame.channel_count; i++) {
            printf("  ch%u: %u\n", (unsigned)(i + 1U), frame.channels[i]);
        }
    } else {
        printf("No CRSF frame received\n");
    }

    return 0;
}
