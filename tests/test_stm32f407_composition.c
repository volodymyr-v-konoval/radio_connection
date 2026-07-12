#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_stm32f4_hal.h"
#include "fk407m3_vet6_v1_1_radio.h"
#include "radio_composition.h"

#define TEST_ASSERT(condition, message)       \
    do {                                      \
        if (!(condition)) {                   \
            printf("FAIL: %s\n", message);   \
            return false;                     \
        }                                     \
    } while (0)

static void init_receiver_uart(
    UART_HandleTypeDef *uart,
    DMA_HandleTypeDef *dma
)
{
    memset(uart, 0, sizeof(*uart));
    memset(dma, 0, sizeof(*dma));

    dma->Instance = FK407M3_RADIO_DMA_INSTANCE;
    dma->Init.Channel = FK407M3_RADIO_DMA_CHANNEL;
    dma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    dma->Init.Mode = DMA_CIRCULAR;

    uart->Instance = FK407M3_RADIO_UART_INSTANCE;
    uart->Init.BaudRate = FK407M3_RADIO_UART_BAUDRATE;
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    uart->Init.StopBits = UART_STOPBITS_1;
    uart->Init.Parity = UART_PARITY_NONE;
    uart->Init.Mode = UART_MODE_TX_RX;
    uart->hdmarx = dma;
}

static void init_debug_uart(
    UART_HandleTypeDef *uart
)
{
    memset(uart, 0, sizeof(*uart));
    uart->Instance = FK407M3_DEBUG_UART_INSTANCE;
    uart->Init.BaudRate = FK407M3_DEBUG_UART_BAUDRATE;
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    uart->Init.StopBits = UART_STOPBITS_1;
    uart->Init.Parity = UART_PARITY_NONE;
    uart->Init.Mode = UART_MODE_TX_RX;
}

static bool test_composition_receives_split_crsf_frame_and_recovers(void)
{
    static const uint8_t crsf_frame[] = {
        0xC8, 0x18, 0x16,
        0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0,
        0x81, 0x0F, 0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0,
        0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C,
        0xAD
    };

    fake_stm32f4_hal_reset();

    UART_HandleTypeDef receiver_uart;
    DMA_HandleTypeDef receiver_dma;
    UART_HandleTypeDef debug_uart;
    uint8_t dma_buffer[64] = { 0U };

    init_receiver_uart(&receiver_uart, &receiver_dma);
    init_debug_uart(&debug_uart);

    Stm32f407RadioComposition composition;
    const Stm32f407RadioCompositionConfig config = {
        .receiver_uart = &receiver_uart,
        .receiver_dma_buffer = dma_buffer,
        .receiver_dma_buffer_size = sizeof(dma_buffer),
        .logger_uart = &debug_uart,
        .logger_timeout_ms = 20U,
        .log_level = RADIO_LOG_LEVEL_WARN,
        .failsafe_timeout_ms = 100U
    };

    TEST_ASSERT(
        stm32f407_radio_composition_init(&composition, &config),
        "STM32F407 composition should initialize"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->receive_calls == 1U,
        "Composition should start UART DMA reception"
    );

    memcpy(dma_buffer, crsf_frame, 10U);
    stm32f407_radio_composition_on_uart_rx_event(
        &composition,
        &receiver_uart,
        10U
    );
    stm32f407_radio_composition_process(&composition);

    RcInputFrame frame;
    TEST_ASSERT(
        !stm32f407_radio_composition_get_latest_frame(
            &composition,
            &frame),
        "Partial CRSF frame should not be published"
    );

    memcpy(
        &dma_buffer[10],
        &crsf_frame[10],
        sizeof(crsf_frame) - 10U
    );
    stm32f407_radio_composition_on_uart_rx_event(
        &composition,
        &receiver_uart,
        (uint16_t)sizeof(crsf_frame)
    );
    stm32f407_radio_composition_process(&composition);

    TEST_ASSERT(
        stm32f407_radio_composition_get_latest_frame(
            &composition,
            &frame),
        "Complete CRSF frame should be published"
    );
    TEST_ASSERT(frame.channel_count == 16U, "CRSF should expose 16 channels");

    for (uint8_t i = 0U; i < frame.channel_count; i++) {
        TEST_ASSERT(frame.channels[i] == 992U, "Each sample channel should be 992");
    }

    Stm32f407RadioDiagnostics diagnostics = { 0U };

    TEST_ASSERT(
        stm32f407_radio_composition_get_diagnostics(
            &composition,
            &diagnostics),
        "Composition should expose diagnostics"
    );

    TEST_ASSERT(
        diagnostics.received_bytes == sizeof(crsf_frame),
        "Diagnostics should report all DMA-produced bytes"
    );

    TEST_ASSERT(
        diagnostics.processed_bytes == sizeof(crsf_frame),
        "Diagnostics should report all processed bytes"
    );

    TEST_ASSERT(
        diagnostics.received_frames == 1U,
        "Diagnostics should report one parsed CRSF frame"
    );

    TEST_ASSERT(
        diagnostics.valid_frames == 1U,
        "Diagnostics should report one valid CRSF frame"
    );

    TEST_ASSERT(
        diagnostics.crc_errors == 0U,
        "Valid CRSF frame should not produce CRC errors"
    );

    TEST_ASSERT(
        diagnostics.length_errors == 0U,
        "Valid CRSF frame should not produce length errors"
    );

    TEST_ASSERT(
        diagnostics.unsupported_frames == 0U,
        "RC frame should not be reported as unsupported"
    );

    TEST_ASSERT(
        diagnostics.dma_rx_events == 2U,
        "Two DMA position events should be reported"
    );

    TEST_ASSERT(
        diagnostics.dma_overrun_events == 0U,
        "DMA transport should not overrun"
    );

    TEST_ASSERT(
        diagnostics.dma_dropped_bytes == 0U,
        "DMA transport should not drop bytes"
    );

    receiver_uart.ErrorCode = 0x40U;
    stm32f407_radio_composition_on_uart_error(
        &composition,
        &receiver_uart
    );

    TEST_ASSERT(
        fake_stm32f4_hal_state()->stop_calls == 0U,
        "UART error ISR forwarding should not stop DMA directly"
    );

    stm32f407_radio_composition_process(&composition);

    TEST_ASSERT(
        fake_stm32f4_hal_state()->stop_calls == 1U,
        "Composition processing should perform deferred recovery"
    );
    TEST_ASSERT(
        fake_stm32f4_hal_state()->receive_calls == 2U,
        "Composition should restart UART DMA after error"
    );

    TEST_ASSERT(
        stm32f407_radio_composition_get_diagnostics(
            &composition,
            &diagnostics),
        "Diagnostics should remain available after recovery"
    );

    TEST_ASSERT(
        diagnostics.uart_error_events == 1U,
        "UART error should be included in diagnostics"
    );

    TEST_ASSERT(
        diagnostics.uart_recovery_attempts == 1U,
        "One deferred recovery should be attempted"
    );

    TEST_ASSERT(
        diagnostics.uart_recovery_successes == 1U,
        "Deferred UART recovery should succeed"
    );

    TEST_ASSERT(
        diagnostics.uart_recovery_failures == 0U,
        "Successful recovery should not count as failure"
    );

    TEST_ASSERT(
        diagnostics.last_uart_error == 0x40U,
        "Diagnostics should preserve the last UART error"
    );

    return true;
}

int main(void)
{
    if (!test_composition_receives_split_crsf_frame_and_recovers()) {
        printf("STM32F407 composition test failed\n");
        return 1;
    }

    printf("STM32F407 composition test passed\n");
    return 0;
}
