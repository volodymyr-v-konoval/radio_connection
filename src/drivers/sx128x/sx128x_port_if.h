#ifndef SX128X_PORT_IF_H
#define SX128X_PORT_IF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    void *context;

    /*
     * Full-duplex SPI transfer.
     *
     * tx_data may be NULL to transmit zero bytes.
     * rx_data may be NULL when received bytes are not required.
     */
    bool (*spi_transfer)(
        void *context,
        const uint8_t *tx_data,
        uint8_t *rx_data,
        size_t length
    );

    void (*nss_write)(
        void *context,
        bool high
    );

    void (*reset_write)(
        void *context,
        bool high
    );

    bool (*busy_read)(
        void *context
    );

    void (*delay_ms)(
        void *context,
        uint32_t delay_ms
    );

    uint32_t (*time_ms)(
        void *context
    );

    bool (*take_dio1_event)(
        void *context
    );
} Sx128xPort;

#endif /* SX128X_PORT_IF_H */
