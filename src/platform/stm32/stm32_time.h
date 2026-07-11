#ifndef STM32_TIME_H
#define STM32_TIME_H

#include <stdbool.h>
#include <stdint.h>

#include "radio_time_if.h"

typedef uint32_t (*Stm32TimeNowMsFn)(
    void *backend_context
);

typedef struct
{
    void *backend_context;
    Stm32TimeNowMsFn now_ms;
} Stm32TimeContext;

bool stm32_time_init(
    RadioTime *time,
    Stm32TimeContext *context,
    Stm32TimeNowMsFn now_ms,
    void *backend_context
);

#endif /* STM32_TIME_H */
