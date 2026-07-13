#include "crsf_protocol.h"
#include "crsf_crc.h"

#include <string.h>

#define CRSF_FRAME_ADDRESS_INDEX 0U
#define CRSF_FRAME_LENGTH_INDEX  1U
#define CRSF_FRAME_TYPE_INDEX    2U

#define CRSF_MIN_FRAME_SIZE      4U
#define CRSF_RC_PAYLOAD_SIZE     22U
#define CRSF_RC_CHANNEL_COUNT    16U

static void crsf_protocol_reset(RadioProtocol *self);
static void crsf_parser_reset(CrsfProtocolContext *ctx);
static void crsf_parser_reset(CrsfProtocolContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->parser_state = CRSF_PARSER_WAIT_ADDRESS;
    ctx->frame_pos = 0U;
    ctx->expected_length = 0U;
}

static RadioParseResult crsf_protocol_process_byte(
    RadioProtocol *self,
    uint8_t byte,
    uint32_t now_ms
);
static bool crsf_protocol_get_frame(
    RadioProtocol *self,
    RcInputFrame *out_frame
);

static bool crsf_protocol_get_uart_config(
    RadioProtocol *self,
    RadioUartConfig *out_config
);

static bool crsf_decode_rc_channels(
    const uint8_t *payload,
    RcInputFrame *out_frame
);

void crsf_protocol_init(
    RadioProtocol *protocol,
    CrsfProtocolContext *context
)
{
    if (protocol == NULL || context == NULL) {
        return;
    }

    memset(context, 0, sizeof(*context));

    protocol->context = context;
    protocol->type = RADIO_PROTOCOL_CRSF;
    protocol->reset = crsf_protocol_reset;
    protocol->process_byte = crsf_protocol_process_byte;
    protocol->get_frame = crsf_protocol_get_frame;
    protocol->get_uart_config = crsf_protocol_get_uart_config;

    crsf_protocol_reset(protocol);
}

static void crsf_protocol_reset(RadioProtocol *self)
{
    if (self == NULL || self->context == NULL) {
        return;
    }

    CrsfProtocolContext *ctx = (CrsfProtocolContext *)self->context;

    ctx->parser_state = CRSF_PARSER_WAIT_ADDRESS;
    ctx->frame_pos = 0U;
    ctx->expected_length = 0U;
    ctx->frame_ready = false;
    ctx->device_ping_pending = false;
    ctx->device_ping_origin = 0U;
}

static RadioParseResult crsf_protocol_process_byte(
    RadioProtocol *self,
    uint8_t byte,
    uint32_t now_ms
)
{
    (void)now_ms;

    if (self == NULL || self->context == NULL) {
        return RADIO_PARSE_ERROR;
    }

    CrsfProtocolContext *ctx = (CrsfProtocolContext *)self->context;

    switch (ctx->parser_state) {
    case CRSF_PARSER_WAIT_ADDRESS:
        if (byte == CRSF_ADDRESS_FLIGHT_CTRL ||
            byte == CRSF_ADDRESS_CRSF_RECEIVER) {
            ctx->frame_buffer[0] = byte;
            ctx->frame_pos = 1U;
            ctx->parser_state = CRSF_PARSER_WAIT_LENGTH;
        }
        return RADIO_PARSE_IN_PROGRESS;

    case CRSF_PARSER_WAIT_LENGTH:
        if (byte < 2U || byte > (CRSF_MAX_FRAME_SIZE - 2U)) {
            ctx->length_errors++;
            crsf_protocol_reset(self);
            return RADIO_PARSE_ERROR;
        }

        ctx->expected_length = byte;
        ctx->frame_buffer[1] = byte;
        ctx->frame_pos = 2U;
        ctx->parser_state = CRSF_PARSER_READ_PAYLOAD;
        return RADIO_PARSE_IN_PROGRESS;

    case CRSF_PARSER_READ_PAYLOAD:
        ctx->frame_buffer[ctx->frame_pos] = byte;
        ctx->frame_pos++;

        if (ctx->frame_pos >= (uint8_t)(ctx->expected_length + 2U)) {
            ctx->received_frames++;

            const uint8_t frame_type = ctx->frame_buffer[CRSF_FRAME_TYPE_INDEX];

            const uint8_t *crc_data = &ctx->frame_buffer[CRSF_FRAME_TYPE_INDEX];
            const size_t crc_data_len = ctx->expected_length - 1U;

            const uint8_t expected_crc =
                ctx->frame_buffer[ctx->frame_pos - 1U];

            const uint8_t calculated_crc =
                crsf_crc8_dvb_s2(crc_data, crc_data_len);

            if (calculated_crc != expected_crc) {
                ctx->crc_errors++;
                crsf_protocol_reset(self);
                return RADIO_PARSE_ERROR;
            }

            if (frame_type == CRSF_FRAME_TYPE_PARAMETER_PING) {
                if (ctx->expected_length !=
                    CRSF_EXTENDED_PING_FRAME_LENGTH) {
                    ctx->length_errors++;
                    crsf_parser_reset(ctx);
                    return RADIO_PARSE_ERROR;
                }

                const uint8_t destination = ctx->frame_buffer[3];
                const uint8_t origin = ctx->frame_buffer[4];

                if (destination == CRSF_ADDRESS_FLIGHT_CTRL ||
                    destination == 0x00U) {
                    ctx->device_ping_pending = true;
                    ctx->device_ping_origin = origin;
                }

                crsf_parser_reset(ctx);
                return RADIO_PARSE_IDLE;
            }

            if (frame_type == CRSF_FRAME_TYPE_RC_CHANNELS_PACKED) {
                const uint8_t *payload =
                    &ctx->frame_buffer[CRSF_FRAME_TYPE_INDEX + 1U];

                if (crsf_decode_rc_channels(payload, &ctx->latest_frame)) {
                    ctx->latest_frame.protocol = RADIO_PROTOCOL_CRSF;
                    ctx->latest_frame.frame_valid = true;
                    ctx->latest_frame.failsafe = false;
                    ctx->latest_frame.frame_lost = false;

                    ctx->frame_ready = true;
                    ctx->valid_frames++;

                    crsf_protocol_reset(self);
                    ctx->frame_ready = true;

                    return RADIO_PARSE_FRAME_READY;
                }

                ctx->length_errors++;
                crsf_protocol_reset(self);
                return RADIO_PARSE_ERROR;
            }

            ctx->unsupported_frames++;
            crsf_protocol_reset(self);
            return RADIO_PARSE_IN_PROGRESS;
        }

        return RADIO_PARSE_IN_PROGRESS;

    default:
        crsf_protocol_reset(self);
        return RADIO_PARSE_ERROR;
    }
}

