#ifndef LINK_CRC16_H
#define LINK_CRC16_H

#include <stddef.h>
#include <stdint.h>

#define LINK_CRC16_CCITT_FALSE_INIT 0xFFFFU
#define LINK_CRC16_CCITT_FALSE_POLY 0x1021U

uint16_t link_crc16_ccitt_false(
    const uint8_t *data,
    size_t length
);

#endif /* LINK_CRC16_H */
