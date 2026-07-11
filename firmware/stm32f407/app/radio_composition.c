#include "radio_composition.h"

#include <string.h>

#include "fk407m3_vet6_v1_1_radio.h"
#include "stm32f4_time_backend.h"

bool stm32f407_radio_composition_init(
    Stm32f407RadioComposition *composition,
    const Stm32f407RadioCompositionConfig *config
)
{
    if (composition == NULL ||
        config == NULL ||
        config->receiver_uart == NULL ||
        config->receiver_dma_buffer == NULL ||
        config->receiver_dma_buffer_size == 0U ||
        config->logger_uart == NULL ||
        !fk407m3_vet6_v1_1_validate_receiver_uart(
            config->receiver_uart) ||
        !fk407m3_vet6_v1_1_validate_debug_uart(
            config->logger_uart)) {
        return false;
    }

    memset(composition, 0, sizeof(*composition));

    const Stm32f4UartDmaBackendConfig dma_backend_config = {
        .uart = config->receiver_uart,
        .rx_buffer = config->receiver_dma_buffer,
        .rx_buffer_size = config->receiver_dma_buffer_size,
        .disable_half_transfer_irq = false
    };

    if (!stm32f4_uart_dma_backend_init(
            &composition->uart_dma_backend,
            &dma_backend_config) ||
        !stm32f4_uart_dma_backend_start(
            &composition->uart_dma_backend)) {
        return false;
    }

    const Stm32UartDmaTransportConfig transport_config = {
        .rx_buffer = config->receiver_dma_buffer,
        .rx_buffer_size = config->receiver_dma_buffer_size,
        .backend_context = &composition->uart_dma_backend,
        .get_produced_count =
            stm32f4_uart_dma_backend_get_produced_count
    };

    if (!stm32_uart_dma_transport_init(
            &composition->transport,
            &composition->transport_context,
            &transport_config) ||
        !stm32f4_uart_logger_backend_init(
            &composition->logger_backend,
            config->logger_uart,
            config->logger_timeout_ms) ||
        !stm32_logger_init(
            &composition->logger,
            &composition->logger_context,
            config->log_level,
            stm32f4_uart_logger_backend_write,
            &composition->logger_backend) ||
        !stm32_time_init(
            &composition->time,
            &composition->time_context,
            stm32f4_time_backend_now_ms,
            NULL)) {
        (void)stm32f4_uart_dma_backend_stop(
            &composition->uart_dma_backend);
        return false;
    }

    crsf_protocol_init(
        &composition->protocol,
        &composition->crsf_context
    );

    if (!rc_receiver_service_init(
            &composition->service,
            &composition->transport,
            &composition->protocol,
            &composition->logger,
            &composition->time,
            config->failsafe_timeout_ms)) {
        (void)stm32f4_uart_dma_backend_stop(
            &composition->uart_dma_backend);
        return false;
    }

    composition->initialized = true;
    return true;
}

void stm32f407_radio_composition_process(
    Stm32f407RadioComposition *composition
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    if (stm32f4_uart_dma_backend_process(
            &composition->uart_dma_backend)) {
        /* Discard stale bytes from the failed DMA epoch. */
        stm32_uart_dma_transport_reset(
            &composition->transport_context);

        if (composition->protocol.reset != NULL) {
            composition->protocol.reset(&composition->protocol);
        }
    }

    rc_receiver_service_process(&composition->service);
}

void stm32f407_radio_composition_on_uart_rx_event(
    Stm32f407RadioComposition *composition,
    UART_HandleTypeDef *uart,
    uint16_t dma_position
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    stm32f4_uart_dma_backend_on_rx_event(
        &composition->uart_dma_backend,
        uart,
        dma_position
    );
}

void stm32f407_radio_composition_on_uart_error(
    Stm32f407RadioComposition *composition,
    UART_HandleTypeDef *uart
)
{
    if (composition == NULL || !composition->initialized) {
        return;
    }

    stm32f4_uart_dma_backend_on_error(
        &composition->uart_dma_backend,
        uart
    );
}

bool stm32f407_radio_composition_get_latest_frame(
    Stm32f407RadioComposition *composition,
    RcInputFrame *out_frame
)
{
    if (composition == NULL || !composition->initialized) {
        return false;
    }

    return rc_receiver_service_get_latest_frame(
        &composition->service,
        out_frame
    );
}

bool stm32f407_radio_composition_is_failsafe(
    Stm32f407RadioComposition *composition
)
{
    if (composition == NULL || !composition->initialized) {
        return true;
    }

    return rc_receiver_service_is_failsafe(
        &composition->service
    );
}
