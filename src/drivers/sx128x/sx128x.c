#include "sx128x.h"

#include <string.h>

static bool sx128x_port_is_valid(
    const Sx128xPort *port
)
{
    return
        port != NULL &&
        port->spi_transfer != NULL &&
        port->nss_write != NULL &&
        port->reset_write != NULL &&
        port->busy_read != NULL &&
        port->delay_ms != NULL &&
        port->time_ms != NULL &&
        port->take_dio1_event != NULL;
}

static bool sx128x_is_ready(
    const Sx128x *device
)
{
    return
        device != NULL &&
        device->initialized;
}

static bool sx128x_spi_transfer(
    Sx128x *device,
    const uint8_t *tx_data,
    uint8_t *rx_data,
    size_t length
)
{
    return device->port.spi_transfer(
        device->port.context,
        tx_data,
        rx_data,
        length
    );
}

static void sx128x_write_u16_be(
    uint8_t *destination,
    uint16_t value
)
{
    destination[0] = (uint8_t)(value >> 8U);
    destination[1] = (uint8_t)(value & 0x00FFU);
}

static uint16_t sx128x_read_u16_be(
    const uint8_t *source
)
{
    return (uint16_t)(
        ((uint16_t)source[0] << 8U) |
        (uint16_t)source[1]
    );
}

static bool sx128x_timeout_base_is_valid(
    Sx128xTimeoutBase timeout_base
)
{
    return
        timeout_base == SX128X_TIMEOUT_BASE_15_625_US ||
        timeout_base == SX128X_TIMEOUT_BASE_62_5_US ||
        timeout_base == SX128X_TIMEOUT_BASE_1_MS ||
        timeout_base == SX128X_TIMEOUT_BASE_4_MS;
}

static bool sx128x_lora_config_is_valid(
    const Sx128xLoRaConfig *config
)
{
    return
        config != NULL &&
        config->frequency_hz >=
            SX128X_RF_FREQUENCY_MIN_HZ &&
        config->frequency_hz <=
            SX128X_RF_FREQUENCY_MAX_HZ &&
        config->preamble_symbols > 0U &&
        config->payload_length > 0U &&
        config->tx_power_dbm >=
            SX128X_TX_POWER_MIN_DBM &&
        config->tx_power_dbm <=
            SX128X_TX_POWER_MAX_DBM;
}

