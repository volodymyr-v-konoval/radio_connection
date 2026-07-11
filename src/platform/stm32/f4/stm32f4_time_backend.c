#include "stm32f4_time_backend.h"

#include "stm32f4xx_hal.h"

uint32_t stm32f4_time_backend_now_ms(
    void *backend_context
)
{
    (void)backend_context;
    return HAL_GetTick();
}
