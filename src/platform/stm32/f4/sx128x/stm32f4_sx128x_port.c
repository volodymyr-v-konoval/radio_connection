#include "stm32f4_sx128x_port.h"

#include <limits.h>
#include <string.h>

static bool stm32f4_sx128x_port_spi_transfer(
    void *context,
    const uint8_t *tx_data,
    uint8_t *rx_data,
    size_t length
);

static void stm32f4_sx128x_port_nss_write(
    void *context,
    bool high
);

static void stm32f4_sx128x_port_reset_write(
    void *context,
    bool high
);

static bool stm32f4_sx128x_port_busy_read(
    void *context
);

static void stm32f4_sx128x_port_delay_ms(
    void *context,
    uint32_t delay_ms
);

static uint32_t stm32f4_sx128x_port_time_ms(
    void *context
);

static bool stm32f4_sx128x_port_take_dio1_event(
    void *context
);

static bool stm32f4_sx128x_port_config_is_valid(
    const Stm32f4Sx128xPortConfig *config
)
{
    return config != NULL &&
        config->spi != NULL &&
        config->nss_port != NULL &&
        config->nss_pin != 0U &&
        config->reset_port != NULL &&
        config->reset_pin != 0U &&
        config->busy_port != NULL &&
        config->busy_pin != 0U &&
        config->dio1_pin != 0U &&
        config->spi_timeout_ms != 0U;
}

bool stm32f4_sx128x_port_init(
    Stm32f4Sx128xPort *backend,
    Sx128xPort *port,
    const Stm32f4Sx128xPortConfig *config
)
{
    if (backend == NULL ||
        port == NULL ||
        !stm32f4_sx128x_port_config_is_valid(config)) {
        return false;
    }

    memset(backend, 0, sizeof(*backend));
    memset(port, 0, sizeof(*port));

    backend->config = *config;
    backend->initialized = true;

    port->context = backend;
    port->spi_transfer = stm32f4_sx128x_port_spi_transfer;
    port->nss_write = stm32f4_sx128x_port_nss_write;
    port->reset_write = stm32f4_sx128x_port_reset_write;
    port->busy_read = stm32f4_sx128x_port_busy_read;
    port->delay_ms = stm32f4_sx128x_port_delay_ms;
    port->time_ms = stm32f4_sx128x_port_time_ms;
    port->take_dio1_event = stm32f4_sx128x_port_take_dio1_event;

    stm32f4_sx128x_port_nss_write(backend, true);
    stm32f4_sx128x_port_reset_write(backend, true);

    return true;
}

void stm32f4_sx128x_port_on_dio1_irq(
    Stm32f4Sx128xPort *backend,
    uint16_t gpio_pin
)
{
    if (backend == NULL ||
        !backend->initialized ||
        gpio_pin != backend->config.dio1_pin) {
        return;
    }

    backend->stats.dio1_irq_events++;

    if (backend->dio1_produced_count !=
        backend->dio1_consumed_count) {
        backend->stats.dio1_coalesced_events++;
    }

    __DMB();
    backend->dio1_produced_count++;
}

void stm32f4_sx128x_port_get_stats(
    const Stm32f4Sx128xPort *backend,
    Stm32f4Sx128xPortStats *out_stats
)
{
    if (backend == NULL || out_stats == NULL) {
        return;
    }

    __DMB();
    *out_stats = backend->stats;
}

static bool stm32f4_sx128x_port_spi_transfer(
    void *context,
    const uint8_t *tx_data,
    uint8_t *rx_data,
    size_t length
)
{
    Stm32f4Sx128xPort *backend =
        (Stm32f4Sx128xPort *)context;

    if (backend == NULL || !backend->initialized) {
        return false;
    }

    backend->stats.spi_transfer_calls++;

    if (length == 0U) {
        return true;
    }

    if (length > UINT16_MAX) {
        backend->stats.spi_errors++;
        return false;
    }

    uint8_t zero_tx[STM32F4_SX128X_PORT_SPI_SCRATCH_SIZE] = { 0U };
    uint8_t discard_rx[STM32F4_SX128X_PORT_SPI_SCRATCH_SIZE];
    size_t offset = 0U;

    while (offset < length) {
        size_t chunk_length = length - offset;

        if (chunk_length > STM32F4_SX128X_PORT_SPI_SCRATCH_SIZE) {
            chunk_length = STM32F4_SX128X_PORT_SPI_SCRATCH_SIZE;
        }

        uint8_t *hal_tx = zero_tx;
        uint8_t *hal_rx = discard_rx;

        if (tx_data != NULL) {
            hal_tx = (uint8_t *)&tx_data[offset];
        }

        if (rx_data != NULL) {
            hal_rx = &rx_data[offset];
        }

        backend->stats.spi_hal_calls++;

        const HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
            backend->config.spi,
            hal_tx,
            hal_rx,
            (uint16_t)chunk_length,
            backend->config.spi_timeout_ms
        );

        if (status != HAL_OK) {
            backend->stats.spi_errors++;
            return false;
        }

        offset += chunk_length;
    }

    backend->stats.spi_bytes_transferred += (uint32_t)length;
    return true;
}

static void stm32f4_sx128x_port_nss_write(
    void *context,
    bool high
)
{
    Stm32f4Sx128xPort *backend =
        (Stm32f4Sx128xPort *)context;

    if (backend == NULL || !backend->initialized) {
        return;
    }

    HAL_GPIO_WritePin(
        backend->config.nss_port,
        backend->config.nss_pin,
        high ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    backend->stats.nss_writes++;
}

static void stm32f4_sx128x_port_reset_write(
    void *context,
    bool high
)
{
    Stm32f4Sx128xPort *backend =
        (Stm32f4Sx128xPort *)context;

    if (backend == NULL || !backend->initialized) {
        return;
    }

    HAL_GPIO_WritePin(
        backend->config.reset_port,
        backend->config.reset_pin,
        high ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    backend->stats.reset_writes++;
}

static bool stm32f4_sx128x_port_busy_read(
    void *context
)
{
    Stm32f4Sx128xPort *backend =
        (Stm32f4Sx128xPort *)context;

    if (backend == NULL || !backend->initialized) {
        return false;
    }

    backend->stats.busy_reads++;

    return HAL_GPIO_ReadPin(
        backend->config.busy_port,
        backend->config.busy_pin
    ) == GPIO_PIN_SET;
}

static void stm32f4_sx128x_port_delay_ms(
    void *context,
    uint32_t delay_ms
)
{
    Stm32f4Sx128xPort *backend =
        (Stm32f4Sx128xPort *)context;

    if (backend == NULL || !backend->initialized) {
        return;
    }

    backend->stats.delay_calls++;
    HAL_Delay(delay_ms);
}

static uint32_t stm32f4_sx128x_port_time_ms(
    void *context
)
{
    const Stm32f4Sx128xPort *backend =
        (const Stm32f4Sx128xPort *)context;

    if (backend == NULL || !backend->initialized) {
        return 0U;
    }

    return HAL_GetTick();
}

static bool stm32f4_sx128x_port_take_dio1_event(
    void *context
)
{
    Stm32f4Sx128xPort *backend =
        (Stm32f4Sx128xPort *)context;

    if (backend == NULL || !backend->initialized) {
        return false;
    }

    const uint32_t produced = backend->dio1_produced_count;
    __DMB();

    if (produced == backend->dio1_consumed_count) {
        return false;
    }

    backend->dio1_consumed_count = produced;
    backend->stats.dio1_events_taken++;
    return true;
}
