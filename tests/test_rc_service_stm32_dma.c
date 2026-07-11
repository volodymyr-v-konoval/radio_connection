#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "crsf_protocol.h"
#include "fake_uart_dma_backend.h"
#include "rc_receiver_service.h"
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

static uint32_t fake_time_now_ms(void *backend_context)
{
    const FakeTimeBackend *backend =
        (const FakeTimeBackend *)backend_context;

    return backend != NULL ? backend->now_ms : 0U;
}

static bool test_service_receives_partial_crsf_frame_from_dma(void)
{
    const uint8_t frame_bytes[] = {
        0xC8U, 0x18U, 0x16U,
        0xE0U, 0x03U, 0x1FU, 0xF8U, 0xC0U, 0x07U, 0x3EU, 0xF0U,
        0x81U, 0x0FU, 0x7CU, 0xE0U, 0x03U, 0x1FU, 0xF8U, 0xC0U,
        0x07U, 0x3EU, 0xF0U, 0x81U, 0x0FU, 0x7CU,
        0xADU
    };

    uint8_t dma_buffer[64];
    FakeUartDmaBackend dma_backend;

    TEST_ASSERT(
        fake_uart_dma_backend_init(
            &dma_backend,
            dma_buffer,
            sizeof(dma_buffer)),
        "Fake DMA backend init should succeed"
    );

    RadioTransport transport;
    Stm32UartDmaTransportContext transport_context;

    const Stm32UartDmaTransportConfig transport_config = {
        .rx_buffer = dma_buffer,
        .rx_buffer_size = sizeof(dma_buffer),
        .backend_context = &dma_backend,
        .get_produced_count =
            fake_uart_dma_backend_get_produced_count
    };

    TEST_ASSERT(
        stm32_uart_dma_transport_init(
            &transport,
            &transport_context,
            &transport_config),
        "STM32 DMA transport init should succeed"
    );

    FakeTimeBackend time_backend = { .now_ms = 100U };
    Stm32TimeContext time_context;
    RadioTime time;

    TEST_ASSERT(
        stm32_time_init(
            &time,
            &time_context,
            fake_time_now_ms,
            &time_backend),
        "STM32 time init should succeed"
    );

    RadioProtocol protocol;
    CrsfProtocolContext protocol_context;
    crsf_protocol_init(&protocol, &protocol_context);

    RcReceiverService service;
    TEST_ASSERT(
        rc_receiver_service_init(
            &service,
            &transport,
            &protocol,
            NULL,
            &time,
            100U),
        "Receiver service init should succeed"
    );

    fake_uart_dma_backend_write(&dma_backend, frame_bytes, 7U);
    rc_receiver_service_process(&service);

    RcInputFrame frame;
    TEST_ASSERT(
        !rc_receiver_service_get_latest_frame(&service, &frame),
        "Partial CRSF frame should not be published"
    );

    time_backend.now_ms = 101U;
    fake_uart_dma_backend_write(
        &dma_backend,
        &frame_bytes[7],
        sizeof(frame_bytes) - 7U
    );
    rc_receiver_service_process(&service);

    TEST_ASSERT(
        rc_receiver_service_get_latest_frame(&service, &frame),
        "Completed CRSF frame should be published"
    );
    TEST_ASSERT(frame.protocol == RADIO_PROTOCOL_CRSF, "Protocol should be CRSF");
    TEST_ASSERT(frame.channel_count == 16U, "CRSF should provide 16 channels");
    TEST_ASSERT(frame.frame_valid, "Received frame should be valid");
    TEST_ASSERT(!frame.failsafe, "Received frame should clear failsafe");

    for (uint8_t channel = 0U; channel < frame.channel_count; channel++) {
        TEST_ASSERT(
            frame.channels[channel] == 992U,
            "Each sample CRSF channel should equal 992"
        );
    }

    return true;
}

int main(void)
{
    if (!test_service_receives_partial_crsf_frame_from_dma()) {
        printf("RC service STM32 DMA integration test failed\n");
        return 1;
    }

    printf("RC service STM32 DMA integration test passed\n");
    return 0;
}
