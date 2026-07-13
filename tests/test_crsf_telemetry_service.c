#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crsf_crc.h"
#include "crsf_protocol.h"
#include "crsf_telemetry_service.h"
#include "fake_radio_tx.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));   \
            return false;                        \
        }                                        \
    } while (0)

static void init_service(
    CrsfTelemetryService *service,
    RadioTx *tx,
    FakeRadioTxContext *tx_context,
    CrsfProtocolContext *crsf_context
)
{
    CrsfTelemetryServiceConfig config;
    crsf_telemetry_service_config_init(&config);

    (void)fake_radio_tx_init(tx, tx_context);
    (void)crsf_telemetry_service_init(
        service,
        tx,
        crsf_context,
        &config
    );
}

static bool test_periodic_frames_and_busy_deferral(void)
{
    CrsfTelemetryService service;
    RadioTx tx;
    FakeRadioTxContext tx_context;
    CrsfProtocolContext crsf_context;
    memset(&crsf_context, 0, sizeof(crsf_context));

    init_service(&service, &tx, &tx_context, &crsf_context);

    const CrsfBatteryTelemetry battery = {
        .voltage_mv = 24000U,
        .current_ma = 1500U,
        .consumed_mah = 123U,
        .remaining_percent = 80U
    };

    crsf_telemetry_service_set_battery(&service, &battery);
    TEST_ASSERT(
        crsf_telemetry_service_set_flight_mode(&service, "BENCH"),
        "Flight mode should be accepted"
    );

    TEST_ASSERT(
        crsf_telemetry_service_process(&service, 0U),
        "Initial pending frame should start"
    );
    TEST_ASSERT(tx_context.frame[2] == CRSF_FRAME_TYPE_BATTERY_SENSOR,
        "Round-robin should start with battery data");

    TEST_ASSERT(
        !crsf_telemetry_service_process(&service, 1U),
        "Busy transmitter should defer the next frame"
    );

    fake_radio_tx_complete(&tx_context);

    TEST_ASSERT(
        crsf_telemetry_service_process(&service, 1U),
        "Heartbeat should start after completion"
    );
    TEST_ASSERT(tx_context.frame[2] == CRSF_FRAME_TYPE_HEARTBEAT,
        "Second scheduled frame should be heartbeat");

    fake_radio_tx_complete(&tx_context);

    TEST_ASSERT(
        crsf_telemetry_service_process(&service, 2U),
        "Flight mode should start after heartbeat"
    );
    TEST_ASSERT(tx_context.frame[2] == CRSF_FRAME_TYPE_FLIGHT_MODE,
        "Third scheduled frame should be flight mode");

    fake_radio_tx_complete(&tx_context);

    TEST_ASSERT(
        !crsf_telemetry_service_process(&service, 100U),
        "No frame should be due before its period"
    );

    TEST_ASSERT(
        crsf_telemetry_service_process(&service, 500U),
        "Battery should become due after 500 ms"
    );
    TEST_ASSERT(tx_context.frame[2] == CRSF_FRAME_TYPE_BATTERY_SENSOR,
        "Due battery frame should be transmitted");

    CrsfTelemetryServiceStats stats;
    crsf_telemetry_service_get_stats(&service, &stats);

    TEST_ASSERT(stats.frames_started == 4U,
        "Four frames should have started");
    TEST_ASSERT(stats.battery_frames == 2U,
        "Two battery frames should have started");
    TEST_ASSERT(stats.busy_deferrals == 1U,
        "One busy deferral should be counted");

    return true;
}

static bool test_ping_response_has_priority(void)
{
    CrsfTelemetryService service;
    RadioTx tx;
    FakeRadioTxContext tx_context;
    CrsfProtocolContext crsf_context;
    memset(&crsf_context, 0, sizeof(crsf_context));

    init_service(&service, &tx, &tx_context, &crsf_context);

    crsf_context.device_ping_pending = true;
    crsf_context.device_ping_origin = CRSF_ADDRESS_CRSF_RECEIVER;

    TEST_ASSERT(
        crsf_telemetry_service_process(&service, 0U),
        "Device info response should start"
    );

    TEST_ASSERT(tx_context.frame[2] == CRSF_FRAME_TYPE_DEVICE_INFO,
        "Ping should produce device info");
    TEST_ASSERT(tx_context.frame[3] == CRSF_ADDRESS_CRSF_RECEIVER,
        "Device info should target the ping origin");

    CrsfTelemetryServiceStats stats;
    crsf_telemetry_service_get_stats(&service, &stats);

    TEST_ASSERT(stats.ping_requests == 1U,
        "Ping request should be counted");
    TEST_ASSERT(stats.device_info_frames == 1U,
        "Device info frame should be counted");

    return true;
}

static bool test_custom_frame_overwrites_pending_value(void)
{
    CrsfTelemetryService service;
    RadioTx tx;
    FakeRadioTxContext tx_context;
    CrsfProtocolContext crsf_context;
    memset(&crsf_context, 0, sizeof(crsf_context));

    init_service(&service, &tx, &tx_context, &crsf_context);

    const uint8_t first_payload[] = { 1U, 2U };
    const uint8_t second_payload[] = { 9U, 8U, 7U };

    TEST_ASSERT(
        crsf_telemetry_service_queue_custom_broadcast(
            &service,
            0x7EU,
            first_payload,
            sizeof(first_payload)),
        "First custom frame should queue"
    );

    TEST_ASSERT(
        crsf_telemetry_service_queue_custom_broadcast(
            &service,
            0x7DU,
            second_payload,
            sizeof(second_payload)),
        "Newest custom frame should replace the pending one"
    );

    TEST_ASSERT(
        crsf_telemetry_service_process(&service, 0U),
        "Custom frame should start"
    );

    TEST_ASSERT(tx_context.frame[2] == 0x7DU,
        "Newest custom frame should be transmitted");
    TEST_ASSERT(tx_context.frame[3] == 9U,
        "Newest custom payload should be transmitted");

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_periodic_frames_and_busy_deferral()) failures++;
    if (!test_ping_response_has_priority()) failures++;
    if (!test_custom_frame_overwrites_pending_value()) failures++;

    if (failures != 0) {
        printf("%d CRSF telemetry service test(s) failed\n", failures);
        return 1;
    }

    printf("All CRSF telemetry service tests passed\n");
    return 0;
}
