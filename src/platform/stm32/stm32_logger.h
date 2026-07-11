#ifndef STM32_LOGGER_H
#define STM32_LOGGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_logger_if.h"

#define STM32_LOGGER_BUFFER_SIZE 192U

typedef void (*Stm32LoggerWriteFn)(
    void *backend_context,
    const uint8_t *data,
    size_t length
);

typedef struct
{
    void *backend_context;
    Stm32LoggerWriteFn write;
    char buffer[STM32_LOGGER_BUFFER_SIZE];
} Stm32LoggerContext;

bool stm32_logger_init(
    RadioLogger *logger,
    Stm32LoggerContext *context,
    RadioLogLevel min_level,
    Stm32LoggerWriteFn write,
    void *backend_context
);

#endif /* STM32_LOGGER_H */
