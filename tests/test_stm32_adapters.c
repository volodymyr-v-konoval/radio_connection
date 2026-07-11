#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_uart_dma_backend.h"
#include "stm32_logger.h"
#include "stm32_time.h"
#include "stm32_uart_dma_transport.h"

#define TEST_ASSERT(condition, message)       \
    do {                                      \
        if (!(condition)) {                   \
            printf("FAIL: %s\n", message);   \
            return false;                     \
        }                                     \
    } while (0)

typedef struct
{
    uint32_t now_ms;
} FakeTimeBackend;

typedef struct
{
    uint8_t data[512];
    size_t length;
} FakeLogSink;

static uint32_t fake_time_now_ms(void *backend_context)
{
    const FakeTimeBackend *backend =
        (const FakeTimeBackend *)backend_context;

    return backend != NULL ? backend->now_ms : 0U;
}

static void fake_log_write(
    void *backend_context,
    const uint8_t *data,
    size_t length
)
{
    FakeLogSink *sink = (FakeLogSink *)backend_context;

    if (sink == NULL || data == NULL) {
        return;
    }

    size_t available = sizeof(sink->data) - sink->length;

    if (length > available) {
        length = available;
    }

    memcpy(&sink->data[sink->length], data, length);
    sink->length += length;
}

static bool init_dma_transport(
    RadioTransport *transport,
    Stm32UartDmaTransportContext *transport_context,
    FakeUartDmaBackend *backend,
    uint8_t *dma_buffer,
    size_t dma_buffer_size
)
{
    if (!fake_uart_dma_backend_init(
            backend,
            dma_buffer,
            dma_buffer_size)) {
        return false;
    }

    const Stm32UartDmaTransportConfig config = {
        .rx_buffer = dma_buffer,
        .rx_buffer_size = dma_buffer_size,
        .backend_context = backend,
        .get_produced_count =
            fake_uart_dma_backend_get_produced_count
    };

    return stm32_uart_dma_transport_init(
        transport,
        transport_context,
        &config
    );
}

static bool test_dma_transport_empty(void)
{
    uint8_t dma_buffer[8];
    FakeUartDmaBackend backend;
    RadioTransport transport;
    Stm32UartDmaTransportContext context;

    TEST_ASSERT(
        init_dma_transport(
            &transport,
            &context,
            &backend,
            dma_buffer,
            sizeof(dma_buffer)),
        "DMA transport init should succeed"
    );

    uint8_t byte = 0U;
    TEST_ASSERT(
        !transport.read_byte(&transport, &byte),
        "Empty transport should not return a byte"
    );
    TEST_ASSERT(
        stm32_uart_dma_transport_available(&context) == 0U,
        "Empty transport should report zero available bytes"
    );

    return true;
}

static bool test_dma_transport_partial_reads(void)
{
    uint8_t dma_buffer[8];
    FakeUartDmaBackend backend;
    RadioTransport transport;
    Stm32UartDmaTransportContext context;

    TEST_ASSERT(
        init_dma_transport(
            &transport,
            &context,
            &backend,
            dma_buffer,
            sizeof(dma_buffer)),
        "DMA transport init should succeed"
    );

    const uint8_t input[] = { 1U, 2U, 3U, 4U, 5U };
    fake_uart_dma_backend_write(&backend, input, sizeof(input));

    uint8_t output[5] = { 0U };

    TEST_ASSERT(
        transport.read(&transport, output, 2U) == 2U,
        "First partial read should return two bytes"
    );
    TEST_ASSERT(
        transport.read(&transport, &output[2], 3U) == 3U,
        "Second partial read should return remaining bytes"
    );
    TEST_ASSERT(
        memcmp(input, output, sizeof(input)) == 0,
        "Partial reads should preserve byte order"
    );

    return true;
}

