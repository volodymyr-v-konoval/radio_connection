#ifndef RC_RECEIVER_SERVICE_H
#define RC_RECEIVER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "rc_types.h"
#include "radio_protocol_if.h"
#include "radio_transport_if.h"
#include "radio_logger_if.h"
#include "radio_time_if.h"

typedef struct
{
    RadioTransport *transport;
    RadioProtocol  *protocol;
    RadioLogger    *logger;
    RadioTime      *time;

    RcInputFrame latest_frame;

    uint32_t last_frame_time_ms;
    uint32_t failsafe_timeout_ms;

    bool has_frame;
    bool failsafe_active;
} RcReceiverService;

bool rc_receiver_service_init(
    RcReceiverService *service,
    RadioTransport *transport,
    RadioProtocol *protocol,
    RadioLogger *logger,
    RadioTime *time,
    uint32_t failsafe_timeout_ms
);

void rc_receiver_service_process(
    RcReceiverService *service
);

bool rc_receiver_service_get_latest_frame(
    RcReceiverService *service,
    RcInputFrame *out_frame
);

bool rc_receiver_service_is_failsafe(
    RcReceiverService *service
);

#endif /* RC_RECEIVER_SERVICE_H */
