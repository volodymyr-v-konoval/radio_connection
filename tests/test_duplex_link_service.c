#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "duplex_link_service.h"
#include "fake_packet_radio.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));    \
            return false;                        \
        }                                        \
    } while (0)

typedef struct
{
    uint32_t now_ms;
} FakeTimeContext;

static uint32_t fake_time_now_ms(
    RadioTime *self
)
{
    const FakeTimeContext *context =
        (const FakeTimeContext *)self->context;
    return context->now_ms;
}

static uint32_t fake_time_elapsed_ms(
    RadioTime *self,
    uint32_t since_ms
)
{
    const FakeTimeContext *context =
        (const FakeTimeContext *)self->context;
    return context->now_ms - since_ms;
}

static void fake_time_init(
    RadioTime *time,
    FakeTimeContext *context
)
{
    memset(context, 0, sizeof(*context));
    time->context = context;
    time->now_ms = fake_time_now_ms;
    time->elapsed_ms = fake_time_elapsed_ms;
}

static void fake_time_advance(
    FakeTimeContext *context,
    uint32_t delta_ms
)
{
    context->now_ms += delta_ms;
}

static DuplexLinkConfig make_config(
    DuplexLinkRole role,
    uint8_t local_node_id,
    uint8_t peer_node_id,
    uint16_t initial_sequence
)
{
    const DuplexLinkConfig config = {
        .role = role,
        .local_node_id = local_node_id,
        .peer_node_id = peer_node_id,
        .initial_sequence = initial_sequence,
        .response_timeout_ms = 20U,
        .operation_timeout_ms = 10U,
        .max_retries = 1U
    };

    return config;
}

static void fill_payload(
    uint8_t *payload,
    uint8_t first_value
)
{
    for (size_t i = 0U; i < LINK_PACKET_PAYLOAD_SIZE; i++) {
        payload[i] = (uint8_t)(first_value + (uint8_t)i);
    }
}

static bool build_response_frame(
    LinkMessageType message_type,
    uint8_t source_id,
    uint8_t destination_id,
    uint16_t sequence,
    uint16_t acknowledged_sequence,
    const uint8_t *payload,
    uint8_t *encoded
)
{
    LinkPacket response = {
        .message_type = message_type,
        .source_id = source_id,
        .destination_id = destination_id,
        .sequence = sequence,
        .acknowledged_sequence = acknowledged_sequence,
        .payload_length = LINK_PACKET_PAYLOAD_SIZE,
        .flags = LINK_PACKET_FLAG_NONE
    };

    memcpy(
        response.payload,
        payload,
        LINK_PACKET_PAYLOAD_SIZE
    );

    return link_packet_encode(
        &response,
        encoded,
        LINK_PACKET_ENCODED_SIZE
    ) == LINK_PACKET_STATUS_OK;
}

