cmake_minimum_required(VERSION 3.16)

project(radio_connection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(RADIO_INCLUDE_DIRS
    src
    src/radio/common
    src/radio/control
    src/radio/mapping
    src/radio/protocol
    src/radio/port
    src/radio/service
    src/platform/pc
    src/platform/stm32
    src/platform/stm32/f4
    src/platform/stm32/f4/sx128x
    src/radio/protocol/crsf
    src/radio/telemetry/crsf
    src/radio/protocol/sbus
    src/link/common
    src/link/port
    src/link/service
    src/link/adapter
    src/drivers/sx128x
    firmware/stm32f407/board
    firmware/stm32f407/app
    firmware/stm32f401/board
    firmware/stm32f401/app
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
    src/radio/mapping/rc_channel_mapper.c
    src/radio/control/rc_control_policy.c
)

target_include_directories(radio_core PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_options(radio_core PRIVATE ${RADIO_WARNING_FLAGS})

add_library(radio_crsf STATIC
    src/radio/protocol/crsf/crsf_crc.c
    src/radio/protocol/crsf/crsf_protocol.c
)

target_include_directories(radio_crsf PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_options(radio_crsf PRIVATE ${RADIO_WARNING_FLAGS})

add_library(link_core STATIC
    src/link/common/link_crc16.c
    src/link/common/link_packet.c
)

target_include_directories(link_core PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_options(link_core PRIVATE ${RADIO_WARNING_FLAGS})

add_library(duplex_link_service STATIC
    src/link/service/duplex_link_service.c
)

target_include_directories(duplex_link_service PUBLIC
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(duplex_link_service PUBLIC
    link_core
)

target_compile_options(duplex_link_service PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_library(sx128x_driver STATIC
    src/drivers/sx128x/sx128x.c
)

target_include_directories(sx128x_driver PUBLIC ${RADIO_INCLUDE_DIRS})
target_compile_options(sx128x_driver PRIVATE ${RADIO_WARNING_FLAGS})

add_library(sx128x_packet_radio STATIC
    src/link/adapter/sx128x_packet_radio.c
)

target_include_directories(sx128x_packet_radio PUBLIC
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(sx128x_packet_radio PUBLIC
    sx128x_driver
)

target_compile_options(sx128x_packet_radio PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_library(radio_crsf_telemetry STATIC
    src/radio/telemetry/crsf/crsf_telemetry.c
    src/radio/telemetry/crsf/crsf_telemetry_service.c
)

target_include_directories(radio_crsf_telemetry PUBLIC
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(radio_crsf_telemetry PUBLIC
    radio_crsf
)

target_compile_options(radio_crsf_telemetry PRIVATE
    ${RADIO_WARNING_FLAGS}
)

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
    tests/support/fake_radio_tx.c
    tests/support/fake_packet_radio.c
    tests/support/fake_sx128x_port.c
    tests/support/fake_uart_dma_backend.c
)

target_include_directories(test_support PUBLIC
    tests/support
    ${RADIO_INCLUDE_DIRS}
)
target_compile_options(test_support PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_rc_control_policy
    tests/test_rc_control_policy.c
)

target_link_libraries(test_rc_control_policy PRIVATE
    radio_core
)

target_compile_options(test_rc_control_policy PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_rc_channel_mapper
    tests/test_rc_channel_mapper.c
)

target_link_libraries(test_rc_channel_mapper PRIVATE
    radio_core
)

target_compile_options(test_rc_channel_mapper PRIVATE
    ${RADIO_WARNING_FLAGS}
)

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

add_test(
    NAME rc_control_policy_tests
    COMMAND test_rc_control_policy
)

add_test(
    NAME rc_channel_mapper_tests
    COMMAND test_rc_channel_mapper
)

add_test(NAME crsf_protocol_tests COMMAND test_crsf)
add_test(NAME rc_service_crsf_tests COMMAND test_rc_service_crsf)

add_executable(test_link_crc16
    tests/test_link_crc16.c
)

target_link_libraries(test_link_crc16 PRIVATE link_core)
target_compile_options(test_link_crc16 PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_link_packet
    tests/test_link_packet.c
)

target_link_libraries(test_link_packet PRIVATE link_core)
target_compile_options(test_link_packet PRIVATE ${RADIO_WARNING_FLAGS})

add_test(NAME link_crc16_tests COMMAND test_link_crc16)
add_test(NAME link_packet_tests COMMAND test_link_packet)

add_executable(test_packet_radio_if
    tests/test_packet_radio_if.c
)

target_include_directories(test_packet_radio_if PRIVATE
    tests/support
)

target_link_libraries(test_packet_radio_if PRIVATE
    test_support
)

target_compile_options(test_packet_radio_if PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME packet_radio_if_tests
    COMMAND test_packet_radio_if
)

add_executable(test_duplex_link_service
    tests/test_duplex_link_service.c
)

target_include_directories(test_duplex_link_service PRIVATE
    tests/support
)

target_link_libraries(test_duplex_link_service PRIVATE
    duplex_link_service
    test_support
)

target_compile_options(test_duplex_link_service PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME duplex_link_service_tests
    COMMAND test_duplex_link_service
)

add_executable(test_sx128x_driver
    tests/test_sx128x_driver.c
)

target_include_directories(test_sx128x_driver PRIVATE
    tests/support
)

target_link_libraries(test_sx128x_driver PRIVATE
    sx128x_driver
    test_support
)

target_compile_options(test_sx128x_driver PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME sx128x_driver_tests
    COMMAND test_sx128x_driver
)

add_executable(test_sx128x_packet_radio
    tests/test_sx128x_packet_radio.c
)

target_include_directories(test_sx128x_packet_radio PRIVATE
    tests/support
)

target_link_libraries(test_sx128x_packet_radio PRIVATE
    sx128x_packet_radio
    test_support
)

target_compile_options(test_sx128x_packet_radio PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME sx128x_packet_radio_tests
    COMMAND test_sx128x_packet_radio
)

add_test(NAME stm32_adapter_tests COMMAND test_stm32_adapters)
add_test(NAME rc_service_stm32_dma_tests COMMAND test_rc_service_stm32_dma)


# STM32F4 HAL-dependent code is compiled on the host only against test stubs.
add_library(fake_stm32f4_hal STATIC
    tests/support/fake_stm32f4_hal.c
)

target_include_directories(fake_stm32f4_hal PUBLIC
    tests/stubs
    tests/support
)
target_compile_options(fake_stm32f4_hal PRIVATE ${RADIO_WARNING_FLAGS})

add_library(platform_stm32f4_backend_host STATIC
    src/platform/stm32/f4/stm32f4_uart_tx_backend.c
    src/platform/stm32/f4/stm32f4_uart_dma_backend.c
    src/platform/stm32/f4/stm32f4_uart_logger_backend.c
    src/platform/stm32/f4/stm32f4_time_backend.c
    src/platform/stm32/f4/sx128x/stm32f4_sx128x_port.c
)

target_include_directories(platform_stm32f4_backend_host PUBLIC
    tests/stubs
    ${RADIO_INCLUDE_DIRS}
)
target_link_libraries(platform_stm32f4_backend_host PUBLIC fake_stm32f4_hal)
target_compile_options(platform_stm32f4_backend_host PRIVATE ${RADIO_WARNING_FLAGS})

add_library(stm32f407_radio_port_host STATIC
    firmware/stm32f407/board/fk407m3_vet6_v1_1_radio.c
    firmware/stm32f407/app/radio_composition.c
)

target_include_directories(stm32f407_radio_port_host PUBLIC
    tests/stubs
    ${RADIO_INCLUDE_DIRS}
)
target_link_libraries(stm32f407_radio_port_host PUBLIC
    radio_core
    radio_crsf
    platform_stm32_adapters
    platform_stm32f4_backend_host
)
target_link_libraries(stm32f407_radio_port_host PUBLIC
    radio_crsf_telemetry
)

target_compile_options(stm32f407_radio_port_host PRIVATE ${RADIO_WARNING_FLAGS})

add_library(stm32f401_link_port_host STATIC
    firmware/stm32f401/board/blackpill_f401cc_sx128x.c
    firmware/stm32f401/app/link_composition.c
)

target_include_directories(stm32f401_link_port_host PUBLIC
    tests/stubs
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(stm32f401_link_port_host PUBLIC
    platform_stm32_adapters
    platform_stm32f4_backend_host
    sx128x_driver
    sx128x_packet_radio
    duplex_link_service
)

target_compile_options(stm32f401_link_port_host PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_stm32f4_backend
    tests/test_stm32f4_backend.c
)

target_include_directories(test_stm32f4_backend PRIVATE
    tests/stubs
    tests/support
    ${RADIO_INCLUDE_DIRS}
)
target_link_libraries(test_stm32f4_backend PRIVATE
    platform_stm32f4_backend_host
    fake_stm32f4_hal
)
target_compile_options(test_stm32f4_backend PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_stm32f407_composition
    tests/test_stm32f407_composition.c
)

target_include_directories(test_stm32f407_composition PRIVATE
    tests/stubs
    tests/support
    ${RADIO_INCLUDE_DIRS}
)
target_link_libraries(test_stm32f407_composition PRIVATE
    stm32f407_radio_port_host
    fake_stm32f4_hal
)
target_compile_options(test_stm32f407_composition PRIVATE ${RADIO_WARNING_FLAGS})

add_executable(test_stm32f4_sx128x_port
    tests/test_stm32f4_sx128x_port.c
)

target_include_directories(test_stm32f4_sx128x_port PRIVATE
    tests/stubs
    tests/support
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(test_stm32f4_sx128x_port PRIVATE
    platform_stm32f4_backend_host
    fake_stm32f4_hal
)

target_compile_options(test_stm32f4_sx128x_port PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME stm32f4_sx128x_port_tests
    COMMAND test_stm32f4_sx128x_port
)

add_executable(test_stm32f401_link_composition
    tests/test_stm32f401_link_composition.c
)

target_include_directories(test_stm32f401_link_composition PRIVATE
    tests/stubs
    tests/support
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(test_stm32f401_link_composition PRIVATE
    stm32f401_link_port_host
    fake_stm32f4_hal
)

target_compile_options(test_stm32f401_link_composition PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME stm32f401_link_composition_tests
    COMMAND test_stm32f401_link_composition
)

add_test(NAME stm32f4_backend_tests COMMAND test_stm32f4_backend)
add_test(NAME stm32f407_composition_tests COMMAND test_stm32f407_composition)
add_test(
    NAME architecture_dependency_test
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_architecture.cmake
)

# Stage 9: optional FreeRTOS receiver wrapper host validation
#
# The production wrapper lives under src/platform/freertos and includes
# FreeRTOS headers. Host tests compile it against deterministic fake kernel
# headers, while the existing bare-metal STM32 firmware remains unchanged.
add_library(fake_freertos STATIC
    tests/fakes/freertos/fake_freertos.c
)

target_include_directories(fake_freertos PUBLIC
    tests/fakes/freertos
)

target_compile_options(fake_freertos PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_library(platform_freertos_wrapper_host STATIC
    src/platform/freertos/rc_receiver_task.c
)

target_include_directories(platform_freertos_wrapper_host PUBLIC
    src/platform/freertos
    tests/fakes/freertos
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(platform_freertos_wrapper_host PUBLIC
    fake_freertos
)

target_compile_options(platform_freertos_wrapper_host PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_rc_receiver_task
    tests/test_rc_receiver_task.c
)

target_include_directories(test_rc_receiver_task PRIVATE
    src/platform/freertos
    tests/fakes/freertos
    ${RADIO_INCLUDE_DIRS}
)

target_link_libraries(test_rc_receiver_task PRIVATE
    platform_freertos_wrapper_host
    fake_freertos
)

target_compile_options(test_rc_receiver_task PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME rc_receiver_task_tests
    COMMAND test_rc_receiver_task
)

# Stage 10: SBUS protocol and protocol-specific UART configuration
add_library(radio_sbus STATIC
    src/radio/protocol/sbus/sbus_protocol.c
)

target_include_directories(radio_sbus PUBLIC
    ${RADIO_INCLUDE_DIRS}
)

target_compile_options(radio_sbus PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_sbus_protocol
    tests/test_sbus_protocol.c
)

target_link_libraries(test_sbus_protocol PRIVATE
    radio_sbus
)

target_compile_options(test_sbus_protocol PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_rc_service_sbus
    tests/test_rc_service_sbus.c
)

target_link_libraries(test_rc_service_sbus PRIVATE
    radio_core
    radio_sbus
    platform_pc
)

target_compile_options(test_rc_service_sbus PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_protocol_uart_config
    tests/test_protocol_uart_config.c
)

target_link_libraries(test_protocol_uart_config PRIVATE
    radio_crsf
    radio_sbus
)

target_compile_options(test_protocol_uart_config PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME sbus_protocol_tests
    COMMAND test_sbus_protocol
)

add_test(
    NAME rc_service_sbus_tests
    COMMAND test_rc_service_sbus
)

add_test(
    NAME protocol_uart_config_tests
    COMMAND test_protocol_uart_config
)

# Stage 11: bidirectional CRSF telemetry
add_executable(test_crsf_telemetry
    tests/test_crsf_telemetry.c
)

target_link_libraries(test_crsf_telemetry PRIVATE
    radio_crsf_telemetry
)

target_compile_options(test_crsf_telemetry PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_crsf_telemetry_service
    tests/test_crsf_telemetry_service.c
)

target_include_directories(test_crsf_telemetry_service PRIVATE
    tests/support
)

target_link_libraries(test_crsf_telemetry_service PRIVATE
    radio_crsf_telemetry
    test_support
)

target_compile_options(test_crsf_telemetry_service PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_executable(test_stm32f4_uart_tx_backend
    tests/test_stm32f4_uart_tx_backend.c
)

target_include_directories(test_stm32f4_uart_tx_backend PRIVATE
    tests/stubs
    tests/support
)

target_link_libraries(test_stm32f4_uart_tx_backend PRIVATE
    platform_stm32f4_backend_host
    fake_stm32f4_hal
)

target_compile_options(test_stm32f4_uart_tx_backend PRIVATE
    ${RADIO_WARNING_FLAGS}
)

add_test(
    NAME crsf_telemetry_tests
    COMMAND test_crsf_telemetry
)

add_test(
    NAME crsf_telemetry_service_tests
    COMMAND test_crsf_telemetry_service
)

add_test(
    NAME stm32f4_uart_tx_backend_tests
    COMMAND test_stm32f4_uart_tx_backend
)
