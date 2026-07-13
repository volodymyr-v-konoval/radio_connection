#include "rc_receiver_task.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_freertos.h"

#define TEST_ASSERT(condition, message)      \
    do {                                     \
        if (!(condition)) {                  \
            printf("FAIL: %s\n", message); \
            return false;                    \
        }                                    \
    } while (0)

typedef struct
{
    uint32_t process_calls;
    bool has_frame;
    RcInputFrame frame;
} FakeReceiver;

static void fake_receiver_process(
    void *receiver_context
)
{
    FakeReceiver *receiver =
        (FakeReceiver *)receiver_context;

    if (receiver != NULL) {
        receiver->process_calls++;
    }
}

static bool fake_receiver_get_latest_frame(
    void *receiver_context,
    RcInputFrame *out_frame
)
{
    FakeReceiver *receiver =
        (FakeReceiver *)receiver_context;

    if (receiver == NULL ||
        out_frame == NULL ||
        !receiver->has_frame) {
        return false;
    }

    *out_frame = receiver->frame;
    return true;
}

static RcInputFrame make_frame(
    uint16_t channel_value,
    uint32_t timestamp_ms
)
{
    RcInputFrame frame;
    memset(&frame, 0, sizeof(frame));

    frame.channel_count = 16U;
    frame.frame_valid = true;
    frame.failsafe = false;
    frame.frame_lost = false;
    frame.timestamp_ms = timestamp_ms;
    frame.protocol = RADIO_PROTOCOL_CRSF;

    for (uint8_t index = 0U;
         index < frame.channel_count;
         index++) {
        frame.channels[index] = channel_value;
    }

    return frame;
}

static bool init_task(
    RcReceiverTask *task,
    FakeReceiver *receiver
)
{
    RcReceiverTaskConfig config;

    rc_receiver_task_config_init(
        &config,
        receiver,
        fake_receiver_process,
        fake_receiver_get_latest_frame
    );

    config.task_name = "test_rc_rx";
    config.stack_depth = 640U;
    config.priority = 5U;
    config.period_ticks = 7U;

    return rc_receiver_task_init(task, &config);
}

static bool test_init_uses_defined_task_configuration(void)
{
    fake_freertos_reset();

    FakeReceiver receiver = { 0 };
    RcReceiverTask task = { 0 };

    TEST_ASSERT(
        init_task(&task, &receiver),
        "Task initialization should succeed"
    );

    TEST_ASSERT(
        fake_freertos_task_create_calls() == 1U,
        "One task should be created"
    );

    TEST_ASSERT(
        fake_freertos_queue_create_calls() == 1U,
        "One frame mailbox should be created"
    );

    TEST_ASSERT(
        fake_freertos_created_stack_depth() == 640U,
        "Configured stack depth should be used"
    );

    TEST_ASSERT(
        fake_freertos_created_priority() == 5U,
        "Configured priority should be used"
    );

    TEST_ASSERT(
        strcmp(
            fake_freertos_created_task_name(),
            "test_rc_rx") == 0,
        "Configured task name should be used"
    );

    rc_receiver_task_deinit(&task);
    return true;
}

static bool test_process_publishes_latest_frame(void)
{
    fake_freertos_reset();

    FakeReceiver receiver = {
        .has_frame = true,
        .frame = make_frame(992U, 1234U)
    };

    RcReceiverTask task = { 0 };

    TEST_ASSERT(
        init_task(&task, &receiver),
        "Task initialization should succeed"
    );

    TEST_ASSERT(
        rc_receiver_task_process_once(&task),
        "One receiver processing cycle should succeed"
    );

    RcInputFrame published = { 0 };

    TEST_ASSERT(
        rc_receiver_task_get_latest_frame(
            &task,
            &published),
        "Published frame should be readable"
    );

    TEST_ASSERT(
        receiver.process_calls == 1U,
        "Receiver processing callback should run once"
    );

    TEST_ASSERT(
        published.timestamp_ms == 1234U &&
        published.channels[0] == 992U &&
        published.channel_count == 16U,
        "Frame mailbox should preserve the frame"
    );

    rc_receiver_task_deinit(&task);
    return true;
}

static bool test_notification_wakeup_runs_processing(void)
{
    fake_freertos_reset();

    FakeReceiver receiver = {
        .has_frame = true,
        .frame = make_frame(1200U, 2000U)
    };

    RcReceiverTask task = { 0 };

    TEST_ASSERT(
        init_task(&task, &receiver),
        "Task initialization should succeed"
    );

    fake_freertos_set_notification_count(3U);

    TEST_ASSERT(
        rc_receiver_task_run_iteration(&task),
        "Notified task iteration should succeed"
    );

    RcReceiverTaskStats stats = { 0 };

    TEST_ASSERT(
        rc_receiver_task_get_stats(&task, &stats),
        "Task statistics should be available"
    );

    TEST_ASSERT(
        stats.notification_wakeups == 1U &&
        stats.notifications_received == 3U &&
        stats.timeout_wakeups == 0U,
        "Notification count should be recorded"
    );

    TEST_ASSERT(
        fake_freertos_last_wait_ticks() == 7U,
        "Configured period should be used as wait timeout"
    );

    rc_receiver_task_deinit(&task);
    return true;
}

