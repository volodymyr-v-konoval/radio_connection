#include "duplex_link_service.h"

#include <stdarg.h>
#include <string.h>

#define DUPLEX_LINK_TAG "DUPLEX_LINK"
#define DUPLEX_LINK_INTERNAL_ERROR 0xFFFFU

static uint32_t duplex_link_now_ms(
    DuplexLinkService *service
)
{
    return service->time->now_ms(service->time);
}

static uint32_t duplex_link_elapsed_ms(
    DuplexLinkService *service,
    uint32_t since_ms
)
{
    return service->time->elapsed_ms(service->time, since_ms);
}

static void duplex_link_log(
    DuplexLinkService *service,
    RadioLogLevel level,
    const char *fmt,
    ...
)
{
    if (service == NULL || service->logger == NULL ||
        service->logger->vlog == NULL) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    service->logger->vlog(
        service->logger,
        level,
        DUPLEX_LINK_TAG,
        fmt,
        args
    );
    va_end(args);
}

static void duplex_link_set_state(
    DuplexLinkService *service,
    DuplexLinkState state
)
{
    service->state = state;
    service->state_started_ms = duplex_link_now_ms(service);
}

static bool duplex_link_push_event(
    DuplexLinkService *service,
    const DuplexLinkEvent *event
)
{
    if (service->event_count >= DUPLEX_LINK_EVENT_CAPACITY) {
        duplex_link_log(
            service,
            RADIO_LOG_LEVEL_WARN,
            "Event queue full, dropping event=%u",
            (unsigned int)event->type
        );
        return false;
    }

    const size_t index =
        (service->event_head + service->event_count) %
        DUPLEX_LINK_EVENT_CAPACITY;

    service->events[index] = *event;
    service->event_count++;
    return true;
}

static void duplex_link_publish_invalid(
    DuplexLinkService *service,
    DuplexLinkInvalidReason reason,
    LinkPacketStatus packet_status,
    const LinkPacket *packet,
    const PacketRadioRxFrame *frame
)
{
    DuplexLinkEvent event = {
        .type = DUPLEX_LINK_EVENT_INVALID_PACKET,
        .invalid_reason = reason,
        .packet_status = packet_status
    };

    if (packet != NULL) {
        event.packet = *packet;
    }

    if (frame != NULL) {
        event.rssi_dbm_x2 = frame->rssi_dbm_x2;
        event.snr_db_x4 = frame->snr_db_x4;
    }

    service->stats.invalid_packets++;
    (void)duplex_link_push_event(service, &event);
}

static bool duplex_link_payload_length_is_valid(
    size_t payload_length
)
{
    return payload_length >= LINK_PACKET_MIN_PAYLOAD_SIZE &&
        payload_length <= LINK_PACKET_PAYLOAD_SIZE;
}

static bool duplex_link_request_type_is_valid(
    LinkMessageType message_type
)
{
    return message_type == LINK_MESSAGE_TYPE_DATA ||
        message_type == LINK_MESSAGE_TYPE_PING;
}

static LinkMessageType duplex_link_expected_response_type(
    LinkMessageType request_type
)
{
    if (request_type == LINK_MESSAGE_TYPE_PING) {
        return LINK_MESSAGE_TYPE_PONG;
    }

    return LINK_MESSAGE_TYPE_ACK;
}

static bool duplex_link_prepare_request(
    DuplexLinkService *service,
    LinkMessageType message_type,
    const uint8_t *payload,
    size_t payload_length
)
{
    if (service == NULL || payload == NULL ||
        !duplex_link_request_type_is_valid(message_type) ||
        !duplex_link_payload_length_is_valid(payload_length) ||
        service->config.role != DUPLEX_LINK_ROLE_INITIATOR ||
        service->state != DUPLEX_LINK_STATE_IDLE ||
        service->request_active) {
        return false;
    }

    LinkPacket packet = {
        .message_type = message_type,
        .source_id = service->config.local_node_id,
        .destination_id = service->config.peer_node_id,
        .sequence = service->next_sequence,
        .acknowledged_sequence = 0U,
        .payload_length = (uint8_t)payload_length,
        .flags = LINK_PACKET_FLAG_NONE
    };

    memcpy(packet.payload, payload, payload_length);

    if (link_packet_encode(
            &packet,
            service->pending_request_frame,
            sizeof(service->pending_request_frame)
        ) != LINK_PACKET_STATUS_OK) {
        return false;
    }

    service->pending_request = packet;
    service->next_sequence++;
    service->request_active = true;
    service->retry_count = 0U;
    service->attempt_count = 0U;
    duplex_link_set_state(service, DUPLEX_LINK_STATE_START_REQUEST_TX);

    return true;
}

