#include "crsf_crc.h"

#define CRSF_CRC8_DVB_S2_POLY 0xD5U

uint8_t crsf_crc8_dvb_s2(
    const uint8_t *data,
    size_t length
)
{
    uint8_t crc = 0x00U;

    if (data == NULL) {
        return 0U;
    }

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x80U) != 0U) {
                crc = (uint8_t)((crc << 1U) ^ CRSF_CRC8_DVB_S2_POLY);
            } else {
                crc = (uint8_t)(crc << 1U);
            }
        }
    }

    return crc;
}
