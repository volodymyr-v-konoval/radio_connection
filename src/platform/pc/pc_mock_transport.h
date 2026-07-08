#ifndef PC_MOCK_TRANSPORT_H
#define PC_MOCK_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

#include "radio_transport_if.h"

typedef struct
{
    const uint8_t *data;
    size_t size;
    size_t position;
} PcMockTransportContext;

void pc_mock_transport_init(
    RadioTransport *transport,
    PcMockTransportContext *context,
    const uint8_t *data,
    size_t size
);

void pc_mock_transport_reset(
    PcMockTransportContext *context
);

#endif /* PC_MOCK_TRANSPORT_H */
