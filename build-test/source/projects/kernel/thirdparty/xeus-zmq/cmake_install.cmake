# Install script for directory: /Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/libxeus-zmq.6.1.1.dylib"
    "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/libxeus-zmq.6.dylib"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus-zmq.6.1.1.dylib"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus-zmq.6.dylib"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" -x "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/libxeus-zmq.dylib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/xeus-zmq" TYPE FILE FILES
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xclient_zmq.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xcontrol_default_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xcontrol_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xdap_tcp_client.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xdebugger_base.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xeus-zmq.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xmiddleware.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xserver_zmq.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xserver_zmq_split.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xshell_default_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xshell_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xthread.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xzmq_context.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/libxeus-zmq.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus-zmq.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus-zmq.a")
    execute_process(COMMAND "/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus-zmq.a")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/xeus-zmq" TYPE FILE FILES
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xclient_zmq.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xcontrol_default_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xcontrol_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xdap_tcp_client.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xdebugger_base.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xeus-zmq.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xmiddleware.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xserver_zmq.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xserver_zmq_split.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xshell_default_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xshell_runner.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xthread.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/include/xeus-zmq/xzmq_context.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus-zmq" TYPE FILE FILES
    "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/CMakeFiles/xeus-zmqConfig.cmake"
    "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/xeus-zmqConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus-zmq/xeus-zmqTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus-zmq/xeus-zmqTargets.cmake"
         "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/CMakeFiles/Export/ac2e5001fe7815e8ec87d8b14bf5c78f/xeus-zmqTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus-zmq/xeus-zmqTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus-zmq/xeus-zmqTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus-zmq" TYPE FILE FILES "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/CMakeFiles/Export/ac2e5001fe7815e8ec87d8b14bf5c78f/xeus-zmqTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus-zmq" TYPE FILE FILES "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/CMakeFiles/Export/ac2e5001fe7815e8ec87d8b14bf5c78f/xeus-zmqTargets-noconfig.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus-zmq/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
