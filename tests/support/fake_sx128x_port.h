#ifndef FAKE_SX128X_PORT_H
#define FAKE_SX128X_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sx128x_port_if.h"

#define FAKE_SX128X_SPI_LOG_CAPACITY 512U
#define FAKE_SX128X_RX_SCRIPT_CAPACITY 512U

typedef struct
{
    uint8_t spi_tx_log[FAKE_SX128X_SPI_LOG_CAPACITY];
    size_t spi_tx_count;

    uint8_t spi_rx_script[FAKE_SX128X_RX_SCRIPT_CAPACITY];
    size_t spi_rx_script_length;
    size_t spi_rx_script_offset;

    size_t spi_transfer_calls;
    size_t fail_spi_on_call;

    bool nss_high;
    bool reset_high;
    size_t nss_low_calls;
    size_t nss_high_calls;
    size_t reset_low_calls;
    size_t reset_high_calls;

    bool busy_stuck_high;
    uint32_t busy_release_at_ms;
    size_t busy_read_calls;

    uint32_t now_ms;
    uint32_t total_delay_ms;
    size_t delay_calls;

    bool dio1_pending;
} FakeSx128xPortContext;

bool fake_sx128x_port_init(
    Sx128xPort *port,
    FakeSx128xPortContext *context
);

void fake_sx128x_port_clear_spi_log(
    FakeSx128xPortContext *context
);

bool fake_sx128x_port_set_rx_script(
    FakeSx128xPortContext *context,
    const uint8_t *data,
    size_t length
);

#endif /* FAKE_SX128X_PORT_H */