static Sx128xResult sx128x_set_timeout_command(
    Sx128x *device,
    Sx128xCommand command,
    Sx128xTimeoutBase timeout_base,
    uint16_t timeout_count
)
{
    if (!sx128x_timeout_base_is_valid(timeout_base)) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    uint8_t parameters[3];

    parameters[0] = (uint8_t)timeout_base;
    sx128x_write_u16_be(&parameters[1], timeout_count);

    return sx128x_write_command(
        device,
        command,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_init(
    Sx128x *device,
    const Sx128xPort *port,
    uint32_t busy_timeout_ms
)
{
    if (device == NULL || port == NULL) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    if (!sx128x_port_is_valid(port)) {
        return SX128X_RESULT_INVALID_PORT;
    }

    if (busy_timeout_ms == 0U) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    memset(device, 0, sizeof(*device));

    device->port = *port;
    device->busy_timeout_ms = busy_timeout_ms;
    device->initialized = true;

    device->port.nss_write(
        device->port.context,
        true
    );

    device->port.reset_write(
        device->port.context,
        true
    );

    return SX128X_RESULT_OK;
}

Sx128xResult sx128x_hardware_reset(
    Sx128x *device
)
{
    if (!sx128x_is_ready(device)) {
        return SX128X_RESULT_NOT_INITIALIZED;
    }

    device->port.nss_write(
        device->port.context,
        true
    );

    device->port.reset_write(
        device->port.context,
        false
    );

    device->port.delay_ms(
        device->port.context,
        SX128X_RESET_PULSE_MS
    );

    device->port.reset_write(
        device->port.context,
        true
    );

    device->port.delay_ms(
        device->port.context,
        SX128X_RESET_SETTLE_MS
    );

    return sx128x_wait_while_busy(device);
}

Sx128xResult sx128x_wait_while_busy(
    Sx128x *device
)
{
    if (!sx128x_is_ready(device)) {
        return SX128X_RESULT_NOT_INITIALIZED;
    }

    const uint32_t start_ms = device->port.time_ms(
        device->port.context
    );

    while (device->port.busy_read(
        device->port.context
    )) {
        const uint32_t current_ms =
            device->port.time_ms(
                device->port.context
            );

        if ((uint32_t)(current_ms - start_ms) >=
            device->busy_timeout_ms) {
            return SX128X_RESULT_BUSY_TIMEOUT;
        }

        device->port.delay_ms(
            device->port.context,
            1U
        );
    }

    return SX128X_RESULT_OK;
}

Sx128xResult sx128x_write_command(
    Sx128x *device,
    Sx128xCommand command,
    const uint8_t *parameters,
    size_t parameter_count
)
{
    if (!sx128x_is_ready(device)) {
        return SX128X_RESULT_NOT_INITIALIZED;
    }

    if (parameter_count > 0U && parameters == NULL) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    Sx128xResult result = sx128x_wait_while_busy(device);

    if (result != SX128X_RESULT_OK) {
        return result;
    }

    const uint8_t opcode = (uint8_t)command;

    device->port.nss_write(
        device->port.context,
        false
    );

    if (!sx128x_spi_transfer(
            device,
            &opcode,
            NULL,
            1U)) {
        result = SX128X_RESULT_SPI_ERROR;
    } else if (parameter_count > 0U &&
               !sx128x_spi_transfer(
                   device,
                   parameters,
                   NULL,
                   parameter_count)) {
        result = SX128X_RESULT_SPI_ERROR;
    }

    device->port.nss_write(
        device->port.context,
        true
    );

    return result;
}

Sx128xResult sx128x_read_command(
    Sx128x *device,
    Sx128xCommand command,
    uint8_t *response,
    size_t response_length
)
{
    if (!sx128x_is_ready(device)) {
        return SX128X_RESULT_NOT_INITIALIZED;
    }

    if (response == NULL || response_length == 0U) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    Sx128xResult result = sx128x_wait_while_busy(device);

    if (result != SX128X_RESULT_OK) {
        return result;
    }

    const uint8_t opcode = (uint8_t)command;
    const uint8_t nop = SX128X_SPI_NOP;
    uint8_t ignored = 0U;

    device->port.nss_write(
        device->port.context,
        false
    );

    if (!sx128x_spi_transfer(
            device,
            &opcode,
            &ignored,
            1U)) {
        result = SX128X_RESULT_SPI_ERROR;
    } else if (!sx128x_spi_transfer(
                   device,
                   &nop,
                   &ignored,
                   1U)) {
        result = SX128X_RESULT_SPI_ERROR;
    } else if (!sx128x_spi_transfer(
                   device,
                   NULL,
                   response,
                   response_length)) {
        result = SX128X_RESULT_SPI_ERROR;
    }

    device->port.nss_write(
        device->port.context,
        true
    );

    return result;
}

Sx128xResult sx128x_get_status(
    Sx128x *device,
    uint8_t *status
)
{
    if (!sx128x_is_ready(device)) {
        return SX128X_RESULT_NOT_INITIALIZED;
    }

    if (status == NULL) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    Sx128xResult result = sx128x_wait_while_busy(device);

    if (result != SX128X_RESULT_OK) {
        return result;
    }

    const uint8_t opcode =
        (uint8_t)SX128X_COMMAND_GET_STATUS;

    device->port.nss_write(
        device->port.context,
        false
    );

    if (!sx128x_spi_transfer(
            device,
            &opcode,
            status,
            1U)) {
        result = SX128X_RESULT_SPI_ERROR;
    }

    device->port.nss_write(
        device->port.context,
        true
    );

    return result;
}

Sx128xResult sx128x_set_standby(
    Sx128x *device,
    Sx128xStandbyMode mode
)
{
    if (mode != SX128X_STANDBY_RC &&
        mode != SX128X_STANDBY_XOSC) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    const uint8_t parameter = (uint8_t)mode;

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_STANDBY,
        &parameter,
        1U
    );
}

Sx128xResult sx128x_set_packet_type(
    Sx128x *device,
    Sx128xPacketType packet_type
)
{
    if (packet_type < SX128X_PACKET_TYPE_GFSK ||
        packet_type > SX128X_PACKET_TYPE_BLE) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    const uint8_t parameter = (uint8_t)packet_type;

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_PACKET_TYPE,
        &parameter,
        1U
    );
}