static void duplex_link_finish_failed_exchange(
    DuplexLinkService *service
)
{
    DuplexLinkEvent event = {
        .type = DUPLEX_LINK_EVENT_EXCHANGE_FAILED,
        .packet = service->pending_request,
        .attempts = service->attempt_count
    };

    service->stats.failed_exchanges++;
    service->request_active = false;
    duplex_link_set_state(service, DUPLEX_LINK_STATE_IDLE);
    (void)duplex_link_push_event(service, &event);

    duplex_link_log(
        service,
        RADIO_LOG_LEVEL_WARN,
        "Exchange failed seq=%u attempts=%u",
        (unsigned int)event.packet.sequence,
        (unsigned int)event.attempts
    );
}

static void duplex_link_retry_or_fail(
    DuplexLinkService *service
)
{
    if (!service->request_active) {
        duplex_link_set_state(service, DUPLEX_LINK_STATE_IDLE);
        return;
    }

    if (service->retry_count < service->config.max_retries) {
        service->retry_count++;
        service->stats.retries++;
        duplex_link_set_state(service, DUPLEX_LINK_STATE_START_REQUEST_TX);

        duplex_link_log(
            service,
            RADIO_LOG_LEVEL_WARN,
            "Retry seq=%u retry=%u/%u",
            (unsigned int)service->pending_request.sequence,
            (unsigned int)service->retry_count,
            (unsigned int)service->config.max_retries
        );
        return;
    }

    duplex_link_finish_failed_exchange(service);
}

static bool duplex_link_recover_radio(
    DuplexLinkService *service,
    uint16_t error_code
)
{
    if (packet_radio_recover(service->radio)) {
        return true;
    }

    DuplexLinkEvent event = {
        .type = DUPLEX_LINK_EVENT_DEVICE_ERROR,
        .device_error_code = error_code
    };

    service->stats.device_errors++;
    duplex_link_set_state(service, DUPLEX_LINK_STATE_ERROR);
    (void)duplex_link_push_event(service, &event);
    return false;
}

static void duplex_link_handle_device_error(
    DuplexLinkService *service,
    uint16_t error_code
)
{
    DuplexLinkEvent event = {
        .type = DUPLEX_LINK_EVENT_DEVICE_ERROR,
        .device_error_code = error_code,
        .attempts = service->attempt_count
    };

    service->stats.device_errors++;
    (void)duplex_link_push_event(service, &event);

    if (!packet_radio_recover(service->radio)) {
        duplex_link_set_state(service, DUPLEX_LINK_STATE_ERROR);
        return;
    }

    if (service->config.role == DUPLEX_LINK_ROLE_RESPONDER) {
        duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONDER_RX);
    } else if (service->request_active) {
        duplex_link_retry_or_fail(service);
    } else {
        duplex_link_set_state(service, DUPLEX_LINK_STATE_IDLE);
    }
}

static bool duplex_link_build_response(
    DuplexLinkService *service,
    const LinkPacket *request
)
{
    LinkPacket response = {
        .message_type = duplex_link_expected_response_type(
            request->message_type
        ),
        .source_id = service->config.local_node_id,
        .destination_id = service->config.peer_node_id,
        .sequence = service->next_sequence,
        .acknowledged_sequence = request->sequence,
        .payload_length = service->response_payload_length,
        .flags = LINK_PACKET_FLAG_NONE
    };

    memcpy(
        response.payload,
        service->response_payload,
        service->response_payload_length
    );

    if (link_packet_encode(
            &response,
            service->cached_response_frame,
            sizeof(service->cached_response_frame)
        ) != LINK_PACKET_STATUS_OK) {
        return false;
    }

    service->next_sequence++;
    service->has_cached_response = true;
    return true;
}

