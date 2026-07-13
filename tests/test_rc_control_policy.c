#include "rc_control_policy.h"

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

static RcControlPolicyConfig make_config(
    bool require_release
)
{
    const RcControlPolicyConfig config = {
        .arm_on_threshold = 750,
        .arm_off_threshold = 250,
        .throttle_arm_max = 50,
        .require_arm_release_after_failsafe =
            require_release
    };

    return config;
}

static void set_function(
    RcMappedInput *input,
    RcLogicalFunction function,
    int16_t value
)
{
    input->normalized[function] = value;
    input->pulse_us[function] = 1500U;

    input->available_mask |=
        (uint32_t)(1UL << (uint32_t)function);
}

static RcMappedInput make_valid_input(
    int16_t throttle,
    int16_t arm
)
{
    RcMappedInput input;

    memset(&input, 0, sizeof(input));

    input.frame_valid = true;
    input.frame_lost = false;
    input.failsafe = false;
    input.timestamp_ms = 1234U;
    input.protocol = RADIO_PROTOCOL_CRSF;

    set_function(
        &input,
        RC_FUNCTION_ROLL,
        100
    );

    set_function(
        &input,
        RC_FUNCTION_PITCH,
        -200
    );

    set_function(
        &input,
        RC_FUNCTION_THROTTLE,
        throttle
    );

    set_function(
        &input,
        RC_FUNCTION_YAW,
        300
    );

    set_function(
        &input,
        RC_FUNCTION_MODE,
        1000
    );

    set_function(
        &input,
        RC_FUNCTION_ARM,
        arm
    );

    return input;
}

static bool test_no_input_is_safe(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(true);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            NULL,
            &command),
        "No-input state should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_SAFE,
        "No input must produce SAFE state"
    );

    TEST_ASSERT(
        command.reason == RC_CONTROL_REASON_NO_INPUT,
        "No input reason should be reported"
    );

    TEST_ASSERT(
        !command.armed &&
        !command.outputs_enabled &&
        command.safe_state_active,
        "No input must disable outputs"
    );

    return true;
}

static bool test_startup_requires_arm_release(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(true);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    RcMappedInput input = make_valid_input(
        0,
        1000
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "High arm input should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_SAFE,
        "High arm at startup must remain safe"
    );

    TEST_ASSERT(
        command.reason ==
            RC_CONTROL_REASON_ARM_RELEASE_REQUIRED,
        "Arm release should be required at startup"
    );

    input = make_valid_input(
        0,
        0
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Low arm input should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_READY,
        "Low arm should clear release lock"
    );

    input = make_valid_input(
        0,
        1000
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Second high arm input should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_ACTIVE,
        "System should arm after explicit release"
    );

    return true;
}

static bool test_active_control_passthrough(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(false);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    RcMappedInput input = make_valid_input(
        0,
        1000
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Active controls should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_ACTIVE,
        "Valid arm request should activate outputs"
    );

    TEST_ASSERT(
        command.armed &&
        command.outputs_enabled &&
        !command.safe_state_active,
        "Active command should enable outputs"
    );

    TEST_ASSERT(
        command.roll == 100 &&
        command.pitch == -200 &&
        command.throttle == 0 &&
        command.yaw == 300 &&
        command.mode == 1000,
        "Mapped controls should reach active output"
    );

    return true;
}

static bool test_high_throttle_blocks_arming(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(false);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    RcMappedInput input = make_valid_input(
        500,
        1000
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "High-throttle arm request should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_READY,
        "High throttle must prevent activation"
    );

    TEST_ASSERT(
        command.reason ==
            RC_CONTROL_REASON_THROTTLE_NOT_LOW,
        "High throttle reason should be reported"
    );

    TEST_ASSERT(
        !command.armed &&
        !command.outputs_enabled &&
        command.throttle == 0,
        "Blocked arming must keep safe outputs"
    );

    return true;
}