static bool test_data_ack_exchange(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio initiator_radio;
    PacketRadio responder_radio;
    FakePacketRadioContext initiator_radio_context;
    FakePacketRadioContext responder_radio_context;

    TEST_ASSERT(
        fake_packet_radio_init(
            &initiator_radio,
            &initiator_radio_context
        ),
        "Initiator radio should initialize"
    );
    TEST_ASSERT(
        fake_packet_radio_init(
            &responder_radio,
            &responder_radio_context
        ),
        "Responder radio should initialize"
    );

    DuplexLinkService initiator;
    DuplexLinkService responder;
    const DuplexLinkConfig initiator_config = make_config(
        DUPLEX_LINK_ROLE_INITIATOR,
        1U,
        2U,
        10U
    );
    const DuplexLinkConfig responder_config = make_config(
        DUPLEX_LINK_ROLE_RESPONDER,
        2U,
        1U,
        100U
    );

    TEST_ASSERT(
        duplex_link_service_init(
            &initiator,
            &initiator_radio,
            NULL,
            &time,
            &initiator_config
        ),
        "Initiator service should initialize"
    );
    TEST_ASSERT(
        duplex_link_service_init(
            &responder,
            &responder_radio,
            NULL,
            &time,
            &responder_config
        ),
        "Responder service should initialize"
    );

    uint8_t request_payload[LINK_PACKET_PAYLOAD_SIZE];
    uint8_t response_payload[LINK_PACKET_PAYLOAD_SIZE];
    fill_payload(request_payload, 0x10U);
    fill_payload(response_payload, 0x80U);

    TEST_ASSERT(
        duplex_link_service_set_response_payload(
            &responder,
            response_payload,
            sizeof(response_payload)
        ),
        "Responder payload should be configured"
    );

    duplex_link_service_process(&responder);
    TEST_ASSERT(
        responder_radio_context.state == PACKET_RADIO_STATE_RX,
        "Responder should enter continuous RX"
    );
    TEST_ASSERT(
        responder_radio_context.last_rx_timeout_ms == 0U,
        "Responder RX should be continuous"
    );

    TEST_ASSERT(
        duplex_link_service_start_data(
            &initiator,
            request_payload,
            sizeof(request_payload)
        ),
        "DATA request should start"
    );
    duplex_link_service_process(&initiator);

    TEST_ASSERT(
        initiator_radio_context.state == PACKET_RADIO_STATE_TX,
        "Initiator should start request TX"
    );
    TEST_ASSERT(
        initiator_radio_context.last_tx_length ==
            LINK_PACKET_ENCODED_SIZE,
        "Request frame should be 30 bytes"
    );

    uint8_t request_frame[LINK_PACKET_ENCODED_SIZE];
    memcpy(
        request_frame,
        initiator_radio_context.last_tx_frame,
        sizeof(request_frame)
    );

    LinkPacket decoded_request;
    TEST_ASSERT(
        link_packet_decode(
            request_frame,
            sizeof(request_frame),
            2U,
            &decoded_request
        ) == LINK_PACKET_STATUS_OK,
        "Request frame should decode"
    );
    TEST_ASSERT(
        decoded_request.message_type == LINK_MESSAGE_TYPE_DATA &&
        decoded_request.sequence == 10U,
        "Request should contain DATA and sequence 10"
    );

    TEST_ASSERT(
        fake_packet_radio_complete_tx(
            &initiator_radio_context
        ),
        "Initiator TX should complete"
    );
    duplex_link_service_process(&initiator);
    TEST_ASSERT(
        initiator_radio_context.state == PACKET_RADIO_STATE_RX,
        "Initiator should open response RX window"
    );

    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &responder_radio_context,
            request_frame,
            sizeof(request_frame),
            -120,
            24
        ),
        "Responder should receive request"
    );
    duplex_link_service_process(&responder);

    DuplexLinkEvent responder_event;
    TEST_ASSERT(
        duplex_link_service_take_event(
            &responder,
            &responder_event
        ),
        "Responder should publish request event"
    );
    TEST_ASSERT(
        responder_event.type ==
            DUPLEX_LINK_EVENT_REQUEST_RECEIVED &&
        responder_event.packet.sequence == 10U,
        "Responder should deliver request once"
    );
    TEST_ASSERT(
        responder_radio_context.state == PACKET_RADIO_STATE_TX,
        "Responder should start ACK TX"
    );

    uint8_t response_frame[LINK_PACKET_ENCODED_SIZE];
    memcpy(
        response_frame,
        responder_radio_context.last_tx_frame,
        sizeof(response_frame)
    );

    LinkPacket decoded_response;
    TEST_ASSERT(
        link_packet_decode(
            response_frame,
            sizeof(response_frame),
            1U,
            &decoded_response
        ) == LINK_PACKET_STATUS_OK,
        "Response frame should decode"
    );
    TEST_ASSERT(
        decoded_response.message_type == LINK_MESSAGE_TYPE_ACK &&
        decoded_response.acknowledged_sequence == 10U &&
        decoded_response.payload_length == LINK_PACKET_PAYLOAD_SIZE,
        "Responder should create ACK with 16-byte payload"
    );
    TEST_ASSERT(
        memcmp(
            decoded_response.payload,
            response_payload,
            sizeof(response_payload)
        ) == 0,
        "ACK payload should match configured response"
    );

    fake_time_advance(&time_context, 17U);

    TEST_ASSERT(
        fake_packet_radio_complete_tx(
            &responder_radio_context
        ),
        "Responder TX should complete"
    );
    duplex_link_service_process(&responder);
    TEST_ASSERT(
        responder_radio_context.state == PACKET_RADIO_STATE_RX,
        "Responder should return to RX"
    );

    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &initiator_radio_context,
            response_frame,
            sizeof(response_frame),
            -171,
            29
        ),
        "Initiator should receive ACK"
    );
    duplex_link_service_process(&initiator);

    DuplexLinkEvent initiator_event;
    TEST_ASSERT(
        duplex_link_service_take_event(
            &initiator,
            &initiator_event
        ),
        "Initiator should publish success event"
    );
    TEST_ASSERT(
        initiator_event.type ==
            DUPLEX_LINK_EVENT_EXCHANGE_SUCCEEDED &&
        initiator_event.packet.message_type == LINK_MESSAGE_TYPE_ACK &&
        initiator_event.packet.acknowledged_sequence == 10U,
        "Initiator should accept matching ACK"
    );
    TEST_ASSERT(
        initiator_event.attempts == 1U &&
        initiator_event.rtt_ms == 17U &&
        initiator_event.rssi_dbm_x2 == -171 &&
        initiator_event.snr_db_x4 == 29,
        "Success diagnostics should include attempts, RTT, RSSI and SNR"
    );
    TEST_ASSERT(
        duplex_link_service_get_state(&initiator) ==
            DUPLEX_LINK_STATE_IDLE,
        "Initiator should return idle after success"
    );

    return true;
}

