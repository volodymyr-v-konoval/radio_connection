#ifndef STM32F401_APP_MAIN_H
#define STM32F401_APP_MAIN_H

#include <stdbool.h>
#include <stdint.h>

bool app_main_init(void);
void app_main_process(void);
void app_main_on_gpio_exti(uint16_t gpio_pin);

#endif /* STM32F401_APP_MAIN_H */
