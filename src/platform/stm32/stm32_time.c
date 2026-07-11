#include "stm32_time.h"

#include <stddef.h>
#include <string.h>

static uint32_t stm32_time_now_ms(
    RadioTime *self
);

static uint32_t stm32_time_elapsed_ms(
    RadioTime *self,
    uint32_t since_ms
);

bool stm32_time_init(
    RadioTime *time,
    Stm32TimeContext *context,
    Stm32TimeNowMsFn now_ms,
    void *backend_context
)
{
    if (time == NULL || context == NULL || now_ms == NULL) {
        return false;
    }

    memset(context, 0, sizeof(*context));
    context->backend_context = backend_context;
    context->now_ms = now_ms;

    time->context = context;
    time->now_ms = stm32_time_now_ms;
    time->elapsed_ms = stm32_time_elapsed_ms;

    return true;
}

static uint32_t stm32_time_now_ms(
    RadioTime *self
)
{
    if (self == NULL || self->context == NULL) {
        return 0U;
    }

    Stm32TimeContext *context =
        (Stm32TimeContext *)self->context;

    if (context->now_ms == NULL) {
        return 0U;
    }

    return context->now_ms(context->backend_context);
}

static uint32_t stm32_time_elapsed_ms(
    RadioTime *self,
    uint32_t since_ms
)
{
    return stm32_time_now_ms(self) - since_ms;
}