static bool test_timeout_wakeup_keeps_failsafe_processing_alive(void)
{
    fake_freertos_reset();

    FakeReceiver receiver = { 0 };
    RcReceiverTask task = { 0 };

    TEST_ASSERT(
        init_task(&task, &receiver),
        "Task initialization should succeed"
    );

    fake_freertos_set_notification_count(0U);

    TEST_ASSERT(
        rc_receiver_task_run_iteration(&task),
        "Timeout task iteration should still process receiver"
    );

    RcReceiverTaskStats stats = { 0 };

    TEST_ASSERT(
        rc_receiver_task_get_stats(&task, &stats),
        "Task statistics should be available"
    );

    TEST_ASSERT(
        stats.timeout_wakeups == 1U &&
        stats.process_calls == 1U,
        "Timeout wakeup must run receiver processing"
    );

    TEST_ASSERT(
        receiver.process_calls == 1U,
        "Failsafe processing must not depend on UART events"
    );

    rc_receiver_task_deinit(&task);
    return true;
}

static bool test_isr_notification_only_wakes_task(void)
{
    fake_freertos_reset();

    FakeReceiver receiver = { 0 };
    RcReceiverTask task = { 0 };

    TEST_ASSERT(
        init_task(&task, &receiver),
        "Task initialization should succeed"
    );

    TEST_ASSERT(
        rc_receiver_task_notify_from_isr(&task),
        "ISR notification should succeed"
    );

    TEST_ASSERT(
        receiver.process_calls == 0U,
        "ISR notification must not process the receiver"
    );

    TEST_ASSERT(
        fake_freertos_yield_from_isr_calls() == 1U,
        "ISR should request a context switch when required"
    );

    TEST_ASSERT(
        rc_receiver_task_run_iteration(&task),
        "Notified task should process outside ISR context"
    );

    TEST_ASSERT(
        receiver.process_calls == 1U,
        "Receiver processing should happen in task context"
    );

    rc_receiver_task_deinit(&task);
    return true;
}

static bool test_stack_high_water_mark_is_reported(void)
{
    fake_freertos_reset();
    fake_freertos_set_stack_high_water_mark(173U);

    FakeReceiver receiver = { 0 };
    RcReceiverTask task = { 0 };

    TEST_ASSERT(
        init_task(&task, &receiver),
        "Task initialization should succeed"
    );

    RcReceiverTaskStats stats = { 0 };

    TEST_ASSERT(
        rc_receiver_task_get_stats(&task, &stats),
        "Task statistics should be available"
    );

    TEST_ASSERT(
        stats.minimum_free_stack_words == 173U,
        "Stack high-water mark should be reported in words"
    );

    rc_receiver_task_deinit(&task);
    return true;
}

static bool test_deinit_releases_task_and_queue(void)
{
    fake_freertos_reset();

    FakeReceiver receiver = { 0 };
    RcReceiverTask task = { 0 };

    TEST_ASSERT(
        init_task(&task, &receiver),
        "Task initialization should succeed"
    );

    rc_receiver_task_deinit(&task);

    TEST_ASSERT(
        fake_freertos_task_delete_calls() == 1U,
        "Task should be deleted"
    );

    TEST_ASSERT(
        fake_freertos_queue_delete_calls() == 1U,
        "Frame queue should be deleted"
    );

    TEST_ASSERT(
        !task.initialized,
        "Task object should return to uninitialized state"
    );

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_init_uses_defined_task_configuration()) {
        failed++;
    }

    if (!test_process_publishes_latest_frame()) {
        failed++;
    }

    if (!test_notification_wakeup_runs_processing()) {
        failed++;
    }

    if (!test_timeout_wakeup_keeps_failsafe_processing_alive()) {
        failed++;
    }

    if (!test_isr_notification_only_wakes_task()) {
        failed++;
    }

    if (!test_stack_high_water_mark_is_reported()) {
        failed++;
    }

    if (!test_deinit_releases_task_and_queue()) {
        failed++;
    }

    if (failed != 0) {
        printf(
            "%d RC receiver task test(s) failed\n",
            failed
        );

        return 1;
    }

    printf("All RC receiver task tests passed\n");
    return 0;
}
