#ifndef TEST_FAKE_FREERTOS_CONTROL_H
#define TEST_FAKE_FREERTOS_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"

void fake_freertos_reset(void);

void fake_freertos_set_notification_count(
    uint32_t notification_count
);

void fake_freertos_set_stack_high_water_mark(
    UBaseType_t stack_words
);

uint32_t fake_freertos_task_create_calls(void);
uint32_t fake_freertos_task_delete_calls(void);
uint32_t fake_freertos_queue_create_calls(void);
uint32_t fake_freertos_queue_delete_calls(void);
uint32_t fake_freertos_yield_from_isr_calls(void);

TickType_t fake_freertos_last_wait_ticks(void);
UBaseType_t fake_freertos_created_priority(void);
configSTACK_DEPTH_TYPE fake_freertos_created_stack_depth(void);
const char *fake_freertos_created_task_name(void);

#endif /* TEST_FAKE_FREERTOS_CONTROL_H */
