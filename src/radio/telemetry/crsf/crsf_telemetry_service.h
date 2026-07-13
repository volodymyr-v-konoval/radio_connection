#ifndef CRSF_TELEMETRY_SERVICE_H
#define CRSF_TELEMETRY_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crsf_protocol.h"
#include "crsf_telemetry.h"
#include "radio_tx_if.h"

#define CRSF_TELEMETRY_DEFAULT_BATTERY_PERIOD_MS     500U
#define CRSF_TELEMETRY_DEFAULT_HEARTBEAT_PERIOD_MS  1000U
#define CRSF_TELEMETRY_DEFAULT_FLIGHT_MODE_PERIOD_MS 2000U

typedef struct
{
    uint32_t battery_period_ms;
    uint32_t heartbeat_period_ms;
    uint32_t flight_mode_period_ms;
    CrsfDeviceInfo device_info;
} CrsfTelemetryServiceConfig;

typedef struct
{
    uint32_t process_calls;
    uint32_t frames_started;
    uint32_t battery_frames;
    uint32_t heartbeat_frames;
    uint32_t flight_mode_frames;
    uint32_t device_info_frames;
    uint32_t custom_frames;
    uint32_t busy_deferrals;
    uint32_t encode_errors;
    uint32_t write_errors;
    uint32_t ping_requests;
} CrsfTelemetryServiceStats;

typedef struct
{
    RadioTx *tx;
    CrsfProtocolContext *crsf_context;
    CrsfTelemetryServiceConfig config;

    CrsfBatteryTelemetry battery;
    char flight_mode[CRSF_TELEMETRY_FLIGHT_MODE_MAX + 1U];

    uint8_t tx_frame[CRSF_TELEMETRY_MAX_FRAME_SIZE];
    uint8_t custom_frame[CRSF_TELEMETRY_MAX_FRAME_SIZE];
    size_t custom_frame_length;

    uint32_t last_battery_ms;
    uint32_t last_heartbeat_ms;
    uint32_t last_flight_mode_ms;

    uint8_t pending_device_destination;
    uint8_t round_robin_index;

    bool initialized;
    bool battery_valid;
    bool flight_mode_valid;
    bool battery_pending;
    bool heartbeat_pending;
    bool flight_mode_pending;
    bool device_info_pending;
    bool custom_pending;

    CrsfTelemetryServiceStats stats;
} CrsfTelemetryService;

void crsf_telemetry_service_config_init(
    CrsfTelemetryServiceConfig *config
);

bool crsf_telemetry_service_init(
    CrsfTelemetryService *service,
    RadioTx *tx,
    CrsfProtocolContext *crsf_context,
    const CrsfTelemetryServiceConfig *config
);

void crsf_telemetry_service_set_battery(
    CrsfTelemetryService *service,
    const CrsfBatteryTelemetry *battery
);

bool crsf_telemetry_service_set_flight_mode(
    CrsfTelemetryService *service,
    const char *flight_mode
);

bool crsf_telemetry_service_queue_custom_broadcast(
    CrsfTelemetryService *service,
    uint8_t frame_type,
    const uint8_t *payload,
    size_t payload_length
);

/*
 * Sends at most one complete CRSF frame per call.
 * Returns true only when a transmission was successfully started.
 */
bool crsf_telemetry_service_process(
    CrsfTelemetryService *service,
    uint32_t now_ms
);

void crsf_telemetry_service_get_stats(
    const CrsfTelemetryService *service,
    CrsfTelemetryServiceStats *out_stats
);

#endif /* CRSF_TELEMETRY_SERVICE_H */