static bool test_ping_pong_exchange(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio radio;
    FakePacketRadioContext radio_context;
    TEST_ASSERT(
        fake_packet_radio_init(&radio, &radio_context),
        "Radio should initialize"
    );

    DuplexLinkService service;
    DuplexLinkConfig config = make_config(
        DUPLEX_LINK_ROLE_INITIATOR,
        1U,
        2U,
        50U
    );

    TEST_ASSERT(
        duplex_link_service_init(
            &service,
            &radio,
            NULL,
            &time,
            &config
        ),
        "Service should initialize"
    );

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE];
    uint8_t pong_payload[LINK_PACKET_PAYLOAD_SIZE];
    fill_payload(payload, 0x21U);
    fill_payload(pong_payload, 0x61U);

    TEST_ASSERT(
        duplex_link_service_start_ping(
            &service,
            payload,
            sizeof(payload)
        ),
        "PING should start"
    );
    duplex_link_service_process(&service);

    LinkPacket ping;
    TEST_ASSERT(
        link_packet_decode(
            radio_context.last_tx_frame,
            radio_context.last_tx_length,
            2U,
            &ping
        ) == LINK_PACKET_STATUS_OK &&
        ping.message_type == LINK_MESSAGE_TYPE_PING,
        "Outgoing request should be PING"
    );

    TEST_ASSERT(
        fake_packet_radio_complete_tx(&radio_context),
        "PING TX should complete"
    );
    duplex_link_service_process(&service);

    uint8_t pong_frame[LINK_PACKET_ENCODED_SIZE];
    TEST_ASSERT(
        build_response_frame(
            LINK_MESSAGE_TYPE_PONG,
            2U,
            1U,
            90U,
            ping.sequence,
            pong_payload,
            pong_frame
        ),
        "PONG frame should build"
    );

    fake_time_advance(&time_context, 5U);
    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &radio_context,
            pong_frame,
            sizeof(pong_frame),
            -100,
            12
        ),
        "PONG should be delivered"
    );
    duplex_link_service_process(&service);

    DuplexLinkEvent event;
    TEST_ASSERT(
        duplex_link_service_take_event(&service, &event) &&
        event.type == DUPLEX_LINK_EVENT_EXCHANGE_SUCCEEDED &&
        event.packet.message_type == LINK_MESSAGE_TYPE_PONG,
        "Matching PONG should complete exchange"
    );

    return true;
}

