#ifndef TEST_FAKE_FREERTOS_H
#define TEST_FAKE_FREERTOS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;
typedef uint16_t configSTACK_DEPTH_TYPE;

#define pdFALSE 0
#define pdTRUE  1
#define pdFAIL  0
#define pdPASS  1

#define tskIDLE_PRIORITY ((UBaseType_t)0U)
#define configMINIMAL_STACK_SIZE 128U
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#define pdMS_TO_TICKS(milliseconds) \
    ((TickType_t)(milliseconds))

void fake_freertos_port_yield_from_isr(
    BaseType_t higher_priority_task_woken
);

#define portYIELD_FROM_ISR(value) \
    fake_freertos_port_yield_from_isr((value))

#ifdef __cplusplus
}
#endif

#endif /* TEST_FAKE_FREERTOS_H */
