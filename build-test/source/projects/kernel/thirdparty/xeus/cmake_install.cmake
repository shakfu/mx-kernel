# Install script for directory: /Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus/libxeus.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus.a")
    execute_process(COMMAND "/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libxeus.a")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/xeus" TYPE FILE FILES
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xbase64.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xbasic_fixed_string.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xcomm.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xcontrol_messenger.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xdebugger.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xeus.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xeus_context.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xguid.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xhash.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xhistory_manager.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xinput.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xinterpreter.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xjson.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xkernel.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xkernel_configuration.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xlogger.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xmessage.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xhelper.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xserver.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xstring_utils.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xsystem.hpp"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/include/xeus/xrequest_context.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus" TYPE FILE FILES
    "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus/CMakeFiles/xeusConfig.cmake"
    "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus/xeusConfigVersion.cmake"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/cmake/FindLibUUID.cmake"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/cmake/WasmBuildOptions.cmake"
    "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus/cmake/CompilerWarnings.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus/xeusTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus/xeusTargets.cmake"
         "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus/CMakeFiles/Export/9b080057564ade8f0cef2ea1f2d3686f/xeusTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus/xeusTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus/xeusTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus" TYPE FILE FILES "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus/CMakeFiles/Export/9b080057564ade8f0cef2ea1f2d3686f/xeusTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/xeus" TYPE FILE FILES "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus/CMakeFiles/Export/9b080057564ade8f0cef2ea1f2d3686f/xeusTargets-noconfig.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/sa/projects/personal/mx-kernel/build-test/source/projects/kernel/thirdparty/xeus/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
