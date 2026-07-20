#ifndef SX128X_TYPES_H
#define SX128X_TYPES_H

#include <stdint.h>

typedef enum
{
    SX128X_RESULT_OK = 0,
    SX128X_RESULT_INVALID_ARGUMENT,
    SX128X_RESULT_INVALID_PORT,
    SX128X_RESULT_NOT_INITIALIZED,
    SX128X_RESULT_BUSY_TIMEOUT,
    SX128X_RESULT_SPI_ERROR,
    SX128X_RESULT_OUT_OF_RANGE
} Sx128xResult;

typedef enum
{
    SX128X_STANDBY_RC = 0x00U,
    SX128X_STANDBY_XOSC = 0x01U
} Sx128xStandbyMode;

typedef enum
{
    SX128X_PACKET_TYPE_GFSK = 0x00U,
    SX128X_PACKET_TYPE_LORA = 0x01U,
    SX128X_PACKET_TYPE_RANGING = 0x02U,
    SX128X_PACKET_TYPE_FLRC = 0x03U,
    SX128X_PACKET_TYPE_BLE = 0x04U
} Sx128xPacketType;

typedef enum
{
    SX128X_LORA_SF5 = 0x50U,
    SX128X_LORA_SF6 = 0x60U,
    SX128X_LORA_SF7 = 0x70U,
    SX128X_LORA_SF8 = 0x80U,
    SX128X_LORA_SF9 = 0x90U,
    SX128X_LORA_SF10 = 0xA0U,
    SX128X_LORA_SF11 = 0xB0U,
    SX128X_LORA_SF12 = 0xC0U
} Sx128xLoRaSpreadingFactor;

typedef enum
{
    SX128X_LORA_BW_203_125_KHZ = 0x34U,
    SX128X_LORA_BW_406_25_KHZ = 0x26U,
    SX128X_LORA_BW_812_5_KHZ = 0x18U,
    SX128X_LORA_BW_1625_KHZ = 0x0AU
} Sx128xLoRaBandwidth;

typedef enum
{
    SX128X_LORA_CR_4_5 = 0x01U,
    SX128X_LORA_CR_4_6 = 0x02U,
    SX128X_LORA_CR_4_7 = 0x03U,
    SX128X_LORA_CR_4_8 = 0x04U,
    SX128X_LORA_CR_LI_4_5 = 0x05U,
    SX128X_LORA_CR_LI_4_6 = 0x06U,
    SX128X_LORA_CR_LI_4_8 = 0x07U
} Sx128xLoRaCodingRate;

typedef enum
{
    SX128X_LORA_HEADER_EXPLICIT = 0x00U,
    SX128X_LORA_HEADER_IMPLICIT = 0x80U
} Sx128xLoRaHeaderType;

typedef enum
{
    SX128X_LORA_CRC_OFF = 0x00U,
    SX128X_LORA_CRC_ON = 0x20U
} Sx128xLoRaCrcMode;

typedef enum
{
    SX128X_LORA_IQ_INVERTED = 0x00U,
    SX128X_LORA_IQ_NORMAL = 0x40U
} Sx128xLoRaIqMode;

typedef enum
{
    SX128X_RAMP_2_US = 0x00U,
    SX128X_RAMP_4_US = 0x20U,
    SX128X_RAMP_6_US = 0x40U,
    SX128X_RAMP_8_US = 0x60U,
    SX128X_RAMP_10_US = 0x80U,
    SX128X_RAMP_12_US = 0xA0U,
    SX128X_RAMP_16_US = 0xC0U,
    SX128X_RAMP_20_US = 0xE0U
} Sx128xRampTime;

typedef enum
{
    SX128X_TIMEOUT_BASE_15_625_US = 0x00U,
    SX128X_TIMEOUT_BASE_62_5_US = 0x01U,
    SX128X_TIMEOUT_BASE_1_MS = 0x02U,
    SX128X_TIMEOUT_BASE_4_MS = 0x03U
} Sx128xTimeoutBase;

typedef enum
{
    SX128X_IRQ_NONE = 0x0000U,
    SX128X_IRQ_TX_DONE = 0x0001U,
    SX128X_IRQ_RX_DONE = 0x0002U,
    SX128X_IRQ_SYNCWORD_VALID = 0x0004U,
    SX128X_IRQ_SYNCWORD_ERROR = 0x0008U,
    SX128X_IRQ_HEADER_VALID = 0x0010U,
    SX128X_IRQ_HEADER_ERROR = 0x0020U,
    SX128X_IRQ_CRC_ERROR = 0x0040U,
    SX128X_IRQ_CAD_DONE = 0x1000U,
    SX128X_IRQ_CAD_DETECTED = 0x2000U,
    SX128X_IRQ_RX_TX_TIMEOUT = 0x4000U,
    SX128X_IRQ_PREAMBLE_DETECTED = 0x8000U,
    SX128X_IRQ_ALL = 0xFFFFU
} Sx128xIrqMask;

typedef struct
{
    uint32_t frequency_hz;
    Sx128xLoRaSpreadingFactor spreading_factor;
    Sx128xLoRaBandwidth bandwidth;
    Sx128xLoRaCodingRate coding_rate;
    uint8_t preamble_symbols;
    Sx128xLoRaHeaderType header_type;
    uint8_t payload_length;
    Sx128xLoRaCrcMode crc_mode;
    Sx128xLoRaIqMode iq_mode;
    int8_t tx_power_dbm;
    Sx128xRampTime ramp_time;
    uint8_t tx_base_address;
    uint8_t rx_base_address;
    uint16_t irq_mask;
    uint16_t dio1_mask;
} Sx128xLoRaConfig;

typedef struct
{
    uint8_t payload_length;
    uint8_t start_buffer_pointer;
} Sx128xRxBufferStatus;

typedef struct
{
    int16_t rssi_dbm_x2;
    int16_t snr_db_x4;
} Sx128xLoRaPacketStatus;

#endif /* SX128X_TYPES_H */
