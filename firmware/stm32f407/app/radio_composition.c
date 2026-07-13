#include "radio_composition.h"

#include <string.h>

#include "fk407m3_vet6_v1_1_radio.h"
#include "stm32f4_time_backend.h"

bool stm32f407_radio_composition_init(
    Stm32f407RadioComposition *composition,
    const Stm32f407RadioCompositionConfig *config
)
{
    if (composition == NULL ||
        config == NULL ||
        config->receiver_uart == NULL ||
        config->receiver_dma_buffer == NULL ||
        config->receiver_dma_buffer_size == 0U ||
        config->logger_uart == NULL ||
        !fk407m3_vet6_v1_1_validate_receiver_uart(
            config->receiver_uart) ||
        !fk407m3_vet6_v1_1_validate_debug_uart(
            config->logger_uart)) {
        return false;
    }

    memset(composition, 0, sizeof(*composition));

    const Stm32f4UartDmaBackendConfig dma_backend_config = {
        .uart = config->receiver_uart,
        .rx_buffer = config->receiver_dma_buffer,
        .rx_buffer_size = config->receiver_dma_buffer_size,
        .disable_half_transfer_irq = false
    };

    if (!stm32f4_uart_dma_backend_init(
            &composition->uart_dma_backend,
            &dma_backend_config) ||
        !stm32f4_uart_dma_backend_start(
            &composition->uart_dma_backend)) {
        return false;
    }

    const Stm32UartDmaTransportConfig transport_config = {
        .rx_buffer = config->receiver_dma_buffer,
        .rx_buffer_size = config->receiver_dma_buffer_size,
        .backend_context = &composition->uart_dma_backend,
        .get_produced_count =
            stm32f4_uart_dma_backend_get_produced_count
    };

    if (!stm32_uart_dma_transport_init(
            &composition->transport,
            &composition->transport_context,
            &transport_config) ||
        !stm32f4_uart_logger_backend_init(
            &composition->logger_backend,
            config->logger_uart,
            config->logger_timeout_ms) ||
        !stm32_logger_init(
            &composition->logger,
            &composition->logger_context,
            config->log_level,
            stm32f4_uart_logger_backend_write,
            &composition->logger_backend) ||
        !stm32_time_init(
            &composition->time,
            &composition->time_context,
            stm32f4_time_backend_now_ms,
            NULL)) {
        (void)stm32f4_uart_dma_backend_stop(
            &composition->uart_dma_backend);
        return false;
    }

    crsf_protocol_init(
        &composition->protocol,
        &composition->crsf_context
    );

    if (!stm32f4_uart_tx_backend_init(
            &composition->uart_tx_backend,
            &composition->telemetry_tx,
            config->receiver_uart)) {
        (void)stm32f4_uart_dma_backend_stop(
            &composition->uart_dma_backend);
        return false;
    }

    CrsfTelemetryServiceConfig telemetry_config;
    crsf_telemetry_service_config_init(&telemetry_config);

    if (config->telemetry_battery_period_ms > 0U) {
        telemetry_config.battery_period_ms =
            config->telemetry_battery_period_ms;
    }

    if (config->telemetry_heartbeat_period_ms > 0U) {
        telemetry_config.heartbeat_period_ms =
            config->telemetry_heartbeat_period_ms;
    }

    if (config->telemetry_flight_mode_period_ms > 0U) {
        telemetry_config.flight_mode_period_ms =
            config->telemetry_flight_mode_period_ms;
    }

    if (config->telemetry_device_info.name[0] != '\0') {
        telemetry_config.device_info =
            config->telemetry_device_info;
    }

    if (!crsf_telemetry_service_init(
            &composition->telemetry_service,
            &composition->telemetry_tx,
            &composition->crsf_context,
            &telemetry_config)) {
        (void)stm32f4_uart_dma_backend_stop(
            &composition->uart_dma_backend);
        return false;
    }

    if (!rc_receiver_service_init(
            &composition->service,
            &composition->transport,
            &composition->protocol,
            &composition->logger,
            &composition->time,
            config->failsafe_timeout_ms)) {
        (void)stm32f4_uart_dma_backend_stop(
            &composition->uart_dma_backend);
        return false;
    }

    composition->initialized = true;
    return true;
}

void stm32f407_radio_composition_process(
    Stm32f407RadioComposition *composition
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    if (stm32f4_uart_dma_backend_process(
            &composition->uart_dma_backend)) {
        /* Discard stale bytes from the failed DMA epoch. */
        stm32_uart_dma_transport_reset(
            &composition->transport_context);

        if (composition->protocol.reset != NULL) {
            composition->protocol.reset(&composition->protocol);
        }
    }

    rc_receiver_service_process(&composition->service);


    (void)crsf_telemetry_service_process(
        &composition->telemetry_service,
        composition->time.now_ms(&composition->time)
    );
}

void stm32f407_radio_composition_on_uart_rx_event(
    Stm32f407RadioComposition *composition,
    UART_HandleTypeDef *uart,
    uint16_t dma_position
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    stm32f4_uart_dma_backend_on_rx_event(
        &composition->uart_dma_backend,
        uart,
        dma_position
    );
}

