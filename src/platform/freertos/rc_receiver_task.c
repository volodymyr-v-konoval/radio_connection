#include "rc_receiver_task.h"

#include <stddef.h>
#include <string.h>

#define RC_RECEIVER_TASK_QUEUE_LENGTH 1U

static void rc_receiver_task_entry(
    void *argument
);

static bool rc_receiver_task_config_is_valid(
    const RcReceiverTaskConfig *config
);

void rc_receiver_task_config_init(
    RcReceiverTaskConfig *config,
    void *receiver_context,
    RcReceiverTaskProcessFn process,
    RcReceiverTaskGetLatestFrameFn get_latest_frame
)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));

    config->receiver_context = receiver_context;
    config->process = process;
    config->get_latest_frame = get_latest_frame;

    config->task_name = RC_RECEIVER_TASK_DEFAULT_NAME;
    config->stack_depth =
        RC_RECEIVER_TASK_DEFAULT_STACK_DEPTH;
    config->priority = RC_RECEIVER_TASK_DEFAULT_PRIORITY;
    config->period_ticks =
        RC_RECEIVER_TASK_DEFAULT_PERIOD_TICKS;
}

bool rc_receiver_task_init(
    RcReceiverTask *task,
    const RcReceiverTaskConfig *config
)
{
    if (task == NULL ||
        task->initialized ||
        !rc_receiver_task_config_is_valid(config)) {
        return false;
    }

    memset(task, 0, sizeof(*task));
    task->config = *config;

    task->frame_queue = xQueueCreate(
        RC_RECEIVER_TASK_QUEUE_LENGTH,
        (UBaseType_t)sizeof(RcInputFrame)
    );

    if (task->frame_queue == NULL) {
        memset(task, 0, sizeof(*task));
        return false;
    }

    task->initialized = true;

    const BaseType_t create_result = xTaskCreate(
        rc_receiver_task_entry,
        task->config.task_name,
        task->config.stack_depth,
        task,
        task->config.priority,
        &task->task_handle
    );

    if (create_result != pdPASS ||
        task->task_handle == NULL) {
        task->initialized = false;
        vQueueDelete(task->frame_queue);
        memset(task, 0, sizeof(*task));
        return false;
    }

    return true;
}

void rc_receiver_task_deinit(
    RcReceiverTask *task
)
{
    if (task == NULL || !task->initialized) {
        return;
    }

    if (task->task_handle != NULL) {
        vTaskDelete(task->task_handle);
    }

    if (task->frame_queue != NULL) {
        vQueueDelete(task->frame_queue);
    }

    memset(task, 0, sizeof(*task));
}

bool rc_receiver_task_notify(
    RcReceiverTask *task
)
{
    if (task == NULL ||
        !task->initialized ||
        task->task_handle == NULL) {
        return false;
    }

    (void)xTaskNotifyGive(task->task_handle);
    return true;
}

bool rc_receiver_task_notify_from_isr(
    RcReceiverTask *task
)
{
    if (task == NULL ||
        !task->initialized ||
        task->task_handle == NULL) {
        return false;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;

    vTaskNotifyGiveFromISR(
        task->task_handle,
        &higher_priority_task_woken
    );

    portYIELD_FROM_ISR(higher_priority_task_woken);
    return true;
}

bool rc_receiver_task_process_once(
    RcReceiverTask *task
)
{
    if (task == NULL ||
        !task->initialized ||
        task->config.process == NULL ||
        task->config.get_latest_frame == NULL ||
        task->frame_queue == NULL) {
        return false;
    }

    task->config.process(task->config.receiver_context);
    task->stats.process_calls++;

    RcInputFrame frame = { 0 };

    if (!task->config.get_latest_frame(
            task->config.receiver_context,
            &frame)) {
        return true;
    }

    if (xQueueOverwrite(
            task->frame_queue,
            &frame) != pdPASS) {
        task->stats.frame_publish_failures++;
        return false;
    }

    task->stats.frames_published++;
    return true;
}

bool rc_receiver_task_run_iteration(
    RcReceiverTask *task
)
{
    if (task == NULL || !task->initialized) {
        return false;
    }

    const uint32_t notifications = ulTaskNotifyTake(
        pdTRUE,
        task->config.period_ticks
    );

    task->stats.wakeups++;

    if (notifications == 0U) {
        task->stats.timeout_wakeups++;
    } else {
        task->stats.notification_wakeups++;
        task->stats.notifications_received += notifications;
    }

    return rc_receiver_task_process_once(task);
}

bool rc_receiver_task_get_latest_frame(
    RcReceiverTask *task,
    RcInputFrame *out_frame
)
{
    if (task == NULL ||
        out_frame == NULL ||
        !task->initialized ||
        task->frame_queue == NULL) {
        return false;
    }

    return xQueuePeek(
        task->frame_queue,
        out_frame,
        (TickType_t)0U
    ) == pdPASS;
}

bool rc_receiver_task_get_stats(
    RcReceiverTask *task,
    RcReceiverTaskStats *out_stats
)
{
    if (task == NULL ||
        out_stats == NULL ||
        !task->initialized) {
        return false;
    }

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
    if (task->task_handle != NULL) {
        task->stats.minimum_free_stack_words =
            uxTaskGetStackHighWaterMark(
                task->task_handle
            );
    }
#endif

    *out_stats = task->stats;
    return true;
}

static void rc_receiver_task_entry(
    void *argument
)
{
    RcReceiverTask *task =
        (RcReceiverTask *)argument;

    for (;;) {
        (void)rc_receiver_task_run_iteration(task);
    }
}

static bool rc_receiver_task_config_is_valid(
    const RcReceiverTaskConfig *config
)
{
    if (config == NULL ||
        config->receiver_context == NULL ||
        config->process == NULL ||
        config->get_latest_frame == NULL ||
        config->task_name == NULL ||
        config->task_name[0] == '\0' ||
        config->period_ticks == (TickType_t)0U ||
        config->stack_depth <
            (configSTACK_DEPTH_TYPE)configMINIMAL_STACK_SIZE) {
        return false;
    }

    return true;
}
