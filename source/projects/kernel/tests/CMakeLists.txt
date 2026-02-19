cmake_minimum_required(VERSION 3.19)
project(kernel_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/doctest
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/xeus/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(kernel_tests
    test_main.cpp
    test_connection.cpp
    test_message_queue.cpp
    ../connection.cpp
)

# No Max SDK dependency -- tests only cover pure logic
