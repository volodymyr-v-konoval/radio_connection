#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "link_crc16.h"

#define TEST_ASSERT(condition, message)          \
    do {                                         \
        if (!(condition)) {                      \
            printf("FAIL: %s\n", (message));     \
            return false;                        \
        }                                        \
    } while (0)

static bool test_standard_check_vector(void)
{
    static const uint8_t data[] = {
        '1', '2', '3', '4', '5',
        '6', '7', '8', '9'
    };

    TEST_ASSERT(
        link_crc16_ccitt_false(
            data,
            sizeof(data)
        ) == 0x29B1U,
        "CRC16 check vector should be 0x29B1"
    );

    return true;
}

static bool test_empty_input_uses_initial_value(void)
{
    TEST_ASSERT(
        link_crc16_ccitt_false(NULL, 0U) ==
            LINK_CRC16_CCITT_FALSE_INIT,
        "Empty input should preserve 0xFFFF"
    );

    return true;
}

static bool test_null_nonempty_input_is_rejected(void)
{
    TEST_ASSERT(
        link_crc16_ccitt_false(NULL, 1U) == 0U,
        "NULL non-empty input should return zero"
    );

    return true;
}

int main(void)
{
    int failures = 0;

    if (!test_standard_check_vector()) {
        failures++;
    }

    if (!test_empty_input_uses_initial_value()) {
        failures++;
    }

    if (!test_null_nonempty_input_is_rejected()) {
        failures++;
    }

    if (failures != 0) {
        printf(
            "%d link CRC16 test(s) failed\n",
            failures
        );
        return 1;
    }

    printf("All link CRC16 tests passed\n");
    return 0;
}
