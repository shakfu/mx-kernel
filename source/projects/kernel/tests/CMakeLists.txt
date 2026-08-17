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

# Covers everything that does not touch the Max SDK. external.cpp is the only
# translation unit that needs Max, and it is deliberately excluded.
add_executable(kernel_tests
    test_main.cpp
    test_connection.cpp
    test_message_queue.cpp
    test_interpreter.cpp
    test_server_shutdown.cpp
    ../connection.cpp
    ../interpreter.cpp
    ../types.cpp
)

# interpreter.cpp derives from xeus::xinterpreter, and the shutdown tests start
# a real ZMQ server, so the tests link both xeus and xeus-zmq. They still do not
# link the Max SDK.
target_link_libraries(kernel_tests PRIVATE xeus-static xeus-zmq-static)

target_include_directories(kernel_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/xeus/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/xeus-zmq/include
)
