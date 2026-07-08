#include "pc_time.h"

#include <time.h>
#include <stddef.h>

static uint32_t pc_time_now_ms(
    RadioTime *self
)
{
    (void)self;

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    uint64_t ms =
        ((uint64_t)ts.tv_sec * 1000ULL) +
        ((uint64_t)ts.tv_nsec / 1000000ULL);

    return (uint32_t)ms;
}

static uint32_t pc_time_elapsed_ms(
    RadioTime *self,
    uint32_t since_ms
)
{
    uint32_t now = pc_time_now_ms(self);

    return now - since_ms;
}

void pc_time_init(
    RadioTime *time
)
{
    if (time == NULL) {
        return;
    }

    time->context = NULL;
    time->now_ms = pc_time_now_ms;
    time->elapsed_ms = pc_time_elapsed_ms;
}
