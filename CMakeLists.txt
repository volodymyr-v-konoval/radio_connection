cmake_minimum_required(VERSION 3.16)

project(radio_connection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(RADIO_INCLUDE_DIRS
    src
    src/radio/common
    src/radio/protocol
    src/radio/port
    src/radio/service
    src/platform/pc
    src/radio/protocol/crsf
)

add_library(radio_core STATIC
    src/radio/service/rc_receiver_service.c
)

target_include_directories(radio_core PUBLIC ${RADIO_INCLUDE_DIRS})

add_library(radio_crsf STATIC
    src/radio/protocol/crsf/crsf_crc.c
    src/radio/protocol/crsf/crsf_protocol.c
)

target_include_directories(radio_crsf PUBLIC ${RADIO_INCLUDE_DIRS})

add_library(platform_pc STATIC
    src/platform/pc/pc_logger.c
    src/platform/pc/pc_time.c
    src/platform/pc/pc_mock_transport.c
)

target_include_directories(platform_pc PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_definitions(platform_pc PRIVATE _POSIX_C_SOURCE=200809L)

foreach(target radio_core radio_crsf platform_pc)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
    )
endforeach()

add_executable(test_crsf
    tests/test_crsf.c
)

target_link_libraries(test_crsf PRIVATE radio_crsf)
target_compile_options(test_crsf PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
)

add_executable(test_rc_service_crsf
    tests/test_rc_service_crsf.c
)

target_link_libraries(test_rc_service_crsf PRIVATE
    radio_core
    radio_crsf
    platform_pc
)

target_compile_options(test_rc_service_crsf PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
)

add_test(NAME crsf_protocol_tests COMMAND test_crsf)
add_test(NAME rc_service_crsf_tests COMMAND test_rc_service_crsf)
