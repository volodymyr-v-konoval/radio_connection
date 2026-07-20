#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_stm32f4_hal.h"
#include "stm32f4_sx128x_port.h"

#define TEST_ASSERT(condition, message)       \
    do {                                      \
        if (!(condition)) {                   \
            printf("FAIL: %s\n", message);   \
            return false;                     \
        }                                     \
    } while (0)

static Stm32f4Sx128xPortConfig make_config(
    SPI_HandleTypeDef *spi
)
{
    const Stm32f4Sx128xPortConfig config = {
        .spi = spi,
        .nss_port = GPIOB,
        .nss_pin = GPIO_PIN_0,
        .reset_port = GPIOB,
        .reset_pin = GPIO_PIN_10,
        .busy_port = GPIOB,
        .busy_pin = GPIO_PIN_1,
        .dio1_pin = GPIO_PIN_2,
        .spi_timeout_ms = 25U
    };

    return config;
}

static bool init_port(
    Stm32f4Sx128xPort *backend,
    Sx128xPort *port,
    SPI_HandleTypeDef *spi
)
{
    memset(spi, 0, sizeof(*spi));
    spi->Instance = SPI1;

    const Stm32f4Sx128xPortConfig config =
        make_config(spi);

    return stm32f4_sx128x_port_init(
        backend,
        port,
        &config
    );
}

static bool test_init_validation_and_idle_pins(void)
{
    fake_stm32f4_hal_reset();

    Stm32f4Sx128xPort backend;
    Sx128xPort port;
    SPI_HandleTypeDef spi;
    memset(&spi, 0, sizeof(spi));

    Stm32f4Sx128xPortConfig config = make_config(&spi);

    TEST_ASSERT(
        !stm32f4_sx128x_port_init(NULL, &port, &config),
        "NULL backend should be rejected"
    );
    TEST_ASSERT(
        !stm32f4_sx128x_port_init(&backend, NULL, &config),
        "NULL port should be rejected"
    );

    config.spi_timeout_ms = 0U;
    TEST_ASSERT(
        !stm32f4_sx128x_port_init(&backend, &port, &config),
        "Zero SPI timeout should be rejected"
    );

    config = make_config(&spi);
    config.busy_pin = 0U;
    TEST_ASSERT(
        !stm32f4_sx128x_port_init(&backend, &port, &config),
        "Zero BUSY pin should be rejected"
    );

    TEST_ASSERT(
        init_port(&backend, &port, &spi),
        "Valid STM32F4 SX128x port should initialize"
    );
    TEST_ASSERT(
        port.context == &backend &&
        port.spi_transfer != NULL &&
        port.nss_write != NULL &&
        port.reset_write != NULL &&
        port.busy_read != NULL &&
        port.delay_ms != NULL &&
        port.time_ms != NULL &&
        port.take_dio1_event != NULL,
        "All SX128x port callbacks should be bound"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->gpio_write_count == 2U,
        "Initialization should deassert NSS and RESET"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->gpio_writes[0].gpio == GPIOB &&
        fake_stm32f4_hal_state()->gpio_writes[0].pin == GPIO_PIN_0 &&
        fake_stm32f4_hal_state()->gpio_writes[0].state == GPIO_PIN_SET,
        "NSS should initialize high"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->gpio_writes[1].gpio == GPIOB &&
        fake_stm32f4_hal_state()->gpio_writes[1].pin == GPIO_PIN_10 &&
        fake_stm32f4_hal_state()->gpio_writes[1].state == GPIO_PIN_SET,
        "RESET should initialize high"
    );

    return true;
}

