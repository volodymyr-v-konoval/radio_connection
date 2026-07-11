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
static RadioParseResult crsf_protocol_process_byte(
    RadioProtocol *self,
    uint8_t byte,
    uint32_t now_ms
);
static bool crsf_protocol_get_frame(
    RadioProtocol *self,
    RcInputFrame *out_frame
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
