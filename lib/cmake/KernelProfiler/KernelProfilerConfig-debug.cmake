#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "KernelProfiler::KernelProfiler" for configuration "Debug"
set_property(TARGET KernelProfiler::KernelProfiler APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(KernelProfiler::KernelProfiler PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libKernelProfiler.a"
  )

list(APPEND _cmake_import_check_targets KernelProfiler::KernelProfiler )
list(APPEND _cmake_import_check_files_for_KernelProfiler::KernelProfiler "${_IMPORT_PREFIX}/lib/libKernelProfiler.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
