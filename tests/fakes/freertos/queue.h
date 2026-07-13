#ifndef TEST_FAKE_FREERTOS_QUEUE_H
#define TEST_FAKE_FREERTOS_QUEUE_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FakeQueueControl *QueueHandle_t;

QueueHandle_t xQueueCreate(
    UBaseType_t queue_length,
    UBaseType_t item_size
);

BaseType_t xQueueOverwrite(
    QueueHandle_t queue,
    const void *item
);

BaseType_t xQueuePeek(
    QueueHandle_t queue,
    void *out_item,
    TickType_t ticks_to_wait
);

void vQueueDelete(
    QueueHandle_t queue
);

#ifdef __cplusplus
}
#endif

#endif /* TEST_FAKE_FREERTOS_QUEUE_H */
