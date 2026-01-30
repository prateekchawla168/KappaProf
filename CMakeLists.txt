cmake_minimum_required(VERSION 3.22)

project(KernelProfiler
    DESCRIPTION
    "A simple kernel profiling library written in C++"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES
    src/profiler.cpp
)

set(HEADERS
    include/profiler.hpp
    include/ErrorHandler.hpp
)

# tmp stuff for now, delete later
set(CMAKE_BUILD_TYPE MinSizeRel)

# set(CMAKE_CXX_FLAGS "-fsanitize=undefined")

set(TARGET_NAME KernelProfiler)

add_library(${TARGET_NAME} ${SOURCES} ${HEADERS})

option(BUILD_DEMO "Build demo executable for the library" OFF)
if(BUILD_DEMO)
    add_subdirectory(demo)

    target_link_libraries(${TARGET_NAME}_demo ${TARGET_NAME})

endif()


target_include_directories(${TARGET_NAME}
    PRIVATE
    src
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include(GNUInstallDirs)

install(
    TARGETS ${TARGET_NAME}
    EXPORT "${TARGET_NAME}Config"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT "${TARGET_NAME}Config"
    FILE "${TARGET_NAME}Config.cmake"
    NAMESPACE ${TARGET_NAME}::
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/${TARGET_NAME}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}"
)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
