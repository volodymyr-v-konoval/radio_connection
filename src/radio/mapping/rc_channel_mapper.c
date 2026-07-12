#include "rc_channel_mapper.h"

#include <stddef.h>
#include <string.h>

static bool rc_channel_map_entry_is_valid(
    const RcChannelMapEntry *entry
);

static bool rc_channel_mapper_has_duplicate_function(
    const RcChannelMapEntry *entries,
    uint8_t entry_index
);

static uint16_t rc_clamp_raw(
    uint16_t raw_value,
    uint16_t raw_min,
    uint16_t raw_max
);

static int16_t rc_normalize_centered(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
);

static int16_t rc_normalize_unipolar(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
);

static int16_t rc_normalize_switch_2pos(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
);

static int16_t rc_normalize_switch_3pos(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
);

static int16_t rc_apply_reverse(
    int16_t normalized,
    RcChannelMode mode,
    bool reversed
);

static uint16_t rc_normalized_to_pulse_us(
    int16_t normalized,
    RcChannelMode mode
);

bool rc_channel_mapper_init(
    RcChannelMapper *mapper,
    const RcChannelMapEntry *entries,
    uint8_t entry_count
)
{
    if (mapper == NULL ||
        entries == NULL ||
        entry_count == 0U ||
        entry_count > RC_CHANNEL_MAPPER_MAX_ENTRIES) {
        return false;
    }

    memset(mapper, 0, sizeof(*mapper));

    for (uint8_t i = 0U; i < entry_count; i++) {
        if (!rc_channel_map_entry_is_valid(&entries[i])) {
            return false;
        }

        if (rc_channel_mapper_has_duplicate_function(
                entries,
                i)) {
            return false;
        }

        mapper->entries[i] = entries[i];
    }

    mapper->entry_count = entry_count;
    mapper->initialized = true;

    return true;
}

bool rc_channel_mapper_map(
    const RcChannelMapper *mapper,
    const RcInputFrame *input,
    RcMappedInput *output
)
{
    if (mapper == NULL ||
        input == NULL ||
        output == NULL ||
        !mapper->initialized) {
        return false;
    }

    memset(output, 0, sizeof(*output));

    output->frame_valid = input->frame_valid;
    output->frame_lost = input->frame_lost;
    output->failsafe = input->failsafe;
    output->timestamp_ms = input->timestamp_ms;
    output->protocol = input->protocol;

    /*
     * Never publish mapped control values from an invalid,
     * lost, or failsafe frame.
     */
    if (!input->frame_valid ||
        input->frame_lost ||
        input->failsafe) {
        return false;
    }

    for (uint8_t i = 0U; i < mapper->entry_count; i++) {
        const RcChannelMapEntry *entry =
            &mapper->entries[i];

        const uint8_t channel_index =
            (uint8_t)(entry->source_channel - 1U);

        if (channel_index >= input->channel_count ||
            channel_index >= RC_CHANNEL_MAPPER_MAX_CHANNELS) {
            memset(output, 0, sizeof(*output));
            output->frame_valid = input->frame_valid;
            output->frame_lost = input->frame_lost;
            output->failsafe = input->failsafe;
            output->timestamp_ms = input->timestamp_ms;
            output->protocol = input->protocol;

            return false;
        }

        const uint16_t raw_value =
            input->channels[channel_index];

        int16_t normalized = 0;

        switch (entry->mode) {
        case RC_CHANNEL_MODE_CENTERED:
            normalized = rc_normalize_centered(
                raw_value,
                entry
            );
            break;

        case RC_CHANNEL_MODE_UNIPOLAR:
            normalized = rc_normalize_unipolar(
                raw_value,
                entry
            );
            break;

        case RC_CHANNEL_MODE_SWITCH_2POS:
            normalized = rc_normalize_switch_2pos(
                raw_value,
                entry
            );
            break;

        case RC_CHANNEL_MODE_SWITCH_3POS:
            normalized = rc_normalize_switch_3pos(
                raw_value,
                entry
            );
            break;

        default:
            return false;
        }

        normalized = rc_apply_reverse(
            normalized,
            entry->mode,
            entry->reversed
        );

        output->normalized[entry->function] =
            normalized;

        output->pulse_us[entry->function] =
            rc_normalized_to_pulse_us(
                normalized,
                entry->mode
            );

        output->available_mask |=
            (uint32_t)(1UL << (uint32_t)entry->function);
    }

    return true;
}

bool rc_mapped_input_get(
    const RcMappedInput *input,
    RcLogicalFunction function,
    int16_t *normalized,
    uint16_t *pulse_us
)
{
    if (input == NULL ||
        normalized == NULL ||
        pulse_us == NULL ||
        function <= RC_FUNCTION_NONE ||
        function >= RC_FUNCTION_COUNT) {
        return false;
    }

    const uint32_t function_bit =
        (uint32_t)(1UL << (uint32_t)function);

    if ((input->available_mask & function_bit) == 0U) {
        return false;
    }

    *normalized = input->normalized[function];
    *pulse_us = input->pulse_us[function];

    return true;
}