static bool test_disarm_forces_zero_outputs(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(false);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    RcMappedInput input = make_valid_input(
        0,
        1000
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Arm input should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_ACTIVE,
        "System should become active"
    );

    input = make_valid_input(
        700,
        0
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Disarm input should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_READY,
        "Disarm must return to READY"
    );

    TEST_ASSERT(
        command.roll == 0 &&
        command.pitch == 0 &&
        command.throttle == 0 &&
        command.yaw == 0 &&
        command.mode == 0,
        "Disarm must force every output to zero"
    );

    return true;
}

static bool test_failsafe_disarms_and_requires_release(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(true);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    RcMappedInput input = make_valid_input(
        0,
        0
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Initial arm release should be processed"
    );

    input = make_valid_input(
        0,
        1000
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Arming should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_ACTIVE,
        "System should be active before failsafe"
    );

    input.failsafe = true;
    input.frame_valid = false;
    input.frame_lost = true;

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Failsafe should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_SAFE,
        "Failsafe must enter SAFE state"
    );

    TEST_ASSERT(
        command.reason == RC_CONTROL_REASON_FAILSAFE,
        "Failsafe reason should be reported"
    );

    TEST_ASSERT(
        !command.armed &&
        !command.outputs_enabled,
        "Failsafe must disarm and disable outputs"
    );

    input = make_valid_input(
        0,
        1000
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Reconnect with high arm should be processed"
    );

    TEST_ASSERT(
        command.reason ==
            RC_CONTROL_REASON_ARM_RELEASE_REQUIRED,
        "Reconnect must not automatically re-arm"
    );

    input = make_valid_input(
        0,
        0
    );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Low arm after reconnect should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_READY,
        "Low arm should clear the reconnect lock"
    );

    return true;
}

static bool test_incomplete_mapping_is_safe(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(false);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    RcMappedInput input = make_valid_input(
        0,
        1000
    );

    input.available_mask &=
        (uint32_t)~(
            1UL << (uint32_t)RC_FUNCTION_YAW
        );

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Incomplete input should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_SAFE,
        "Incomplete mapping must enter SAFE state"
    );

    TEST_ASSERT(
        command.reason ==
            RC_CONTROL_REASON_MAPPING_INCOMPLETE,
        "Incomplete mapping reason should be reported"
    );

    return true;
}

static bool test_frame_loss_is_safe(void)
{
    RcControlPolicy policy;
    RcControlCommand command;

    const RcControlPolicyConfig config =
        make_config(false);

    TEST_ASSERT(
        rc_control_policy_init(&policy, &config),
        "Policy should initialize"
    );

    RcMappedInput input = make_valid_input(
        0,
        1000
    );

    input.frame_lost = true;

    TEST_ASSERT(
        rc_control_policy_process(
            &policy,
            &input,
            &command),
        "Lost frame should be processed"
    );

    TEST_ASSERT(
        command.state == RC_CONTROL_STATE_SAFE,
        "Lost frame must enter SAFE state"
    );

    TEST_ASSERT(
        command.reason ==
            RC_CONTROL_REASON_FRAME_LOST,
        "Frame-loss reason should be reported"
    );

    return true;
}

int main(void)
{
    int failed = 0;

    if (!test_no_input_is_safe()) {
        failed++;
    }

    if (!test_startup_requires_arm_release()) {
        failed++;
    }

    if (!test_active_control_passthrough()) {
        failed++;
    }

    if (!test_high_throttle_blocks_arming()) {
        failed++;
    }

    if (!test_disarm_forces_zero_outputs()) {
        failed++;
    }

    if (!test_failsafe_disarms_and_requires_release()) {
        failed++;
    }

    if (!test_incomplete_mapping_is_safe()) {
        failed++;
    }

    if (!test_frame_loss_is_safe()) {
        failed++;
    }

    if (failed != 0) {
        printf(
            "%d RC control policy test(s) failed\n",
            failed
        );

        return 1;
    }

    printf("All RC control policy tests passed\n");

    return 0;
}
