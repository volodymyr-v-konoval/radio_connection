#ifndef RADIO_TRANSPORT_IF_H
#define RADIO_TRANSPORT_IF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct RadioTransport RadioTransport;

struct RadioTransport
{
    void *context;

    bool (*read_byte)(
        RadioTransport *self,
        uint8_t *out_byte
    );

    size_t (*read)(
        RadioTransport *self,
        uint8_t *buffer,
        size_t max_len
    );
};

#endif /* RADIO_TRANSPORT_IF_H */
