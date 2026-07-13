#ifndef FAKE_RADIO_TX_H
#define FAKE_RADIO_TX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_tx_if.h"

#define FAKE_RADIO_TX_MAX_FRAME_SIZE 64U

typedef struct
{
    uint8_t frame[FAKE_RADIO_TX_MAX_FRAME_SIZE];
    size_t frame_length;
    uint32_t write_calls;
    bool busy;
    bool accept_writes;
} FakeRadioTxContext;

bool fake_radio_tx_init(
    RadioTx *tx,
    FakeRadioTxContext *context
);

void fake_radio_tx_complete(
    FakeRadioTxContext *context
);

#endif /* FAKE_RADIO_TX_H */