static bool test_dma_transport_wraparound(void)
{
    uint8_t dma_buffer[8];
    FakeUartDmaBackend backend;
    RadioTransport transport;
    Stm32UartDmaTransportContext context;

    TEST_ASSERT(
        init_dma_transport(
            &transport,
            &context,
            &backend,
            dma_buffer,
            sizeof(dma_buffer)),
        "DMA transport init should succeed"
    );

    const uint8_t first[] = { 0U, 1U, 2U, 3U, 4U, 5U };
    fake_uart_dma_backend_write(&backend, first, sizeof(first));

    uint8_t discarded[5];
    TEST_ASSERT(
        transport.read(&transport, discarded, sizeof(discarded)) ==
            sizeof(discarded),
        "Initial bytes should be consumed"
    );

    const uint8_t second[] = { 6U, 7U, 8U, 9U, 10U };
    fake_uart_dma_backend_write(&backend, second, sizeof(second));

    const uint8_t expected[] = { 5U, 6U, 7U, 8U, 9U, 10U };
    uint8_t output[sizeof(expected)] = { 0U };

    TEST_ASSERT(
        transport.read(&transport, output, sizeof(output)) ==
            sizeof(output),
        "Wrapped data should be fully readable"
    );
    TEST_ASSERT(
        memcmp(expected, output, sizeof(expected)) == 0,
        "Wraparound should preserve byte order"
    );

    return true;
}

static bool test_dma_transport_overflow_keeps_newest_data(void)
{
    uint8_t dma_buffer[8];
    FakeUartDmaBackend backend;
    RadioTransport transport;
    Stm32UartDmaTransportContext context;

    TEST_ASSERT(
        init_dma_transport(
            &transport,
            &context,
            &backend,
            dma_buffer,
            sizeof(dma_buffer)),
        "DMA transport init should succeed"
    );

    const uint8_t input[] = {
        0U, 1U, 2U, 3U, 4U, 5U,
        6U, 7U, 8U, 9U, 10U, 11U
    };

    fake_uart_dma_backend_write(&backend, input, sizeof(input));

    uint8_t output[8] = { 0U };
    TEST_ASSERT(
        transport.read(&transport, output, sizeof(output)) ==
            sizeof(output),
        "Overflow recovery should expose one full DMA buffer"
    );

    const uint8_t expected[] = { 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U };
    TEST_ASSERT(
        memcmp(expected, output, sizeof(expected)) == 0,
        "Overflow recovery should keep the newest bytes"
    );

    Stm32UartDmaTransportStats stats;
    stm32_uart_dma_transport_get_stats(&context, &stats);

    TEST_ASSERT(stats.overflow_events == 1U, "Overflow count should be one");
    TEST_ASSERT(stats.dropped_bytes == 4U, "Four old bytes should be dropped");
    TEST_ASSERT(stats.bytes_read == 8U, "Eight bytes should be read");

    return true;
}

static bool test_dma_transport_counter_wraparound(void)
{
    uint8_t dma_buffer[8];
    FakeUartDmaBackend backend;

    TEST_ASSERT(
        fake_uart_dma_backend_init(
            &backend,
            dma_buffer,
            sizeof(dma_buffer)),
        "Fake DMA backend init should succeed"
    );

    backend.produced_count = UINT32_MAX - 2U;

    RadioTransport transport;
    Stm32UartDmaTransportContext context;
    const Stm32UartDmaTransportConfig config = {
        .rx_buffer = dma_buffer,
        .rx_buffer_size = sizeof(dma_buffer),
        .backend_context = &backend,
        .get_produced_count =
            fake_uart_dma_backend_get_produced_count
    };

    TEST_ASSERT(
        stm32_uart_dma_transport_init(
            &transport,
            &context,
            &config),
        "DMA transport init should succeed near counter wrap"
    );

    const uint8_t input[] = { 10U, 11U, 12U, 13U, 14U };
    fake_uart_dma_backend_write(&backend, input, sizeof(input));

    uint8_t output[sizeof(input)] = { 0U };
    TEST_ASSERT(
        transport.read(&transport, output, sizeof(output)) ==
            sizeof(output),
        "Transport should read across producer counter wrap"
    );
    TEST_ASSERT(
        memcmp(input, output, sizeof(input)) == 0,
        "Counter wrap should preserve byte order"
    );

    return true;
}

