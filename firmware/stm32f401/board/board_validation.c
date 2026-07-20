#include "board_validation.h"

#include "main.h"
#include "spi.h"
#include "usart.h"

bool stm32f401_board_validation_run(void)
{
    if (hspi1.Instance != SPI1 ||
        huart2.Instance != USART2) {
        return false;
    }

    if (SX128X_NSS_GPIO_Port != GPIOA ||
        SX128X_NSS_Pin != GPIO_PIN_4) {
        return false;
    }

    if (SX128X_RESET_GPIO_Port != GPIOB ||
        SX128X_RESET_Pin != GPIO_PIN_0) {
        return false;
    }

    if (SX128X_DIO1_GPIO_Port != GPIOB ||
        SX128X_DIO1_Pin != GPIO_PIN_1) {
        return false;
    }

    if (SX128X_BUSY_GPIO_Port != GPIOB ||
        SX128X_BUSY_Pin != GPIO_PIN_2) {
        return false;
    }

    return true;
}
