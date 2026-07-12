#include "app_main.h"

#include "board_validation.h"
#include "fk407m3_vet6_v1_1_radio.h"
#include "main.h"
#include "radio_composition.h"
#include "usart.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define APP_STATUS_PERIOD_MS       1000U
#define APP_LOGGER_TIMEOUT_MS      20U
#define APP_LOG_MESSAGE_SIZE       384U
#define APP_CRSF_CHANNEL_COUNT      16U

static uint8_t s_crsf_dma_buffer[
    FK407M3_RADIO_DMA_BUFFER_SIZE
];

static Stm32f407RadioComposition s_radio;

static bool s_initialized = false;
static bool s_link_seen = false;
static bool s_previous_failsafe = true;

static uint32_t s_last_status_time_ms = 0U;

static void app_log(const char *format, ...);
static void app_report_link_transition(void);
static void app_report_status(uint32_t now_ms);
static void app_report_channels(const RcInputFrame *frame);

static void app_log(
    const char *format,
    ...
)
{
    if (format == NULL) {
        return;
    }

    char message[APP_LOG_MESSAGE_SIZE];

    va_list args;
    va_start(args, format);

    const int result = vsnprintf(
        message,
        sizeof(message),
        format,
        args
    );

    va_end(args);

    if (result <= 0) {
        return;
    }

    size_t message_length = (size_t)result;

    if (message_length >= sizeof(message)) {
        message_length = sizeof(message) - 1U;
    }

    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)message,
        (uint16_t)message_length,
        APP_LOGGER_TIMEOUT_MS
    );
}

void app_main_init(void)
{
    app_log("\r\n");
    app_log(
        "[BOOT] radio_connection STM32F407 Stage 5\r\n"
    );

    const uint32_t validation_errors =
        board_validate_configuration();

    if (validation_errors != 0U) {
        app_log(
            "[BOARD] validation FAILED: 0x%08lX\r\n",
            (unsigned long)validation_errors
        );

        Error_Handler();
    }

    app_log("[BOARD] validation OK\r\n");

    const Stm32f407RadioCompositionConfig config = {
        .receiver_uart = &huart2,
        .receiver_dma_buffer = s_crsf_dma_buffer,
        .receiver_dma_buffer_size =
            sizeof(s_crsf_dma_buffer),

        .logger_uart = &huart1,
        .logger_timeout_ms = APP_LOGGER_TIMEOUT_MS,

        /*
         * INFO is intentionally disabled here because the receiver
         * service currently emits one INFO log for every valid frame.
         * At CRSF frame rates that would overload the debug UART.
         */
        .log_level = RADIO_LOG_LEVEL_WARN,

        .failsafe_timeout_ms =
            FK407M3_RADIO_FAILSAFE_TIMEOUT_MS
    };

    if (!stm32f407_radio_composition_init(
            &s_radio,
            &config)) {
        app_log(
            "[CRSF] radio composition initialization FAILED\r\n"
        );

        Error_Handler();
    }

    s_initialized = true;
    s_link_seen = false;
    s_previous_failsafe =
        stm32f407_radio_composition_is_failsafe(&s_radio);

    s_last_status_time_ms = HAL_GetTick();

    app_log(
        "[CRSF] USART2 Receive-to-IDLE circular DMA started\r\n"
    );
    app_log(
        "[CRSF] waiting for RadioMaster RP2 V2 frames\r\n"
    );
    app_log(
        "[BOOT] Stage 5 initialization complete\r\n"
    );
}

void app_main_poll(void)
{
    if (!s_initialized) {
        return;
    }

    /*
     * Runs outside interrupt context:
     * - performs deferred UART recovery;
     * - reads new DMA bytes;
     * - feeds bytes to the CRSF parser;
     * - publishes the latest RcInputFrame;
     * - updates failsafe state.
     */
    stm32f407_radio_composition_process(&s_radio);

    app_report_link_transition();

    const uint32_t now_ms = HAL_GetTick();

    if ((uint32_t)(now_ms - s_last_status_time_ms) >=
        APP_STATUS_PERIOD_MS) {
        s_last_status_time_ms = now_ms;
        app_report_status(now_ms);
    }
}

static void app_report_link_transition(void)
{
    const bool failsafe =
        stm32f407_radio_composition_is_failsafe(&s_radio);

    /*
     * The first valid CRSF frame establishes the initial link.
     * This is kept separate from reconnect reporting.
     */
    if (!failsafe && !s_link_seen) {
        s_link_seen = true;
        app_log("[CRSF] link active\r\n");
    } else if (failsafe != s_previous_failsafe) {
        if (failsafe) {
            app_log("[RC] failsafe entered\r\n");
        } else {
            app_log("[CRSF] link recovered\r\n");
        }
    }

    s_previous_failsafe = failsafe;
}