static bool crsf_protocol_get_uart_config(
    RadioProtocol *self,
    RadioUartConfig *out_config
)
{
    if (self == NULL ||
        self->type != RADIO_PROTOCOL_CRSF ||
        out_config == NULL) {
        return false;
    }

    out_config->baud_rate = 420000U;
    out_config->data_bits = 8U;
    out_config->parity = RADIO_UART_PARITY_NONE;
    out_config->stop_bits = RADIO_UART_STOP_BITS_1;
    out_config->signal_inverted = false;

    return true;
}

bool crsf_protocol_take_device_ping(
    CrsfProtocolContext *context,
    uint8_t *out_origin
)
{
    if (context == NULL || out_origin == NULL ||
        !context->device_ping_pending) {
        return false;
    }

    *out_origin = context->device_ping_origin;
    context->device_ping_pending = false;
    context->device_ping_origin = 0U;
    return true;
}

static bool crsf_protocol_get_frame(
    RadioProtocol *self,
    RcInputFrame *out_frame
)
{
    if (self == NULL || self->context == NULL || out_frame == NULL) {
        return false;
    }

    CrsfProtocolContext *ctx = (CrsfProtocolContext *)self->context;

    if (!ctx->frame_ready) {
        return false;
    }

    *out_frame = ctx->latest_frame;
    ctx->frame_ready = false;

    return true;
}

static bool crsf_decode_rc_channels(
    const uint8_t *payload,
    RcInputFrame *out_frame
)
{
    if (payload == NULL || out_frame == NULL) {
        return false;
    }

    memset(out_frame, 0, sizeof(*out_frame));

    out_frame->channels[0]  = ((payload[0]       | payload[1] << 8) & 0x07FF);
    out_frame->channels[1]  = ((payload[1] >> 3  | payload[2] << 5) & 0x07FF);
    out_frame->channels[2]  = ((payload[2] >> 6  | payload[3] << 2 | payload[4] << 10) & 0x07FF);
    out_frame->channels[3]  = ((payload[4] >> 1  | payload[5] << 7) & 0x07FF);
    out_frame->channels[4]  = ((payload[5] >> 4  | payload[6] << 4) & 0x07FF);
    out_frame->channels[5]  = ((payload[6] >> 7  | payload[7] << 1 | payload[8] << 9) & 0x07FF);
    out_frame->channels[6]  = ((payload[8] >> 2  | payload[9] << 6) & 0x07FF);
    out_frame->channels[7]  = ((payload[9] >> 5  | payload[10] << 3) & 0x07FF);
    out_frame->channels[8]  = ((payload[11]      | payload[12] << 8) & 0x07FF);
    out_frame->channels[9]  = ((payload[12] >> 3 | payload[13] << 5) & 0x07FF);
    out_frame->channels[10] = ((payload[13] >> 6 | payload[14] << 2 | payload[15] << 10) & 0x07FF);
    out_frame->channels[11] = ((payload[15] >> 1 | payload[16] << 7) & 0x07FF);
    out_frame->channels[12] = ((payload[16] >> 4 | payload[17] << 4) & 0x07FF);
    out_frame->channels[13] = ((payload[17] >> 7 | payload[18] << 1 | payload[19] << 9) & 0x07FF);
    out_frame->channels[14] = ((payload[19] >> 2 | payload[20] << 6) & 0x07FF);
    out_frame->channels[15] = ((payload[20] >> 5 | payload[21] << 3) & 0x07FF);

    out_frame->channel_count = CRSF_RC_CHANNEL_COUNT;

    return true;
}
