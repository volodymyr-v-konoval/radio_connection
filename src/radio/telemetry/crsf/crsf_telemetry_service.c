#include "crsf_telemetry_service.h"

#include <string.h>

#define CRSF_TELEMETRY_SCHEDULE_SLOTS 3U

static size_t bounded_string_length(
    const char *text,
    size_t max_length
)
{
    size_t length = 0U;

    if (text == NULL) {
        return 0U;
    }

    while (length < max_length && text[length] != '\0') {
        length++;
    }

    return length;
}

static bool period_due(
    uint32_t now_ms,
    uint32_t last_ms,
    uint32_t period_ms
)
{
    return period_ms > 0U &&
        (uint32_t)(now_ms - last_ms) >= period_ms;
}

static bool service_start_frame(
    CrsfTelemetryService *service,
    const uint8_t *frame,
    size_t frame_length
)
{
    if (!radio_tx_try_write(service->tx, frame, frame_length)) {
        service->stats.write_errors++;
        return false;
    }

    service->stats.frames_started++;
    return true;
}

void crsf_telemetry_service_config_init(
    CrsfTelemetryServiceConfig *config
)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));

    config->battery_period_ms =
        CRSF_TELEMETRY_DEFAULT_BATTERY_PERIOD_MS;
    config->heartbeat_period_ms =
        CRSF_TELEMETRY_DEFAULT_HEARTBEAT_PERIOD_MS;
    config->flight_mode_period_ms =
        CRSF_TELEMETRY_DEFAULT_FLIGHT_MODE_PERIOD_MS;

    (void)strncpy(
        config->device_info.name,
        "radio_connection",
        sizeof(config->device_info.name) - 1U
    );

    config->device_info.serial_number = 0x52414449UL; /* RADI */
    config->device_info.hardware_id = 0x00040701UL;
    config->device_info.firmware_id = 0x00030000UL;
    config->device_info.parameter_count = 0U;
    config->device_info.parameter_version = 1U;
}

bool crsf_telemetry_service_init(
    CrsfTelemetryService *service,
    RadioTx *tx,
    CrsfProtocolContext *crsf_context,
    const CrsfTelemetryServiceConfig *config
)
{
    if (service == NULL || tx == NULL ||
        tx->try_write == NULL || tx->is_busy == NULL ||
        config == NULL || config->device_info.name[0] == '\0') {
        return false;
    }

    memset(service, 0, sizeof(*service));

    service->tx = tx;
    service->crsf_context = crsf_context;
    service->config = *config;
    service->heartbeat_pending = true;
    service->initialized = true;

    return true;
}

void crsf_telemetry_service_set_battery(
    CrsfTelemetryService *service,
    const CrsfBatteryTelemetry *battery
)
{
    if (service == NULL || !service->initialized || battery == NULL) {
        return;
    }

    service->battery = *battery;
    service->battery_valid = true;
    service->battery_pending = true;
}

bool crsf_telemetry_service_set_flight_mode(
    CrsfTelemetryService *service,
    const char *flight_mode
)
{
    if (service == NULL || !service->initialized || flight_mode == NULL) {
        return false;
    }

    const size_t length = bounded_string_length(
        flight_mode,
        CRSF_TELEMETRY_FLIGHT_MODE_MAX + 1U
    );

    if (length == 0U || length > CRSF_TELEMETRY_FLIGHT_MODE_MAX) {
        return false;
    }

    memcpy(service->flight_mode, flight_mode, length);
    service->flight_mode[length] = '\0';
    service->flight_mode_valid = true;
    service->flight_mode_pending = true;

    return true;
}

bool crsf_telemetry_service_queue_custom_broadcast(
    CrsfTelemetryService *service,
    uint8_t frame_type,
    const uint8_t *payload,
    size_t payload_length
)
{
    if (service == NULL || !service->initialized) {
        return false;
    }

    size_t frame_length = 0U;

    if (!crsf_telemetry_encode_broadcast(
            frame_type,
            payload,
            payload_length,
            service->custom_frame,
            sizeof(service->custom_frame),
            &frame_length)) {
        service->stats.encode_errors++;
        return false;
    }

    service->custom_frame_length = frame_length;
    service->custom_pending = true;
    return true;
}