static void duplex_link_handle_responder_frame(
    DuplexLinkService *service,
    const PacketRadioRxFrame *frame
)
{
    LinkPacket request = { 0 };
    const LinkPacketStatus status = link_packet_decode(
        frame->data,
        frame->length,
        service->config.local_node_id,
        &request
    );

    if (status != LINK_PACKET_STATUS_OK) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_DECODE,
            status,
            NULL,
            frame
        );
        duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONDER_RX);
        return;
    }

    service->stats.rx_packets++;

    if (request.source_id != service->config.peer_node_id) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_SOURCE,
            LINK_PACKET_STATUS_OK,
            &request,
            frame
        );
        duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONDER_RX);
        return;
    }

    if (!duplex_link_request_type_is_valid(request.message_type)) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_MESSAGE_TYPE,
            LINK_PACKET_STATUS_OK,
            &request,
            frame
        );
        duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONDER_RX);
        return;
    }

    const bool duplicate =
        service->has_last_remote_sequence &&
        request.sequence == service->last_remote_sequence;

    if (duplicate) {
        DuplexLinkEvent event = {
            .type = DUPLEX_LINK_EVENT_DUPLICATE_SUPPRESSED,
            .packet = request,
            .rssi_dbm_x2 = frame->rssi_dbm_x2,
            .snr_db_x4 = frame->snr_db_x4
        };

        service->stats.duplicate_packets++;
        (void)duplex_link_push_event(service, &event);

        if (service->has_cached_response) {
            duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONSE_TX);
        } else {
            duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONDER_RX);
        }
        return;
    }

    service->has_last_remote_sequence = true;
    service->last_remote_sequence = request.sequence;
    service->last_remote_message_type = request.message_type;

    DuplexLinkEvent event = {
        .type = DUPLEX_LINK_EVENT_REQUEST_RECEIVED,
        .packet = request,
        .rssi_dbm_x2 = frame->rssi_dbm_x2,
        .snr_db_x4 = frame->snr_db_x4
    };
    (void)duplex_link_push_event(service, &event);

    if (!duplex_link_build_response(service, &request)) {
        duplex_link_handle_device_error(
            service,
            DUPLEX_LINK_INTERNAL_ERROR
        );
        return;
    }

    duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONSE_TX);
}

static void duplex_link_handle_initiator_frame(
    DuplexLinkService *service,
    const PacketRadioRxFrame *frame
)
{
    LinkPacket response = { 0 };
    const LinkPacketStatus status = link_packet_decode(
        frame->data,
        frame->length,
        service->config.local_node_id,
        &response
    );

    if (status != LINK_PACKET_STATUS_OK) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_DECODE,
            status,
            NULL,
            frame
        );
        duplex_link_retry_or_fail(service);
        return;
    }

    service->stats.rx_packets++;

    if (response.source_id != service->config.peer_node_id) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_SOURCE,
            LINK_PACKET_STATUS_OK,
            &response,
            frame
        );
        duplex_link_retry_or_fail(service);
        return;
    }

    if (response.message_type != duplex_link_expected_response_type(
            service->pending_request.message_type
        )) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_MESSAGE_TYPE,
            LINK_PACKET_STATUS_OK,
            &response,
            frame
        );
        duplex_link_retry_or_fail(service);
        return;
    }

    if (response.acknowledged_sequence !=
        service->pending_request.sequence) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_ACK_SEQUENCE,
            LINK_PACKET_STATUS_OK,
            &response,
            frame
        );
        duplex_link_retry_or_fail(service);
        return;
    }

    DuplexLinkEvent event = {
        .type = DUPLEX_LINK_EVENT_EXCHANGE_SUCCEEDED,
        .packet = response,
        .attempts = service->attempt_count,
        .rtt_ms = duplex_link_elapsed_ms(
            service,
            service->attempt_started_ms
        ),
        .rssi_dbm_x2 = frame->rssi_dbm_x2,
        .snr_db_x4 = frame->snr_db_x4
    };

    service->stats.completed_exchanges++;
    service->request_active = false;
    duplex_link_set_state(service, DUPLEX_LINK_STATE_IDLE);
    (void)duplex_link_push_event(service, &event);

    duplex_link_log(
        service,
        RADIO_LOG_LEVEL_INFO,
        "Exchange OK seq=%u ack=%u attempts=%u rtt=%lu ms rssi_x2=%d snr_x4=%d",
        (unsigned int)response.sequence,
        (unsigned int)response.acknowledged_sequence,
        (unsigned int)event.attempts,
        (unsigned long)event.rtt_ms,
        (int)event.rssi_dbm_x2,
        (int)event.snr_db_x4
    );
}

