#ifndef RC_CONTROL_POLICY_H
#define RC_CONTROL_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "rc_channel_mapper.h"

typedef enum
{
    RC_CONTROL_STATE_SAFE = 0,
    RC_CONTROL_STATE_READY,
    RC_CONTROL_STATE_ACTIVE
} RcControlState;

typedef enum
{
    RC_CONTROL_REASON_NONE = 0,

    RC_CONTROL_REASON_NO_INPUT,
    RC_CONTROL_REASON_INVALID_FRAME,
    RC_CONTROL_REASON_FRAME_LOST,
    RC_CONTROL_REASON_FAILSAFE,
    RC_CONTROL_REASON_MAPPING_INCOMPLETE,

    RC_CONTROL_REASON_ARM_RELEASE_REQUIRED,
    RC_CONTROL_REASON_THROTTLE_NOT_LOW,
    RC_CONTROL_REASON_DISARMED
} RcControlSafetyReason;

typedef struct
{
    /*
     * Arm is considered ON at or above arm_on_threshold.
     */
    int16_t arm_on_threshold;

    /*
     * Arm is considered OFF at or below arm_off_threshold.
     *
     * Using two thresholds adds hysteresis and prevents unstable
     * arm/disarm transitions near one value.
     */
    int16_t arm_off_threshold;

    /*
     * Arming is allowed only when normalized throttle is at or below
     * this value.
     */
    int16_t throttle_arm_max;

    /*
     * After startup, failsafe, frame loss, or invalid input, the arm
     * switch must be moved to OFF before arming is allowed again.
     */
    bool require_arm_release_after_failsafe;
} RcControlPolicyConfig;

typedef struct
{
    RcControlPolicyConfig config;

    bool initialized;
    bool armed_latched;
    bool arm_release_required;
} RcControlPolicy;

typedef struct
{
    /*
     * Final output values.
     *
     * They are forced to zero whenever outputs_enabled is false.
     */
    int16_t roll;
    int16_t pitch;
    int16_t throttle;
    int16_t yaw;
    int16_t mode;

    bool input_valid;
    bool armed;
    bool outputs_enabled;
    bool safe_state_active;

    RcControlState state;
    RcControlSafetyReason reason;

    uint32_t timestamp_ms;
} RcControlCommand;

bool rc_control_policy_init(
    RcControlPolicy *policy,
    const RcControlPolicyConfig *config
);

void rc_control_policy_reset(
    RcControlPolicy *policy
);

bool rc_control_policy_process(
    RcControlPolicy *policy,
    const RcMappedInput *input,
    RcControlCommand *output
);

#endif /* RC_CONTROL_POLICY_H */
