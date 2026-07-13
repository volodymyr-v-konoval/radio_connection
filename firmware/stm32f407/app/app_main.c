#include "app_main.h"

#include "board_validation.h"
#include "fk407m3_vet6_v1_1_radio.h"
#include "main.h"
#include "radio_composition.h"
#include "radio_control_profile.h"
#include "rc_control_policy.h"
#include "usart.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define APP_STATUS_PERIOD_MS        1000U
#define APP_LOGGER_TIMEOUT_MS         20U
#define APP_LOG_MESSAGE_SIZE         384U
#define APP_CRSF_CHANNEL_COUNT        16U

static uint8_t s_crsf_dma_buffer[
    FK407M3_RADIO_DMA_BUFFER_SIZE
];

static Stm32f407RadioComposition s_radio;
static RcChannelMapper s_channel_mapper;
static RcControlPolicy s_control_policy;

static RcControlCommand s_latest_control_command;

static bool s_initialized = false;
static bool s_link_seen = false;
static bool s_previous_failsafe = true;
static bool s_has_control_command = false;

static uint32_t s_last_status_time_ms = 0U;

static void app_log(const char *format, ...);
static void app_update_control(void);
static void app_report_link_transition(void);
static void app_report_status(uint32_t now_ms);
static void app_report_channels(const RcInputFrame *frame);
static void app_report_mapped_controls(const RcInputFrame *frame);
static void app_report_control_command(void);

static const char *app_control_state_name(
    RcControlState state
);

static const char *app_control_reason_name(
    RcControlSafetyReason reason
);

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
        "[BOOT] radio_connection STM32F407 Stage 8\r\n"
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

    if (!radio_control_profile_init(
            &s_channel_mapper)) {
        app_log(
            "[MAP] radio control profile initialization FAILED\r\n"
        );

        Error_Handler();
    }

    app_log(
        "[MAP] profile ready: "
        "CH1=roll CH2=pitch CH3=throttle "
        "CH4=yaw CH7=mode CH8=arm\r\n"
    );

    const RcControlPolicyConfig control_config = {
        .arm_on_threshold = 750,
        .arm_off_threshold = 250,
        .throttle_arm_max = 50,
        .require_arm_release_after_failsafe = true
    };

    if (!rc_control_policy_init(
            &s_control_policy,
            &control_config)) {
        app_log(
            "[CONTROL] policy initialization FAILED\r\n"
        );

        Error_Handler();
    }

    s_has_control_command =
        rc_control_policy_process(
            &s_control_policy,
            NULL,
            &s_latest_control_command
        );

    app_log(
        "[CONTROL] policy ready: "
        "arm_on=750 arm_off=250 "
        "throttle_arm_max=50 "
        "release_after_failsafe=1\r\n"
    );

    s_initialized = true;
    s_link_seen = false;

    s_previous_failsafe =
        stm32f407_radio_composition_is_failsafe(
            &s_radio
        );

    s_last_status_time_ms = HAL_GetTick();

    app_log(
        "[CRSF] USART2 Receive-to-IDLE circular DMA started\r\n"
    );

    app_log(
        "[CRSF] waiting for RadioMaster RP2 V2 frames\r\n"
    );

    app_log(
        "[BOOT] Stage 8 initialization complete\r\n"
    );
}

void app_main_poll(void)
{
    if (!s_initialized) {
        return;
    }

    stm32f407_radio_composition_process(&s_radio);

    app_update_control();
    app_report_link_transition();

    const uint32_t now_ms = HAL_GetTick();

    if ((uint32_t)(now_ms - s_last_status_time_ms) >=
        APP_STATUS_PERIOD_MS) {
        s_last_status_time_ms = now_ms;
        app_report_status(now_ms);
    }
}