static void duplex_link_handle_rx_done(
    DuplexLinkService *service
)
{
    PacketRadioRxFrame frame;

    if (!packet_radio_read_rx_frame(service->radio, &frame)) {
        duplex_link_publish_invalid(
            service,
            DUPLEX_LINK_INVALID_RX_FRAME,
            LINK_PACKET_STATUS_INVALID_ARGUMENT,
            NULL,
            NULL
        );

        if (service->config.role == DUPLEX_LINK_ROLE_RESPONDER) {
            duplex_link_set_state(service, DUPLEX_LINK_STATE_START_RESPONDER_RX);
        } else {
            duplex_link_retry_or_fail(service);
        }
        return;
    }

    if (service->config.role == DUPLEX_LINK_ROLE_RESPONDER &&
        service->state == DUPLEX_LINK_STATE_RESPONDER_RX) {
        duplex_link_handle_responder_frame(service, &frame);
        return;
    }

    if (service->config.role == DUPLEX_LINK_ROLE_INITIATOR &&
        service->state == DUPLEX_LINK_STATE_WAIT_RESPONSE &&
        service->request_active) {
        duplex_link_handle_initiator_frame(service, &frame);
        return;
    }

    duplex_link_publish_invalid(
        service,
        DUPLEX_LINK_INVALID_RX_FRAME,
        LINK_PACKET_STATUS_INVALID_ARGUMENT,
        NULL,
        &frame
    );
}

static void duplex_link_handle_radio_event(
    DuplexLinkService *service,
    const PacketRadioEvent *event
)
{
    switch (event->type) {
        case PACKET_RADIO_EVENT_TX_DONE:
            if (service->state == DUPLEX_LINK_STATE_WAIT_REQUEST_TX_DONE) {
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_START_RESPONSE_RX
                );
            } else if (service->state ==
                DUPLEX_LINK_STATE_WAIT_RESPONSE_TX_DONE) {
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_START_RESPONDER_RX
                );
            }
            break;

        case PACKET_RADIO_EVENT_RX_DONE:
            duplex_link_handle_rx_done(service);
            break;

        case PACKET_RADIO_EVENT_RX_TIMEOUT:
            service->stats.timeouts++;
            if (service->config.role == DUPLEX_LINK_ROLE_INITIATOR &&
                service->state == DUPLEX_LINK_STATE_WAIT_RESPONSE) {
                duplex_link_retry_or_fail(service);
            } else if (service->config.role == DUPLEX_LINK_ROLE_RESPONDER) {
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_START_RESPONDER_RX
                );
            }
            break;

        case PACKET_RADIO_EVENT_CRC_ERROR:
            service->stats.radio_crc_errors++;
            if (service->config.role == DUPLEX_LINK_ROLE_INITIATOR &&
                service->request_active) {
                duplex_link_retry_or_fail(service);
            } else if (service->config.role == DUPLEX_LINK_ROLE_RESPONDER) {
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_START_RESPONDER_RX
                );
            }
            break;

        case PACKET_RADIO_EVENT_DEVICE_ERROR:
            duplex_link_handle_device_error(
                service,
                event->device_error_code
            );
            break;

        case PACKET_RADIO_EVENT_NONE:
        default:
            break;
    }
}

static void duplex_link_process_timeouts(
    DuplexLinkService *service
)
{
    if (service->state == DUPLEX_LINK_STATE_WAIT_RESPONSE &&
        duplex_link_elapsed_ms(
            service,
            service->response_wait_started_ms
        ) >= service->config.response_timeout_ms) {
        service->stats.timeouts++;
        if (duplex_link_recover_radio(
                service,
                DUPLEX_LINK_INTERNAL_ERROR
            )) {
            duplex_link_retry_or_fail(service);
        }
        return;
    }

    if ((service->state == DUPLEX_LINK_STATE_WAIT_REQUEST_TX_DONE ||
         service->state == DUPLEX_LINK_STATE_WAIT_RESPONSE_TX_DONE) &&
        duplex_link_elapsed_ms(
            service,
            service->state_started_ms
        ) >= service->config.operation_timeout_ms) {
        service->stats.timeouts++;

        if (!duplex_link_recover_radio(
                service,
                DUPLEX_LINK_INTERNAL_ERROR
            )) {
            return;
        }

        if (service->state == DUPLEX_LINK_STATE_WAIT_REQUEST_TX_DONE) {
            duplex_link_retry_or_fail(service);
        } else {
            duplex_link_set_state(
                service,
                DUPLEX_LINK_STATE_START_RESPONDER_RX
            );
        }
    }
}

