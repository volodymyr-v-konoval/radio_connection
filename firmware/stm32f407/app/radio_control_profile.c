#include "radio_control_profile.h"

#include <stdint.h>

#define CRSF_RAW_MIN       172U
#define CRSF_RAW_CENTER    992U
#define CRSF_RAW_MAX      1811U

#define STICK_DEADBAND      20U
#define SWITCH_DEADBAND    80U

static const RcChannelMapEntry s_radio_control_profile[] = {
    {
        .source_channel = 1U,
        .function = RC_FUNCTION_ROLL,
        .mode = RC_CHANNEL_MODE_CENTERED,
        .raw_min = CRSF_RAW_MIN,
        .raw_center = CRSF_RAW_CENTER,
        .raw_max = CRSF_RAW_MAX,
        .deadband = STICK_DEADBAND,
        .reversed = false
    },
    {
        .source_channel = 2U,
        .function = RC_FUNCTION_PITCH,
        .mode = RC_CHANNEL_MODE_CENTERED,
        .raw_min = CRSF_RAW_MIN,
        .raw_center = CRSF_RAW_CENTER,
        .raw_max = CRSF_RAW_MAX,
        .deadband = STICK_DEADBAND,
        .reversed = false
    },
    {
        .source_channel = 3U,
        .function = RC_FUNCTION_THROTTLE,
        .mode = RC_CHANNEL_MODE_UNIPOLAR,
        .raw_min = CRSF_RAW_MIN,
        .raw_center = CRSF_RAW_CENTER,
        .raw_max = CRSF_RAW_MAX,
        .deadband = STICK_DEADBAND,
        .reversed = false
    },
    {
        .source_channel = 4U,
        .function = RC_FUNCTION_YAW,
        .mode = RC_CHANNEL_MODE_CENTERED,
        .raw_min = CRSF_RAW_MIN,
        .raw_center = CRSF_RAW_CENTER,
        .raw_max = CRSF_RAW_MAX,
        .deadband = STICK_DEADBAND,
        .reversed = false
    },
    {
        .source_channel = 7U,
        .function = RC_FUNCTION_MODE,
        .mode = RC_CHANNEL_MODE_SWITCH_3POS,
        .raw_min = CRSF_RAW_MIN,
        .raw_center = CRSF_RAW_CENTER,
        .raw_max = CRSF_RAW_MAX,
        .deadband = SWITCH_DEADBAND,
        .reversed = false
    },
    {
        .source_channel = 8U,
        .function = RC_FUNCTION_ARM,
        .mode = RC_CHANNEL_MODE_SWITCH_2POS,
        .raw_min = CRSF_RAW_MIN,
        .raw_center = CRSF_RAW_CENTER,
        .raw_max = CRSF_RAW_MAX,
        .deadband = SWITCH_DEADBAND,
        .reversed = false
    }
};

bool radio_control_profile_init(
    RcChannelMapper *mapper
)
{
    return rc_channel_mapper_init(
        mapper,
        s_radio_control_profile,
        (uint8_t)(
            sizeof(s_radio_control_profile) /
            sizeof(s_radio_control_profile[0])
        )
    );
}
