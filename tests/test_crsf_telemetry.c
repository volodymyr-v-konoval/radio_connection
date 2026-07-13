#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crsf_crc.h"
#include "crsf_protocol.h"
#include "crsf_telemetry.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));   \
            return false;                        \
        }                                        \
    } while (0)

static bool frame_crc_is_valid(
    const uint8_t *frame,
    size_t length
)
{
    if (frame == NULL || length < 4U) {
        return false;
    }

    const uint8_t expected = frame[length - 1U];
    const uint8_t calculated = crsf_crc8_dvb_s2(
        &frame[2],
        length - 3U
    );

    return expected == calculated;
}

static bool test_battery_frame(void)
{
    const CrsfBatteryTelemetry battery = {
        .voltage_mv = 16800U,
        .current_ma = 1250U,
        .consumed_mah = 42U,
        .remaining_percent = 75U
    };

    uint8_t frame[CRSF_TELEMETRY_MAX_FRAME_SIZE];
    size_t length = 0U;

    TEST_ASSERT(
        crsf_telemetry_encode_battery(
            &battery,
            frame,
            sizeof(frame),
            &length),
        "Battery frame should encode"
    );

    TEST_ASSERT(length == 12U, "Battery frame length should be 12");
    TEST_ASSERT(frame[0] == 0xC8U, "Battery sync should be FC address");
    TEST_ASSERT(frame[1] == 0x0AU, "Battery length field should be 10");
    TEST_ASSERT(frame[2] == 0x08U, "Battery frame type should be 0x08");
    TEST_ASSERT(frame[3] == 0x00U && frame[4] == 0xA8U,
        "16800 mV should encode as 168 tenths of a volt");
    TEST_ASSERT(frame[5] == 0x00U && frame[6] == 0x0DU,
        "1250 mA should round to 13 tenths of an amp");
    TEST_ASSERT(frame[7] == 0x00U && frame[8] == 0x00U &&
        frame[9] == 0x2AU,
        "42 mAh should use the 24-bit capacity field");
    TEST_ASSERT(frame[10] == 75U, "Remaining percentage should encode");
    TEST_ASSERT(frame_crc_is_valid(frame, length), "Battery CRC should be valid");

    return true;
}

static bool test_heartbeat_and_flight_mode(void)
{
    uint8_t frame[CRSF_TELEMETRY_MAX_FRAME_SIZE];
    size_t length = 0U;

    TEST_ASSERT(
        crsf_telemetry_encode_heartbeat(
            CRSF_ADDRESS_FLIGHT_CONTROLLER,
            frame,
            sizeof(frame),
            &length),
        "Heartbeat should encode"
    );

    TEST_ASSERT(length == 6U, "Heartbeat total length should be 6");
    TEST_ASSERT(frame[1] == 4U && frame[2] == 0x0BU,
        "Heartbeat header should be correct");
    TEST_ASSERT(frame[3] == 0U && frame[4] == 0xC8U,
        "Heartbeat origin should be encoded as 16-bit FC address");
    TEST_ASSERT(frame_crc_is_valid(frame, length), "Heartbeat CRC should be valid");

    TEST_ASSERT(
        crsf_telemetry_encode_flight_mode(
            "BENCH",
            frame,
            sizeof(frame),
            &length),
        "Flight mode should encode"
    );

    TEST_ASSERT(frame[2] == 0x21U, "Flight mode type should be 0x21");
    TEST_ASSERT(memcmp(&frame[3], "BENCH", 5U) == 0,
        "Flight mode text should be copied");
    TEST_ASSERT(frame[8] == '\0', "Flight mode should be null terminated");
    TEST_ASSERT(frame_crc_is_valid(frame, length), "Flight mode CRC should be valid");

    return true;
}

static bool test_device_info(void)
{
    CrsfDeviceInfo info;
    memset(&info, 0, sizeof(info));
    (void)strncpy(info.name, "radio_connection", sizeof(info.name) - 1U);
    info.serial_number = 0x11223344UL;
    info.hardware_id = 0x55667788UL;
    info.firmware_id = 0x01020304UL;
    info.parameter_count = 0U;
    info.parameter_version = 1U;

    uint8_t frame[CRSF_TELEMETRY_MAX_FRAME_SIZE];
    size_t length = 0U;

    TEST_ASSERT(
        crsf_telemetry_encode_device_info(
            CRSF_ADDRESS_CRSF_RECEIVER,
            &info,
            frame,
            sizeof(frame),
            &length),
        "Device info should encode"
    );

    TEST_ASSERT(frame[2] == 0x29U, "Device info type should be 0x29");
    TEST_ASSERT(frame[3] == CRSF_ADDRESS_CRSF_RECEIVER,
        "Device info destination should match ping origin");
    TEST_ASSERT(frame[4] == CRSF_ADDRESS_FLIGHT_CONTROLLER,
        "Device info origin should be the flight controller");
    TEST_ASSERT(memcmp(&frame[5], "radio_connection", 16U) == 0,
        "Device name should be encoded");
    TEST_ASSERT(frame_crc_is_valid(frame, length), "Device info CRC should be valid");

    return true;
}

static bool test_custom_frame_limits(void)
{
    uint8_t payload[61] = { 0U };
    uint8_t frame[CRSF_TELEMETRY_MAX_FRAME_SIZE];
    size_t length = 0U;

    TEST_ASSERT(
        !crsf_telemetry_encode_broadcast(
            0x7FU,
            payload,
            sizeof(payload),
            frame,
            sizeof(frame),
            &length),
        "Oversized custom frame should be rejected"
    );

    TEST_ASSERT(
        !crsf_telemetry_encode_flight_mode(
            "THIS_MODE_NAME_IS_TOO_LONG",
            frame,
            sizeof(frame),
            &length),
        "Oversized flight mode should be rejected"
    );

    return true;
}

static bool test_parameter_ping_is_exposed(void)
{
    RadioProtocol protocol;
    CrsfProtocolContext context;

    crsf_protocol_init(&protocol, &context);

    uint8_t ping[] = {
        CRSF_ADDRESS_FLIGHT_CONTROLLER,
        4U,
        CRSF_FRAME_TYPE_PARAMETER_PING,
        CRSF_ADDRESS_FLIGHT_CONTROLLER,
        CRSF_ADDRESS_CRSF_RECEIVER,
        0U
    };

    ping[5] = crsf_crc8_dvb_s2(&ping[2], 3U);

    for (size_t i = 0U; i < sizeof(ping); i++) {
        (void)protocol.process_byte(
            &protocol,
            ping[i],
            (uint32_t)i
        );
    }

    uint8_t origin = 0U;

    TEST_ASSERT(
        crsf_protocol_take_device_ping(&context, &origin),
        "Valid parameter ping should become pending"
    );
    TEST_ASSERT(origin == CRSF_ADDRESS_CRSF_RECEIVER,
        "Ping origin should be exposed");
    TEST_ASSERT(
        !crsf_protocol_take_device_ping(&context, &origin),
        "Taking a ping should clear the event"
    );

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_battery_frame()) failures++;
    if (!test_heartbeat_and_flight_mode()) failures++;
    if (!test_device_info()) failures++;
    if (!test_custom_frame_limits()) failures++;
    if (!test_parameter_ping_is_exposed()) failures++;

    if (failures != 0) {
        printf("%d CRSF telemetry test(s) failed\n", failures);
        return 1;
    }

    printf("All CRSF telemetry tests passed\n");
    return 0;
}
