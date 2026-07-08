#ifndef RADIO_LOGGER_IF_H
#define RADIO_LOGGER_IF_H

#include <stdarg.h>
#include <stdbool.h>

typedef enum
{
    RADIO_LOG_LEVEL_NONE = 0,
    RADIO_LOG_LEVEL_ERROR,
    RADIO_LOG_LEVEL_WARN,
    RADIO_LOG_LEVEL_INFO,
    RADIO_LOG_LEVEL_DEBUG,
    RADIO_LOG_LEVEL_TRACE
} RadioLogLevel;

typedef struct RadioLogger RadioLogger;

struct RadioLogger
{
    void *context;

    RadioLogLevel min_level;

    bool (*is_enabled)(
        RadioLogger *self,
        RadioLogLevel level
    );

    void (*vlog)(
        RadioLogger *self,
        RadioLogLevel level,
        const char *tag,
        const char *fmt,
        va_list args
    );

    void (*log)(
        RadioLogger *self,
        RadioLogLevel level,
        const char *tag,
        const char *fmt,
        ...
    );
};

#endif /* RADIO_LOGGER_IF_H */