static bool test_full_duplex_spi_transfer(void)
{
    fake_stm32f4_hal_reset();

    Stm32f4Sx128xPort backend;
    Sx128xPort port;
    SPI_HandleTypeDef spi;

    TEST_ASSERT(
        init_port(&backend, &port, &spi),
        "Port should initialize"
    );

    const uint8_t tx[] = { 0x8AU, 0x01U, 0x55U, 0xAAU };
    const uint8_t scripted_rx[] = { 0x10U, 0x20U, 0x30U, 0x40U };
    uint8_t rx[sizeof(tx)] = { 0U };

    fake_stm32f4_hal_set_spi_rx_script(
        scripted_rx,
        sizeof(scripted_rx)
    );

    TEST_ASSERT(
        port.spi_transfer(
            port.context,
            tx,
            rx,
            sizeof(tx)
        ),
        "Full-duplex SPI transfer should succeed"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->spi_calls == 1U,
        "Short transfer should use one HAL SPI call"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->last_spi == &spi &&
        fake_stm32f4_hal_state()->last_spi_timeout == 25U,
        "SPI handle and timeout should be forwarded"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->spi_tx_capture_length == sizeof(tx) &&
        memcmp(
            fake_stm32f4_hal_state()->spi_tx_capture,
            tx,
            sizeof(tx)
        ) == 0,
        "SPI TX bytes should be preserved"
    );
    TEST_ASSERT(
        memcmp(rx, scripted_rx, sizeof(rx)) == 0,
        "SPI RX bytes should be returned"
    );

    Stm32f4Sx128xPortStats stats;
    stm32f4_sx128x_port_get_stats(&backend, &stats);

    TEST_ASSERT(
        stats.spi_transfer_calls == 1U &&
        stats.spi_hal_calls == 1U &&
        stats.spi_bytes_transferred == sizeof(tx) &&
        stats.spi_errors == 0U,
        "Successful SPI transfer should update statistics"
    );

    return true;
}

static bool test_null_buffers_and_chunking(void)
{
    fake_stm32f4_hal_reset();

    Stm32f4Sx128xPort backend;
    Sx128xPort port;
    SPI_HandleTypeDef spi;

    TEST_ASSERT(
        init_port(&backend, &port, &spi),
        "Port should initialize"
    );

    uint8_t scripted_rx[130];
    uint8_t rx[130];

    for (size_t i = 0U; i < sizeof(scripted_rx); i++) {
        scripted_rx[i] = (uint8_t)i;
    }

    fake_stm32f4_hal_set_spi_rx_script(
        scripted_rx,
        sizeof(scripted_rx)
    );

    TEST_ASSERT(
        port.spi_transfer(
            port.context,
            NULL,
            rx,
            sizeof(rx)
        ),
        "NULL TX should clock zero bytes"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->spi_calls == 3U,
        "130-byte transfer should be split into three HAL calls"
    );
    TEST_ASSERT(
        memcmp(rx, scripted_rx, sizeof(rx)) == 0,
        "Chunked RX data should remain contiguous"
    );

    for (size_t i = 0U;
         i < fake_stm32f4_hal_state()->spi_tx_capture_length;
         i++) {
        TEST_ASSERT(
            fake_stm32f4_hal_state()->spi_tx_capture[i] == 0U,
            "NULL TX should transmit zeros"
        );
    }

    const uint8_t tx[] = { 1U, 2U, 3U };

    TEST_ASSERT(
        port.spi_transfer(
            port.context,
            tx,
            NULL,
            sizeof(tx)
        ),
        "NULL RX should discard received bytes"
    );

    return true;
}

static bool test_spi_error_and_zero_length(void)
{
    fake_stm32f4_hal_reset();

    Stm32f4Sx128xPort backend;
    Sx128xPort port;
    SPI_HandleTypeDef spi;

    TEST_ASSERT(
        init_port(&backend, &port, &spi),
        "Port should initialize"
    );

    TEST_ASSERT(
        port.spi_transfer(port.context, NULL, NULL, 0U),
        "Zero-length SPI transfer should be a no-op"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->spi_calls == 0U,
        "Zero-length transfer should not call HAL"
    );

    fake_stm32f4_hal_state()->spi_status = HAL_ERROR;

    const uint8_t tx = 0xC0U;
    uint8_t rx = 0U;

    TEST_ASSERT(
        !port.spi_transfer(
            port.context,
            &tx,
            &rx,
            1U
        ),
        "HAL SPI error should be reported"
    );

    Stm32f4Sx128xPortStats stats;
    stm32f4_sx128x_port_get_stats(&backend, &stats);

    TEST_ASSERT(
        stats.spi_errors == 1U,
        "HAL SPI error should be counted"
    );

    return true;
}

