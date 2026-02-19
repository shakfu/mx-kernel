

# Build xeus first
add_subdirectory(thirdparty/xeus)

# Make xeus include directories available to xeus-zmq
set(xeus_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/xeus/include)
include_directories(${xeus_INCLUDE_DIR})

# Configure xeus-zmq to link with static xeus but dynamic system libraries (zmq, openssl)
set(XEUS_ZMQ_STATIC_DEPENDENCIES OFF CACHE BOOL "" FORCE)

# Create an alias so xeus-zmq can find xeus-static as xeus
add_library(xeus ALIAS xeus-static)

add_subdirectory(thirdparty/xeus-zmq)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################

include_directories(
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/json/include"
)

set(PROJECT_SRC
    external.cpp
    connection.cpp
    interpreter.cpp
    connection.h
    interpreter.h
    message_queue.h
    types.h
)

add_library(
	${PROJECT_NAME}
	MODULE
	${PROJECT_SRC}
)


target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	xeus-static
	xeus-zmq-static
)


target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/xeus/include
	${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/xeus-zmq/include
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
