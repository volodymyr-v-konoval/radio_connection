#include "rc_channel_mapper.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(condition, message)             \
    do {                                            \
        if (!(condition)) {                         \
            printf("FAIL: %s\n", (message));        \
            return false;                           \
        }                                           \
    } while (0)

static RcChannelMapEntry make_entry(
    uint8_t source_channel,
    RcLogicalFunction function,
    RcChannelMode mode,
    bool reversed
)
{
    const RcChannelMapEntry entry = {
        .source_channel = source_channel,
        .function = function,
        .mode = mode,
        .raw_min = 172U,
        .raw_center = 992U,
        .raw_max = 1811U,
        .deadband = 20U,
        .reversed = reversed
    };

    return entry;
}

static RcInputFrame make_valid_frame(void)
{
    RcInputFrame frame;

    memset(&frame, 0, sizeof(frame));

    frame.channel_count = 16U;
    frame.frame_valid = true;
    frame.frame_lost = false;
    frame.failsafe = false;
    frame.protocol = RADIO_PROTOCOL_CRSF;
    frame.timestamp_ms = 1234U;

    for (uint8_t i = 0U; i < frame.channel_count; i++) {
        frame.channels[i] = 992U;
    }

    return frame;
}

static bool test_centered_channel_normalization(void)
{
    const RcChannelMapEntry entry = make_entry(
        1U,
        RC_FUNCTION_ROLL,
        RC_CHANNEL_MODE_CENTERED,
        false
    );

    RcChannelMapper mapper;

    TEST_ASSERT(
        rc_channel_mapper_init(&mapper, &entry, 1U),
        "Centered mapper should initialize"
    );

    RcInputFrame frame = make_valid_frame();
    RcMappedInput mapped;

    frame.channels[0] = 172U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Minimum centered value should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_ROLL] == -1000,
        "Centered minimum should normalize to -1000"
    );

    TEST_ASSERT(
        mapped.pulse_us[RC_FUNCTION_ROLL] == 1000U,
        "Centered minimum should become 1000 us"
    );

    frame.channels[0] = 992U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Centered midpoint should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_ROLL] == 0,
        "Centered midpoint should normalize to zero"
    );

    TEST_ASSERT(
        mapped.pulse_us[RC_FUNCTION_ROLL] == 1500U,
        "Centered midpoint should become 1500 us"
    );

    frame.channels[0] = 1811U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Maximum centered value should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_ROLL] == 1000,
        "Centered maximum should normalize to +1000"
    );

    TEST_ASSERT(
        mapped.pulse_us[RC_FUNCTION_ROLL] == 2000U,
        "Centered maximum should become 2000 us"
    );

    return true;
}

static bool test_centered_deadband(void)
{
    const RcChannelMapEntry entry = make_entry(
        1U,
        RC_FUNCTION_PITCH,
        RC_CHANNEL_MODE_CENTERED,
        false
    );

    RcChannelMapper mapper;

    TEST_ASSERT(
        rc_channel_mapper_init(&mapper, &entry, 1U),
        "Deadband mapper should initialize"
    );

    RcInputFrame frame = make_valid_frame();
    RcMappedInput mapped;

    frame.channels[0] = 1000U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Deadband value should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_PITCH] == 0,
        "Value inside deadband should normalize to zero"
    );

    return true;
}

static bool test_reversed_centered_channel(void)
{
    const RcChannelMapEntry entry = make_entry(
        1U,
        RC_FUNCTION_YAW,
        RC_CHANNEL_MODE_CENTERED,
        true
    );

    RcChannelMapper mapper;

    TEST_ASSERT(
        rc_channel_mapper_init(&mapper, &entry, 1U),
        "Reversed mapper should initialize"
    );

    RcInputFrame frame = make_valid_frame();
    RcMappedInput mapped;

    frame.channels[0] = 172U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Reversed value should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_YAW] == 1000,
        "Reversed minimum should become +1000"
    );

    return true;
}