static bool test_dma_transport_reset_discards_pending_data(void)
{
    uint8_t dma_buffer[8];
    FakeUartDmaBackend backend;
    RadioTransport transport;
    Stm32UartDmaTransportContext context;

    TEST_ASSERT(
        init_dma_transport(
            &transport,
            &context,
            &backend,
            dma_buffer,
            sizeof(dma_buffer)),
        "DMA transport init should succeed"
    );

    const uint8_t input[] = { 1U, 2U, 3U };
    fake_uart_dma_backend_write(&backend, input, sizeof(input));
    stm32_uart_dma_transport_reset(&context);

    TEST_ASSERT(
        stm32_uart_dma_transport_available(&context) == 0U,
        "Reset should discard pending bytes"
    );

    return true;
}

static bool test_stm32_time_wraparound(void)
{
    FakeTimeBackend backend = { .now_ms = 3U };
    Stm32TimeContext context;
    RadioTime time;

    TEST_ASSERT(
        stm32_time_init(
            &time,
            &context,
            fake_time_now_ms,
            &backend),
        "STM32 time init should succeed"
    );

    const uint32_t since_ms = UINT32_MAX - 5U;
    TEST_ASSERT(
        time.elapsed_ms(&time, since_ms) == 9U,
        "Elapsed time should handle uint32 wraparound"
    );

    return true;
}

static bool test_stm32_logger_filtering_and_format(void)
{
    FakeLogSink sink = { .data = { 0U }, .length = 0U };
    Stm32LoggerContext context;
    RadioLogger logger;

    TEST_ASSERT(
        stm32_logger_init(
            &logger,
            &context,
            RADIO_LOG_LEVEL_INFO,
            fake_log_write,
            &sink),
        "STM32 logger init should succeed"
    );

    logger.log(
        &logger,
        RADIO_LOG_LEVEL_DEBUG,
        "TEST",
        "hidden"
    );
    TEST_ASSERT(sink.length == 0U, "Debug message should be filtered");

    logger.log(
        &logger,
        RADIO_LOG_LEVEL_WARN,
        "TEST",
        "value=%u",
        42U
    );

    const char expected[] = "[WARN] [TEST] value=42\n";
    TEST_ASSERT(
        sink.length == sizeof(expected) - 1U,
        "Logger should write the expected number of bytes"
    );
    TEST_ASSERT(
        memcmp(sink.data, expected, sizeof(expected) - 1U) == 0,
        "Logger output should include level, tag, and message"
    );

    return true;
}

int main(void)
{
    int failed = 0;
    int total = 0;

#define RUN_TEST(test_function)               \
    do {                                      \
        total++;                              \
        if (!(test_function())) {             \
            failed++;                         \
        }                                     \
    } while (0)

    RUN_TEST(test_dma_transport_empty);
    RUN_TEST(test_dma_transport_partial_reads);
    RUN_TEST(test_dma_transport_wraparound);
    RUN_TEST(test_dma_transport_overflow_keeps_newest_data);
    RUN_TEST(test_dma_transport_counter_wraparound);
    RUN_TEST(test_dma_transport_reset_discards_pending_data);
    RUN_TEST(test_stm32_time_wraparound);
    RUN_TEST(test_stm32_logger_filtering_and_format);

#undef RUN_TEST

    if (failed == 0) {
        printf("All STM32 adapter tests passed (%d tests)\n", total);
        return 0;
    }

    printf("%d of %d STM32 adapter test(s) failed\n", failed, total);
    return 1;
}