bool crsf_telemetry_service_process(
    CrsfTelemetryService *service,
    uint32_t now_ms
)
{
    if (service == NULL || !service->initialized) {
        return false;
    }

    service->stats.process_calls++;

    if (!service->device_info_pending && service->crsf_context != NULL) {
        uint8_t origin = 0U;

        if (crsf_protocol_take_device_ping(
                service->crsf_context,
                &origin)) {
            service->pending_device_destination = origin;
            service->device_info_pending = true;
            service->stats.ping_requests++;
        }
    }

    if (service->battery_valid && period_due(
            now_ms,
            service->last_battery_ms,
            service->config.battery_period_ms)) {
        service->battery_pending = true;
    }

    if (period_due(
            now_ms,
            service->last_heartbeat_ms,
            service->config.heartbeat_period_ms)) {
        service->heartbeat_pending = true;
    }

    if (service->flight_mode_valid && period_due(
            now_ms,
            service->last_flight_mode_ms,
            service->config.flight_mode_period_ms)) {
        service->flight_mode_pending = true;
    }

    if (radio_tx_is_busy(service->tx)) {
        service->stats.busy_deferrals++;
        return false;
    }

    size_t frame_length = 0U;

    if (service->device_info_pending) {
        if (!crsf_telemetry_encode_device_info(
                service->pending_device_destination,
                &service->config.device_info,
                service->tx_frame,
                sizeof(service->tx_frame),
                &frame_length)) {
            service->stats.encode_errors++;
            return false;
        }

        if (!service_start_frame(service, service->tx_frame, frame_length)) {
            return false;
        }

        service->device_info_pending = false;
        service->stats.device_info_frames++;
        return true;
    }

    if (service->custom_pending) {
        if (!service_start_frame(
                service,
                service->custom_frame,
                service->custom_frame_length)) {
            return false;
        }

        service->custom_pending = false;
        service->stats.custom_frames++;
        return true;
    }

    for (uint8_t attempt = 0U;
         attempt < CRSF_TELEMETRY_SCHEDULE_SLOTS;
         attempt++) {
        const uint8_t slot = service->round_robin_index;
        service->round_robin_index =
            (uint8_t)((service->round_robin_index + 1U) %
                CRSF_TELEMETRY_SCHEDULE_SLOTS);

        bool encoded = false;

        switch (slot) {
        case 0U:
            if (!service->battery_pending || !service->battery_valid) {
                continue;
            }

            encoded = crsf_telemetry_encode_battery(
                &service->battery,
                service->tx_frame,
                sizeof(service->tx_frame),
                &frame_length
            );
            break;

        case 1U:
            if (!service->heartbeat_pending) {
                continue;
            }

            encoded = crsf_telemetry_encode_heartbeat(
                CRSF_ADDRESS_FLIGHT_CONTROLLER,
                service->tx_frame,
                sizeof(service->tx_frame),
                &frame_length
            );
            break;

        case 2U:
            if (!service->flight_mode_pending ||
                !service->flight_mode_valid) {
                continue;
            }

            encoded = crsf_telemetry_encode_flight_mode(
                service->flight_mode,
                service->tx_frame,
                sizeof(service->tx_frame),
                &frame_length
            );
            break;

        default:
            continue;
        }

        if (!encoded) {
            service->stats.encode_errors++;
            return false;
        }

        if (!service_start_frame(service, service->tx_frame, frame_length)) {
            return false;
        }

        if (slot == 0U) {
            service->battery_pending = false;
            service->last_battery_ms = now_ms;
            service->stats.battery_frames++;
        } else if (slot == 1U) {
            service->heartbeat_pending = false;
            service->last_heartbeat_ms = now_ms;
            service->stats.heartbeat_frames++;
        } else {
            service->flight_mode_pending = false;
            service->last_flight_mode_ms = now_ms;
            service->stats.flight_mode_frames++;
        }

        return true;
    }

    return false;
}

void crsf_telemetry_service_get_stats(
    const CrsfTelemetryService *service,
    CrsfTelemetryServiceStats *out_stats
)
{
    if (service == NULL || out_stats == NULL) {
        return;
    }

    *out_stats = service->stats;
}
