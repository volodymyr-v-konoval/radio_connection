#ifndef RADIO_PROTOCOL_IF_H
#define RADIO_PROTOCOL_IF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rc_types.h"

typedef enum
{
    RADIO_PARSE_IDLE = 0,
    RADIO_PARSE_IN_PROGRESS,
    RADIO_PARSE_FRAME_READY,
    RADIO_PARSE_ERROR
} RadioParseResult;

typedef enum
{
    RADIO_UART_PARITY_NONE = 0,
    RADIO_UART_PARITY_EVEN,
    RADIO_UART_PARITY_ODD
} RadioUartParity;

typedef enum
{
    RADIO_UART_STOP_BITS_1 = 0,
    RADIO_UART_STOP_BITS_2
} RadioUartStopBits;

typedef struct
{
    uint32_t baud_rate;
    uint8_t data_bits;
    RadioUartParity parity;
    RadioUartStopBits stop_bits;
    bool signal_inverted;
} RadioUartConfig;

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

    bool (*get_uart_config)(
        RadioProtocol *self,
        RadioUartConfig *out_config
    );
};

static inline bool radio_protocol_get_uart_config(
    RadioProtocol *protocol,
    RadioUartConfig *out_config
)
{
    return protocol != NULL &&
           out_config != NULL &&
           protocol->get_uart_config != NULL &&
           protocol->get_uart_config(
               protocol,
               out_config
           );
}

#endif /* RADIO_PROTOCOL_IF_H */
