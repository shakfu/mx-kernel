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
####### The input file was xeusConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

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



include(CMakeFindDependencyMacro)
# nlohmann_json requires libraries that exchange json objects to be linked
# with the same version of nlohmann_json. 
find_dependency(nlohmann_json 3.12.0 EXACT)

# This is required when linking with the static target
if(NOT EMSCRIPTEN)
    if(UNIX AND NOT APPLE)
        find_dependency(LibUUID)
    endif()
endif()

if(NOT TARGET xeus AND NOT TARGET xeus-static)
    include("${CMAKE_CURRENT_LIST_DIR}/xeusTargets.cmake")

    if (TARGET xeus AND TARGET xeus-static)
        get_target_property(xeus_INCLUDE_DIR xeus INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(xeus_LIBRARY xeus LOCATION)
        get_target_property(xeus_STATIC_LIBRARY xeus-static LOCATION)
    elseif (TARGET xeus)
        get_target_property(xeus_INCLUDE_DIR xeus INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(xeus_LIBRARY xeus LOCATION)
    elseif (TARGET xeus-static)
        get_target_property(xeus_INCLUDE_DIR xeus-static INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(xeus_STATIC_LIBRARY xeus-static LOCATION)
        set(xeus_LIBRARY ${xeus_STATIC_LIBRARY})
    endif ()
endif()