static void duplex_link_process_state_action(
    DuplexLinkService *service
)
{
    switch (service->state) {
        case DUPLEX_LINK_STATE_START_REQUEST_TX:
            if (packet_radio_try_start_tx(
                    service->radio,
                    service->pending_request_frame,
                    sizeof(service->pending_request_frame)
                )) {
                service->attempt_count++;
                service->stats.tx_packets++;
                service->attempt_started_ms = duplex_link_now_ms(service);
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_WAIT_REQUEST_TX_DONE
                );
            } else if (duplex_link_elapsed_ms(
                    service,
                    service->state_started_ms
                ) >= service->config.operation_timeout_ms) {
                (void)duplex_link_recover_radio(
                    service,
                    DUPLEX_LINK_INTERNAL_ERROR
                );
                service->state_started_ms = duplex_link_now_ms(service);
            }
            break;

        case DUPLEX_LINK_STATE_START_RESPONSE_RX:
            if (packet_radio_try_start_rx(
                    service->radio,
                    service->config.response_timeout_ms
                )) {
                service->response_wait_started_ms =
                    duplex_link_now_ms(service);
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_WAIT_RESPONSE
                );
            } else if (duplex_link_elapsed_ms(
                    service,
                    service->state_started_ms
                ) >= service->config.operation_timeout_ms) {
                if (duplex_link_recover_radio(
                        service,
                        DUPLEX_LINK_INTERNAL_ERROR
                    )) {
                    duplex_link_retry_or_fail(service);
                }
            }
            break;

        case DUPLEX_LINK_STATE_START_RESPONDER_RX:
            if (packet_radio_try_start_rx(service->radio, 0U)) {
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_RESPONDER_RX
                );
            } else if (duplex_link_elapsed_ms(
                    service,
                    service->state_started_ms
                ) >= service->config.operation_timeout_ms) {
                if (duplex_link_recover_radio(
                        service,
                        DUPLEX_LINK_INTERNAL_ERROR
                    )) {
                    service->state_started_ms = duplex_link_now_ms(service);
                }
            }
            break;

        case DUPLEX_LINK_STATE_START_RESPONSE_TX:
            if (service->has_cached_response &&
                packet_radio_try_start_tx(
                    service->radio,
                    service->cached_response_frame,
                    sizeof(service->cached_response_frame)
                )) {
                service->stats.tx_packets++;
                duplex_link_set_state(
                    service,
                    DUPLEX_LINK_STATE_WAIT_RESPONSE_TX_DONE
                );
            } else if (duplex_link_elapsed_ms(
                    service,
                    service->state_started_ms
                ) >= service->config.operation_timeout_ms) {
                if (duplex_link_recover_radio(
                        service,
                        DUPLEX_LINK_INTERNAL_ERROR
                    )) {
                    duplex_link_set_state(
                        service,
                        DUPLEX_LINK_STATE_START_RESPONDER_RX
                    );
                }
            }
            break;

        case DUPLEX_LINK_STATE_UNINITIALIZED:
        case DUPLEX_LINK_STATE_IDLE:
        case DUPLEX_LINK_STATE_WAIT_REQUEST_TX_DONE:
        case DUPLEX_LINK_STATE_WAIT_RESPONSE:
        case DUPLEX_LINK_STATE_RESPONDER_RX:
        case DUPLEX_LINK_STATE_WAIT_RESPONSE_TX_DONE:
        case DUPLEX_LINK_STATE_ERROR:
        default:
            break;
    }
}