static bool rc_channel_map_entry_is_valid(
    const RcChannelMapEntry *entry
)
{
    if (entry == NULL) {
        return false;
    }

    if (entry->source_channel == 0U ||
        entry->source_channel >
            RC_CHANNEL_MAPPER_MAX_CHANNELS) {
        return false;
    }

    if (entry->function <= RC_FUNCTION_NONE ||
        entry->function >= RC_FUNCTION_COUNT) {
        return false;
    }

    if (entry->mode > RC_CHANNEL_MODE_SWITCH_3POS) {
        return false;
    }

    if (entry->raw_min >= entry->raw_center ||
        entry->raw_center >= entry->raw_max) {
        return false;
    }

    const uint16_t lower_span =
        (uint16_t)(entry->raw_center - entry->raw_min);

    const uint16_t upper_span =
        (uint16_t)(entry->raw_max - entry->raw_center);

    if (entry->deadband >= lower_span ||
        entry->deadband >= upper_span) {
        return false;
    }

    return true;
}

static bool rc_channel_mapper_has_duplicate_function(
    const RcChannelMapEntry *entries,
    uint8_t entry_index
)
{
    for (uint8_t i = 0U; i < entry_index; i++) {
        if (entries[i].function ==
            entries[entry_index].function) {
            return true;
        }
    }

    return false;
}

static uint16_t rc_clamp_raw(
    uint16_t raw_value,
    uint16_t raw_min,
    uint16_t raw_max
)
{
    if (raw_value < raw_min) {
        return raw_min;
    }

    if (raw_value > raw_max) {
        return raw_max;
    }

    return raw_value;
}

static int16_t rc_normalize_centered(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
)
{
    const uint16_t raw = rc_clamp_raw(
        raw_value,
        entry->raw_min,
        entry->raw_max
    );

    const uint16_t lower_deadband =
        (uint16_t)(entry->raw_center - entry->deadband);

    const uint16_t upper_deadband =
        (uint16_t)(entry->raw_center + entry->deadband);

    if (raw >= lower_deadband &&
        raw <= upper_deadband) {
        return RC_NORMALIZED_CENTER;
    }

    if (raw < lower_deadband) {
        const int32_t distance =
            (int32_t)lower_deadband - (int32_t)raw;

        const int32_t span =
            (int32_t)lower_deadband -
            (int32_t)entry->raw_min;

        const int32_t normalized =
            -(distance * RC_NORMALIZED_MAX) / span;

        return (int16_t)normalized;
    }

    const int32_t distance =
        (int32_t)raw - (int32_t)upper_deadband;

    const int32_t span =
        (int32_t)entry->raw_max -
        (int32_t)upper_deadband;

    const int32_t normalized =
        (distance * RC_NORMALIZED_MAX) / span;

    return (int16_t)normalized;
}

static int16_t rc_normalize_unipolar(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
)
{
    const uint16_t raw = rc_clamp_raw(
        raw_value,
        entry->raw_min,
        entry->raw_max
    );

    const uint16_t active_min =
        (uint16_t)(entry->raw_min + entry->deadband);

    if (raw <= active_min) {
        return 0;
    }

    const int32_t distance =
        (int32_t)raw - (int32_t)active_min;

    const int32_t span =
        (int32_t)entry->raw_max -
        (int32_t)active_min;

    const int32_t normalized =
        (distance * RC_NORMALIZED_MAX) / span;

    return (int16_t)normalized;
}

static int16_t rc_normalize_switch_2pos(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
)
{
    return raw_value >= entry->raw_center
        ? RC_NORMALIZED_MAX
        : 0;
}

static int16_t rc_normalize_switch_3pos(
    uint16_t raw_value,
    const RcChannelMapEntry *entry
)
{
    const uint16_t lower_threshold =
        (uint16_t)(entry->raw_center - entry->deadband);

    const uint16_t upper_threshold =
        (uint16_t)(entry->raw_center + entry->deadband);

    if (raw_value < lower_threshold) {
        return RC_NORMALIZED_MIN;
    }

    if (raw_value > upper_threshold) {
        return RC_NORMALIZED_MAX;
    }

    return RC_NORMALIZED_CENTER;
}

static int16_t rc_apply_reverse(
    int16_t normalized,
    RcChannelMode mode,
    bool reversed
)
{
    if (!reversed) {
        return normalized;
    }

    if (mode == RC_CHANNEL_MODE_UNIPOLAR ||
        mode == RC_CHANNEL_MODE_SWITCH_2POS) {
        return (int16_t)(
            RC_NORMALIZED_MAX - normalized
        );
    }

    return (int16_t)(-normalized);
}

static uint16_t rc_normalized_to_pulse_us(
    int16_t normalized,
    RcChannelMode mode
)
{
    int32_t pulse_us;

    if (mode == RC_CHANNEL_MODE_UNIPOLAR ||
        mode == RC_CHANNEL_MODE_SWITCH_2POS) {
        pulse_us =
            (int32_t)RC_PULSE_MIN_US +
            (int32_t)normalized;
    } else {
        pulse_us =
            (int32_t)RC_PULSE_CENTER_US +
            ((int32_t)normalized / 2);
    }

    if (pulse_us < (int32_t)RC_PULSE_MIN_US) {
        pulse_us = RC_PULSE_MIN_US;
    }

    if (pulse_us > (int32_t)RC_PULSE_MAX_US) {
        pulse_us = RC_PULSE_MAX_US;
    }

    return (uint16_t)pulse_us;
}
