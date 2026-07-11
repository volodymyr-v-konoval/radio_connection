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
    src/platform/stm32
    src/radio/protocol/crsf
)

set(RADIO_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
)

add_library(radio_core STATIC
    src/radio/service/rc_receiver_service.c
)

target_include_directories(radio_core PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_options(radio_core PRIVATE ${RADIO_WARNING_FLAGS})

add_library(radio_crsf STATIC
    src/radio/protocol/crsf/crsf_crc.c
    src/radio/protocol/crsf/crsf_protocol.c
)

target_include_directories(radio_crsf PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_options(radio_crsf PRIVATE ${RADIO_WARNING_FLAGS})

add_library(platform_pc STATIC
    src/platform/pc/pc_logger.c
    src/platform/pc/pc_time.c
    src/platform/pc/pc_mock_transport.c
)

target_include_directories(platform_pc PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_definitions(platform_pc PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(platform_pc PRIVATE ${RADIO_WARNING_FLAGS})

add_library(platform_stm32_adapters STATIC
    src/platform/stm32/stm32_uart_dma_transport.c
    src/platform/stm32/stm32_logger.c
    src/platform/stm32/stm32_time.c
)

target_include_directories(platform_stm32_adapters PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_options(platform_stm32_adapters PRIVATE ${RADIO_WARNING_FLAGS})

add_library(test_support STATIC
    tests/support/fake_uart_dma_backend.c
)

target_include_directories(test_support PUBLIC
    tests/support
    ${RADIO_INCLUDE_DIRS}
)
target_compile_options(test_support PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_crsf
    tests/test_crsf.c
)

target_link_libraries(test_crsf PRIVATE radio_crsf)
target_compile_options(test_crsf PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_rc_service_crsf
    tests/test_rc_service_crsf.c
)

target_link_libraries(test_rc_service_crsf PRIVATE
    radio_core
    radio_crsf
    platform_pc
)

target_compile_options(test_rc_service_crsf PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_stm32_adapters
    tests/test_stm32_adapters.c
)

target_link_libraries(test_stm32_adapters PRIVATE
    platform_stm32_adapters
    test_support
)

target_compile_options(test_stm32_adapters PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_rc_service_stm32_dma
    tests/test_rc_service_stm32_dma.c
)

target_link_libraries(test_rc_service_stm32_dma PRIVATE
    radio_core
    radio_crsf
    platform_stm32_adapters
    test_support
)

target_compile_options(test_rc_service_stm32_dma PRIVATE ${RADIO_WARNING_FLAGS})

add_test(NAME crsf_protocol_tests COMMAND test_crsf)
add_test(NAME rc_service_crsf_tests COMMAND test_rc_service_crsf)
add_test(NAME stm32_adapter_tests COMMAND test_stm32_adapters)
add_test(NAME rc_service_stm32_dma_tests COMMAND test_rc_service_stm32_dma)