bool duplex_link_service_init(
    DuplexLinkService *service,
    PacketRadio *radio,
    RadioLogger *logger,
    RadioTime *time,
    const DuplexLinkConfig *config
)
{
    if (service == NULL || radio == NULL || time == NULL ||
        config == NULL || time->now_ms == NULL ||
        time->elapsed_ms == NULL || config->local_node_id == 0U ||
        config->peer_node_id == 0U ||
        config->local_node_id == config->peer_node_id ||
        config->response_timeout_ms == 0U ||
        config->operation_timeout_ms == 0U) {
        return false;
    }

    memset(service, 0, sizeof(*service));
    service->radio = radio;
    service->logger = logger;
    service->time = time;
    service->config = *config;
    service->next_sequence = config->initial_sequence;
    service->response_payload_length = LINK_PACKET_PAYLOAD_SIZE;
    service->state_started_ms = time->now_ms(time);

    if (config->role == DUPLEX_LINK_ROLE_RESPONDER) {
        service->state = DUPLEX_LINK_STATE_START_RESPONDER_RX;
    } else if (config->role == DUPLEX_LINK_ROLE_INITIATOR) {
        service->state = DUPLEX_LINK_STATE_IDLE;
    } else {
        return false;
    }

    duplex_link_log(
        service,
        RADIO_LOG_LEVEL_INFO,
        "Initialized role=%u local=%u peer=%u timeout=%lu ms retries=%u",
        (unsigned int)config->role,
        (unsigned int)config->local_node_id,
        (unsigned int)config->peer_node_id,
        (unsigned long)config->response_timeout_ms,
        (unsigned int)config->max_retries
    );

    return true;
}

bool duplex_link_service_set_response_payload(
    DuplexLinkService *service,
    const uint8_t *payload,
    size_t payload_length
)
{
    if (service == NULL || payload == NULL ||
        service->config.role != DUPLEX_LINK_ROLE_RESPONDER ||
        !duplex_link_payload_length_is_valid(payload_length)) {
        return false;
    }

    memset(service->response_payload, 0, sizeof(service->response_payload));
    memcpy(service->response_payload, payload, payload_length);
    service->response_payload_length = (uint8_t)payload_length;
    return true;
}

bool duplex_link_service_start_data(
    DuplexLinkService *service,
    const uint8_t *payload,
    size_t payload_length
)
{
    return duplex_link_prepare_request(
        service,
        LINK_MESSAGE_TYPE_DATA,
        payload,
        payload_length
    );
}

bool duplex_link_service_start_ping(
    DuplexLinkService *service,
    const uint8_t *payload,
    size_t payload_length
)
{
    return duplex_link_prepare_request(
        service,
        LINK_MESSAGE_TYPE_PING,
        payload,
        payload_length
    );
}

void duplex_link_service_process(
    DuplexLinkService *service
)
{
    if (service == NULL || service->radio == NULL ||
        service->time == NULL ||
        service->state == DUPLEX_LINK_STATE_UNINITIALIZED ||
        service->state == DUPLEX_LINK_STATE_ERROR) {
        return;
    }

    packet_radio_process(service->radio);

    PacketRadioEvent event;
    while (packet_radio_take_event(service->radio, &event)) {
        duplex_link_handle_radio_event(service, &event);
        if (service->state == DUPLEX_LINK_STATE_ERROR) {
            return;
        }
    }

    duplex_link_process_timeouts(service);
    if (service->state != DUPLEX_LINK_STATE_ERROR) {
        duplex_link_process_state_action(service);
    }
}

bool duplex_link_service_take_event(
    DuplexLinkService *service,
    DuplexLinkEvent *event
)
{
    if (service == NULL || event == NULL || service->event_count == 0U) {
        return false;
    }

    *event = service->events[service->event_head];
    service->event_head =
        (service->event_head + 1U) % DUPLEX_LINK_EVENT_CAPACITY;
    service->event_count--;
    return true;
}

DuplexLinkState duplex_link_service_get_state(
    const DuplexLinkService *service
)
{
    if (service == NULL) {
        return DUPLEX_LINK_STATE_UNINITIALIZED;
    }

    return service->state;
}

bool duplex_link_service_is_busy(
    const DuplexLinkService *service
)
{
    if (service == NULL) {
        return false;
    }

    return service->state != DUPLEX_LINK_STATE_IDLE &&
        service->state != DUPLEX_LINK_STATE_RESPONDER_RX;
}

const DuplexLinkStats *duplex_link_service_get_stats(
    const DuplexLinkService *service
)
{
    if (service == NULL) {
        return NULL;
    }

    return &service->stats;
}
