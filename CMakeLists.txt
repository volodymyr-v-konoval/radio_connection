cmake_minimum_required(VERSION 3.16)

project(radio_connection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(radio_demo
    src/main.c
    src/radio/service/rc_receiver_service.c
    src/platform/pc/pc_logger.c
    src/platform/pc/pc_time.c
    src/platform/pc/pc_mock_transport.c
)

target_include_directories(radio_demo PRIVATE
    src
    src/radio/common
    src/radio/protocol
    src/radio/port
    src/radio/service
    src/platform/pc
)
