#ifndef RADIO_PROTOCOL_IF_H
#define RADIO_PROTOCOL_IF_H

#include <stdint.h>
#include <stdbool.h>

#include "rc_types.h"

typedef enum
{
    RADIO_PARSE_IDLE = 0,
    RADIO_PARSE_IN_PROGRESS,
    RADIO_PARSE_FRAME_READY,
    RADIO_PARSE_ERROR
} RadioParseResult;

typedef struct RadioProtocol RadioProtocol;

struct RadioProtocol
{
    void *context;

    RadioProtocolType type;

    void (*reset)(RadioProtocol *self);

    RadioParseResult (*process_byte)(
        RadioProtocol *self,
        uint8_t byte,
        uint32_t now_ms
    );

    bool (*get_frame)(
        RadioProtocol *self,
        RcInputFrame *out_frame
    );
};

#endif /* RADIO_PROTOCOL_IF_H */