static void app_report_status(
    uint32_t now_ms
)
{
    Stm32f4UartDmaBackendStats dma_stats = { 0U };
    Stm32UartDmaTransportStats transport_stats = { 0U };

    stm32f4_uart_dma_backend_get_stats(
        &s_radio.uart_dma_backend,
        &dma_stats
    );

    stm32_uart_dma_transport_get_stats(
        &s_radio.transport_context,
        &transport_stats
    );

    const uint32_t produced_bytes =
        stm32f4_uart_dma_backend_get_produced_count(
            &s_radio.uart_dma_backend
        );

    const bool failsafe =
        stm32f407_radio_composition_is_failsafe(&s_radio);

    app_log(
        "[CRSF] bytes=%lu read=%lu frames=%lu valid=%lu "
        "crc=%lu len=%lu unsupported=%lu\r\n",
        (unsigned long)produced_bytes,
        (unsigned long)transport_stats.bytes_read,
        (unsigned long)s_radio.crsf_context.received_frames,
        (unsigned long)s_radio.crsf_context.valid_frames,
        (unsigned long)s_radio.crsf_context.crc_errors,
        (unsigned long)s_radio.crsf_context.length_errors,
        (unsigned long)s_radio.crsf_context.unsupported_frames
    );

    app_log(
        "[DMA] events=%lu duplicate=%lu invalid=%lu "
        "overrun=%lu dropped=%lu uart_err=%lu "
        "recovery=%lu/%lu last_err=0x%08lX\r\n",
        (unsigned long)dma_stats.rx_events,
        (unsigned long)dma_stats.duplicate_events,
        (unsigned long)dma_stats.invalid_events,
        (unsigned long)transport_stats.overflow_events,
        (unsigned long)transport_stats.dropped_bytes,
        (unsigned long)dma_stats.uart_error_events,
        (unsigned long)dma_stats.recovery_successes,
        (unsigned long)dma_stats.recovery_attempts,
        (unsigned long)dma_stats.last_uart_error
    );

    RcInputFrame frame = { 0 };

    if (!stm32f407_radio_composition_get_latest_frame(
            &s_radio,
            &frame)) {
        app_log(
            "[RC] no frame yet failsafe=%u\r\n",
            failsafe ? 1U : 0U
        );

        return;
    }

    const uint32_t frame_age_ms =
        (uint32_t)(now_ms - frame.timestamp_ms);

    app_log(
        "[RC] frame_valid=%u failsafe=%u lost=%u "
        "channels=%u age=%lu ms\r\n",
        frame.frame_valid ? 1U : 0U,
        failsafe ? 1U : 0U,
        frame.frame_lost ? 1U : 0U,
        (unsigned int)frame.channel_count,
        (unsigned long)frame_age_ms
    );

    if (!frame.frame_valid || failsafe || frame.frame_lost) {
        app_log(
            "[RC] channels unavailable: failsafe active, age=%lu ms\r\n",
            (unsigned long)frame_age_ms
        );
        return;
    }

app_report_channels(&frame);

    app_report_channels(&frame);
}

static void app_report_channels(
    const RcInputFrame *frame
)
{
    if (frame == NULL ||
        frame->channel_count < APP_CRSF_CHANNEL_COUNT) {
        return;
    }

    app_log(
        "[RC] ch01=%u ch02=%u ch03=%u ch04=%u "
        "ch05=%u ch06=%u ch07=%u ch08=%u\r\n",
        (unsigned int)frame->channels[0],
        (unsigned int)frame->channels[1],
        (unsigned int)frame->channels[2],
        (unsigned int)frame->channels[3],
        (unsigned int)frame->channels[4],
        (unsigned int)frame->channels[5],
        (unsigned int)frame->channels[6],
        (unsigned int)frame->channels[7]
    );

    app_log(
        "[RC] ch09=%u ch10=%u ch11=%u ch12=%u "
        "ch13=%u ch14=%u ch15=%u ch16=%u\r\n",
        (unsigned int)frame->channels[8],
        (unsigned int)frame->channels[9],
        (unsigned int)frame->channels[10],
        (unsigned int)frame->channels[11],
        (unsigned int)frame->channels[12],
        (unsigned int)frame->channels[13],
        (unsigned int)frame->channels[14],
        (unsigned int)frame->channels[15]
    );
}

/*
 * HAL calls this callback from UART/DMA interrupt context.
 *
 * No parsing and no blocking UART logging are performed here.
 * We only publish the current DMA write position to the backend.
 */
void HAL_UARTEx_RxEventCallback(
    UART_HandleTypeDef *uart,
    uint16_t dma_position
)
{
    if (!s_initialized || uart != &huart2) {
        return;
    }

    stm32f407_radio_composition_on_uart_rx_event(
        &s_radio,
        uart,
        dma_position
    );
}

/*
 * UART recovery is intentionally deferred.
 *
 * This callback only records the error. The actual DMA stop/restart
 * happens later from app_main_poll() through composition_process().
 */
void HAL_UART_ErrorCallback(
    UART_HandleTypeDef *uart
)
{
    if (!s_initialized || uart != &huart2) {
        return;
    }

    stm32f407_radio_composition_on_uart_error(
        &s_radio,
        uart
    );
}
