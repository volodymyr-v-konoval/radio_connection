#ifndef PC_LOGGER_H
#define PC_LOGGER_H

#include "radio_logger_if.h"

void pc_logger_init(
    RadioLogger *logger,
    RadioLogLevel min_level
);

#endif /* PC_LOGGER_H */
