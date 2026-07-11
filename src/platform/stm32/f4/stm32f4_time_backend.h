#ifndef STM32F4_TIME_BACKEND_H
#define STM32F4_TIME_BACKEND_H

#include <stdint.h>

uint32_t stm32f4_time_backend_now_ms(
    void *backend_context
);

#endif /* STM32F4_TIME_BACKEND_H */