static void app_update_control(void)
{
    RcInputFrame frame = { 0 };

    if (!stm32f407_radio_composition_get_latest_frame(
            &s_radio,
            &frame)) {
        s_has_control_command =
            rc_control_policy_process(
                &s_control_policy,
                NULL,
                &s_latest_control_command
            );

        return;
    }

    RcMappedInput mapped = { 0 };

    (void)rc_channel_mapper_map(
        &s_channel_mapper,
        &frame,
        &mapped
    );

    mapped.frame_valid = frame.frame_valid;
    mapped.frame_lost = frame.frame_lost;
    mapped.failsafe =
        frame.failsafe ||
        stm32f407_radio_composition_is_failsafe(
            &s_radio
        );
    mapped.timestamp_ms = frame.timestamp_ms;
    mapped.protocol = frame.protocol;

    s_has_control_command =
        rc_control_policy_process(
            &s_control_policy,
            &mapped,
            &s_latest_control_command
        );
}

static void app_report_link_transition(void)
{
    const bool failsafe =
        stm32f407_radio_composition_is_failsafe(
            &s_radio
        );

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
    Stm32f407RadioDiagnostics diagnostics = { 0U };

    if (!stm32f407_radio_composition_get_diagnostics(
            &s_radio,
            &diagnostics)) {
        app_log(
            "[DIAG] receiver diagnostics unavailable\r\n"
        );

        return;
    }

    const bool failsafe =
        stm32f407_radio_composition_is_failsafe(
            &s_radio
        );

    app_log(
        "[CRSF] bytes=%lu read=%lu frames=%lu valid=%lu "
        "crc=%lu len=%lu unsupported=%lu\r\n",
        (unsigned long)diagnostics.received_bytes,
        (unsigned long)diagnostics.processed_bytes,
        (unsigned long)diagnostics.received_frames,
        (unsigned long)diagnostics.valid_frames,
        (unsigned long)diagnostics.crc_errors,
        (unsigned long)diagnostics.length_errors,
        (unsigned long)diagnostics.unsupported_frames
    );

    app_log(
        "[DMA] events=%lu duplicate=%lu invalid=%lu "
        "overrun=%lu dropped=%lu uart_err=%lu "
        "recovery=%lu/%lu failed=%lu "
        "last_err=0x%08lX\r\n",
        (unsigned long)diagnostics.dma_rx_events,
        (unsigned long)diagnostics.dma_duplicate_events,
        (unsigned long)diagnostics.dma_invalid_events,
        (unsigned long)diagnostics.dma_overrun_events,
        (unsigned long)diagnostics.dma_dropped_bytes,
        (unsigned long)diagnostics.uart_error_events,
        (unsigned long)diagnostics.uart_recovery_successes,
        (unsigned long)diagnostics.uart_recovery_attempts,
        (unsigned long)diagnostics.uart_recovery_failures,
        (unsigned long)diagnostics.last_uart_error
    );

    app_report_control_command();

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

    if (!frame.frame_valid ||
        failsafe ||
        frame.frame_lost) {
        app_log(
            "[RC] channels unavailable: "
            "failsafe active, snapshot_age=%lu ms\r\n",
            (unsigned long)frame_age_ms
        );

        return;
    }

    app_report_channels(&frame);
    app_report_mapped_controls(&frame);
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

static void app_report_mapped_controls(
    const RcInputFrame *frame
)
{
    if (frame == NULL) {
        return;
    }

    RcMappedInput mapped = { 0 };

    if (!rc_channel_mapper_map(
            &s_channel_mapper,
            frame,
            &mapped)) {
        app_log(
            "[MAP] control values unavailable\r\n"
        );

        return;
    }

    int16_t roll = 0;
    int16_t pitch = 0;
    int16_t throttle = 0;
    int16_t yaw = 0;
    int16_t mode = 0;
    int16_t arm = 0;

    uint16_t roll_us = 0U;
    uint16_t pitch_us = 0U;
    uint16_t throttle_us = 0U;
    uint16_t yaw_us = 0U;
    uint16_t mode_us = 0U;
    uint16_t arm_us = 0U;

    const bool profile_complete =
        rc_mapped_input_get(
            &mapped,
            RC_FUNCTION_ROLL,
            &roll,
            &roll_us
        ) &&
        rc_mapped_input_get(
            &mapped,
            RC_FUNCTION_PITCH,
            &pitch,
            &pitch_us
        ) &&
        rc_mapped_input_get(
            &mapped,
            RC_FUNCTION_THROTTLE,
            &throttle,
            &throttle_us
        ) &&
        rc_mapped_input_get(
            &mapped,
            RC_FUNCTION_YAW,
            &yaw,
            &yaw_us
        ) &&
        rc_mapped_input_get(
            &mapped,
            RC_FUNCTION_MODE,
            &mode,
            &mode_us
        ) &&
        rc_mapped_input_get(
            &mapped,
            RC_FUNCTION_ARM,
            &arm,
            &arm_us
        );

    if (!profile_complete) {
        app_log(
            "[MAP] mapped profile is incomplete\r\n"
        );

        return;
    }

    app_log(
        "[MAP] norm roll=%d pitch=%d throttle=%d "
        "yaw=%d mode=%d arm=%d\r\n",
        (int)roll,
        (int)pitch,
        (int)throttle,
        (int)yaw,
        (int)mode,
        (int)arm
    );

    app_log(
        "[MAP] us   roll=%u pitch=%u throttle=%u "
        "yaw=%u mode=%u arm=%u\r\n",
        (unsigned int)roll_us,
        (unsigned int)pitch_us,
        (unsigned int)throttle_us,
        (unsigned int)yaw_us,
        (unsigned int)mode_us,
        (unsigned int)arm_us
    );
}

static void app_report_control_command(void)
{
    if (!s_has_control_command) {
        app_log(
            "[CONTROL] command unavailable\r\n"
        );

        return;
    }

    app_log(
        "[CONTROL] state=%s input=%u armed=%u "
        "enabled=%u safe=%u reason=%s\r\n",
        app_control_state_name(
            s_latest_control_command.state
        ),
        s_latest_control_command.input_valid ? 1U : 0U,
        s_latest_control_command.armed ? 1U : 0U,
        s_latest_control_command.outputs_enabled ? 1U : 0U,
        s_latest_control_command.safe_state_active ? 1U : 0U,
        app_control_reason_name(
            s_latest_control_command.reason
        )
    );

    app_log(
        "[CONTROL] out roll=%d pitch=%d throttle=%d "
        "yaw=%d mode=%d\r\n",
        (int)s_latest_control_command.roll,
        (int)s_latest_control_command.pitch,
        (int)s_latest_control_command.throttle,
        (int)s_latest_control_command.yaw,
        (int)s_latest_control_command.mode
    );
}

static const char *app_control_state_name(
    RcControlState state
)
{
    switch (state) {
    case RC_CONTROL_STATE_SAFE:
        return "SAFE";

    case RC_CONTROL_STATE_READY:
        return "READY";

    case RC_CONTROL_STATE_ACTIVE:
        return "ACTIVE";

    default:
        return "UNKNOWN";
    }
}

static const char *app_control_reason_name(
    RcControlSafetyReason reason
)
{
    switch (reason) {
    case RC_CONTROL_REASON_NONE:
        return "NONE";

    case RC_CONTROL_REASON_NO_INPUT:
        return "NO_INPUT";

    case RC_CONTROL_REASON_INVALID_FRAME:
        return "INVALID_FRAME";

    case RC_CONTROL_REASON_FRAME_LOST:
        return "FRAME_LOST";

    case RC_CONTROL_REASON_FAILSAFE:
        return "FAILSAFE";

    case RC_CONTROL_REASON_MAPPING_INCOMPLETE:
        return "MAPPING_INCOMPLETE";

    case RC_CONTROL_REASON_ARM_RELEASE_REQUIRED:
        return "ARM_RELEASE_REQUIRED";

    case RC_CONTROL_REASON_THROTTLE_NOT_LOW:
        return "THROTTLE_NOT_LOW";

    case RC_CONTROL_REASON_DISARMED:
        return "DISARMED";

    default:
        return "UNKNOWN";
    }
}

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
