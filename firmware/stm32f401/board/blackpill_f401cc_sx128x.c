#include "blackpill_f401cc_sx128x.h"

#include <string.h>

void blackpill_f401cc_sx128x_get_default_pins(
    BlackpillF401ccSx128xPins *pins
)
{
    if (pins == NULL) {
        return;
    }

    memset(pins, 0, sizeof(*pins));

    pins->nss_port = GPIOA;
    pins->nss_pin = GPIO_PIN_4;

    pins->busy_port = GPIOB;
    pins->busy_pin = GPIO_PIN_2;

    pins->dio1_port = GPIOB;
    pins->dio1_pin = GPIO_PIN_1;

    pins->reset_port = GPIOB;
    pins->reset_pin = GPIO_PIN_0;
}

bool blackpill_f401cc_sx128x_validate_spi(
    const SPI_HandleTypeDef *spi
)
{
    return spi != NULL && spi->Instance == SPI1;
}

bool blackpill_f401cc_sx128x_validate_debug_uart(
    const UART_HandleTypeDef *uart
)
{
    return uart != NULL && uart->Instance == USART2;
}
