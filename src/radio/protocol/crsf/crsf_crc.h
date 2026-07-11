#ifndef CRSF_CRC_H
#define CRSF_CRC_H

#include <stdint.h>
#include <stddef.h>

uint8_t crsf_crc8_dvb_s2(
    const uint8_t *data,
    size_t length
);

#endif /* CRSF_CRC_H */