static bool test_gpio_busy_delay_and_time(void)
{
    fake_stm32f4_hal_reset();

    Stm32f4Sx128xPort backend;
    Sx128xPort port;
    SPI_HandleTypeDef spi;

    TEST_ASSERT(
        init_port(&backend, &port, &spi),
        "Port should initialize"
    );

    port.nss_write(port.context, false);
    port.reset_write(port.context, false);

    TEST_ASSERT(
        fake_stm32f4_hal_state()->gpio_writes[2].pin == GPIO_PIN_0 &&
        fake_stm32f4_hal_state()->gpio_writes[2].state == GPIO_PIN_RESET,
        "NSS low should map to GPIO reset"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->gpio_writes[3].pin == GPIO_PIN_10 &&
        fake_stm32f4_hal_state()->gpio_writes[3].state == GPIO_PIN_RESET,
        "RESET low should map to GPIO reset"
    );

    fake_stm32f4_hal_state()->gpio_read_state = GPIO_PIN_SET;

    TEST_ASSERT(
        port.busy_read(port.context),
        "BUSY high should return true"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->last_gpio_read_port == GPIOB &&
        fake_stm32f4_hal_state()->last_gpio_read_pin == GPIO_PIN_1,
        "BUSY GPIO should be read from configured pin"
    );

    fake_stm32f4_hal_state()->tick_ms = 100U;
    port.delay_ms(port.context, 7U);

    TEST_ASSERT(
        fake_stm32f4_hal_state()->delay_calls == 1U &&
        fake_stm32f4_hal_state()->delay_total_ms == 7U,
        "Delay should be forwarded to HAL"
    );
    TEST_ASSERT(
        port.time_ms(port.context) == 107U,
        "Time callback should expose HAL tick"
    );

    return true;
}

static bool test_dio1_is_deferred_and_coalesced(void)
{
    fake_stm32f4_hal_reset();

    Stm32f4Sx128xPort backend;
    Sx128xPort port;
    SPI_HandleTypeDef spi;

    TEST_ASSERT(
        init_port(&backend, &port, &spi),
        "Port should initialize"
    );

    stm32f4_sx128x_port_on_dio1_irq(
        &backend,
        GPIO_PIN_3
    );

    TEST_ASSERT(
        !port.take_dio1_event(port.context),
        "Unrelated EXTI pin should be ignored"
    );

    const uint32_t spi_calls_before =
        fake_stm32f4_hal_state()->spi_calls;
    const uint32_t gpio_calls_before =
        fake_stm32f4_hal_state()->gpio_write_calls;

    stm32f4_sx128x_port_on_dio1_irq(
        &backend,
        GPIO_PIN_2
    );
    stm32f4_sx128x_port_on_dio1_irq(
        &backend,
        GPIO_PIN_2
    );

    TEST_ASSERT(
        fake_stm32f4_hal_state()->spi_calls == spi_calls_before &&
        fake_stm32f4_hal_state()->gpio_write_calls == gpio_calls_before,
        "DIO1 ISR forwarding must not perform SPI or GPIO work"
    );
    TEST_ASSERT(
        port.take_dio1_event(port.context),
        "Pending DIO1 event should be consumed in main context"
    );
    TEST_ASSERT(
        !port.take_dio1_event(port.context),
        "Coalesced DIO1 interrupts should produce one pending event"
    );

    Stm32f4Sx128xPortStats stats;
    stm32f4_sx128x_port_get_stats(&backend, &stats);

    TEST_ASSERT(
        stats.dio1_irq_events == 2U &&
        stats.dio1_events_taken == 1U &&
        stats.dio1_coalesced_events == 1U,
        "DIO1 statistics should report IRQ, take, and coalescing"
    );

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_init_validation_and_idle_pins()) {
        failed++;
    }
    if (!test_full_duplex_spi_transfer()) {
        failed++;
    }
    if (!test_null_buffers_and_chunking()) {
        failed++;
    }
    if (!test_spi_error_and_zero_length()) {
        failed++;
    }
    if (!test_gpio_busy_delay_and_time()) {
        failed++;
    }
    if (!test_dio1_is_deferred_and_coalesced()) {
        failed++;
    }

    if (failed == 0) {
        printf("All STM32F4 SX128x port tests passed (6 tests)\n");
        return 0;
    }

    printf("%d STM32F4 SX128x port test(s) failed\n", failed);
    return 1;
}