static bool test_unipolar_throttle(void)
{
    const RcChannelMapEntry entry = make_entry(
        3U,
        RC_FUNCTION_THROTTLE,
        RC_CHANNEL_MODE_UNIPOLAR,
        false
    );

    RcChannelMapper mapper;

    TEST_ASSERT(
        rc_channel_mapper_init(&mapper, &entry, 1U),
        "Throttle mapper should initialize"
    );

    RcInputFrame frame = make_valid_frame();
    RcMappedInput mapped;

    frame.channels[2] = 172U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Minimum throttle should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_THROTTLE] == 0,
        "Minimum throttle should normalize to zero"
    );

    TEST_ASSERT(
        mapped.pulse_us[RC_FUNCTION_THROTTLE] == 1000U,
        "Minimum throttle should become 1000 us"
    );

    frame.channels[2] = 1811U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Maximum throttle should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_THROTTLE] == 1000,
        "Maximum throttle should normalize to 1000"
    );

    TEST_ASSERT(
        mapped.pulse_us[RC_FUNCTION_THROTTLE] == 2000U,
        "Maximum throttle should become 2000 us"
    );

    return true;
}

static bool test_switch_mapping(void)
{
    const RcChannelMapEntry entries[] = {
        {
            .source_channel = 7U,
            .function = RC_FUNCTION_MODE,
            .mode = RC_CHANNEL_MODE_SWITCH_3POS,
            .raw_min = 172U,
            .raw_center = 992U,
            .raw_max = 1811U,
            .deadband = 80U,
            .reversed = false
        },
        {
            .source_channel = 8U,
            .function = RC_FUNCTION_ARM,
            .mode = RC_CHANNEL_MODE_SWITCH_2POS,
            .raw_min = 172U,
            .raw_center = 992U,
            .raw_max = 1811U,
            .deadband = 80U,
            .reversed = false
        }
    };

    RcChannelMapper mapper;

    TEST_ASSERT(
        rc_channel_mapper_init(&mapper, entries, 2U),
        "Switch mapper should initialize"
    );

    RcInputFrame frame = make_valid_frame();
    RcMappedInput mapped;

    frame.channels[6] = 172U;
    frame.channels[7] = 172U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Low switches should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_MODE] == -1000,
        "Low mode should become -1000"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_ARM] == 0,
        "Low arm should become zero"
    );

    frame.channels[6] = 992U;
    frame.channels[7] = 1811U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Center and high switches should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_MODE] == 0,
        "Center mode should become zero"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_ARM] == 1000,
        "High arm should become 1000"
    );

    frame.channels[6] = 1811U;

    TEST_ASSERT(
        rc_channel_mapper_map(&mapper, &frame, &mapped),
        "High mode should map"
    );

    TEST_ASSERT(
        mapped.normalized[RC_FUNCTION_MODE] == 1000,
        "High mode should become +1000"
    );

    return true;
}

static bool test_failsafe_frame_is_rejected(void)
{
    const RcChannelMapEntry entry = make_entry(
        1U,
        RC_FUNCTION_ROLL,
        RC_CHANNEL_MODE_CENTERED,
        false
    );

    RcChannelMapper mapper;

    TEST_ASSERT(
        rc_channel_mapper_init(&mapper, &entry, 1U),
        "Failsafe mapper should initialize"
    );

    RcInputFrame frame = make_valid_frame();
    RcMappedInput mapped;

    frame.failsafe = true;
    frame.frame_valid = false;
    frame.frame_lost = true;

    TEST_ASSERT(
        !rc_channel_mapper_map(&mapper, &frame, &mapped),
        "Failsafe frame must not produce mapped controls"
    );

    TEST_ASSERT(
        mapped.available_mask == 0U,
        "Failsafe frame must expose no logical controls"
    );

    return true;
}

static bool test_invalid_configuration_is_rejected(void)
{
    RcChannelMapEntry entries[] = {
        make_entry(
            1U,
            RC_FUNCTION_ROLL,
            RC_CHANNEL_MODE_CENTERED,
            false
        ),
        make_entry(
            2U,
            RC_FUNCTION_ROLL,
            RC_CHANNEL_MODE_CENTERED,
            false
        )
    };

    RcChannelMapper mapper;

    TEST_ASSERT(
        !rc_channel_mapper_init(&mapper, entries, 2U),
        "Duplicate logical function must be rejected"
    );

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_centered_channel_normalization()) {
        failed++;
    }

    if (!test_centered_deadband()) {
        failed++;
    }

    if (!test_reversed_centered_channel()) {
        failed++;
    }

    if (!test_unipolar_throttle()) {
        failed++;
    }

    if (!test_switch_mapping()) {
        failed++;
    }

    if (!test_failsafe_frame_is_rejected()) {
        failed++;
    }

    if (!test_invalid_configuration_is_rejected()) {
        failed++;
    }

    if (failed != 0) {
        printf("%d RC channel mapper test(s) failed\n", failed);
        return 1;
    }

    printf("All RC channel mapper tests passed\n");
    return 0;
}