Sx128xResult sx128x_set_rf_frequency(
    Sx128x *device,
    uint32_t frequency_hz
)
{
    if (frequency_hz < SX128X_RF_FREQUENCY_MIN_HZ ||
        frequency_hz > SX128X_RF_FREQUENCY_MAX_HZ) {
        return SX128X_RESULT_OUT_OF_RANGE;
    }

    const uint32_t frequency_register =
        (uint32_t)(
            ((uint64_t)frequency_hz << 18U) /
            SX128X_XTAL_FREQUENCY_HZ
        );

    const uint8_t parameters[3] = {
        (uint8_t)(frequency_register >> 16U),
        (uint8_t)(frequency_register >> 8U),
        (uint8_t)frequency_register
    };

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_RF_FREQUENCY,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_set_buffer_base_address(
    Sx128x *device,
    uint8_t tx_base_address,
    uint8_t rx_base_address
)
{
    const uint8_t parameters[2] = {
        tx_base_address,
        rx_base_address
    };

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_BUFFER_BASE_ADDRESS,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_set_lora_modulation_params(
    Sx128x *device,
    Sx128xLoRaSpreadingFactor spreading_factor,
    Sx128xLoRaBandwidth bandwidth,
    Sx128xLoRaCodingRate coding_rate
)
{
    const uint8_t parameters[3] = {
        (uint8_t)spreading_factor,
        (uint8_t)bandwidth,
        (uint8_t)coding_rate
    };

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_MODULATION_PARAMS,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_set_lora_packet_params(
    Sx128x *device,
    uint8_t preamble_symbols,
    Sx128xLoRaHeaderType header_type,
    uint8_t payload_length,
    Sx128xLoRaCrcMode crc_mode,
    Sx128xLoRaIqMode iq_mode
)
{
    if (preamble_symbols == 0U ||
        payload_length == 0U) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    const uint8_t parameters[5] = {
        preamble_symbols,
        (uint8_t)header_type,
        payload_length,
        (uint8_t)crc_mode,
        (uint8_t)iq_mode
    };

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_PACKET_PARAMS,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_set_tx_params(
    Sx128x *device,
    int8_t power_dbm,
    Sx128xRampTime ramp_time
)
{
    if (power_dbm < SX128X_TX_POWER_MIN_DBM ||
        power_dbm > SX128X_TX_POWER_MAX_DBM) {
        return SX128X_RESULT_OUT_OF_RANGE;
    }

    const uint8_t parameters[2] = {
        (uint8_t)((int16_t)power_dbm + 18),
        (uint8_t)ramp_time
    };

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_TX_PARAMS,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_set_dio_irq_params(
    Sx128x *device,
    uint16_t irq_mask,
    uint16_t dio1_mask,
    uint16_t dio2_mask,
    uint16_t dio3_mask
)
{
    uint8_t parameters[8];

    sx128x_write_u16_be(&parameters[0], irq_mask);
    sx128x_write_u16_be(&parameters[2], dio1_mask);
    sx128x_write_u16_be(&parameters[4], dio2_mask);
    sx128x_write_u16_be(&parameters[6], dio3_mask);

    return sx128x_write_command(
        device,
        SX128X_COMMAND_SET_DIO_IRQ_PARAMS,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_get_irq_status(
    Sx128x *device,
    uint16_t *irq_status
)
{
    if (irq_status == NULL) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    uint8_t response[2];

    const Sx128xResult result = sx128x_read_command(
        device,
        SX128X_COMMAND_GET_IRQ_STATUS,
        response,
        sizeof(response)
    );

    if (result == SX128X_RESULT_OK) {
        *irq_status = sx128x_read_u16_be(response);
    }

    return result;
}

Sx128xResult sx128x_clear_irq_status(
    Sx128x *device,
    uint16_t irq_mask
)
{
    uint8_t parameters[2];

    sx128x_write_u16_be(parameters, irq_mask);

    return sx128x_write_command(
        device,
        SX128X_COMMAND_CLEAR_IRQ_STATUS,
        parameters,
        sizeof(parameters)
    );
}

Sx128xResult sx128x_write_buffer(
    Sx128x *device,
    uint8_t offset,
    const uint8_t *data,
    size_t length
)
{
    if (!sx128x_is_ready(device)) {
        return SX128X_RESULT_NOT_INITIALIZED;
    }

    if (data == NULL ||
        length == 0U ||
        (size_t)offset + length > SX128X_BUFFER_SIZE) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    Sx128xResult result = sx128x_wait_while_busy(device);

    if (result != SX128X_RESULT_OK) {
        return result;
    }

    const uint8_t opcode =
        (uint8_t)SX128X_COMMAND_WRITE_BUFFER;

    device->port.nss_write(
        device->port.context,
        false
    );

    if (!sx128x_spi_transfer(
            device,
            &opcode,
            NULL,
            1U) ||
        !sx128x_spi_transfer(
            device,
            &offset,
            NULL,
            1U) ||
        !sx128x_spi_transfer(
            device,
            data,
            NULL,
            length)) {
        result = SX128X_RESULT_SPI_ERROR;
    }

    device->port.nss_write(
        device->port.context,
        true
    );

    return result;
}

Sx128xResult sx128x_read_buffer(
    Sx128x *device,
    uint8_t offset,
    uint8_t *data,
    size_t length
)
{
    if (!sx128x_is_ready(device)) {
        return SX128X_RESULT_NOT_INITIALIZED;
    }

    if (data == NULL ||
        length == 0U ||
        (size_t)offset + length > SX128X_BUFFER_SIZE) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    Sx128xResult result = sx128x_wait_while_busy(device);

    if (result != SX128X_RESULT_OK) {
        return result;
    }

    const uint8_t opcode =
        (uint8_t)SX128X_COMMAND_READ_BUFFER;
    const uint8_t nop = SX128X_SPI_NOP;
    uint8_t ignored = 0U;

    device->port.nss_write(
        device->port.context,
        false
    );

    if (!sx128x_spi_transfer(
            device,
            &opcode,
            &ignored,
            1U) ||
        !sx128x_spi_transfer(
            device,
            &offset,
            &ignored,
            1U) ||
        !sx128x_spi_transfer(
            device,
            &nop,
            &ignored,
            1U) ||
        !sx128x_spi_transfer(
            device,
            NULL,
            data,
            length)) {
        result = SX128X_RESULT_SPI_ERROR;
    }

    device->port.nss_write(
        device->port.context,
        true
    );

    return result;
}

Sx128xResult sx128x_get_rx_buffer_status(
    Sx128x *device,
    Sx128xRxBufferStatus *status
)
{
    if (status == NULL) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    uint8_t response[2];

    const Sx128xResult result = sx128x_read_command(
        device,
        SX128X_COMMAND_GET_RX_BUFFER_STATUS,
        response,
        sizeof(response)
    );

    if (result == SX128X_RESULT_OK) {
        status->payload_length = response[0];
        status->start_buffer_pointer = response[1];
    }

    return result;
}

Sx128xResult sx128x_get_lora_packet_status(
    Sx128x *device,
    Sx128xLoRaPacketStatus *status
)
{
    if (status == NULL) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    uint8_t response[SX128X_LORA_PACKET_STATUS_SIZE];

    const Sx128xResult result = sx128x_read_command(
        device,
        SX128X_COMMAND_GET_PACKET_STATUS,
        response,
        sizeof(response)
    );

    if (result == SX128X_RESULT_OK) {
        status->rssi_dbm_x2 =
            -(int16_t)response[0];
        status->snr_db_x4 =
            (int16_t)(int8_t)response[1];
    }

    return result;
}

Sx128xResult sx128x_set_tx(
    Sx128x *device,
    Sx128xTimeoutBase timeout_base,
    uint16_t timeout_count
)
{
    return sx128x_set_timeout_command(
        device,
        SX128X_COMMAND_SET_TX,
        timeout_base,
        timeout_count
    );
}

Sx128xResult sx128x_set_rx(
    Sx128x *device,
    Sx128xTimeoutBase timeout_base,
    uint16_t timeout_count
)
{
    return sx128x_set_timeout_command(
        device,
        SX128X_COMMAND_SET_RX,
        timeout_base,
        timeout_count
    );
}

Sx128xResult sx128x_configure_lora(
    Sx128x *device,
    const Sx128xLoRaConfig *config
)
{
    if (!sx128x_lora_config_is_valid(config)) {
        return SX128X_RESULT_INVALID_ARGUMENT;
    }

    Sx128xResult result = sx128x_set_standby(
        device,
        SX128X_STANDBY_RC
    );

    if (result == SX128X_RESULT_OK) {
        result = sx128x_set_packet_type(
            device,
            SX128X_PACKET_TYPE_LORA
        );
    }

    if (result == SX128X_RESULT_OK) {
        result = sx128x_set_rf_frequency(
            device,
            config->frequency_hz
        );
    }

    if (result == SX128X_RESULT_OK) {
        result = sx128x_set_buffer_base_address(
            device,
            config->tx_base_address,
            config->rx_base_address
        );
    }

    if (result == SX128X_RESULT_OK) {
        result = sx128x_set_lora_modulation_params(
            device,
            config->spreading_factor,
            config->bandwidth,
            config->coding_rate
        );
    }

    if (result == SX128X_RESULT_OK) {
        result = sx128x_set_lora_packet_params(
            device,
            config->preamble_symbols,
            config->header_type,
            config->payload_length,
            config->crc_mode,
            config->iq_mode
        );
    }

    if (result == SX128X_RESULT_OK) {
        result = sx128x_set_tx_params(
            device,
            config->tx_power_dbm,
            config->ramp_time
        );
    }

    if (result == SX128X_RESULT_OK) {
        result = sx128x_set_dio_irq_params(
            device,
            config->irq_mask,
            config->dio1_mask,
            SX128X_IRQ_NONE,
            SX128X_IRQ_NONE
        );
    }

    return result;
}

bool sx128x_take_dio1_event(
    Sx128x *device
)
{
    return
        sx128x_is_ready(device) &&
        device->port.take_dio1_event(
            device->port.context
        );
}
