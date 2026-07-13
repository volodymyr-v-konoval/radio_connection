#include "fake_freertos.h"

#include <stddef.h>
#include <string.h>

#include "queue.h"
#include "task.h"

#define FAKE_FREERTOS_MAX_QUEUE_ITEM_SIZE 256U

struct FakeTaskControl
{
    TaskFunction_t task_function;
    void *task_parameter;
    const char *task_name;
    configSTACK_DEPTH_TYPE stack_depth;
    UBaseType_t priority;
    uint32_t notification_count;
    bool in_use;
};

struct FakeQueueControl
{
    uint8_t storage[FAKE_FREERTOS_MAX_QUEUE_ITEM_SIZE];
    UBaseType_t item_size;
    bool has_item;
    bool in_use;
};

typedef struct
{
    struct FakeTaskControl task;
    struct FakeQueueControl queue;

    UBaseType_t stack_high_water_mark;
    TickType_t last_wait_ticks;

    uint32_t task_create_calls;
    uint32_t task_delete_calls;
    uint32_t queue_create_calls;
    uint32_t queue_delete_calls;
    uint32_t yield_from_isr_calls;
} FakeFreeRtosState;

static FakeFreeRtosState s_state;

void fake_freertos_reset(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.stack_high_water_mark = 256U;
}

void fake_freertos_set_notification_count(
    uint32_t notification_count
)
{
    s_state.task.notification_count = notification_count;
}

void fake_freertos_set_stack_high_water_mark(
    UBaseType_t stack_words
)
{
    s_state.stack_high_water_mark = stack_words;
}

uint32_t fake_freertos_task_create_calls(void)
{
    return s_state.task_create_calls;
}

uint32_t fake_freertos_task_delete_calls(void)
{
    return s_state.task_delete_calls;
}

uint32_t fake_freertos_queue_create_calls(void)
{
    return s_state.queue_create_calls;
}

uint32_t fake_freertos_queue_delete_calls(void)
{
    return s_state.queue_delete_calls;
}

uint32_t fake_freertos_yield_from_isr_calls(void)
{
    return s_state.yield_from_isr_calls;
}

TickType_t fake_freertos_last_wait_ticks(void)
{
    return s_state.last_wait_ticks;
}

UBaseType_t fake_freertos_created_priority(void)
{
    return s_state.task.priority;
}

configSTACK_DEPTH_TYPE fake_freertos_created_stack_depth(void)
{
    return s_state.task.stack_depth;
}

const char *fake_freertos_created_task_name(void)
{
    return s_state.task.task_name;
}

BaseType_t xTaskCreate(
    TaskFunction_t task_function,
    const char *task_name,
    configSTACK_DEPTH_TYPE stack_depth,
    void *task_parameter,
    UBaseType_t priority,
    TaskHandle_t *created_task
)
{
    s_state.task_create_calls++;

    if (task_function == NULL ||
        task_name == NULL ||
        created_task == NULL ||
        s_state.task.in_use) {
        return pdFAIL;
    }

    s_state.task.task_function = task_function;
    s_state.task.task_parameter = task_parameter;
    s_state.task.task_name = task_name;
    s_state.task.stack_depth = stack_depth;
    s_state.task.priority = priority;
    s_state.task.in_use = true;

    *created_task = &s_state.task;
    return pdPASS;
}

void vTaskDelete(
    TaskHandle_t task
)
{
    if (task == &s_state.task && s_state.task.in_use) {
        s_state.task_delete_calls++;
        memset(&s_state.task, 0, sizeof(s_state.task));
    }
}

BaseType_t xTaskNotifyGive(
    TaskHandle_t task
)
{
    if (task == NULL || !task->in_use) {
        return pdFAIL;
    }

    task->notification_count++;
    return pdPASS;
}

void vTaskNotifyGiveFromISR(
    TaskHandle_t task,
    BaseType_t *higher_priority_task_woken
)
{
    if (task != NULL && task->in_use) {
        task->notification_count++;

        if (higher_priority_task_woken != NULL) {
            *higher_priority_task_woken = pdTRUE;
        }
    }
}

uint32_t ulTaskNotifyTake(
    BaseType_t clear_count_on_exit,
    TickType_t ticks_to_wait
)
{
    s_state.last_wait_ticks = ticks_to_wait;

    const uint32_t notification_count =
        s_state.task.notification_count;

    if (notification_count == 0U) {
        return 0U;
    }

    if (clear_count_on_exit == pdTRUE) {
        s_state.task.notification_count = 0U;
    } else {
        s_state.task.notification_count--;
    }

    return notification_count;
}

UBaseType_t uxTaskGetStackHighWaterMark(
    TaskHandle_t task
)
{
    if (task == NULL || !task->in_use) {
        return 0U;
    }

    return s_state.stack_high_water_mark;
}

QueueHandle_t xQueueCreate(
    UBaseType_t queue_length,
    UBaseType_t item_size
)
{
    s_state.queue_create_calls++;

    if (queue_length != 1U ||
        item_size == 0U ||
        item_size > FAKE_FREERTOS_MAX_QUEUE_ITEM_SIZE ||
        s_state.queue.in_use) {
        return NULL;
    }

    memset(&s_state.queue, 0, sizeof(s_state.queue));
    s_state.queue.item_size = item_size;
    s_state.queue.in_use = true;

    return &s_state.queue;
}

BaseType_t xQueueOverwrite(
    QueueHandle_t queue,
    const void *item
)
{
    if (queue == NULL ||
        !queue->in_use ||
        item == NULL) {
        return pdFAIL;
    }

    memcpy(queue->storage, item, queue->item_size);
    queue->has_item = true;
    return pdPASS;
}

BaseType_t xQueuePeek(
    QueueHandle_t queue,
    void *out_item,
    TickType_t ticks_to_wait
)
{
    (void)ticks_to_wait;

    if (queue == NULL ||
        !queue->in_use ||
        !queue->has_item ||
        out_item == NULL) {
        return pdFAIL;
    }

    memcpy(out_item, queue->storage, queue->item_size);
    return pdPASS;
}

void vQueueDelete(
    QueueHandle_t queue
)
{
    if (queue == &s_state.queue && s_state.queue.in_use) {
        s_state.queue_delete_calls++;
        memset(&s_state.queue, 0, sizeof(s_state.queue));
    }
}

void fake_freertos_port_yield_from_isr(
    BaseType_t higher_priority_task_woken
)
{
    if (higher_priority_task_woken == pdTRUE) {
        s_state.yield_from_isr_calls++;
    }
}
