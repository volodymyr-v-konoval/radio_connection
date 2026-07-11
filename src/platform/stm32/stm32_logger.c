#include "stm32_logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool stm32_logger_is_enabled(
    RadioLogger *self,
    RadioLogLevel level
);

static void stm32_logger_vlog(
    RadioLogger *self,
    RadioLogLevel level,
    const char *tag,
    const char *fmt,
    va_list args
);

static void stm32_logger_log(
    RadioLogger *self,
    RadioLogLevel level,
    const char *tag,
    const char *fmt,
    ...
);

static const char *stm32_logger_level_to_string(
    RadioLogLevel level
);

bool stm32_logger_init(
    RadioLogger *logger,
    Stm32LoggerContext *context,
    RadioLogLevel min_level,
    Stm32LoggerWriteFn write,
    void *backend_context
)
{
    if (logger == NULL || context == NULL || write == NULL) {
        return false;
    }

    memset(context, 0, sizeof(*context));
    context->backend_context = backend_context;
    context->write = write;

    logger->context = context;
    logger->min_level = min_level;
    logger->is_enabled = stm32_logger_is_enabled;
    logger->vlog = stm32_logger_vlog;
    logger->log = stm32_logger_log;

    return true;
}

static bool stm32_logger_is_enabled(
    RadioLogger *self,
    RadioLogLevel level
)
{
    if (self == NULL || level == RADIO_LOG_LEVEL_NONE) {
        return false;
    }

    return level <= self->min_level;
}

static void stm32_logger_vlog(
    RadioLogger *self,
    RadioLogLevel level,
    const char *tag,
    const char *fmt,
    va_list args
)
{
    if (!stm32_logger_is_enabled(self, level) ||
        self->context == NULL ||
        fmt == NULL) {
        return;
    }

    Stm32LoggerContext *context =
        (Stm32LoggerContext *)self->context;

    if (context->write == NULL) {
        return;
    }

    const char *safe_tag = tag != NULL ? tag : "RADIO";

    const int prefix_result = snprintf(
        context->buffer,
        sizeof(context->buffer),
        "[%s] [%s] ",
        stm32_logger_level_to_string(level),
        safe_tag
    );

    if (prefix_result < 0) {
        return;
    }

    const size_t max_content = sizeof(context->buffer) - 2U;
    size_t used = (size_t)prefix_result;

    if (used > max_content) {
        used = max_content;
    }

    const size_t message_capacity = max_content - used;
    va_list args_copy;
    va_copy(args_copy, args);

    const int message_result = vsnprintf(
        &context->buffer[used],
        message_capacity + 1U,
        fmt,
        args_copy
    );

    va_end(args_copy);

    if (message_result < 0) {
        return;
    }

    size_t message_length = (size_t)message_result;

    if (message_length > message_capacity) {
        message_length = message_capacity;
    }

    used += message_length;
    context->buffer[used] = '\n';
    used++;
    context->buffer[used] = '\0';

    context->write(
        context->backend_context,
        (const uint8_t *)context->buffer,
        used
    );
}

static void stm32_logger_log(
    RadioLogger *self,
    RadioLogLevel level,
    const char *tag,
    const char *fmt,
    ...
)
{
    va_list args;
    va_start(args, fmt);
    stm32_logger_vlog(self, level, tag, fmt, args);
    va_end(args);
}

static const char *stm32_logger_level_to_string(
    RadioLogLevel level
)
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
