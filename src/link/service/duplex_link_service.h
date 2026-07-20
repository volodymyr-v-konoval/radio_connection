#ifndef DUPLEX_LINK_SERVICE_H
#define DUPLEX_LINK_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "link_packet.h"
#include "packet_radio_if.h"
#include "radio_logger_if.h"
#include "radio_time_if.h"

#define DUPLEX_LINK_EVENT_CAPACITY 8U

typedef enum
{
    DUPLEX_LINK_ROLE_INITIATOR = 0,
    DUPLEX_LINK_ROLE_RESPONDER
} DuplexLinkRole;

typedef enum
{
    DUPLEX_LINK_STATE_UNINITIALIZED = 0,
    DUPLEX_LINK_STATE_IDLE,
    DUPLEX_LINK_STATE_START_REQUEST_TX,
    DUPLEX_LINK_STATE_WAIT_REQUEST_TX_DONE,
    DUPLEX_LINK_STATE_START_RESPONSE_RX,
    DUPLEX_LINK_STATE_WAIT_RESPONSE,
    DUPLEX_LINK_STATE_START_RESPONDER_RX,
    DUPLEX_LINK_STATE_RESPONDER_RX,
    DUPLEX_LINK_STATE_START_RESPONSE_TX,
    DUPLEX_LINK_STATE_WAIT_RESPONSE_TX_DONE,
    DUPLEX_LINK_STATE_ERROR
} DuplexLinkState;

typedef enum
{
    DUPLEX_LINK_EVENT_NONE = 0,
    DUPLEX_LINK_EVENT_REQUEST_RECEIVED,
    DUPLEX_LINK_EVENT_EXCHANGE_SUCCEEDED,
    DUPLEX_LINK_EVENT_EXCHANGE_FAILED,
    DUPLEX_LINK_EVENT_DUPLICATE_SUPPRESSED,
    DUPLEX_LINK_EVENT_INVALID_PACKET,
    DUPLEX_LINK_EVENT_DEVICE_ERROR
} DuplexLinkEventType;

typedef enum
{
    DUPLEX_LINK_INVALID_NONE = 0,
    DUPLEX_LINK_INVALID_DECODE,
    DUPLEX_LINK_INVALID_SOURCE,
    DUPLEX_LINK_INVALID_MESSAGE_TYPE,
    DUPLEX_LINK_INVALID_ACK_SEQUENCE,
    DUPLEX_LINK_INVALID_RX_FRAME
} DuplexLinkInvalidReason;

typedef struct
{
    DuplexLinkRole role;
    uint8_t local_node_id;
    uint8_t peer_node_id;
    uint16_t initial_sequence;
    uint32_t response_timeout_ms;
    uint32_t operation_timeout_ms;
    uint8_t max_retries;
} DuplexLinkConfig;

typedef struct
{
    DuplexLinkEventType type;
    LinkPacket packet;
    DuplexLinkInvalidReason invalid_reason;
    LinkPacketStatus packet_status;
    uint16_t device_error_code;
    uint8_t attempts;
    uint32_t rtt_ms;
    int16_t rssi_dbm_x2;
    int16_t snr_db_x4;
} DuplexLinkEvent;

typedef struct
{
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t completed_exchanges;
    uint32_t failed_exchanges;
    uint32_t retries;
    uint32_t timeouts;
    uint32_t radio_crc_errors;
    uint32_t invalid_packets;
    uint32_t duplicate_packets;
    uint32_t device_errors;
} DuplexLinkStats;

typedef struct
{
    PacketRadio *radio;
    RadioLogger *logger;
    RadioTime *time;
    DuplexLinkConfig config;
    DuplexLinkState state;

    uint16_t next_sequence;
    uint32_t state_started_ms;
    uint32_t attempt_started_ms;
    uint32_t response_wait_started_ms;

    LinkPacket pending_request;
    uint8_t pending_request_frame[LINK_PACKET_ENCODED_SIZE];
    bool request_active;
    uint8_t retry_count;
    uint8_t attempt_count;

    uint8_t response_payload[LINK_PACKET_PAYLOAD_SIZE];
    uint8_t response_payload_length;

    bool has_last_remote_sequence;
    uint16_t last_remote_sequence;
    LinkMessageType last_remote_message_type;
    uint8_t cached_response_frame[LINK_PACKET_ENCODED_SIZE];
    bool has_cached_response;

    DuplexLinkEvent events[DUPLEX_LINK_EVENT_CAPACITY];
    size_t event_head;
    size_t event_count;

    DuplexLinkStats stats;
} DuplexLinkService;

bool duplex_link_service_init(
    DuplexLinkService *service,
    PacketRadio *radio,
    RadioLogger *logger,
    RadioTime *time,
    const DuplexLinkConfig *config
);

bool duplex_link_service_set_response_payload(
    DuplexLinkService *service,
    const uint8_t *payload,
    size_t payload_length
);

bool duplex_link_service_start_data(
    DuplexLinkService *service,
    const uint8_t *payload,
    size_t payload_length
);

bool duplex_link_service_start_ping(
    DuplexLinkService *service,
    const uint8_t *payload,
    size_t payload_length
);

void duplex_link_service_process(
    DuplexLinkService *service
);

bool duplex_link_service_take_event(
    DuplexLinkService *service,
    DuplexLinkEvent *event
);

DuplexLinkState duplex_link_service_get_state(
    const DuplexLinkService *service
);

bool duplex_link_service_is_busy(
    const DuplexLinkService *service
);

const DuplexLinkStats *duplex_link_service_get_stats(
    const DuplexLinkService *service
);

#endif /* DUPLEX_LINK_SERVICE_H */
