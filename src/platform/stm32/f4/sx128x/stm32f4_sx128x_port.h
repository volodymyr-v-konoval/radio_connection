#ifndef STM32F4_SX128X_PORT_H
#define STM32F4_SX128X_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "sx128x_port_if.h"

#define STM32F4_SX128X_PORT_SPI_SCRATCH_SIZE 64U

typedef struct
{
    SPI_HandleTypeDef *spi;

    GPIO_TypeDef *nss_port;
    uint16_t nss_pin;

    GPIO_TypeDef *reset_port;
    uint16_t reset_pin;

    GPIO_TypeDef *busy_port;
    uint16_t busy_pin;

    uint16_t dio1_pin;
    uint32_t spi_timeout_ms;
} Stm32f4Sx128xPortConfig;

typedef struct
{
    uint32_t spi_transfer_calls;
    uint32_t spi_hal_calls;
    uint32_t spi_bytes_transferred;
    uint32_t spi_errors;
    uint32_t nss_writes;
    uint32_t reset_writes;
    uint32_t busy_reads;
    uint32_t delay_calls;
    uint32_t dio1_irq_events;
    uint32_t dio1_events_taken;
    uint32_t dio1_coalesced_events;
} Stm32f4Sx128xPortStats;

typedef struct
{
    Stm32f4Sx128xPortConfig config;

    volatile uint32_t dio1_produced_count;
    volatile uint32_t dio1_consumed_count;

    Stm32f4Sx128xPortStats stats;
    bool initialized;
} Stm32f4Sx128xPort;

bool stm32f4_sx128x_port_init(
    Stm32f4Sx128xPort *backend,
    Sx128xPort *port,
    const Stm32f4Sx128xPortConfig *config
);

/*
 * Forward HAL_GPIO_EXTI_Callback() here.
 * This function is ISR-safe: it only records a DIO1 event.
 */
void stm32f4_sx128x_port_on_dio1_irq(
    Stm32f4Sx128xPort *backend,
    uint16_t gpio_pin
);

void stm32f4_sx128x_port_get_stats(
    const Stm32f4Sx128xPort *backend,
    Stm32f4Sx128xPortStats *out_stats
);

#endif /* STM32F4_SX128X_PORT_H */
