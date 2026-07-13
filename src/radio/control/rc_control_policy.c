#include "rc_control_policy.h"

#include <stddef.h>
#include <string.h>

static bool rc_control_policy_config_is_valid(
    const RcControlPolicyConfig *config
);

static bool rc_control_policy_get_required_inputs(
    const RcMappedInput *input,
    int16_t *roll,
    int16_t *pitch,
    int16_t *throttle,
    int16_t *yaw,
    int16_t *mode,
    int16_t *arm
);

static void rc_control_policy_enter_safe(
    RcControlPolicy *policy,
    RcControlCommand *output,
    RcControlSafetyReason reason,
    bool require_arm_release
);

static void rc_control_policy_enter_ready(
    RcControlPolicy *policy,
    RcControlCommand *output,
    RcControlSafetyReason reason
);

static void rc_control_policy_enter_active(
    RcControlPolicy *policy,
    RcControlCommand *output,
    int16_t roll,
    int16_t pitch,
    int16_t throttle,
    int16_t yaw,
    int16_t mode
);

bool rc_control_policy_init(
    RcControlPolicy *policy,
    const RcControlPolicyConfig *config
)
{
    if (policy == NULL ||
        !rc_control_policy_config_is_valid(config)) {
        return false;
    }

    memset(policy, 0, sizeof(*policy));

    policy->config = *config;
    policy->initialized = true;

    rc_control_policy_reset(policy);

    return true;
}

void rc_control_policy_reset(
    RcControlPolicy *policy
)
{
    if (policy == NULL || !policy->initialized) {
        return;
    }

    policy->armed_latched = false;

    policy->arm_release_required =
        policy->config.require_arm_release_after_failsafe;
}

bool rc_control_policy_process(
    RcControlPolicy *policy,
    const RcMappedInput *input,
    RcControlCommand *output
)
{
    if (policy == NULL ||
        output == NULL ||
        !policy->initialized) {
        return false;
    }

    memset(output, 0, sizeof(*output));

    output->state = RC_CONTROL_STATE_SAFE;
    output->reason = RC_CONTROL_REASON_NO_INPUT;
    output->safe_state_active = true;

    if (input == NULL) {
        rc_control_policy_enter_safe(
            policy,
            output,
            RC_CONTROL_REASON_NO_INPUT,
            true
        );

        return true;
    }

    output->timestamp_ms = input->timestamp_ms;

    if (input->failsafe) {
        rc_control_policy_enter_safe(
            policy,
            output,
            RC_CONTROL_REASON_FAILSAFE,
            true
        );

        return true;
    }

    if (input->frame_lost) {
        rc_control_policy_enter_safe(
            policy,
            output,
            RC_CONTROL_REASON_FRAME_LOST,
            true
        );

        return true;
    }

    if (!input->frame_valid) {
        rc_control_policy_enter_safe(
            policy,
            output,
            RC_CONTROL_REASON_INVALID_FRAME,
            true
        );

        return true;
    }

    int16_t roll = 0;
    int16_t pitch = 0;
    int16_t throttle = 0;
    int16_t yaw = 0;
    int16_t mode = 0;
    int16_t arm = 0;

    if (!rc_control_policy_get_required_inputs(
            input,
            &roll,
            &pitch,
            &throttle,
            &yaw,
            &mode,
            &arm)) {
        rc_control_policy_enter_safe(
            policy,
            output,
            RC_CONTROL_REASON_MAPPING_INCOMPLETE,
            true
        );

        return true;
    }

    output->input_valid = true;
    output->timestamp_ms = input->timestamp_ms;

    const bool arm_switch_off =
        arm <= policy->config.arm_off_threshold;

    const bool arm_switch_on =
        arm >= policy->config.arm_on_threshold;

    /*
     * After startup or failsafe, a high arm switch must not immediately
     * reactivate the outputs. The operator must first move it to OFF.
     */
    if (policy->arm_release_required) {
        if (arm_switch_off) {
            policy->arm_release_required = false;
        } else {
            rc_control_policy_enter_safe(
                policy,
                output,
                RC_CONTROL_REASON_ARM_RELEASE_REQUIRED,
                false
            );

            output->input_valid = true;
            output->timestamp_ms = input->timestamp_ms;

            return true;
        }
    }

    /*
     * An armed system remains armed until the switch clearly reaches
     * the OFF threshold.
     */
    if (policy->armed_latched) {
        if (arm_switch_off) {
            policy->armed_latched = false;

            rc_control_policy_enter_ready(
                policy,
                output,
                RC_CONTROL_REASON_DISARMED
            );

            output->input_valid = true;
            output->timestamp_ms = input->timestamp_ms;

            return true;
        }

        rc_control_policy_enter_active(
            policy,
            output,
            roll,
            pitch,
            throttle,
            yaw,
            mode
        );

        output->input_valid = true;
        output->timestamp_ms = input->timestamp_ms;

        return true;
    }

    /*
     * The switch is OFF or still between the hysteresis thresholds.
     */
    if (!arm_switch_on) {
        rc_control_policy_enter_ready(
            policy,
            output,
            RC_CONTROL_REASON_DISARMED
        );

        output->input_valid = true;
        output->timestamp_ms = input->timestamp_ms;

        return true;
    }

    /*
     * Prevent arming while throttle is raised.
     */
    if (throttle > policy->config.throttle_arm_max) {
        rc_control_policy_enter_ready(
            policy,
            output,
            RC_CONTROL_REASON_THROTTLE_NOT_LOW
        );

        output->input_valid = true;
        output->timestamp_ms = input->timestamp_ms;

        return true;
    }

    policy->armed_latched = true;

    rc_control_policy_enter_active(
        policy,
        output,
        roll,
        pitch,
        throttle,
        yaw,
        mode
    );

    output->input_valid = true;
    output->timestamp_ms = input->timestamp_ms;

    return true;
}

