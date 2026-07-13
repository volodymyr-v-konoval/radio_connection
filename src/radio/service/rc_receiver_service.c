#include "rc_receiver_service.h"

#include <stddef.h>
#include <string.h>
#include <stdarg.h>

#define RC_SERVICE_TAG "RC_SERVICE"

static void rc_service_log(
    RcReceiverService *service,
    RadioLogLevel level,
    const char *fmt,
    ...
)
{
    if (service == NULL || service->logger == NULL || service->logger->vlog == NULL) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    service->logger->vlog(
        service->logger,
        level,
        RC_SERVICE_TAG,
        fmt,
        args
    );

    va_end(args);
}

bool rc_receiver_service_init(
    RcReceiverService *service,
    RadioTransport *transport,
    RadioProtocol *protocol,
    RadioLogger *logger,
    RadioTime *time,
    uint32_t failsafe_timeout_ms
)
{
    if (service == NULL || transport == NULL || protocol == NULL || time == NULL) {
        return false;
    }

    memset(service, 0, sizeof(*service));

    service->transport = transport;
    service->protocol = protocol;
    service->logger = logger;
    service->time = time;
    service->failsafe_timeout_ms = failsafe_timeout_ms;

    service->last_frame_time_ms = time->now_ms(time);
    service->failsafe_active = true;

    if (protocol->reset != NULL) {
        protocol->reset(protocol);
    }

    rc_service_log(
        service,
        RADIO_LOG_LEVEL_INFO,
        "RC receiver service initialized, protocol=%d, failsafe_timeout=%lu ms",
        protocol->type,
        (unsigned long)failsafe_timeout_ms
    );

    return true;
}

void rc_receiver_service_process(
    RcReceiverService *service
)
{
    if (service == NULL ||
        service->transport == NULL ||
        service->protocol == NULL ||
        service->time == NULL) {
        return;
    }

    uint8_t byte = 0;
    uint32_t now_ms = service->time->now_ms(service->time);

    while (service->transport->read_byte(service->transport, &byte)) {
        RadioParseResult result = service->protocol->process_byte(
            service->protocol,
            byte,
            now_ms
        );

        if (result == RADIO_PARSE_FRAME_READY) {
            RcInputFrame frame;

            if (service->protocol->get_frame(service->protocol, &frame)) {
                service->latest_frame = frame;
                service->latest_frame.timestamp_ms = now_ms;

                service->has_frame = true;
                service->last_frame_time_ms = now_ms;
                service->failsafe_active =
                    frame.failsafe || frame.frame_lost;

                rc_service_log(
                    service,
                    RADIO_LOG_LEVEL_INFO,
                    "Frame OK: protocol=%d channels=%u failsafe=%d",
                    frame.protocol,
                    frame.channel_count,
                    frame.failsafe
                );
            }
        } else if (result == RADIO_PARSE_ERROR) {
            rc_service_log(
                service,
                RADIO_LOG_LEVEL_WARN,
                "Protocol parse error"
            );
        }
    }

    if (service->time->elapsed_ms(
            service->time,
            service->last_frame_time_ms
        ) > service->failsafe_timeout_ms) {
        if (!service->failsafe_active) {
            rc_service_log(
                service,
                RADIO_LOG_LEVEL_WARN,
                "Failsafe activated by timeout"
            );
        }

        service->failsafe_active = true;
        service->latest_frame.failsafe = true;
        service->latest_frame.frame_lost = true;
        service->latest_frame.frame_valid = false;
    }
}

bool rc_receiver_service_get_latest_frame(
    RcReceiverService *service,
    RcInputFrame *out_frame
)
{
    if (service == NULL || out_frame == NULL || !service->has_frame) {
        return false;
    }

    *out_frame = service->latest_frame;
    return true;
}

bool rc_receiver_service_is_failsafe(
    RcReceiverService *service
)
{
    if (service == NULL) {
        return true;
    }

    return service->failsafe_active;
}
