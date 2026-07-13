#include "crsf_telemetry.h"

#include <limits.h>
#include <string.h>

#include "crsf_crc.h"

#define CRSF_FRAME_SYNC_INDEX                 0U
#define CRSF_FRAME_LENGTH_INDEX               1U
#define CRSF_FRAME_TYPE_INDEX                 2U
#define CRSF_STANDARD_HEADER_SIZE             3U
#define CRSF_EXTENDED_HEADER_SIZE             5U
#define CRSF_CRC_SIZE                         1U
#define CRSF_BATTERY_PAYLOAD_SIZE             8U
#define CRSF_HEARTBEAT_PAYLOAD_SIZE           2U
#define CRSF_DEVICE_INFO_FIXED_PAYLOAD_SIZE  14U
#define CRSF_CAPACITY_MAX               0xFFFFFFUL

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

static void write_u16_be(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value >> 8U);
    buffer[1] = (uint8_t)value;
}

static void write_u24_be(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 16U);
    buffer[1] = (uint8_t)(value >> 8U);
    buffer[2] = (uint8_t)value;
}

static void write_u32_be(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 24U);
    buffer[1] = (uint8_t)(value >> 16U);
    buffer[2] = (uint8_t)(value >> 8U);
    buffer[3] = (uint8_t)value;
}

static uint16_t rounded_tenth_units(uint32_t milli_units)
{
    uint32_t value = (milli_units + 50U) / 100U;

    if (value > UINT16_MAX) {
        value = UINT16_MAX;
    }

    return (uint16_t)value;
}

bool crsf_telemetry_encode_broadcast(
    uint8_t frame_type,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
)
{
    if (out_frame == NULL || out_length == NULL ||
        (payload_length > 0U && payload == NULL)) {
        return false;
    }

    const size_t frame_length_field =
        1U + payload_length + CRSF_CRC_SIZE;
    const size_t total_length = frame_length_field + 2U;

    if (frame_length_field > UINT8_MAX ||
        total_length > CRSF_TELEMETRY_MAX_FRAME_SIZE ||
        total_length > out_capacity) {
        return false;
    }

    out_frame[CRSF_FRAME_SYNC_INDEX] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    out_frame[CRSF_FRAME_LENGTH_INDEX] = (uint8_t)frame_length_field;
    out_frame[CRSF_FRAME_TYPE_INDEX] = frame_type;

    if (payload_length > 0U) {
        memcpy(
            &out_frame[CRSF_STANDARD_HEADER_SIZE],
            payload,
            payload_length
        );
    }

    out_frame[total_length - 1U] = crsf_crc8_dvb_s2(
        &out_frame[CRSF_FRAME_TYPE_INDEX],
        1U + payload_length
    );

    *out_length = total_length;
    return true;
}

bool crsf_telemetry_encode_extended(
    uint8_t frame_type,
    uint8_t destination,
    uint8_t origin,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
)
{
    if (out_frame == NULL || out_length == NULL ||
        (payload_length > 0U && payload == NULL)) {
        return false;
    }

    const size_t frame_length_field =
        3U + payload_length + CRSF_CRC_SIZE;
    const size_t total_length = frame_length_field + 2U;

    if (frame_length_field > UINT8_MAX ||
        total_length > CRSF_TELEMETRY_MAX_FRAME_SIZE ||
        total_length > out_capacity) {
        return false;
    }

    out_frame[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    out_frame[1] = (uint8_t)frame_length_field;
    out_frame[2] = frame_type;
    out_frame[3] = destination;
    out_frame[4] = origin;

    if (payload_length > 0U) {
        memcpy(
            &out_frame[CRSF_EXTENDED_HEADER_SIZE],
            payload,
            payload_length
        );
    }

    out_frame[total_length - 1U] = crsf_crc8_dvb_s2(
        &out_frame[CRSF_FRAME_TYPE_INDEX],
        3U + payload_length
    );

    *out_length = total_length;
    return true;
}

bool crsf_telemetry_encode_battery(
    const CrsfBatteryTelemetry *battery,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
)
{
    if (battery == NULL) {
        return false;
    }

    uint8_t payload[CRSF_BATTERY_PAYLOAD_SIZE];

    write_u16_be(
        &payload[0],
        rounded_tenth_units(battery->voltage_mv)
    );

    write_u16_be(
        &payload[2],
        rounded_tenth_units(battery->current_ma)
    );

    uint32_t consumed_mah = battery->consumed_mah;
    if (consumed_mah > CRSF_CAPACITY_MAX) {
        consumed_mah = CRSF_CAPACITY_MAX;
    }

    write_u24_be(&payload[4], consumed_mah);

    payload[7] = battery->remaining_percent > 100U
        ? 100U
        : battery->remaining_percent;

    return crsf_telemetry_encode_broadcast(
        CRSF_FRAME_TYPE_BATTERY_SENSOR,
        payload,
        sizeof(payload),
        out_frame,
        out_capacity,
        out_length
    );
}

bool crsf_telemetry_encode_heartbeat(
    uint8_t origin,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
)
{
    uint8_t payload[CRSF_HEARTBEAT_PAYLOAD_SIZE] = {
        0U,
        origin
    };

    return crsf_telemetry_encode_broadcast(
        CRSF_FRAME_TYPE_HEARTBEAT,
        payload,
        sizeof(payload),
        out_frame,
        out_capacity,
        out_length
    );
}

bool crsf_telemetry_encode_flight_mode(
    const char *flight_mode,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
)
{
    if (flight_mode == NULL) {
        return false;
    }

    const size_t length = bounded_string_length(
        flight_mode,
        CRSF_TELEMETRY_FLIGHT_MODE_MAX + 1U
    );

    if (length == 0U || length > CRSF_TELEMETRY_FLIGHT_MODE_MAX) {
        return false;
    }

    uint8_t payload[CRSF_TELEMETRY_FLIGHT_MODE_MAX + 1U];
    memcpy(payload, flight_mode, length);
    payload[length] = '\0';

    return crsf_telemetry_encode_broadcast(
        CRSF_FRAME_TYPE_FLIGHT_MODE,
        payload,
        length + 1U,
        out_frame,
        out_capacity,
        out_length
    );
}

bool crsf_telemetry_encode_device_info(
    uint8_t destination,
    const CrsfDeviceInfo *device_info,
    uint8_t *out_frame,
    size_t out_capacity,
    size_t *out_length
)
{
    if (device_info == NULL) {
        return false;
    }

    const size_t name_length = bounded_string_length(
        device_info->name,
        sizeof(device_info->name)
    );

    if (name_length == 0U ||
        name_length > CRSF_TELEMETRY_DEVICE_NAME_MAX) {
        return false;
    }

    uint8_t payload[
        CRSF_TELEMETRY_DEVICE_NAME_MAX + 1U +
        CRSF_DEVICE_INFO_FIXED_PAYLOAD_SIZE
    ];

    memcpy(payload, device_info->name, name_length);
    payload[name_length] = '\0';

    size_t offset = name_length + 1U;
    write_u32_be(&payload[offset], device_info->serial_number);
    offset += 4U;
    write_u32_be(&payload[offset], device_info->hardware_id);
    offset += 4U;
    write_u32_be(&payload[offset], device_info->firmware_id);
    offset += 4U;
    payload[offset++] = device_info->parameter_count;
    payload[offset++] = device_info->parameter_version;

    return crsf_telemetry_encode_extended(
        CRSF_FRAME_TYPE_DEVICE_INFO,
        destination,
        CRSF_ADDRESS_FLIGHT_CONTROLLER,
        payload,
        offset,
        out_frame,
        out_capacity,
        out_length
    );
}
