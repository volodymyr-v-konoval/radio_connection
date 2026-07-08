#include "pc_mock_transport.h"

#include <string.h>

static bool pc_mock_transport_read_byte(
    RadioTransport *self,
    uint8_t *out_byte
)
{
    if (self == NULL || self->context == NULL || out_byte == NULL) {
        return false;
    }

    PcMockTransportContext *context =
        (PcMockTransportContext *)self->context;

    if (context->data == NULL || context->position >= context->size) {
        return false;
    }

    *out_byte = context->data[context->position];
    context->position++;

    return true;
}

static size_t pc_mock_transport_read(
    RadioTransport *self,
    uint8_t *buffer,
    size_t max_len
)
{
    if (self == NULL ||
        self->context == NULL ||
        buffer == NULL ||
        max_len == 0U) {
        return 0U;
    }

    PcMockTransportContext *context =
        (PcMockTransportContext *)self->context;

    if (context->data == NULL || context->position >= context->size) {
        return 0U;
    }

    size_t remaining = context->size - context->position;
    size_t to_copy = remaining < max_len ? remaining : max_len;

    memcpy(buffer, &context->data[context->position], to_copy);
    context->position += to_copy;

    return to_copy;
}

void pc_mock_transport_init(
    RadioTransport *transport,
    PcMockTransportContext *context,
    const uint8_t *data,
    size_t size
)
{
    if (transport == NULL || context == NULL) {
        return;
    }

    context->data = data;
    context->size = size;
    context->position = 0U;

    transport->context = context;
    transport->read_byte = pc_mock_transport_read_byte;
    transport->read = pc_mock_transport_read;
}

void pc_mock_transport_reset(
    PcMockTransportContext *context
)
{
    if (context == NULL) {
        return;
    }

    context->position = 0U;
}