static bool test_timeout_retry_and_failure(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio radio;
    FakePacketRadioContext radio_context;
    TEST_ASSERT(
        fake_packet_radio_init(&radio, &radio_context),
        "Radio should initialize"
    );

    DuplexLinkService service;
    DuplexLinkConfig config = make_config(
        DUPLEX_LINK_ROLE_INITIATOR,
        1U,
        2U,
        7U
    );

    TEST_ASSERT(
        duplex_link_service_init(
            &service,
            &radio,
            NULL,
            &time,
            &config
        ),
        "Service should initialize"
    );

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE];
    fill_payload(payload, 0x30U);

    TEST_ASSERT(
        duplex_link_service_start_data(
            &service,
            payload,
            sizeof(payload)
        ),
        "DATA should start"
    );
    duplex_link_service_process(&service);

    uint8_t first_frame[LINK_PACKET_ENCODED_SIZE];
    memcpy(
        first_frame,
        radio_context.last_tx_frame,
        sizeof(first_frame)
    );

    TEST_ASSERT(
        fake_packet_radio_complete_tx(&radio_context),
        "First TX should complete"
    );
    duplex_link_service_process(&service);

    fake_time_advance(&time_context, 20U);
    duplex_link_service_process(&service);

    TEST_ASSERT(
        radio_context.state == PACKET_RADIO_STATE_TX,
        "Timeout should start retry TX"
    );
    TEST_ASSERT(
        memcmp(
            first_frame,
            radio_context.last_tx_frame,
            sizeof(first_frame)
        ) == 0,
        "Retry must use the same encoded packet and sequence"
    );

    const DuplexLinkStats *stats =
        duplex_link_service_get_stats(&service);
    TEST_ASSERT(
        stats != NULL && stats->retries == 1U &&
        stats->timeouts == 1U,
        "First timeout should count one retry"
    );

    TEST_ASSERT(
        fake_packet_radio_complete_tx(&radio_context),
        "Retry TX should complete"
    );
    duplex_link_service_process(&service);

    fake_time_advance(&time_context, 20U);
    duplex_link_service_process(&service);

    DuplexLinkEvent event;
    TEST_ASSERT(
        duplex_link_service_take_event(&service, &event) &&
        event.type == DUPLEX_LINK_EVENT_EXCHANGE_FAILED &&
        event.packet.sequence == 7U &&
        event.attempts == 2U,
        "Second timeout should fail after two attempts"
    );
    TEST_ASSERT(
        duplex_link_service_get_state(&service) ==
            DUPLEX_LINK_STATE_IDLE,
        "Failed exchange should return idle"
    );

    return true;
}

static bool test_duplicate_is_suppressed_but_ack_is_repeated(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio radio;
    FakePacketRadioContext radio_context;
    TEST_ASSERT(
        fake_packet_radio_init(&radio, &radio_context),
        "Radio should initialize"
    );

    DuplexLinkService service;
    DuplexLinkConfig config = make_config(
        DUPLEX_LINK_ROLE_RESPONDER,
        2U,
        1U,
        200U
    );

    TEST_ASSERT(
        duplex_link_service_init(
            &service,
            &radio,
            NULL,
            &time,
            &config
        ),
        "Responder should initialize"
    );
    duplex_link_service_process(&service);

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE];
    fill_payload(payload, 0x40U);

    LinkPacket request = {
        .message_type = LINK_MESSAGE_TYPE_DATA,
        .source_id = 1U,
        .destination_id = 2U,
        .sequence = 42U,
        .acknowledged_sequence = 0U,
        .payload_length = LINK_PACKET_PAYLOAD_SIZE,
        .flags = LINK_PACKET_FLAG_NONE
    };
    memcpy(request.payload, payload, sizeof(payload));

    uint8_t request_frame[LINK_PACKET_ENCODED_SIZE];
    TEST_ASSERT(
        link_packet_encode(
            &request,
            request_frame,
            sizeof(request_frame)
        ) == LINK_PACKET_STATUS_OK,
        "Request should encode"
    );

    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &radio_context,
            request_frame,
            sizeof(request_frame),
            -90,
            16
        ),
        "First request should arrive"
    );
    duplex_link_service_process(&service);

    DuplexLinkEvent event;
    TEST_ASSERT(
        duplex_link_service_take_event(&service, &event) &&
        event.type == DUPLEX_LINK_EVENT_REQUEST_RECEIVED,
        "First request should be delivered to application"
    );

    uint8_t first_ack[LINK_PACKET_ENCODED_SIZE];
    memcpy(first_ack, radio_context.last_tx_frame, sizeof(first_ack));

    TEST_ASSERT(
        fake_packet_radio_complete_tx(&radio_context),
        "First ACK should complete"
    );
    duplex_link_service_process(&service);

    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &radio_context,
            request_frame,
            sizeof(request_frame),
            -92,
            15
        ),
        "Duplicate request should arrive"
    );
    duplex_link_service_process(&service);

    TEST_ASSERT(
        duplex_link_service_take_event(&service, &event) &&
        event.type == DUPLEX_LINK_EVENT_DUPLICATE_SUPPRESSED &&
        event.packet.sequence == 42U,
        "Duplicate should be suppressed"
    );
    TEST_ASSERT(
        !duplex_link_service_take_event(&service, &event),
        "Duplicate must not produce a second request event"
    );
    TEST_ASSERT(
        radio_context.state == PACKET_RADIO_STATE_TX &&
        memcmp(
            first_ack,
            radio_context.last_tx_frame,
            sizeof(first_ack)
        ) == 0,
        "Duplicate should retransmit the cached ACK"
    );

    const DuplexLinkStats *stats =
        duplex_link_service_get_stats(&service);
    TEST_ASSERT(
        stats != NULL && stats->duplicate_packets == 1U,
        "Duplicate counter should increment"
    );

    return true;
}

