#ifndef BLACKPILL_F401CC_SX128X_H
#define BLACKPILL_F401CC_SX128X_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

typedef struct
{
    GPIO_TypeDef *nss_port;
    uint16_t nss_pin;

    GPIO_TypeDef *busy_port;
    uint16_t busy_pin;

    GPIO_TypeDef *dio1_port;
    uint16_t dio1_pin;

    GPIO_TypeDef *reset_port;
    uint16_t reset_pin;
} BlackpillF401ccSx128xPins;

void blackpill_f401cc_sx128x_get_default_pins(
    BlackpillF401ccSx128xPins *pins
);

bool blackpill_f401cc_sx128x_validate_spi(
    const SPI_HandleTypeDef *spi
);

bool blackpill_f401cc_sx128x_validate_debug_uart(
    const UART_HandleTypeDef *uart
);

#endif /* BLACKPILL_F401CC_SX128X_H */
