#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "xeus-zmq" for configuration ""
set_property(TARGET xeus-zmq APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(xeus-zmq PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libxeus-zmq.6.1.1.dylib"
  IMPORTED_SONAME_NOCONFIG "@rpath/libxeus-zmq.6.dylib"
  )

list(APPEND _cmake_import_check_targets xeus-zmq )
list(APPEND _cmake_import_check_files_for_xeus-zmq "${_IMPORT_PREFIX}/lib/libxeus-zmq.6.1.1.dylib" )

# Import target "xeus-zmq-static" for configuration ""
set_property(TARGET xeus-zmq-static APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(xeus-zmq-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libxeus-zmq.a"
  )

list(APPEND _cmake_import_check_targets xeus-zmq-static )
list(APPEND _cmake_import_check_files_for_xeus-zmq-static "${_IMPORT_PREFIX}/lib/libxeus-zmq.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
