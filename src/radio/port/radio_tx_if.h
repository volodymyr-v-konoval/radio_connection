#ifndef RADIO_TX_IF_H
#define RADIO_TX_IF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RadioTx RadioTx;

struct RadioTx
{
    void *context;

    /*
     * Starts transmission of one complete frame.
     * The implementation must copy the supplied bytes before returning
     * when transmission continues asynchronously.
     */
    bool (*try_write)(
        RadioTx *self,
        const uint8_t *data,
        size_t length
    );

    /*
     * Returns true while the backend still owns an active transmission.
     */
    bool (*is_busy)(
        RadioTx *self
    );
};

static inline bool radio_tx_try_write(
    RadioTx *tx,
    const uint8_t *data,
    size_t length
)
{
    return tx != NULL &&
        tx->try_write != NULL &&
        data != NULL &&
        length > 0U &&
        tx->try_write(tx, data, length);
}

static inline bool radio_tx_is_busy(
    RadioTx *tx
)
{
    return tx != NULL &&
        tx->is_busy != NULL &&
        tx->is_busy(tx);
}

#endif /* RADIO_TX_IF_H */
