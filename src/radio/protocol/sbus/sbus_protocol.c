#include "sbus_protocol.h"

#include <stddef.h>
#include <string.h>

static void sbus_protocol_reset(RadioProtocol *self);

static RadioParseResult sbus_protocol_process_byte(
    RadioProtocol *self,
    uint8_t byte,
    uint32_t now_ms
);

static bool sbus_protocol_get_frame(
    RadioProtocol *self,
    RcInputFrame *out_frame
);

static bool sbus_protocol_get_uart_config(
    RadioProtocol *self,
    RadioUartConfig *out_config
);

static void sbus_parser_reset(SbusProtocolContext *context);

static void sbus_start_frame(
    SbusProtocolContext *context,
    uint32_t now_ms
);

static bool sbus_decode_channels(
    const uint8_t *channel_data,
    size_t channel_data_size,
    RcInputFrame *out_frame
);

void sbus_protocol_init(
    RadioProtocol *protocol,
    SbusProtocolContext *context
)
{
    if (protocol == NULL || context == NULL) {
        return;
    }

    memset(protocol, 0, sizeof(*protocol));
    memset(context, 0, sizeof(*context));

    protocol->context = context;
    protocol->type = RADIO_PROTOCOL_SBUS;
    protocol->reset = sbus_protocol_reset;
    protocol->process_byte = sbus_protocol_process_byte;
    protocol->get_frame = sbus_protocol_get_frame;
    protocol->get_uart_config = sbus_protocol_get_uart_config;

    sbus_protocol_reset(protocol);
}

static void sbus_protocol_reset(RadioProtocol *self)
{
    if (self == NULL || self->context == NULL) {
        return;
    }

    SbusProtocolContext *context =
        (SbusProtocolContext *)self->context;

    sbus_parser_reset(context);
    context->frame_ready = false;
}

static void sbus_parser_reset(SbusProtocolContext *context)
{
    if (context == NULL) {
        return;
    }

    context->parser_state = SBUS_PARSER_WAIT_HEADER;
    context->frame_pos = 0U;
    context->last_byte_time_ms = 0U;
}

static void sbus_start_frame(
    SbusProtocolContext *context,
    uint32_t now_ms
)
{
    context->frame_buffer[0] = SBUS_HEADER_BYTE;
    context->frame_pos = 1U;
    context->last_byte_time_ms = now_ms;
    context->parser_state = SBUS_PARSER_READ_FRAME;
}

static RadioParseResult sbus_protocol_process_byte(
    RadioProtocol *self,
    uint8_t byte,
    uint32_t now_ms
)
{
    if (self == NULL || self->context == NULL) {
        return RADIO_PARSE_ERROR;
    }

    SbusProtocolContext *context =
        (SbusProtocolContext *)self->context;

    if (context->parser_state == SBUS_PARSER_READ_FRAME &&
        (uint32_t)(now_ms - context->last_byte_time_ms) >=
            SBUS_INTER_BYTE_TIMEOUT_MS) {
        context->timeout_resets++;
        sbus_parser_reset(context);
    }

    if (context->parser_state == SBUS_PARSER_WAIT_HEADER) {
        if (byte != SBUS_HEADER_BYTE) {
            return RADIO_PARSE_IDLE;
        }

        sbus_start_frame(context, now_ms);
        return RADIO_PARSE_IN_PROGRESS;
    }

    if (context->frame_pos >= SBUS_FRAME_SIZE) {
        context->malformed_frames++;
        sbus_parser_reset(context);
        return RADIO_PARSE_ERROR;
    }

    context->frame_buffer[context->frame_pos] = byte;
    context->frame_pos++;
    context->last_byte_time_ms = now_ms;

    if (context->frame_pos < SBUS_FRAME_SIZE) {
        return RADIO_PARSE_IN_PROGRESS;
    }

    context->received_frames++;

    if (context->frame_buffer[SBUS_FOOTER_INDEX] !=
        SBUS_FOOTER_BYTE) {
        context->malformed_frames++;

        /*
         * A missing footer may place the next frame header in the
         * footer position. Reuse that byte to resynchronize quickly.
         */
        const bool footer_is_next_header =
            context->frame_buffer[SBUS_FOOTER_INDEX] ==
            SBUS_HEADER_BYTE;

        sbus_parser_reset(context);

        if (footer_is_next_header) {
            sbus_start_frame(context, now_ms);
        }

        return RADIO_PARSE_ERROR;
    }

    if (!sbus_decode_channels(
            &context->frame_buffer[1],
            SBUS_CHANNEL_DATA_SIZE,
            &context->latest_frame)) {
        context->malformed_frames++;
        sbus_parser_reset(context);
        return RADIO_PARSE_ERROR;
    }

    const uint8_t flags =
        context->frame_buffer[SBUS_FLAGS_INDEX];

    context->latest_frame.protocol = RADIO_PROTOCOL_SBUS;
    context->latest_frame.frame_valid = true;
    context->latest_frame.frame_lost =
        (flags & SBUS_FLAG_FRAME_LOST) != 0U;
    context->latest_frame.failsafe =
        (flags & SBUS_FLAG_FAILSAFE) != 0U;

    context->frame_ready = true;
    context->valid_frames++;

    sbus_parser_reset(context);
    return RADIO_PARSE_FRAME_READY;
}

static bool sbus_protocol_get_frame(
    RadioProtocol *self,
    RcInputFrame *out_frame
)
{
    if (self == NULL ||
        self->context == NULL ||
        out_frame == NULL) {
        return false;
    }

    SbusProtocolContext *context =
        (SbusProtocolContext *)self->context;

    if (!context->frame_ready) {
        return false;
    }

    *out_frame = context->latest_frame;
    context->frame_ready = false;

    return true;
}

static bool sbus_protocol_get_uart_config(
    RadioProtocol *self,
    RadioUartConfig *out_config
)
{
    if (self == NULL ||
        self->type != RADIO_PROTOCOL_SBUS ||
        out_config == NULL) {
        return false;
    }

    out_config->baud_rate = 100000U;
    out_config->data_bits = 8U;
    out_config->parity = RADIO_UART_PARITY_EVEN;
    out_config->stop_bits = RADIO_UART_STOP_BITS_2;
    out_config->signal_inverted = true;

    return true;
}

static bool sbus_decode_channels(
    const uint8_t *channel_data,
    size_t channel_data_size,
    RcInputFrame *out_frame
)
{
    if (channel_data == NULL ||
        out_frame == NULL ||
        channel_data_size != SBUS_CHANNEL_DATA_SIZE) {
        return false;
    }

    memset(out_frame, 0, sizeof(*out_frame));

    uint32_t bit_buffer = 0U;
    uint8_t bits_available = 0U;
    size_t data_pos = 0U;

    for (uint8_t channel = 0U;
         channel < SBUS_CHANNEL_COUNT;
         channel++) {
        while (bits_available < 11U) {
            if (data_pos >= channel_data_size) {
                return false;
            }

            bit_buffer |=
                ((uint32_t)channel_data[data_pos]) <<
                bits_available;

            data_pos++;
            bits_available =
                (uint8_t)(bits_available + 8U);
        }

        out_frame->channels[channel] =
            (uint16_t)(bit_buffer & 0x07FFU);

        bit_buffer >>= 11U;
        bits_available =
            (uint8_t)(bits_available - 11U);
    }

    out_frame->channel_count = SBUS_CHANNEL_COUNT;
    return true;
}