void stm32f407_radio_composition_on_uart_tx_complete(
    Stm32f407RadioComposition *composition,
    UART_HandleTypeDef *uart
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    stm32f4_uart_tx_backend_on_tx_complete(
        &composition->uart_tx_backend,
        uart
    );
}

void stm32f407_radio_composition_on_uart_error(
    Stm32f407RadioComposition *composition,
    UART_HandleTypeDef *uart
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    stm32f4_uart_dma_backend_on_error(
        &composition->uart_dma_backend,
        uart
    );


    stm32f4_uart_tx_backend_on_error(
        &composition->uart_tx_backend,
        uart
    );
}

bool stm32f407_radio_composition_get_diagnostics(
    const Stm32f407RadioComposition *composition,
    Stm32f407RadioDiagnostics *out_diagnostics
)
{
    if (composition == NULL ||
        out_diagnostics == NULL ||
        !composition->initialized) {
        return false;
    }

    Stm32f4UartDmaBackendStats dma_stats = { 0U };
    Stm32UartDmaTransportStats transport_stats = { 0U };

    stm32f4_uart_dma_backend_get_stats(
        &composition->uart_dma_backend,
        &dma_stats
    );

    stm32_uart_dma_transport_get_stats(
        &composition->transport_context,
        &transport_stats
    );

    memset(
        out_diagnostics,
        0,
        sizeof(*out_diagnostics)
    );

    out_diagnostics->received_bytes =
        stm32f4_uart_dma_backend_get_produced_count(
            (void *)&composition->uart_dma_backend
        );

    out_diagnostics->processed_bytes =
        transport_stats.bytes_read;

    out_diagnostics->received_frames =
        composition->crsf_context.received_frames;

    out_diagnostics->valid_frames =
        composition->crsf_context.valid_frames;

    out_diagnostics->crc_errors =
        composition->crsf_context.crc_errors;

    out_diagnostics->length_errors =
        composition->crsf_context.length_errors;

    out_diagnostics->unsupported_frames =
        composition->crsf_context.unsupported_frames;

    out_diagnostics->dma_rx_events =
        dma_stats.rx_events;

    out_diagnostics->dma_duplicate_events =
        dma_stats.duplicate_events;

    out_diagnostics->dma_invalid_events =
        dma_stats.invalid_events;

    out_diagnostics->dma_overrun_events =
        transport_stats.overflow_events;

    out_diagnostics->dma_dropped_bytes =
        transport_stats.dropped_bytes;

    out_diagnostics->uart_error_events =
        dma_stats.uart_error_events;

    out_diagnostics->uart_recovery_attempts =
        dma_stats.recovery_attempts;

    out_diagnostics->uart_recovery_successes =
        dma_stats.recovery_successes;

    out_diagnostics->uart_recovery_failures =
        dma_stats.recovery_failures;

    out_diagnostics->last_uart_error =
        dma_stats.last_uart_error;

    return true;
}

void stm32f407_radio_composition_set_battery_telemetry(
    Stm32f407RadioComposition *composition,
    const CrsfBatteryTelemetry *battery
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    crsf_telemetry_service_set_battery(
        &composition->telemetry_service,
        battery
    );
}

bool stm32f407_radio_composition_set_flight_mode(
    Stm32f407RadioComposition *composition,
    const char *flight_mode
)
{
    if (composition == NULL || !composition->initialized) {
        return false;
    }

    return crsf_telemetry_service_set_flight_mode(
        &composition->telemetry_service,
        flight_mode
    );
}

bool stm32f407_radio_composition_queue_custom_telemetry(
    Stm32f407RadioComposition *composition,
    uint8_t frame_type,
    const uint8_t *payload,
    size_t payload_length
)
{
    if (composition == NULL || !composition->initialized) {
        return false;
    }

    return crsf_telemetry_service_queue_custom_broadcast(
        &composition->telemetry_service,
        frame_type,
        payload,
        payload_length
    );
}

bool stm32f407_radio_composition_get_telemetry_diagnostics(
    const Stm32f407RadioComposition *composition,
    Stm32f407CrsfTelemetryDiagnostics *out_diagnostics
)
{
    if (composition == NULL || !composition->initialized ||
        out_diagnostics == NULL) {
        return false;
    }

    crsf_telemetry_service_get_stats(
        &composition->telemetry_service,
        &out_diagnostics->service
    );

    stm32f4_uart_tx_backend_get_stats(
        &composition->uart_tx_backend,
        &out_diagnostics->tx
    );

    out_diagnostics->tx_busy =
        stm32f4_uart_tx_backend_is_busy(
            &composition->uart_tx_backend
        );

    return true;
}

bool stm32f407_radio_composition_get_latest_frame(
    Stm32f407RadioComposition *composition,
    RcInputFrame *out_frame
)
{
    if (composition == NULL || !composition->initialized) {
        return false;
    }

    return rc_receiver_service_get_latest_frame(
        &composition->service,
        out_frame
    );
}

bool stm32f407_radio_composition_is_failsafe(
    Stm32f407RadioComposition *composition
)
{
    if (composition == NULL || !composition->initialized) {
        return true;
    }

    return rc_receiver_service_is_failsafe(
        &composition->service
    );
}
