#include "crsf_protocol.h"
#include "sbus_protocol.h"

#include <stdbool.h>
#include <stdio.h>

#define TEST_ASSERT(condition, message)             \
    do {                                            \
        if (!(condition)) {                         \
            printf("FAIL: %s\n", (message));       \
            return false;                           \
        }                                           \
    } while (0)

static bool test_crsf_uart_config(void)
{
    RadioProtocol protocol;
    CrsfProtocolContext context;
    crsf_protocol_init(&protocol, &context);

    RadioUartConfig config;

    TEST_ASSERT(
        radio_protocol_get_uart_config(
            &protocol,
            &config),
        "CRSF UART configuration should be available"
    );

    TEST_ASSERT(
        config.baud_rate == 420000U,
        "CRSF baud rate should be 420000"
    );

    TEST_ASSERT(
        config.data_bits == 8U,
        "CRSF should use eight data bits"
    );

    TEST_ASSERT(
        config.parity == RADIO_UART_PARITY_NONE,
        "CRSF should not use parity"
    );

    TEST_ASSERT(
        config.stop_bits == RADIO_UART_STOP_BITS_1,
        "CRSF should use one stop bit"
    );

    TEST_ASSERT(
        !config.signal_inverted,
        "CRSF signal should be non-inverted"
    );

    return true;
}

static bool test_sbus_uart_config(void)
{
    RadioProtocol protocol;
    SbusProtocolContext context;
    sbus_protocol_init(&protocol, &context);

    RadioUartConfig config;

    TEST_ASSERT(
        radio_protocol_get_uart_config(
            &protocol,
            &config),
        "SBUS UART configuration should be available"
    );

    TEST_ASSERT(
        config.baud_rate == 100000U,
        "SBUS baud rate should be 100000"
    );

    TEST_ASSERT(
        config.data_bits == 8U,
        "SBUS should use eight data bits"
    );

    TEST_ASSERT(
        config.parity == RADIO_UART_PARITY_EVEN,
        "SBUS should use even parity"
    );

    TEST_ASSERT(
        config.stop_bits == RADIO_UART_STOP_BITS_2,
        "SBUS should use two stop bits"
    );

    TEST_ASSERT(
        config.signal_inverted,
        "SBUS signal should be inverted"
    );

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_crsf_uart_config()) {
        failed++;
    }

    if (!test_sbus_uart_config()) {
        failed++;
    }

    if (failed != 0) {
        printf("%d protocol UART configuration test(s) failed\n", failed);
        return 1;
    }

    printf("All protocol UART configuration tests passed\n");
    return 0;
}
