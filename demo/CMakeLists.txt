cmake_minimum_required(VERSION 3.22)
project(KernelProfilerDemo
    DESCRIPTION "Demo for the KernelProfiler Library"
    LANGUAGES CXX
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# for std::format
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE RelWithDebInfo)


set(BLIS_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/TPL/BLIS)
set(BLIS_INCLUDE_DIR ${BLIS_PREFIX}/include)
set(BLIS_LIBRARY ${BLIS_PREFIX}/lib/libblis.so)

if(NOT ((EXISTS "${BLIS_LIBRARY}") AND (EXISTS "${BLIS_INCLUDE_DIR}") AND (IS_DIRECTORY "${BLIS_INCLUDE_DIR}")))
    file(GLOB_RECURSE INCLUDE_FILES "${BLIS_INCLUDE_DIR}/*")
    if(NOT INCLUDE_FILES)

        include(ExternalProject)
        ExternalProject_Add(
            BLIS
            GIT_REPOSITORY https://github.com/flame/blis.git
            GIT_TAG 2.0
            BUILD_IN_SOURCE true
            CONFIGURE_COMMAND ./configure --prefix=${BLIS_PREFIX} auto
            BUILD_COMMAND make -j8
            INSTALL_COMMAND make install
        )

    endif()
endif()

include_directories(../include)
include_directories(include)
include_directories(${BLIS_INCLUDE_DIR})



set(DEMO_HEADERS
    include/kernel.hpp
    include/csvhelper.hpp
)


set(DEMO_SOURCES
    src/main.cpp
)

add_executable(${TARGET_NAME}_demo ${DEMO_SOURCES} ${DEMO_HEADERS})
target_link_libraries(${TARGET_NAME}_demo ${BLIS_LIBRARY})


