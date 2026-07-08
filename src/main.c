#include <stdio.h>
#include <string.h>

#include "rc_receiver_service.h"
#include "pc_logger.h"
#include "pc_time.h"
#include "pc_mock_transport.h"

typedef struct
{
    uint8_t byte_count;
    RcInputFrame frame;
} DummyProtocolContext;

static void dummy_protocol_reset(RadioProtocol *self)
{
    DummyProtocolContext *ctx = (DummyProtocolContext *)self->context;

    ctx->byte_count = 0;
    memset(&ctx->frame, 0, sizeof(ctx->frame));
}

static RadioParseResult dummy_protocol_process_byte(
    RadioProtocol *self,
    uint8_t byte,
    uint32_t now_ms
)
{
    (void)byte;
    (void)now_ms;

    DummyProtocolContext *ctx = (DummyProtocolContext *)self->context;

    ctx->byte_count++;

    if (ctx->byte_count >= 3) {
        ctx->frame.channels[0] = 1000;
        ctx->frame.channels[1] = 1500;
        ctx->frame.channels[2] = 2000;
        ctx->frame.channel_count = 3;
        ctx->frame.frame_valid = true;
        ctx->frame.failsafe = false;
        ctx->frame.frame_lost = false;
        ctx->frame.protocol = RADIO_PROTOCOL_NONE;

        return RADIO_PARSE_FRAME_READY;
    }

    return RADIO_PARSE_IN_PROGRESS;
}

static bool dummy_protocol_get_frame(
    RadioProtocol *self,
    RcInputFrame *out_frame
)
{
    DummyProtocolContext *ctx = (DummyProtocolContext *)self->context;

    if (out_frame == NULL) {
        return false;
    }

    *out_frame = ctx->frame;
    return true;
}

int main(void)
{
    uint8_t fake_uart_data[] = {
        0x11, 0x22, 0x33
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
        fake_uart_data,
        sizeof(fake_uart_data)
    );

    DummyProtocolContext protocol_context;

    RadioProtocol protocol = {
        .context = &protocol_context,
        .type = RADIO_PROTOCOL_NONE,
        .reset = dummy_protocol_reset,
        .process_byte = dummy_protocol_process_byte,
        .get_frame = dummy_protocol_get_frame
    };

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
        printf("Latest frame:\n");
        printf("  channels: %u\n", frame.channel_count);
        printf("  ch1: %u\n", frame.channels[0]);
        printf("  ch2: %u\n", frame.channels[1]);
        printf("  ch3: %u\n", frame.channels[2]);
        printf("  failsafe: %d\n", frame.failsafe);
    } else {
        printf("No frame received\n");
    }

    return 0;
}