static bool test_wrong_ack_is_rejected_and_retried(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio radio;
    FakePacketRadioContext radio_context;
    TEST_ASSERT(
        fake_packet_radio_init(&radio, &radio_context),
        "Radio should initialize"
    );

    DuplexLinkService service;
    DuplexLinkConfig config = make_config(
        DUPLEX_LINK_ROLE_INITIATOR,
        1U,
        2U,
        15U
    );

    TEST_ASSERT(
        duplex_link_service_init(
            &service,
            &radio,
            NULL,
            &time,
            &config
        ),
        "Service should initialize"
    );

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE];
    fill_payload(payload, 0x50U);

    TEST_ASSERT(
        duplex_link_service_start_data(
            &service,
            payload,
            sizeof(payload)
        ),
        "Request should start"
    );
    duplex_link_service_process(&service);
    TEST_ASSERT(
        fake_packet_radio_complete_tx(&radio_context),
        "Request TX should complete"
    );
    duplex_link_service_process(&service);

    uint8_t ack_frame[LINK_PACKET_ENCODED_SIZE];
    TEST_ASSERT(
        build_response_frame(
            LINK_MESSAGE_TYPE_ACK,
            2U,
            1U,
            99U,
            14U,
            payload,
            ack_frame
        ),
        "Wrong ACK should build"
    );
    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &radio_context,
            ack_frame,
            sizeof(ack_frame),
            -110,
            8
        ),
        "Wrong ACK should arrive"
    );
    duplex_link_service_process(&service);

    DuplexLinkEvent event;
    TEST_ASSERT(
        duplex_link_service_take_event(&service, &event) &&
        event.type == DUPLEX_LINK_EVENT_INVALID_PACKET &&
        event.invalid_reason ==
            DUPLEX_LINK_INVALID_ACK_SEQUENCE,
        "Wrong ACK sequence should be diagnosed"
    );
    TEST_ASSERT(
        radio_context.state == PACKET_RADIO_STATE_TX,
        "Wrong ACK should trigger retry"
    );

    return true;
}

static bool test_sequence_rollover(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio radio;
    FakePacketRadioContext radio_context;
    TEST_ASSERT(
        fake_packet_radio_init(&radio, &radio_context),
        "Radio should initialize"
    );

    DuplexLinkService service;
    DuplexLinkConfig config = make_config(
        DUPLEX_LINK_ROLE_INITIATOR,
        1U,
        2U,
        UINT16_MAX
    );

    TEST_ASSERT(
        duplex_link_service_init(
            &service,
            &radio,
            NULL,
            &time,
            &config
        ),
        "Service should initialize"
    );

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE];
    fill_payload(payload, 0x60U);

    TEST_ASSERT(
        duplex_link_service_start_data(
            &service,
            payload,
            sizeof(payload)
        ),
        "First request should start"
    );
    duplex_link_service_process(&service);

    LinkPacket request;
    TEST_ASSERT(
        link_packet_decode(
            radio_context.last_tx_frame,
            radio_context.last_tx_length,
            2U,
            &request
        ) == LINK_PACKET_STATUS_OK &&
        request.sequence == UINT16_MAX,
        "First request should use sequence 65535"
    );

    TEST_ASSERT(
        fake_packet_radio_complete_tx(&radio_context),
        "First request TX should complete"
    );
    duplex_link_service_process(&service);

    uint8_t ack_frame[LINK_PACKET_ENCODED_SIZE];
    TEST_ASSERT(
        build_response_frame(
            LINK_MESSAGE_TYPE_ACK,
            2U,
            1U,
            10U,
            UINT16_MAX,
            payload,
            ack_frame
        ),
        "ACK should build"
    );
    TEST_ASSERT(
        fake_packet_radio_deliver_rx(
            &radio_context,
            ack_frame,
            sizeof(ack_frame),
            -100,
            10
        ),
        "ACK should arrive"
    );
    duplex_link_service_process(&service);

    DuplexLinkEvent event;
    TEST_ASSERT(
        duplex_link_service_take_event(&service, &event) &&
        event.type == DUPLEX_LINK_EVENT_EXCHANGE_SUCCEEDED,
        "First exchange should succeed"
    );

    TEST_ASSERT(
        duplex_link_service_start_data(
            &service,
            payload,
            sizeof(payload)
        ),
        "Second request should start"
    );
    duplex_link_service_process(&service);

    TEST_ASSERT(
        link_packet_decode(
            radio_context.last_tx_frame,
            radio_context.last_tx_length,
            2U,
            &request
        ) == LINK_PACKET_STATUS_OK &&
        request.sequence == 0U,
        "Sequence should roll over to zero"
    );

    return true;
}

