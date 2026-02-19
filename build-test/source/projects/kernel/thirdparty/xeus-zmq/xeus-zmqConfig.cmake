############################################################################
# Copyright (c) 2016, Johan Mabille, Sylvain Corlay, Martin Renou          #
# Copyright (c) 2016, QuantStack                                           #
#                                                                          #
# Distributed under the terms of the BSD 3-Clause License.                 #
#                                                                          #
# The full license is in the file LICENSE, distributed with this software. #
############################################################################

# xeus cmake module
# This module sets the following variables in your project::
#
#   xeus_FOUND - true if xeus found on the system
#   xeus_INCLUDE_DIRS - the directory containing xeus headers
#   xeus_LIBRARY - the library for dynamic linking
#   xeus_STATIC_LIBRARY - the library for static linking


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was xeus-zmqConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../../../../../../../../usr/local" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR};${CMAKE_MODULE_PATH}")

####### Expanded from @XEUS_CONFIG_CODE@ #######
set(CMAKE_MODULE_PATH "/Users/sa/projects/personal/mx-kernel/source/projects/kernel/thirdparty/xeus-zmq/cmake;${CMAKE_MODULE_PATH}")
##################################################

include(CMakeFindDependencyMacro)
find_dependency(nlohmann_json 3.12.0 EXACT)
find_dependency(xeus 5.0.0)
find_dependency(OpenSSL)

if(NOT TARGET xeus-zmq AND NOT TARGET xeus-zmq-static)
    include("${CMAKE_CURRENT_LIST_DIR}/xeus-zmqTargets.cmake")

    if (TARGET xeus-zmq AND TARGET xeus-zmq-static)
        get_target_property(xeus-zmq_INCLUDE_DIR xeus-zmq INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(xeus-zmq_LIBRARY xeus-zmq LOCATION)
        get_target_property(xeus-zmq_STATIC_LIBRARY xeus-zmq-static LOCATION)
    elseif (TARGET xeus-zmq)
        get_target_property(xeus-zmq_INCLUDE_DIR xeus-zmq INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(xeus-zmq_LIBRARY xeus-zmq LOCATION)
    elseif (TARGET xeus-zmq-static)
        get_target_property(xeus-zmq_INCLUDE_DIR xeus-zmq-static INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(xeus-zmq_STATIC_LIBRARY xeus-zmq-static LOCATION)
        set(xeus-zmq_LIBRARY ${xeus-zmq_STATIC_LIBRARY})
    endif ()
endif()
