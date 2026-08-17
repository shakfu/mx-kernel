

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
    types.cpp
    connection.h
    interpreter.h
    message_queue.h
    types.h
    version.h
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

#############################################################
# BUNDLE DEPENDENCIES
#############################################################
# The external links libzmq and libcrypto by absolute Homebrew path, so without
# this step it only loads on a machine with Homebrew at the same prefix. Copy
# them (and their own dependencies) into the bundle and rewrite the install
# names to @rpath.
#
# Registered after max-posttarget so it runs after that file's codesign step;
# rewriting load commands invalidates a signature, so the script re-signs.
if (APPLE)
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND /bin/sh
			"${CMAKE_CURRENT_SOURCE_DIR}/../../../script/bundle_dylibs.sh"
			"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${${PROJECT_NAME}_EXTERN_OUTPUT_NAME}.mxo"
			"${MAX_SDK_CODESIGN_IDENTITY}"
		VERBATIM
		COMMENT "Bundling dependent dylibs into the external"
	)
endif ()
