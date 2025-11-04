cmake_minimum_required(VERSION 3.22)

project(KernelProfiler
        DESCRIPTION
            "A simple kernel profiling library written in C++"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES
    src/utils.cpp
    src/harness.cpp
    src/profiler.cpp
)

set(HEADERS
    include/harness.hpp
    include/profiler.hpp
    include/utils.hpp
)

set(CMAKE_BUILD_TYPE Debug)

set(TARGET_NAME KernelProfiler)

add_library(${TARGET_NAME} ${SOURCES} ${HEADERS})
add_library(${TARGET_NAME}::GEMMProfiler ALIAS ${TARGET_NAME})


set(Kokkos_ROOT "$ENV{HOME}/Kokkos" CACHE STRING "Install directory of Kokkos")
set(KokkosKernels_ROOT "$ENV{HOME}/KokkosKernels" CACHE STRING "Install directory of Kokkos Kernels")
# we need to find Kokkos here!
# Assume it is installed in $HOME/Kokkos
list(APPEND CMAKE_PREFIX_PATH Kokkos_ROOT)
list(APPEND CMAKE_PREFIX_PATH KokkosKernels_ROOT)

find_package(BLAS REQUIRED) 
find_package(OpenMP REQUIRED)
find_package(Kokkos REQUIRED CONFIG)

if (Kokkos_FOUND)
    target_include_directories( ${TARGET_NAME}
        PUBLIC 
            ${KokkosKernels_ROOT}/include
    )
else()
    message(ERROR Kokkos_NOTFOUND)
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


target_link_libraries(${TARGET_NAME} PUBLIC ${BLAS_LIBRARIES} OpenMP::OpenMP_CXX Kokkos::kokkos)
