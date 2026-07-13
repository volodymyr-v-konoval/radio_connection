#ifndef CRSF_TELEMETRY_H
#define CRSF_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRSF_TELEMETRY_MAX_FRAME_SIZE       64U
#define CRSF_TELEMETRY_DEVICE_NAME_MAX      31U
#define CRSF_TELEMETRY_FLIGHT_MODE_MAX      15U

#define CRSF_ADDRESS_BROADCAST               0x00U
#define CRSF_ADDRESS_FLIGHT_CONTROLLER       0xC8U
#define CRSF_ADDRESS_RADIO_TRANSMITTER       0xEAU
#define CRSF_ADDRESS_CRSF_RECEIVER           0xECU
#define CRSF_ADDRESS_CRSF_TRANSMITTER        0xEEU

#define CRSF_FRAME_TYPE_BATTERY_SENSOR       0x08U
#define CRSF_FRAME_TYPE_HEARTBEAT            0x0BU
#define CRSF_FRAME_TYPE_FLIGHT_MODE          0x21U
#define CRSF_FRAME_TYPE_PARAMETER_PING       0x28U
#define CRSF_FRAME_TYPE_DEVICE_INFO          0x29U

typedef struct
{
    uint32_t voltage_mv;
    uint32_t current_ma;
    uint32_t consumed_mah;
    uint8_t remaining_percent;
} CrsfBatteryTelemetry;

typedef struct
{
    char name[CRSF_TELEMETRY_DEVICE_NAME_MAX + 1U];
    uint32_t serial_number;
    uint32_t hardware_id;
    uint32_t firmware_id;
    uint8_t parameter_count;
    uint8_t parameter_version;
} CrsfDeviceInfo;

bool crsf_telemetry_encode_broadcast(
    uint8_t frame_type,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
);

bool crsf_telemetry_encode_extended(
    uint8_t frame_type,
    uint8_t destination,
    uint8_t origin,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
);

bool crsf_telemetry_encode_battery(
    const CrsfBatteryTelemetry *battery,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
);

bool crsf_telemetry_encode_heartbeat(
    uint8_t origin,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
);

bool crsf_telemetry_encode_flight_mode(
    const char *flight_mode,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
);

bool crsf_telemetry_encode_device_info(
    uint8_t destination,
    const CrsfDeviceInfo *device_info,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
);

#endif /* CRSF_TELEMETRY_H */
