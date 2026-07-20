#ifndef SX128X_H
#define SX128X_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sx128x_commands.h"
#include "sx128x_port_if.h"
#include "sx128x_types.h"

#define SX128X_DEFAULT_BUSY_TIMEOUT_MS 20U
#define SX128X_RESET_PULSE_MS 1U
#define SX128X_RESET_SETTLE_MS 10U
#define SX128X_SPI_NOP 0x00U
#define SX128X_XTAL_FREQUENCY_HZ 52000000U
#define SX128X_RF_FREQUENCY_MIN_HZ 2400000000U
#define SX128X_RF_FREQUENCY_MAX_HZ 2500000000U
#define SX128X_BUFFER_SIZE 256U
#define SX128X_LORA_PACKET_STATUS_SIZE 5U
#define SX128X_TX_POWER_MIN_DBM (-18)
#define SX128X_TX_POWER_MAX_DBM 13

#define SX128X_DEFAULT_LINK_IRQ_MASK                  \
    ((uint16_t)(SX128X_IRQ_TX_DONE |                 \
                SX128X_IRQ_RX_DONE |                 \
                SX128X_IRQ_CRC_ERROR |               \
                SX128X_IRQ_RX_TX_TIMEOUT))

typedef struct
{
    Sx128xPort port;
    uint32_t busy_timeout_ms;
    bool initialized;
} Sx128x;

Sx128xResult sx128x_init(
    Sx128x *device,
    const Sx128xPort *port,
    uint32_t busy_timeout_ms
);

Sx128xResult sx128x_hardware_reset(
    Sx128x *device
);

Sx128xResult sx128x_wait_while_busy(
    Sx128x *device
);

Sx128xResult sx128x_write_command(
    Sx128x *device,
    Sx128xCommand command,
    const uint8_t *parameters,
    size_t parameter_count
);

Sx128xResult sx128x_read_command(
    Sx128x *device,
    Sx128xCommand command,
    uint8_t *response,
    size_t response_length
);

Sx128xResult sx128x_get_status(
    Sx128x *device,
    uint8_t *status
);

Sx128xResult sx128x_set_standby(
    Sx128x *device,
    Sx128xStandbyMode mode
);

Sx128xResult sx128x_set_packet_type(
    Sx128x *device,
    Sx128xPacketType packet_type
);

Sx128xResult sx128x_set_rf_frequency(
    Sx128x *device,
    uint32_t frequency_hz
);

Sx128xResult sx128x_set_buffer_base_address(
    Sx128x *device,
    uint8_t tx_base_address,
    uint8_t rx_base_address
);

Sx128xResult sx128x_set_lora_modulation_params(
    Sx128x *device,
    Sx128xLoRaSpreadingFactor spreading_factor,
    Sx128xLoRaBandwidth bandwidth,
    Sx128xLoRaCodingRate coding_rate
);

Sx128xResult sx128x_set_lora_packet_params(
    Sx128x *device,
    uint8_t preamble_symbols,
    Sx128xLoRaHeaderType header_type,
    uint8_t payload_length,
    Sx128xLoRaCrcMode crc_mode,
    Sx128xLoRaIqMode iq_mode
);

Sx128xResult sx128x_set_tx_params(
    Sx128x *device,
    int8_t power_dbm,
    Sx128xRampTime ramp_time
);

Sx128xResult sx128x_set_dio_irq_params(
    Sx128x *device,
    uint16_t irq_mask,
    uint16_t dio1_mask,
    uint16_t dio2_mask,
    uint16_t dio3_mask
);

Sx128xResult sx128x_get_irq_status(
    Sx128x *device,
    uint16_t *irq_status
);

Sx128xResult sx128x_clear_irq_status(
    Sx128x *device,
    uint16_t irq_mask
);

Sx128xResult sx128x_write_buffer(
    Sx128x *device,
    uint8_t offset,
    const uint8_t *data,
    size_t length
);

Sx128xResult sx128x_read_buffer(
    Sx128x *device,
    uint8_t offset,
    uint8_t *data,
    size_t length
);

Sx128xResult sx128x_get_rx_buffer_status(
    Sx128x *device,
    Sx128xRxBufferStatus *status
);

Sx128xResult sx128x_get_lora_packet_status(
    Sx128x *device,
    Sx128xLoRaPacketStatus *status
);

Sx128xResult sx128x_set_tx(
    Sx128x *device,
    Sx128xTimeoutBase timeout_base,
    uint16_t timeout_count
);

Sx128xResult sx128x_set_rx(
    Sx128x *device,
    Sx128xTimeoutBase timeout_base,
    uint16_t timeout_count
);

Sx128xResult sx128x_configure_lora(
    Sx128x *device,
    const Sx128xLoRaConfig *config
);

bool sx128x_take_dio1_event(
    Sx128x *device
);

#endif /* SX128X_H */
