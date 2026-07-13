#ifndef TEST_FAKE_FREERTOS_TASK_H
#define TEST_FAKE_FREERTOS_TASK_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FakeTaskControl *TaskHandle_t;
typedef void (*TaskFunction_t)(void *argument);

BaseType_t xTaskCreate(
    TaskFunction_t task_function,
    const char *task_name,
    configSTACK_DEPTH_TYPE stack_depth,
    void *task_parameter,
    UBaseType_t priority,
    TaskHandle_t *created_task
);

void vTaskDelete(
    TaskHandle_t task
);

BaseType_t xTaskNotifyGive(
    TaskHandle_t task
);

void vTaskNotifyGiveFromISR(
    TaskHandle_t task,
    BaseType_t *higher_priority_task_woken
);

uint32_t ulTaskNotifyTake(
    BaseType_t clear_count_on_exit,
    TickType_t ticks_to_wait
);

UBaseType_t uxTaskGetStackHighWaterMark(
    TaskHandle_t task
);

#ifdef __cplusplus
}
#endif

#endif /* TEST_FAKE_FREERTOS_TASK_H */
