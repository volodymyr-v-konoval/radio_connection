#include "fake_radio_tx.h"

#include <string.h>

static bool fake_radio_tx_try_write(
    RadioTx *self,
    const uint8_t *data,
    size_t length
)
{
    if (self == NULL || self->context == NULL || data == NULL) {
        return false;
    }

    FakeRadioTxContext *context =
        (FakeRadioTxContext *)self->context;

    context->write_calls++;

    if (!context->accept_writes || context->busy ||
        length == 0U || length > sizeof(context->frame)) {
        return false;
    }

    memcpy(context->frame, data, length);
    context->frame_length = length;
    context->busy = true;
    return true;
}

static bool fake_radio_tx_is_busy(
    RadioTx *self
)
{
    if (self == NULL || self->context == NULL) {
        return false;
    }

    const FakeRadioTxContext *context =
        (const FakeRadioTxContext *)self->context;

    return context->busy;
}

bool fake_radio_tx_init(
    RadioTx *tx,
    FakeRadioTxContext *context
)
{
    if (tx == NULL || context == NULL) {
        return false;
    }

    memset(tx, 0, sizeof(*tx));
    memset(context, 0, sizeof(*context));

    context->accept_writes = true;

    tx->context = context;
    tx->try_write = fake_radio_tx_try_write;
    tx->is_busy = fake_radio_tx_is_busy;

    return true;
}

void fake_radio_tx_complete(
    FakeRadioTxContext *context
)
{
    if (context == NULL) {
        return;
    }

    context->busy = false;
}
