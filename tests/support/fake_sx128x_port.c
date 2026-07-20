#include "fake_sx128x_port.h"

#include <string.h>

static bool fake_sx128x_spi_transfer(
    void *context_pointer,
    const uint8_t *tx_data,
    uint8_t *rx_data,
    size_t length
)
{
    if (context_pointer == NULL || length == 0U) {
        return false;
    }

    FakeSx128xPortContext *context =
        (FakeSx128xPortContext *)context_pointer;

    context->spi_transfer_calls++;

    if (context->fail_spi_on_call != 0U &&
        context->spi_transfer_calls ==
            context->fail_spi_on_call) {
        return false;
    }

    if (context->spi_tx_count + length >
        FAKE_SX128X_SPI_LOG_CAPACITY) {
        return false;
    }

    for (size_t index = 0U; index < length; index++) {
        const uint8_t tx_byte =
            tx_data != NULL ? tx_data[index] : 0U;

        context->spi_tx_log[
            context->spi_tx_count
        ] = tx_byte;

        context->spi_tx_count++;

        if (rx_data != NULL) {
            uint8_t rx_byte = 0U;

            if (context->spi_rx_script_offset <
                context->spi_rx_script_length) {
                rx_byte = context->spi_rx_script[
                    context->spi_rx_script_offset
                ];
            }

            rx_data[index] = rx_byte;
        }

        if (context->spi_rx_script_offset <
            context->spi_rx_script_length) {
            context->spi_rx_script_offset++;
        }
    }

    return true;
}

static void fake_sx128x_nss_write(
    void *context_pointer,
    bool high
)
{
    FakeSx128xPortContext *context =
        (FakeSx128xPortContext *)context_pointer;

    context->nss_high = high;

    if (high) {
        context->nss_high_calls++;
    } else {
        context->nss_low_calls++;
    }
}

static void fake_sx128x_reset_write(
    void *context_pointer,
    bool high
)
{
    FakeSx128xPortContext *context =
        (FakeSx128xPortContext *)context_pointer;

    context->reset_high = high;

    if (high) {
        context->reset_high_calls++;
    } else {
        context->reset_low_calls++;
    }
}

static bool fake_sx128x_busy_read(
    void *context_pointer
)
{
    FakeSx128xPortContext *context =
        (FakeSx128xPortContext *)context_pointer;

    context->busy_read_calls++;

    return
        context->busy_stuck_high ||
        context->now_ms < context->busy_release_at_ms;
}

static void fake_sx128x_delay_ms(
    void *context_pointer,
    uint32_t delay_ms
)
{
    FakeSx128xPortContext *context =
        (FakeSx128xPortContext *)context_pointer;

    context->delay_calls++;
    context->total_delay_ms += delay_ms;
    context->now_ms += delay_ms;
}

static uint32_t fake_sx128x_time_ms(
    void *context_pointer
)
{
    const FakeSx128xPortContext *context =
        (const FakeSx128xPortContext *)context_pointer;

    return context->now_ms;
}

static bool fake_sx128x_take_dio1_event(
    void *context_pointer
)
{
    FakeSx128xPortContext *context =
        (FakeSx128xPortContext *)context_pointer;

    const bool was_pending = context->dio1_pending;
    context->dio1_pending = false;

    return was_pending;
}

bool fake_sx128x_port_init(
    Sx128xPort *port,
    FakeSx128xPortContext *context
)
{
    if (port == NULL || context == NULL) {
        return false;
    }

    memset(port, 0, sizeof(*port));
    memset(context, 0, sizeof(*context));

    context->nss_high = true;
    context->reset_high = true;

    port->context = context;
    port->spi_transfer = fake_sx128x_spi_transfer;
    port->nss_write = fake_sx128x_nss_write;
    port->reset_write = fake_sx128x_reset_write;
    port->busy_read = fake_sx128x_busy_read;
    port->delay_ms = fake_sx128x_delay_ms;
    port->time_ms = fake_sx128x_time_ms;
    port->take_dio1_event =
        fake_sx128x_take_dio1_event;

    return true;
}

void fake_sx128x_port_clear_spi_log(
    FakeSx128xPortContext *context
)
{
    if (context == NULL) {
        return;
    }

    memset(
        context->spi_tx_log,
        0,
        sizeof(context->spi_tx_log)
    );

    context->spi_tx_count = 0U;
    context->spi_transfer_calls = 0U;
    context->fail_spi_on_call = 0U;
    context->spi_rx_script_offset = 0U;
}

bool fake_sx128x_port_set_rx_script(
    FakeSx128xPortContext *context,
    const uint8_t *data,
    size_t length
)
{
    if (context == NULL ||
        (length > 0U && data == NULL) ||
        length > FAKE_SX128X_RX_SCRIPT_CAPACITY) {
        return false;
    }

    memset(
        context->spi_rx_script,
        0,
        sizeof(context->spi_rx_script)
    );

    if (length > 0U) {
        memcpy(
            context->spi_rx_script,
            data,
            length
        );
    }

    context->spi_rx_script_length = length;
    context->spi_rx_script_offset = 0U;

    return true;
}