static bool test_device_error_recovers_and_retries(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio radio;
    FakePacketRadioContext radio_context;
    TEST_ASSERT(
        fake_packet_radio_init(&radio, &radio_context),
        "Radio should initialize"
    );

    DuplexLinkService service;
    DuplexLinkConfig config = make_config(
        DUPLEX_LINK_ROLE_INITIATOR,
        1U,
        2U,
        30U
    );

    TEST_ASSERT(
        duplex_link_service_init(
            &service,
            &radio,
            NULL,
            &time,
            &config
        ),
        "Service should initialize"
    );

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE];
    fill_payload(payload, 0x70U);

    TEST_ASSERT(
        duplex_link_service_start_data(
            &service,
            payload,
            sizeof(payload)
        ),
        "Request should start"
    );
    duplex_link_service_process(&service);

    TEST_ASSERT(
        fake_packet_radio_trigger_device_error(
            &radio_context,
            0x1234U
        ),
        "Device error should be injected"
    );
    duplex_link_service_process(&service);

    DuplexLinkEvent event;
    TEST_ASSERT(
        duplex_link_service_take_event(&service, &event) &&
        event.type == DUPLEX_LINK_EVENT_DEVICE_ERROR &&
        event.device_error_code == 0x1234U,
        "Device error should be reported"
    );
    TEST_ASSERT(
        radio_context.recover_calls == 1U &&
        radio_context.state == PACKET_RADIO_STATE_TX &&
        radio_context.tx_start_calls == 2U,
        "Service should recover and retry the request"
    );

    return true;
}

static bool test_payload_and_role_validation(void)
{
    RadioTime time;
    FakeTimeContext time_context;
    fake_time_init(&time, &time_context);

    PacketRadio radio;
    FakePacketRadioContext radio_context;
    TEST_ASSERT(
        fake_packet_radio_init(&radio, &radio_context),
        "Radio should initialize"
    );

    DuplexLinkService initiator;
    DuplexLinkConfig config = make_config(
        DUPLEX_LINK_ROLE_INITIATOR,
        1U,
        2U,
        1U
    );
    TEST_ASSERT(
        duplex_link_service_init(
            &initiator,
            &radio,
            NULL,
            &time,
            &config
        ),
        "Initiator should initialize"
    );

    uint8_t payload[LINK_PACKET_PAYLOAD_SIZE] = { 0U };
    TEST_ASSERT(
        !duplex_link_service_start_data(
            &initiator,
            payload,
            LINK_PACKET_MIN_PAYLOAD_SIZE - 1U
        ),
        "Payload shorter than 10 bytes should fail"
    );
    TEST_ASSERT(
        duplex_link_service_start_data(
            &initiator,
            payload,
            LINK_PACKET_MIN_PAYLOAD_SIZE
        ),
        "10-byte payload should be accepted"
    );
    TEST_ASSERT(
        !duplex_link_service_start_ping(
            &initiator,
            payload,
            LINK_PACKET_PAYLOAD_SIZE
        ),
        "Second request should be rejected while busy"
    );

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_data_ack_exchange()) {
        failures++;
    }
    if (!test_ping_pong_exchange()) {
        failures++;
    }
    if (!test_timeout_retry_and_failure()) {
        failures++;
    }
    if (!test_duplicate_is_suppressed_but_ack_is_repeated()) {
        failures++;
    }
    if (!test_wrong_ack_is_rejected_and_retried()) {
        failures++;
    }
    if (!test_sequence_rollover()) {
        failures++;
    }
    if (!test_device_error_recovers_and_retries()) {
        failures++;
    }
    if (!test_payload_and_role_validation()) {
        failures++;
    }

    if (failures != 0) {
        printf("%d duplex link service test(s) failed\n", failures);
        return 1;
    }

    printf("All duplex link service tests passed\n");
    return 0;
}
