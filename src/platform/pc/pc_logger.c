#include "radio_logger_if.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

static const char *pc_logger_level_to_string(RadioLogLevel level)
{
    switch (level) {
    case RADIO_LOG_LEVEL_ERROR:
        return "ERROR";
    case RADIO_LOG_LEVEL_WARN:
        return "WARN";
    case RADIO_LOG_LEVEL_INFO:
        return "INFO";
    case RADIO_LOG_LEVEL_DEBUG:
        return "DEBUG";
    case RADIO_LOG_LEVEL_TRACE:
        return "TRACE";
    case RADIO_LOG_LEVEL_NONE:
    default:
        return "NONE";
    }
}

static bool pc_logger_is_enabled(
    RadioLogger *self,
    RadioLogLevel level
)
{
    if (self == NULL) {
        return false;
    }

    if (level == RADIO_LOG_LEVEL_NONE) {
        return false;
    }

    return level <= self->min_level;
}

static void pc_logger_vlog(
    RadioLogger *self,
    RadioLogLevel level,
    const char *tag,
    const char *fmt,
    va_list args
)
{
    if (!pc_logger_is_enabled(self, level)) {
        return;
    }

    printf("[%s] [%s] ", pc_logger_level_to_string(level), tag);
    vprintf(fmt, args);
    printf("\n");
}

static void pc_logger_log(
    RadioLogger *self,
    RadioLogLevel level,
    const char *tag,
    const char *fmt,
    ...
)
{
    va_list args;
    va_start(args, fmt);

    pc_logger_vlog(
        self,
        level,
        tag,
        fmt,
        args
    );

    va_end(args);
}

void pc_logger_init(
    RadioLogger *logger,
    RadioLogLevel min_level
)
{
    if (logger == NULL) {
        return;
    }

    logger->context = NULL;
    logger->min_level = min_level;
    logger->is_enabled = pc_logger_is_enabled;
    logger->vlog = pc_logger_vlog;
    logger->log = pc_logger_log;
}
