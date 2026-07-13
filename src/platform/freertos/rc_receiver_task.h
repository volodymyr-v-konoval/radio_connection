#ifndef RC_RECEIVER_TASK_H
#define RC_RECEIVER_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "rc_types.h"

#define RC_RECEIVER_TASK_DEFAULT_NAME "rc_receiver"
#define RC_RECEIVER_TASK_DEFAULT_STACK_DEPTH \
    ((configSTACK_DEPTH_TYPE)512U)
#define RC_RECEIVER_TASK_DEFAULT_PRIORITY \
    ((UBaseType_t)(tskIDLE_PRIORITY + 3U))
#define RC_RECEIVER_TASK_DEFAULT_PERIOD_TICKS \
    (pdMS_TO_TICKS(10U))

typedef void (*RcReceiverTaskProcessFn)(
    void *receiver_context
);

typedef bool (*RcReceiverTaskGetLatestFrameFn)(
    void *receiver_context,
    RcInputFrame *out_frame
);

typedef struct
{
    void *receiver_context;

    RcReceiverTaskProcessFn process;
    RcReceiverTaskGetLatestFrameFn get_latest_frame;

    const char *task_name;
    configSTACK_DEPTH_TYPE stack_depth;
    UBaseType_t priority;
    TickType_t period_ticks;
} RcReceiverTaskConfig;

typedef struct
{
    uint32_t wakeups;
    uint32_t notification_wakeups;
    uint32_t timeout_wakeups;
    uint32_t notifications_received;

    uint32_t process_calls;
    uint32_t frames_published;
    uint32_t frame_publish_failures;

    UBaseType_t minimum_free_stack_words;
} RcReceiverTaskStats;

typedef struct
{
    RcReceiverTaskConfig config;

    TaskHandle_t task_handle;
    QueueHandle_t frame_queue;

    RcReceiverTaskStats stats;

    bool initialized;
} RcReceiverTask;

void rc_receiver_task_config_init(
    RcReceiverTaskConfig *config,
    void *receiver_context,
    RcReceiverTaskProcessFn process,
    RcReceiverTaskGetLatestFrameFn get_latest_frame
);

bool rc_receiver_task_init(
    RcReceiverTask *task,
    const RcReceiverTaskConfig *config
);

void rc_receiver_task_deinit(
    RcReceiverTask *task
);

bool rc_receiver_task_notify(
    RcReceiverTask *task
);

bool rc_receiver_task_notify_from_isr(
    RcReceiverTask *task
);

bool rc_receiver_task_process_once(
    RcReceiverTask *task
);

bool rc_receiver_task_run_iteration(
    RcReceiverTask *task
);

bool rc_receiver_task_get_latest_frame(
    RcReceiverTask *task,
    RcInputFrame *out_frame
);

bool rc_receiver_task_get_stats(
    RcReceiverTask *task,
    RcReceiverTaskStats *out_stats
);

#endif /* RC_RECEIVER_TASK_H */
