file(GLOB_RECURSE RADIO_CORE_FILES
    "${PROJECT_ROOT}/src/radio/*.c"
    "${PROJECT_ROOT}/src/radio/*.h"
    "${PROJECT_ROOT}/src/link/*.c"
    "${PROJECT_ROOT}/src/link/*.h"
    "${PROJECT_ROOT}/src/drivers/*.c"
    "${PROJECT_ROOT}/src/drivers/*.h"
)

set(FORBIDDEN_PATTERNS
    "stm32f4xx_hal\\.h"
    "stm32.*_hal\\.h"
    "FreeRTOS\\.h"
    "cmsis_os"
    "UART_HandleTypeDef"
    "DMA_HandleTypeDef"
)

foreach(FILE_PATH IN LISTS RADIO_CORE_FILES)
    file(READ "${FILE_PATH}" FILE_CONTENT)

    foreach(PATTERN IN LISTS FORBIDDEN_PATTERNS)
        if(FILE_CONTENT MATCHES "${PATTERN}")
            message(FATAL_ERROR
                "Forbidden platform dependency '${PATTERN}' found in ${FILE_PATH}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "Radio, link, and driver dependency boundary check passed")