static bool rc_control_policy_config_is_valid(
    const RcControlPolicyConfig *config
)
{
    if (config == NULL) {
        return false;
    }

    if (config->arm_off_threshold < 0 ||
        config->arm_on_threshold > RC_NORMALIZED_MAX ||
        config->arm_off_threshold >=
            config->arm_on_threshold) {
        return false;
    }

    if (config->throttle_arm_max < 0 ||
        config->throttle_arm_max > RC_NORMALIZED_MAX) {
        return false;
    }

    return true;
}

static bool rc_control_policy_get_required_inputs(
    const RcMappedInput *input,
    int16_t *roll,
    int16_t *pitch,
    int16_t *throttle,
    int16_t *yaw,
    int16_t *mode,
    int16_t *arm
)
{
    if (input == NULL ||
        roll == NULL ||
        pitch == NULL ||
        throttle == NULL ||
        yaw == NULL ||
        mode == NULL ||
        arm == NULL) {
        return false;
    }

    uint16_t pulse_us = 0U;

    return
        rc_mapped_input_get(
            input,
            RC_FUNCTION_ROLL,
            roll,
            &pulse_us
        ) &&
        rc_mapped_input_get(
            input,
            RC_FUNCTION_PITCH,
            pitch,
            &pulse_us
        ) &&
        rc_mapped_input_get(
            input,
            RC_FUNCTION_THROTTLE,
            throttle,
            &pulse_us
        ) &&
        rc_mapped_input_get(
            input,
            RC_FUNCTION_YAW,
            yaw,
            &pulse_us
        ) &&
        rc_mapped_input_get(
            input,
            RC_FUNCTION_MODE,
            mode,
            &pulse_us
        ) &&
        rc_mapped_input_get(
            input,
            RC_FUNCTION_ARM,
            arm,
            &pulse_us
        );
}

static void rc_control_policy_enter_safe(
    RcControlPolicy *policy,
    RcControlCommand *output,
    RcControlSafetyReason reason,
    bool require_arm_release
)
{
    policy->armed_latched = false;

    if (require_arm_release &&
        policy->config.require_arm_release_after_failsafe) {
        policy->arm_release_required = true;
    }

    output->roll = 0;
    output->pitch = 0;
    output->throttle = 0;
    output->yaw = 0;
    output->mode = 0;

    output->armed = false;
    output->outputs_enabled = false;
    output->safe_state_active = true;

    output->state = RC_CONTROL_STATE_SAFE;
    output->reason = reason;
}

static void rc_control_policy_enter_ready(
    RcControlPolicy *policy,
    RcControlCommand *output,
    RcControlSafetyReason reason
)
{
    policy->armed_latched = false;

    output->roll = 0;
    output->pitch = 0;
    output->throttle = 0;
    output->yaw = 0;
    output->mode = 0;

    output->armed = false;
    output->outputs_enabled = false;
    output->safe_state_active = true;

    output->state = RC_CONTROL_STATE_READY;
    output->reason = reason;
}

static void rc_control_policy_enter_active(
    RcControlPolicy *policy,
    RcControlCommand *output,
    int16_t roll,
    int16_t pitch,
    int16_t throttle,
    int16_t yaw,
    int16_t mode
)
{
    policy->armed_latched = true;

    output->roll = roll;
    output->pitch = pitch;
    output->throttle = throttle;
    output->yaw = yaw;
    output->mode = mode;

    output->armed = true;
    output->outputs_enabled = true;
    output->safe_state_active = false;

    output->state = RC_CONTROL_STATE_ACTIVE;
    output->reason = RC_CONTROL_REASON_NONE;
}
