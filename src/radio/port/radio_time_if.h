#ifndef RADIO_TIME_IF_H
#define RADIO_TIME_IF_H

#include <stdint.h>

typedef struct RadioTime RadioTime;

struct RadioTime
{
    void *context;

    uint32_t (*now_ms)(
        RadioTime *self
    );

    uint32_t (*elapsed_ms)(
        RadioTime *self,
        uint32_t since_ms
    );
};

#endif /* RADIO_TIME_IF_H */
